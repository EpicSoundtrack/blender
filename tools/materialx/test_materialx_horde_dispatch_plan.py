#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import inspect
import tempfile
import unittest
from pathlib import Path

import materialx_horde_dispatch_plan


class MaterialXHordeDispatchPlanTest(unittest.TestCase):
    def test_plan_is_deterministic_and_requires_all_guardrails_once(self):
        plan = materialx_horde_dispatch_plan.build_dispatch_plan(
            ["gpu-b", "gpu-a"],
            "C:/secure/horde.env",
            {"batch_id": "color4-reader", "tests": ["Cycles.Color4"]},
        )

        self.assertEqual(plan["workers"], ["gpu-a", "gpu-b"])
        self.assertEqual(plan["credential_file"], "C:/secure/horde.env")
        self.assertEqual(plan["batch_manifest"], {
            "batch_id": "color4-reader",
            "tests": ["Cycles.Color4"],
        })
        steps = {step["id"]: step for step in plan["required_steps"]}
        self.assertEqual(set(steps), {
            "structural_credential_validation",
            "no_write_probe",
            "persist_nvidia_api_key",
            "hermes_process_check",
            "immediate_capacity_alert",
        })
        self.assertEqual(steps["structural_credential_validation"]["required_key"], "NVIDIA_API_KEY")
        self.assertEqual(steps["no_write_probe"]["write_policy"], "forbidden")
        self.assertEqual(steps["persist_nvidia_api_key"], {
            "id": "persist_nvidia_api_key",
            "kind": "environment_assignment",
            "persistence": "exactly_once",
            "variable": "NVIDIA_API_KEY",
        })
        self.assertEqual(steps["hermes_process_check"]["process"], "Hermes")
        self.assertEqual(steps["hermes_process_check"]["scope"], "real_process")
        self.assertEqual(
            sum(step.get("variable") == "NVIDIA_API_KEY" for step in plan["required_steps"]),
            1,
        )
        self.assertEqual(plan["failure_alert"], {
            "batch_id": "color4-reader",
            "kind": "capacity_alert",
            "timing": "immediate",
            "workers": ["gpu-a", "gpu-b"],
        })
        self.assertEqual(
            materialx_horde_dispatch_plan.plan_as_json(plan),
            materialx_horde_dispatch_plan.plan_as_json(plan),
        )

    def test_credential_validation_reports_only_structure_not_value(self):
        with tempfile.TemporaryDirectory() as directory:
            credential_file = Path(directory) / "horde.env"
            credential_file.write_text("NVIDIA_API_KEY=not-a-real-secret\n", encoding="utf-8")

            structure = materialx_horde_dispatch_plan.validate_credential_file(credential_file)

        self.assertEqual(structure, {
            "path": str(credential_file),
            "required_key": "NVIDIA_API_KEY",
            "format": "assignment",
            "status": "valid",
        })
        self.assertNotIn("not-a-real-secret", repr(structure))

    def test_credential_validation_rejects_missing_duplicate_and_malformed_keys(self):
        with tempfile.TemporaryDirectory() as directory:
            credential_file = Path(directory) / "horde.env"
            credential_file.write_text("OTHER_KEY=value\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "exactly one NVIDIA_API_KEY"):
                materialx_horde_dispatch_plan.validate_credential_file(credential_file)

            credential_file.write_text("NVIDIA_API_KEY=one\nNVIDIA_API_KEY=two\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "exactly one NVIDIA_API_KEY"):
                materialx_horde_dispatch_plan.validate_credential_file(credential_file)

            credential_file.write_text("NVIDIA_API_KEY\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "KEY=VALUE"):
                materialx_horde_dispatch_plan.validate_credential_file(credential_file)

    def test_credential_validation_accepts_exactly_three_raw_tokens_without_exposing_them(self):
        with tempfile.TemporaryDirectory() as directory:
            credential_file = Path(directory) / "horde.token"
            credential_file.write_text("first-token second-token third-token\n", encoding="utf-8")

            structure = materialx_horde_dispatch_plan.validate_credential_file(credential_file)

        self.assertEqual(structure["required_key"], "NVIDIA_API_KEY")
        self.assertEqual(structure["format"], "raw_three_token")
        self.assertNotIn("first-token", repr(structure))

    def test_planner_rejects_unsafe_inputs_and_never_executes_remote_work(self):
        with self.assertRaisesRegex(ValueError, "at least one worker"):
            materialx_horde_dispatch_plan.build_dispatch_plan([], "horde.env", {"batch_id": "batch"})
        with self.assertRaisesRegex(ValueError, "duplicate"):
            materialx_horde_dispatch_plan.build_dispatch_plan(
                ["gpu-a", "gpu-a"], "horde.env", {"batch_id": "batch"})
        with self.assertRaisesRegex(ValueError, "batch_id"):
            materialx_horde_dispatch_plan.build_dispatch_plan(["gpu-a"], "horde.env", {})

        source = inspect.getsource(materialx_horde_dispatch_plan)
        self.assertNotIn("subprocess", source)
        self.assertNotIn("ssh", source.lower())


if __name__ == "__main__":
    unittest.main()
