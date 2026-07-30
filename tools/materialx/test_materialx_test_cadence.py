#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

import copy
import unittest

from materialx_project_state import (
    credit_integration,
    new_project_state,
    set_release_gate,
)
from materialx_test_cadence import build_cadence_decision, execute_cadence
from test_materialx_horde_dispatch_plan import REGISTERED_FAMILIES, make_manifest
from materialx_velocity_manifest import validate_batch_manifest


LAYERS = ("native_cycles", "hydra_ovrtx", "blender_authoring")


def config(interval=3):
    return {
        "schema_version": 1,
        "full_suite_interval": interval,
        "full_suite_commands": {
            layer: [["python", "-m", "unittest", f"full_{layer}"]]
            for layer in LAYERS
        },
        "lane_commands": {
            "local_cpu": [["blender", "--background", "--device", "CPU"]],
            "local_cuda": [["blender", "--background", "--device", "CUDA"]],
            "windows_a40_cuda": [["blender", "--background", "--device", "A40"]],
            "golden_review": [["materialx-golden-review", "--release-gate"]],
        },
    }


def integration_record(batch_id="add-a", *, layer="native_cycles", node_start=0):
    assignment = make_manifest(batch_id, node_start=node_start)
    assignment["layer"] = layer
    assignment = validate_batch_manifest(
        assignment, registered_families=REGISTERED_FAMILIES
    )
    receipt = {
        "batch_id": batch_id,
        "layer": layer,
        "base_sha": assignment["integration_base_sha"],
        "head_sha": "b" * 40,
        "focused_commands": list(assignment["focused_test_commands"]),
        "numeric_exits": [0] * len(assignment["focused_test_commands"]),
        "final_state": "integrated",
    }
    return {"assignment": assignment, "receipt": receipt}


def independent_integrations(count):
    families = {}
    records = []
    for index in range(count):
        family_id = f"family_{index}"
        assignment = make_manifest(f"batch-{index}")
        assignment["family_id"] = family_id
        assignment["template_signature"] = {
            **assignment["template_signature"],
            "operation": family_id,
        }
        assignment["node_defs"] = [
            f"ND_{family_id}_{node_index}" for node_index in range(8)
        ]
        assignment["files_allowlist"] = [f"intern/cycles/{family_id}.cpp"]
        families[family_id] = [{
            "template_signature": assignment["template_signature"],
            "node_defs": list(assignment["node_defs"]),
            "generated_evidence_tier": assignment["generated_evidence_tier"],
            "focused_test_commands": list(assignment["focused_test_commands"]),
        }]
        assignment = validate_batch_manifest(
            assignment, registered_families=families
        )
        records.append({
            "assignment": assignment,
            "receipt": {
                "batch_id": assignment["batch_id"],
                "layer": assignment["layer"],
                "base_sha": assignment["integration_base_sha"],
                "head_sha": f"{index + 1:040x}",
                "focused_commands": list(assignment["focused_test_commands"]),
                "numeric_exits": [0],
                "final_state": "integrated",
            },
        })
    return families, records


