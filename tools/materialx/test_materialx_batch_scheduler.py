#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import copy
import json
import tempfile
import unittest
from pathlib import Path

import materialx_nodedef_ledger
import materialx_batch_scheduler


def make_ledger(count=802, template_ids=()):
    return materialx_nodedef_ledger.build_ledger(
        [
            {
                "id": f"ND_test_{index:04d}",
                "category": "math",
                "types": ["float"],
                "source": "libraries/stdlib/test.mtlx",
            }
            for index in range(count)
        ],
        {
            "schema_version": 1,
            "rows": {node_id: {"next_action": "template"} for node_id in template_ids},
        },
    )


def make_classification_metadata(direct_ids, composed_ids):
    return [
        *[
            {"id": node_id, "classification": "direct_template", "next_action": "template"}
            for node_id in direct_ids
        ],
        *[
            {"id": node_id, "classification": "composed_template", "next_action": "template"}
            for node_id in composed_ids
        ],
    ]


def make_registry(node_ids):
    return [
        {
            "id": node_id,
            "template": "unary_componentwise",
            "types": ["float"],
            "input_order": ["in"],
            "broadcast": False,
        }
        for node_id in node_ids
    ]


def make_capacity(*worker_ids):
    return {"healthy_workers": [{"id": worker_id, "state": "active"} for worker_id in worker_ids]}


