#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import copy
import json
import tempfile
import unittest
from pathlib import Path

try:
    import materialx_batch_scheduler
    import materialx_nodedef_ledger
    from materialx_velocity_manifest import EXPECTED_HORDE_WORKERS, validate_batch_manifest
except ModuleNotFoundError:
    from tools.materialx import materialx_batch_scheduler, materialx_nodedef_ledger
    from tools.materialx.materialx_velocity_manifest import EXPECTED_HORDE_WORKERS, validate_batch_manifest


SHA = "a" * 40


def make_ledger(template_ids):
    return materialx_nodedef_ledger.build_ledger(
        [{"id": f"ND_test_{index:04d}", "category": "math", "types": ["float"], "source": "libraries/stdlib/test.mtlx"} for index in range(802)],
        {"schema_version": 1, "rows": {node_id: {"next_action": "template"} for node_id in template_ids}},
    )


def signature(index):
    return {"operation": f"operation-{index}", "input_types": ["float"], "output_type": "float", "broadcast_policy": "none", "output_socket_class": "float"}


def make_inputs():
    ids = [f"ND_test_{index:04d}" for index in range(40)]
    classifications = [{"id": node_id, "classification": "direct_template", "next_action": "template"} for node_id in ids]
    registrations = [
        {"id": node_id, "template": "unary_componentwise", "types": ["float"], "input_order": ["in"], "broadcast": False,
         "family_id": f"family-{index // 8}", "template_signature": signature(index // 8)}
        for index, node_id in enumerate(ids)
    ]
    roles = {
        "blend05": {"implementation": "blend05", "generated_tests": "blendit04", "independent_review": "blendit"},
        "blendit04": {"implementation": "blendit04", "generated_tests": "blendit", "independent_review": "blendit2"},
        "blendit": {"implementation": "blendit", "generated_tests": "blendit2", "independent_review": "blendit3"},
        "blendit2": {"implementation": "blendit2", "generated_tests": "blendit3", "independent_review": "blend05"},
        "blendit3": {"implementation": "blendit3", "generated_tests": "blend05", "independent_review": "blendit04"},
    }
    return {
        "ledger": make_ledger(ids), "registry": registrations, "metadata": classifications,
        "capacity": {"healthy_workers": [{"id": worker, "state": "active"} for worker in EXPECTED_HORDE_WORKERS]},
        "integration_base_sha": SHA, "probed_worker_shas": {worker: SHA for worker in EXPECTED_HORDE_WORKERS},
        "layer": "native_cycles", "role_allocations": roles,
        "files_allowlists": {worker: [f"intern/cycles/{worker}.cpp"] for worker in EXPECTED_HORDE_WORKERS},
        "completed_ids": [], "phase2_ids": [], "active_manifests": [],
    }


def call_schedule(inputs, **overrides):
    args = {
        "integration_base_sha": inputs["integration_base_sha"],
        "probed_worker_shas": inputs["probed_worker_shas"],
        "layer": inputs["layer"],
        "role_allocations": inputs["role_allocations"],
        "files_allowlists": inputs["files_allowlists"],
        "completed_ids": inputs["completed_ids"],
        "phase2_ids": inputs["phase2_ids"],
        "active_manifests": inputs["active_manifests"],
    }
    args.update(overrides)
    return materialx_batch_scheduler.build_batch_schedule(inputs["ledger"], inputs["registry"], inputs["metadata"], inputs["capacity"], **args)


def registered_families(inputs):
    records = {}
    for row in inputs["registry"]:
        record = records.setdefault(
            row["family_id"],
            {
                "template_signature": row["template_signature"],
                "node_defs": [],
                "generated_evidence_tier": "generated_semantic_template",
                "focused_test_commands": [f"cycles_test --gtest_filter=MaterialXSemantic.{row['template']}"],
            },
        )
        record["node_defs"].append(row["id"])
    return records


class MaterialXBatchSchedulerTest(unittest.TestCase):
    def test_emits_one_valid_homogeneous_family_manifest_for_each_horde_worker(self):
        inputs = make_inputs()
        schedule = call_schedule(inputs)
        self.assertEqual(schedule["schema_version"], 2)
        self.assertEqual(schedule["ledger_rows"], 802)
        self.assertEqual(set(schedule["assignments"]), set(EXPECTED_HORDE_WORKERS))
        self.assertEqual(len(schedule["assignments"]), 5)
        seen_files = set()
        for worker, assignment in schedule["assignments"].items():
            with self.subTest(worker=worker):
                self.assertEqual(assignment["roles"]["implementation"], worker)
                self.assertEqual(assignment["worker_source_sha"], SHA)
                self.assertGreaterEqual(len(assignment["node_defs"]), 8)
                self.assertLessEqual(len(assignment["node_defs"]), 16)
                self.assertEqual(set(assignment["roles"]), {"implementation", "generated_tests", "independent_review"})
                self.assertEqual(len(set(assignment["roles"].values())), 3)
                self.assertFalse(seen_files.intersection(assignment["files_allowlist"]))
                seen_files.update(assignment["files_allowlist"])
                validate_batch_manifest(assignment, registered_families=registered_families(inputs))

    def test_rejects_missing_horde_capacity_or_dispatchable_family_window(self):
        inputs = make_inputs()
        short_capacity = copy.deepcopy(inputs["capacity"])
        short_capacity["healthy_workers"].pop()
        with self.assertRaisesRegex(ValueError, "exactly the five"):
            materialx_batch_scheduler.build_batch_schedule(inputs["ledger"], inputs["registry"], inputs["metadata"], short_capacity, integration_base_sha=SHA, probed_worker_shas=inputs["probed_worker_shas"], layer="native_cycles", role_allocations=inputs["role_allocations"], files_allowlists=inputs["files_allowlists"])
        short_inputs = make_inputs()
        short_ids = {f"ND_test_{index:04d}" for index in range(32)}
        short_inputs["ledger"] = make_ledger(short_ids)
        short_inputs["metadata"] = [row for row in short_inputs["metadata"] if row["id"] in short_ids]
        short_inputs["registry"] = [row for row in short_inputs["registry"] if row["id"] in short_ids]
        with self.assertRaisesRegex(ValueError, "queue"):
            call_schedule(short_inputs)

    def test_rejects_overlapping_allowlists_and_more_than_one_complex_exception(self):
        inputs = make_inputs()
        overlap = copy.deepcopy(inputs["files_allowlists"])
        overlap["blendit04"] = overlap["blend05"]
        with self.assertRaisesRegex(ValueError, "allowlists"):
            call_schedule(inputs, files_allowlists=overlap)
        exception = {
            "batch_id": "exception-001", "batch_kind": "complex_exception", "family_id": "family-0", "template_signature": signature(0),
            "node_defs": ["ND_test_0400"], "focused_test_commands": ["cycles_test --gtest_filter=MaterialXSemantic.unary_componentwise"],
            "generated_evidence_tier": "generated_semantic_template", "exception_budget": 1,
            "red_test": "RED_TEST-1a2b3c4d", "approval_record": "APPROVAL-1a2b3c4d",
        }
        with self.assertRaisesRegex(ValueError, "more than one complex exception"):
            call_schedule(inputs, complex_exceptions=[exception, dict(exception, batch_id="exception-002", node_defs=["ND_test_0401"])])

    def test_defers_extra_complete_groups_and_incomplete_tails_without_node_loss(self):
        inputs = make_inputs()
        extra_ids = [f"ND_test_{index:04d}" for index in range(40, 56)]
        tail_ids = [f"ND_test_{index:04d}" for index in range(56, 63)]
        all_ids = [row["id"] for row in inputs["metadata"]] + extra_ids + tail_ids
        inputs["ledger"] = make_ledger(all_ids)
        for node_id in extra_ids + tail_ids:
            family = 5 if node_id in extra_ids else 6
            inputs["metadata"].append({"id": node_id, "classification": "direct_template", "next_action": "template"})
            inputs["registry"].append({"id": node_id, "template": "unary_componentwise", "types": ["float"], "input_order": ["in"], "broadcast": False, "family_id": f"family-{family}", "template_signature": signature(family)})
        schedule = call_schedule(inputs)
        assigned = {node_id for assignment in schedule["assignments"].values() for node_id in assignment["node_defs"]}
        self.assertEqual(schedule["deferred_node_defs"], sorted(extra_ids + tail_ids))
        self.assertFalse(assigned.intersection(schedule["deferred_node_defs"]))
        self.assertEqual(assigned.union(schedule["deferred_node_defs"]), set(all_ids))

    def test_rejects_multiple_complex_exceptions_during_schedule_validation(self):
        inputs = make_inputs()
        schedule = call_schedule(inputs)
        assignments = list(schedule["assignments"])
        first, second = assignments[:2]
        for worker in (first, second):
            schedule["assignments"][worker].update({"batch_kind": "complex_exception", "node_defs": schedule["assignments"][worker]["node_defs"][:7], "exception_budget": 1, "red_test": "RED_TEST-1a2b3c4d", "approval_record": "APPROVAL-1a2b3c4d"})
        with self.assertRaisesRegex(ValueError, "more than one complex exception"):
            materialx_batch_scheduler.validate_batch_schedule(schedule, registered_families=registered_families(inputs), candidate_node_defs=[row["id"] for row in inputs["metadata"]])

    def test_rejects_metadata_for_completed_phase2_or_active_node_defs(self):
        inputs = make_inputs()
        node_id = inputs["metadata"][0]["id"]
        for kwargs, message in (({"completed_ids": {node_id}}, "completed NodeDef"), ({"phase2_ids": {node_id}}, "Phase-2 NodeDef"), ({"active_manifests": [{"layer": "native_cycles", "node_defs": [node_id]}]}, "active NodeDef")):
            with self.subTest(kwargs=kwargs), self.assertRaisesRegex(ValueError, message):
                materialx_batch_scheduler.build_template_candidates(inputs["ledger"], inputs["registry"], inputs["metadata"], **kwargs)

    def test_rejects_unknown_metadata_and_cross_layer_active_ownership(self):
        inputs = make_inputs()
        metadata = copy.deepcopy(inputs["metadata"])
        metadata[0]["id"] = "ND_unknown"
        with self.assertRaisesRegex(ValueError, "unknown ledger row"):
            materialx_batch_scheduler.build_template_candidates(inputs["ledger"], inputs["registry"], metadata)
        node_id = inputs["metadata"][0]["id"]
        with self.assertRaisesRegex(ValueError, "active manifest overlap"):
            materialx_batch_scheduler.build_template_candidates(inputs["ledger"], inputs["registry"], inputs["metadata"], active_manifests=[{"layer": "native_cycles", "node_defs": [node_id]}, {"layer": "hydra_ovrtx", "node_defs": [node_id]}])

    def test_requires_complete_metadata_and_schedulable_registry_rows(self):
        inputs = make_inputs()
        with self.assertRaisesRegex(ValueError, "missing classification metadata"):
            materialx_batch_scheduler.build_template_candidates(inputs["ledger"], inputs["registry"], inputs["metadata"][:-1])
        legacy = copy.deepcopy(inputs["registry"])
        legacy[0].pop("family_id")
        legacy[0].pop("template_signature")
        with self.assertRaisesRegex(ValueError, "schedulable"):
            materialx_batch_scheduler.build_template_candidates(inputs["ledger"], legacy, inputs["metadata"])

    def test_rejects_non_template_metadata_and_nonstring_classification(self):
        inputs = make_inputs()
        metadata = copy.deepcopy(inputs["metadata"])
        metadata.append({"id": "ND_test_0100", "classification": "direct_template", "next_action": "template"})
        with self.assertRaisesRegex(ValueError, "non-template"):
            materialx_batch_scheduler.build_template_candidates(inputs["ledger"], inputs["registry"], metadata)
        bad = copy.deepcopy(inputs["metadata"])
        bad[0]["classification"] = []
        with self.assertRaisesRegex(ValueError, "fields must be strings"):
            materialx_batch_scheduler.build_template_candidates(inputs["ledger"], inputs["registry"], bad)

    def test_schedule_validation_rejects_overlap_and_incomplete_assignment(self):
        inputs = make_inputs()
        schedule = call_schedule(inputs)
        candidates = [row["id"] for row in inputs["metadata"]]
        first = next(iter(schedule["assignments"]))
        overlapping = copy.deepcopy(schedule)
        overlapping["deferred_node_defs"] = [overlapping["assignments"][first]["node_defs"][0]]
        with self.assertRaisesRegex(ValueError, "overlap"):
            materialx_batch_scheduler.validate_batch_schedule(overlapping, registered_families=registered_families(inputs), candidate_node_defs=candidates)
        incomplete = copy.deepcopy(schedule)
        incomplete["assignments"][first]["node_defs"] = incomplete["assignments"][first]["node_defs"][:1]
        with self.assertRaisesRegex(ValueError, "8-16 NodeDefs"):
            materialx_batch_scheduler.validate_batch_schedule(incomplete, registered_families=registered_families(inputs), candidate_node_defs=candidates)

    def test_cli_requires_all_ownership_and_v2_inputs(self):
        inputs = make_inputs()
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            paths = {name: directory / f"{name}.json" for name in ("ledger", "registry", "metadata", "capacity", "completed", "phase2", "active", "shas", "roles", "allowlists")}
            payloads = {"ledger": inputs["ledger"], "registry": inputs["registry"], "metadata": inputs["metadata"], "capacity": inputs["capacity"], "completed": [], "phase2": [], "active": [], "shas": inputs["probed_worker_shas"], "roles": inputs["role_allocations"], "allowlists": inputs["files_allowlists"]}
            for name, payload in payloads.items():
                paths[name].write_text(json.dumps(payload), encoding="utf-8")
            args = ["--ledger", str(paths["ledger"]), "--semantic-registry", str(paths["registry"]), "--classification-metadata", str(paths["metadata"]), "--capacity", str(paths["capacity"]), "--completed-ids", str(paths["completed"]), "--phase2-ids", str(paths["phase2"]), "--active-manifests", str(paths["active"])]
            with self.assertRaises(SystemExit) as context:
                materialx_batch_scheduler.main(args)
            self.assertEqual(context.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