class MaterialXTestCadenceTest(unittest.TestCase):
    def test_every_integrated_family_requires_registered_focused_commands(self):
        decision = build_cadence_decision(
            integrations=[integration_record()],
            project_state=new_project_state(),
            cadence_config=config(),
            registered_families=REGISTERED_FAMILIES,
        )

        self.assertEqual(decision["families"], ["add"])
        self.assertEqual(decision["node_defs"], [f"ND_add_float_{i}" for i in range(8)])
        self.assertEqual(decision["affected_layers"], ["native_cycles"])
        self.assertEqual(decision["milestone_generation"], 0)
        self.assertEqual(
            [item["argv"] for item in decision["commands"]],
            [["cycles_test", "--gtest_filter=MaterialXSemantic.Add"]],
        )
        self.assertEqual(decision["commands"][0]["tier"], "focused")
        self.assertEqual(decision["reason"], ["new_integrated_family"])
        self.assertNotIn("prompt", repr(decision).lower())

    def test_batch_crossing_interval_emits_one_affected_layer_full_suite(self):
        state = new_project_state()
        state["integration_receipts"] = [
            {
                "batch_id": f"old-{index}",
                "layer": "native_cycles",
                "base_sha": "a" * 40,
                "head_sha": chr(ord("b") + index) * 40,
                "final_state": "integrated",
            }
            for index in range(2)
        ]
        decision = build_cadence_decision(
            integrations=[
                integration_record("add-b", node_start=8),
                integration_record("add-a"),
            ],
            project_state=state,
            cadence_config=config(3),
            registered_families=REGISTERED_FAMILIES,
        )

        self.assertEqual(
            [command["tier"] for command in decision["commands"]],
            ["focused", "focused", "full"],
        )
        self.assertEqual(decision["commands"][-1]["argv"], [
            "python", "-m", "unittest", "full_native_cycles"
        ])
        self.assertEqual(decision["reason"], [
            "full_suite_interval",
            "new_integrated_family",
        ])

    def test_interval_is_exact_non_boolean_integer_three_through_five(self):
        for invalid in (True, 2, 6, 3.0):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                build_cadence_decision(
                    integrations=[integration_record()],
                    project_state=new_project_state(),
                    cadence_config=config(invalid),
                    registered_families=REGISTERED_FAMILIES,
                )

    def test_secret_like_or_shell_control_argv_is_rejected(self):
        for argv in (
            ["runner", "NVIDIA_API_KEY=private"],
            ["runner", "|", "other"],
        ):
            invalid = config()
            invalid["full_suite_commands"]["native_cycles"] = [argv]
            with self.subTest(argv=argv), self.assertRaises(ValueError):
                build_cadence_decision(
                    integrations=[],
                    project_state=new_project_state(),
                    cadence_config=invalid,
                    registered_families=REGISTERED_FAMILIES,
                )

    def test_batching_cannot_skip_multiple_full_suite_boundaries(self):
        families, records = independent_integrations(7)
        decision = build_cadence_decision(
            integrations=records,
            project_state=new_project_state(),
            cadence_config=config(3),
            registered_families=families,
        )
        full = [
            command for command in decision["commands"]
            if command["tier"] == "full"
        ]
        self.assertEqual(
            [command["scope"] for command in full],
            ["native_cycles:3", "native_cycles:6"],
        )
        self.assertEqual(
            [command["argv"] for command in full],
            [
                ["python", "-m", "unittest", "full_native_cycles"],
                ["python", "-m", "unittest", "full_native_cycles"],
            ],
        )

    def test_noncanonical_receipt_assignment_or_command_fails_closed(self):
        cases = []
        extra = integration_record()
        extra["receipt"]["stdout"] = "private"
        cases.append(extra)
        stale = integration_record()
        stale["receipt"]["head_sha"] = stale["receipt"]["base_sha"]
        cases.append(stale)
        command = integration_record()
        command["receipt"]["focused_commands"] = ["caller supplied"]
        cases.append(command)
        rejected = integration_record()
        rejected["receipt"] = {
            **rejected["receipt"],
            "final_state": "rejected",
            "failure_classification": "focused_test_failure",
        }
        cases.append(rejected)

        for record in cases:
            with self.subTest(record=record["receipt"].get("final_state")):
                decision = build_cadence_decision(
                    integrations=[record, integration_record("valid-a")],
                    project_state=new_project_state(),
                    cadence_config=config(),
                    registered_families=REGISTERED_FAMILIES,
                )
                self.assertEqual(decision["families"], ["add"])
                self.assertEqual(len(decision["commands"]), 1)
                self.assertEqual(decision["commands"][0]["scope"], "valid-a")

    def test_canonical_project_milestones_drive_gpu_and_release_commands(self):
        state = credit_integration(
            new_project_state(local_green_threshold=32, windows_green_threshold=64),
            newly_integrated_nodedefs=32,
            render_path_edit=False,
            batch_id="milestone",
        )
        state = set_release_gate(state, due=True)
        decision = build_cadence_decision(
            integrations=[],
            project_state=state,
            cadence_config=config(),
            registered_families=REGISTERED_FAMILIES,
        )

        self.assertEqual(
            [command["tier"] for command in decision["commands"]],
            ["golden_review", "local_cpu", "local_cuda"],
        )
        self.assertNotIn("windows_a40_cuda", [
            command["tier"] for command in decision["commands"]
        ])
        self.assertEqual(decision["milestone_generation"], 1)
        self.assertEqual(decision["reason"], [
            "explicit_release_gate",
            "project_lane_due",
        ])

    def test_executor_runs_all_commands_and_sanitizes_failures(self):
        state = credit_integration(
            new_project_state(),
            newly_integrated_nodedefs=32,
            render_path_edit=False,
            batch_id="milestone",
        )
        decision = build_cadence_decision(
            integrations=[integration_record()],
            project_state=state,
            cadence_config=config(),
            registered_families=REGISTERED_FAMILIES,
        )
        calls = []

        def runner(argv, *, timeout_seconds):
            calls.append(list(argv))
            self.assertEqual(timeout_seconds, 900)
            if argv[-1] == "CPU":
                raise RuntimeError("private path and token")
            if argv[-1] == "CUDA":
                return {"exit_code": 7}
            return {"exit_code": 0}

        receipts = execute_cadence(decision, runner=runner)

        self.assertEqual(len(calls), len(decision["commands"]))
        self.assertEqual(len(receipts), len(decision["commands"]))
        self.assertIn("runner_exception", {
            receipt["classification"] for receipt in receipts
        })
        self.assertIn("nonzero_exit", {
            receipt["classification"] for receipt in receipts
        })
        self.assertNotIn("private", repr(receipts))
        self.assertTrue(all(
            type(receipt["exit_code"]) is int
            for receipt in receipts
        ))
        self.assertTrue(all(
            set(receipt) == {
                "receipt_id",
                "command_id",
                "argv",
                "tier",
                "milestone_generation",
                "exit_code",
                "passed",
                "failed",
                "classification",
            }
            for receipt in receipts
        ))

    def test_missing_or_malformed_runner_is_categorical_failure_without_credit(self):
        decision = build_cadence_decision(
            integrations=[integration_record()],
            project_state=new_project_state(),
            cadence_config=config(),
            registered_families=REGISTERED_FAMILIES,
        )
        missing = execute_cadence(decision, runner=None)
        malformed = execute_cadence(
            decision,
            runner=lambda argv, *, timeout_seconds: {"exit_code": True},
        )

        self.assertTrue(all(item["classification"] == "missing_runner" for item in missing))
        self.assertTrue(all(item["classification"] == "malformed_result" for item in malformed))
        self.assertTrue(all(item["passed"] == 0 and item["failed"] == 1 for item in missing + malformed))


if __name__ == "__main__":
    unittest.main()