class MaterialXBatchSchedulerTest(unittest.TestCase):
    def setUp(self):
        self.direct_ids = [f"ND_test_{index:04d}" for index in range(8)]
        self.composed_ids = [f"ND_test_{index:04d}" for index in range(8, 16)]
        self.ledger = make_ledger(template_ids=self.direct_ids + self.composed_ids)
        self.classification_metadata = make_classification_metadata(self.direct_ids, self.composed_ids)
        self.registry = make_registry(self.direct_ids + self.composed_ids)

    def test_assigns_deterministic_owned_batches_with_direct_templates_first(self):
        schedule = materialx_batch_scheduler.build_batch_schedule(
            self.ledger, self.registry, self.classification_metadata, make_capacity("worker-b", "worker-a")
        )

        self.assertEqual([batch["worker_id"] for batch in schedule["batches"]], ["worker-a", "worker-b"])
        self.assertEqual(schedule["batches"][0]["node_defs"], self.direct_ids)
        self.assertEqual(schedule["batches"][1]["node_defs"], self.composed_ids)
        self.assertEqual(schedule["batches"][0]["exception_budget"], 0)
        self.assertEqual(
            schedule["batches"][0]["focused_test_command"],
            "cycles_test --gtest_filter=MaterialXSemantic.unary_componentwise",
        )
        self.assertEqual(schedule["batches"][0]["generated_evidence_tier"], "generated_semantic_template")
        self.assertEqual(
            materialx_batch_scheduler.schedule_as_json(schedule),
            materialx_batch_scheduler.schedule_as_json(schedule),
        )

    def test_rejects_idle_healthy_worker(self):
        ledger = make_ledger(template_ids=self.direct_ids)
        with self.assertRaisesRegex(ValueError, "idle healthy worker"):
            materialx_batch_scheduler.build_batch_schedule(
                ledger,
                make_registry(self.direct_ids),
                make_classification_metadata(self.direct_ids, []),
                make_capacity("worker-a", "worker-b"),
            )

    def test_rejects_overlap_and_incomplete_batch_records(self):
        schedule = materialx_batch_scheduler.build_batch_schedule(
            self.ledger, self.registry, self.classification_metadata, make_capacity("worker-a", "worker-b")
        )
        overlapping = copy.deepcopy(schedule)
        overlapping["batches"][1]["node_defs"][0] = overlapping["batches"][0]["node_defs"][0]
        with self.assertRaisesRegex(ValueError, "overlap"):
            materialx_batch_scheduler.validate_batch_schedule(overlapping, ["worker-a", "worker-b"])

        incomplete = copy.deepcopy(schedule)
        incomplete["batches"][0]["node_defs"] = [incomplete["batches"][0]["node_defs"][0]]
        with self.assertRaisesRegex(ValueError, "between 8 and 16"):
            materialx_batch_scheduler.validate_batch_schedule(incomplete, ["worker-a", "worker-b"])

    def test_rejects_registry_entry_missing_required_semantic_metadata(self):
        ledger = make_ledger(template_ids=self.direct_ids)
        registry = make_registry(self.direct_ids)
        del registry[0]["template"]
        with self.assertRaisesRegex(ValueError, "Registry row fields"):
            materialx_batch_scheduler.build_batch_schedule(
                ledger, registry, make_classification_metadata(self.direct_ids, []), make_capacity("worker-a")
            )

    def test_rejects_metadata_for_completed_phase2_or_active_node_defs(self):
        metadata = make_classification_metadata(self.direct_ids, [])
        active_manifest = {"layer": "native_cycles", "node_defs": [self.direct_ids[0]]}
        for kwargs, message in (
            ({"completed_ids": {self.direct_ids[0]}}, "completed NodeDef"),
            ({"phase2_ids": {self.direct_ids[0]}}, "Phase-2 NodeDef"),
            ({"active_manifests": [active_manifest]}, "active NodeDef"),
        ):
            with self.subTest(kwargs=kwargs), self.assertRaisesRegex(ValueError, message):
                materialx_batch_scheduler.build_template_candidates(
                    self.ledger, self.registry, metadata, **kwargs
                )

    def test_rejects_unknown_metadata_and_cross_layer_active_ownership(self):
        unknown = make_classification_metadata(self.direct_ids, [])
        unknown[0]["id"] = "ND_unknown"
        with self.assertRaisesRegex(ValueError, "unknown ledger row"):
            materialx_batch_scheduler.build_template_candidates(self.ledger, self.registry, unknown)

        manifests = [
            {"layer": "native_cycles", "node_defs": [self.direct_ids[0]]},
            {"layer": "hydra_ovrtx", "node_defs": [self.direct_ids[0]]},
        ]
        with self.assertRaisesRegex(ValueError, "active manifest overlap"):
            materialx_batch_scheduler.build_template_candidates(
                self.ledger, self.registry, make_classification_metadata(self.direct_ids, []),
                active_manifests=manifests,
            )

    def test_requires_complete_metadata_and_semantics_for_ledger_template_rows(self):
        with self.assertRaisesRegex(ValueError, "missing classification metadata"):
            materialx_batch_scheduler.build_template_candidates(
                self.ledger,
                self.registry,
                make_classification_metadata(self.direct_ids, []),
            )

        with self.assertRaisesRegex(ValueError, "missing semantic registry metadata"):
            materialx_batch_scheduler.build_template_candidates(
                self.ledger,
                make_registry(self.direct_ids),
                self.classification_metadata,
            )

    def test_rejects_metadata_for_non_template_ledger_rows(self):
        metadata = make_classification_metadata(self.direct_ids, [])
        metadata.append(
            {"id": "ND_test_0016", "classification": "direct_template", "next_action": "template"}
        )
        with self.assertRaisesRegex(ValueError, "non-template ledger row"):
            materialx_batch_scheduler.build_template_candidates(self.ledger, self.registry, metadata)

    def test_rejects_overlapping_phase2_and_active_ownership(self):
        manifests = [{"layer": "native_cycles", "node_defs": [self.direct_ids[0]]}]
        with self.assertRaisesRegex(ValueError, "Phase-2 and active NodeDef overlap"):
            materialx_batch_scheduler.build_template_candidates(
                self.ledger,
                self.registry,
                self.classification_metadata,
                phase2_ids={self.direct_ids[0]},
                active_manifests=manifests,
            )

    def test_cli_requires_ownership_files_and_rejects_owned_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            paths = {
                "ledger": directory / "ledger.json",
                "registry": directory / "registry.json",
                "metadata": directory / "metadata.json",
                "capacity": directory / "capacity.json",
                "completed": directory / "completed.json",
                "phase2": directory / "phase2.json",
                "active": directory / "active.json",
            }
            payloads = {
                "ledger": self.ledger,
                "registry": self.registry,
                "metadata": self.classification_metadata,
                "capacity": make_capacity("worker-a", "worker-b"),
                "completed": [],
                "phase2": [],
                "active": [],
            }
            for name, payload in payloads.items():
                paths[name].write_text(json.dumps(payload), encoding="utf-8")
            common_args = [
                "--ledger", str(paths["ledger"]),
                "--semantic-registry", str(paths["registry"]),
                "--classification-metadata", str(paths["metadata"]),
                "--capacity", str(paths["capacity"]),
            ]
            with self.assertRaises(SystemExit) as context:
                materialx_batch_scheduler.main(common_args)
            self.assertEqual(context.exception.code, 2)

            for name, ownership in (
                ("completed", {"completed": [self.direct_ids[0]]}),
                ("phase2", {"phase2": [self.direct_ids[0]]}),
                ("active", {"active": [{"layer": "native_cycles", "node_defs": [self.direct_ids[0]]}]}),
            ):
                for ownership_name in ("completed", "phase2", "active"):
                    paths[ownership_name].write_text(
                        json.dumps(ownership.get(ownership_name, [])), encoding="utf-8"
                    )
                with self.subTest(name=name):
                    self.assertEqual(
                        materialx_batch_scheduler.main(
                            common_args + [
                                "--completed-ids", str(paths["completed"]),
                                "--phase2-ids", str(paths["phase2"]),
                                "--active-manifests", str(paths["active"]),
                            ]
                        ),
                        1,
                    )


if __name__ == "__main__":
    unittest.main()
