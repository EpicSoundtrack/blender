#!/usr/bin/env python3

import copy
import unittest

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


class MaterialXBatchSchedulerTest(unittest.TestCase):
    def test_emits_one_valid_homogeneous_family_manifest_for_each_horde_worker(self):
        inputs = make_inputs()
        schedule = materialx_batch_scheduler.build_batch_schedule(
            inputs["ledger"], inputs["registry"], inputs["metadata"], inputs["capacity"],
            integration_base_sha=inputs["integration_base_sha"], probed_worker_shas=inputs["probed_worker_shas"],
            layer=inputs["layer"], role_allocations=inputs["role_allocations"], files_allowlists=inputs["files_allowlists"],
            completed_ids=inputs["completed_ids"], phase2_ids=inputs["phase2_ids"], active_manifests=inputs["active_manifests"],
        )
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
                validate_batch_manifest(assignment, registered_families={f"family-{index}" for index in range(5)})

    def test_rejects_missing_horde_capacity_or_dispatchable_family_window(self):
        inputs = make_inputs()
        short_capacity = copy.deepcopy(inputs["capacity"])
        short_capacity["healthy_workers"].pop()
        with self.assertRaisesRegex(ValueError, "exactly the five"):
            materialx_batch_scheduler.build_batch_schedule(
                inputs["ledger"], inputs["registry"], inputs["metadata"], short_capacity,
                integration_base_sha=SHA, probed_worker_shas=inputs["probed_worker_shas"], layer="native_cycles",
                role_allocations=inputs["role_allocations"], files_allowlists=inputs["files_allowlists"],
            )
        short_inputs = make_inputs()
        short_ids = {f"ND_test_{index:04d}" for index in range(32)}
        short_inputs["ledger"] = make_ledger(short_ids)
        short_inputs["metadata"] = [row for row in short_inputs["metadata"] if row["id"] in short_ids]
        short_inputs["registry"] = [row for row in short_inputs["registry"] if row["id"] in short_ids]
        with self.assertRaisesRegex(ValueError, "queue"):
            materialx_batch_scheduler.build_batch_schedule(
                short_inputs["ledger"], short_inputs["registry"], short_inputs["metadata"], short_inputs["capacity"],
                integration_base_sha=SHA, probed_worker_shas=short_inputs["probed_worker_shas"], layer="native_cycles",
                role_allocations=short_inputs["role_allocations"], files_allowlists=short_inputs["files_allowlists"],
            )

    def test_rejects_overlapping_allowlists_and_more_than_one_complex_exception(self):
        inputs = make_inputs()
        overlap = copy.deepcopy(inputs["files_allowlists"])
        overlap["blendit04"] = overlap["blend05"]
        with self.assertRaisesRegex(ValueError, "allowlists"):
            materialx_batch_scheduler.build_batch_schedule(
                inputs["ledger"], inputs["registry"], inputs["metadata"], inputs["capacity"],
                integration_base_sha=SHA, probed_worker_shas=inputs["probed_worker_shas"], layer="native_cycles",
                role_allocations=inputs["role_allocations"], files_allowlists=overlap,
            )
        exception = {
            "batch_id": "exception-001", "batch_kind": "complex_exception", "family_id": "family-0", "template_signature": signature(0),
            "node_defs": ["ND_test_0400"], "focused_test_commands": ["cycles_test --gtest_filter=MaterialXSemantic.unary_componentwise"],
            "generated_evidence_tier": "generated_semantic_template", "exception_budget": 1,
            "red_test": "RED_TEST-1a2b3c4d", "approval_record": "APPROVAL-1a2b3c4d",
        }
        with self.assertRaisesRegex(ValueError, "more than one complex exception"):
            materialx_batch_scheduler.build_batch_schedule(
                inputs["ledger"], inputs["registry"], inputs["metadata"], inputs["capacity"],
                integration_base_sha=SHA, probed_worker_shas=inputs["probed_worker_shas"], layer="native_cycles",
                role_allocations=inputs["role_allocations"], files_allowlists=inputs["files_allowlists"],
                complex_exceptions=[exception, dict(exception, batch_id="exception-002", node_defs=["ND_test_0401"])],
            )
