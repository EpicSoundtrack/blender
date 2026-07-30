#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import copy
import inspect
import json
import re
import tempfile
import unittest
from pathlib import Path

import materialx_horde_dispatch_plan


SHA = "a" * 40
SIGNATURE = {
    "operation": "add",
    "input_types": ["float", "float"],
    "output_type": "float",
    "broadcast_policy": "componentwise",
    "output_socket_class": "float",
}
TEST_COMMAND = "cycles_test --gtest_filter=MaterialXSemantic.Add"
ALL_NODE_DEFS = [f"ND_add_float_{index}" for index in range(16)]
REGISTERED_FAMILIES = {
    "add": [{
        "template_signature": SIGNATURE,
        "node_defs": ALL_NODE_DEFS,
        "generated_evidence_tier": "generated_semantic_template",
        "focused_test_commands": [TEST_COMMAND],
    }],
}


def make_manifest(
    batch_id: str = "add-a",
    *,
    node_start: int = 0,
    roles: dict[str, str] | None = None,
    base_sha: str = SHA,
) -> dict[str, object]:
    return {
        "schema_version": 2,
        "batch_id": batch_id,
        "batch_kind": "family",
        "family_id": "add",
        "template_signature": SIGNATURE,
        "layer": "native_cycles",
        "node_defs": ALL_NODE_DEFS[node_start:node_start + 8],
        "integration_base_sha": base_sha,
        "worker_source_sha": base_sha,
        "roles": roles or {
            "implementation": "blend05",
            "generated_tests": "blendit04",
            "independent_review": "blendit",
        },
        "files_allowlist": [f"intern/cycles/add_{node_start}.cpp"],
        "focused_test_commands": [TEST_COMMAND],
        "generated_evidence_tier": "generated_semantic_template",
        "exception_budget": 0,
        "red_test": "",
        "approval_record": "",
    }


class MaterialXHordeDispatchPlanTest(unittest.TestCase):
    def test_plan_v2_normalizes_assignments_and_derives_workers_and_tasks(self):
        second = make_manifest(
            "add-b",
            node_start=8,
            roles={
                "implementation": "blendit2",
                "generated_tests": "blendit3",
                "independent_review": "blend05",
            },
        )
        plan = materialx_horde_dispatch_plan.build_dispatch_plan(
            [second, make_manifest()],
            "C:/secure/horde.env",
            registered_families=REGISTERED_FAMILIES,
        )

        self.assertEqual(plan["schema_version"], 2)
        self.assertRegex(plan["dispatch_id"], r"^dispatch-[0-9a-f]{24}$")
        self.assertEqual([item["batch_id"] for item in plan["assignments"]], ["add-a", "add-b"])
        self.assertEqual(plan["workers"], ["blend05", "blendit", "blendit04", "blendit2", "blendit3"])
        self.assertEqual(plan["credential_file"], "C:/secure/horde.env")
        self.assertEqual(plan["worker_tasks"], {
            "blend05": [
                {"batch_id": "add-a", "role": "implementation"},
                {"batch_id": "add-b", "role": "independent_review"},
            ],
            "blendit": [{"batch_id": "add-a", "role": "independent_review"}],
            "blendit04": [{"batch_id": "add-a", "role": "generated_tests"}],
            "blendit2": [{"batch_id": "add-b", "role": "implementation"}],
            "blendit3": [{"batch_id": "add-b", "role": "generated_tests"}],
        })
        self.assertNotIn("batch_manifest", plan)
        self.assertNotIn("prompt", json.dumps(plan))
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
        self.assertEqual(plan["failure_alert"]["batch_ids"], ["add-a", "add-b"])
        self.assertEqual(plan["failure_alert"]["workers"], plan["workers"])
        self.assertEqual(
            materialx_horde_dispatch_plan.plan_as_json(plan),
            materialx_horde_dispatch_plan.plan_as_json(plan),
        )

    def test_planner_rejects_prompt_only_and_arbitrary_prose_fields(self):
        for manifest in (
            {"batch_id": "legacy", "prompt": "do arbitrary work"},
            {**make_manifest(), "prompt": "do arbitrary work"},
            {**make_manifest(), "goal": "do arbitrary work"},
            {**make_manifest(), "worker_prompts": {"blend05": "do arbitrary work"}},
        ):
            with self.subTest(fields=set(manifest)), self.assertRaises(ValueError):
                materialx_horde_dispatch_plan.build_dispatch_plan(
                    [manifest],
                    "horde.env",
                    registered_families=REGISTERED_FAMILIES,
                )

    def test_planner_rejects_duplicate_ownership_and_mixed_bases(self):
        valid = make_manifest()
        cases = []
        duplicate_batch = make_manifest("add-a", node_start=8)
        cases.append(("batch_id", duplicate_batch))
        duplicate_nodes = make_manifest("add-b", node_start=0)
        duplicate_nodes["files_allowlist"] = ["intern/cycles/other.cpp"]
        cases.append(("NodeDef", duplicate_nodes))
        duplicate_files = make_manifest("add-b", node_start=8)
        duplicate_files["files_allowlist"] = list(valid["files_allowlist"])
        cases.append(("file", duplicate_files))
        mixed_base = make_manifest("add-b", node_start=8, base_sha="b" * 40)
        cases.append(("integration_base_sha", mixed_base))

        for message, second in cases:
            with self.subTest(message=message), self.assertRaisesRegex(ValueError, message):
                materialx_horde_dispatch_plan.build_dispatch_plan(
                    [valid, second],
                    "horde.env",
                    registered_families=REGISTERED_FAMILIES,
                )

    def test_planner_does_not_mutate_scheduler_assignments(self):
        manifest = make_manifest()
        original = copy.deepcopy(manifest)
        materialx_horde_dispatch_plan.build_dispatch_plan(
            [manifest], "horde.env", registered_families=REGISTERED_FAMILIES
        )
        self.assertEqual(manifest, original)

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
        with self.assertRaisesRegex(ValueError, "at least one"):
            materialx_horde_dispatch_plan.build_dispatch_plan(
                [], "horde.env", registered_families=REGISTERED_FAMILIES
            )
        with self.assertRaisesRegex(ValueError, "registered family"):
            materialx_horde_dispatch_plan.build_dispatch_plan(
                [make_manifest()], "horde.env", registered_families={"add": "not-a-contract"}
            )

        source = inspect.getsource(materialx_horde_dispatch_plan)
        self.assertNotIn("subprocess", source)
        self.assertNotIn("ssh", source.lower())


if __name__ == "__main__":
    unittest.main()
