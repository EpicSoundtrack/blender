# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Tests for the fail-closed MaterialX velocity manifests."""

from __future__ import annotations

import unittest

try:
    import materialx_velocity_manifest as velocity_manifest
except ModuleNotFoundError:
    from tools.materialx import materialx_velocity_manifest as velocity_manifest


SHA = "a" * 40
SIGNATURE = {
    "operation": "add", "input_types": ["float", "float"], "output_type": "float",
    "broadcast_policy": "none", "output_socket_class": "float",
}
COMMANDS = ["cycles_test --gtest_filter=MaterialXSemantic.add_float"]
FAMILY_NODE_DEFS = [f"ND_add_float_{index}" for index in range(8)]
REGISTERED_FAMILIES = {
    "add-float": [{
        "template_signature": SIGNATURE,
        "node_defs": FAMILY_NODE_DEFS,
        "generated_evidence_tier": "generated_semantic_template",
        "focused_test_commands": COMMANDS,
    }]
}


def make_batch_manifest(**overrides: object) -> dict[str, object]:
    manifest: dict[str, object] = {
        "schema_version": 2, "batch_id": "native-add-float-001", "batch_kind": "family",
        "family_id": "add-float", "template_signature": SIGNATURE, "layer": "native_cycles",
        "node_defs": FAMILY_NODE_DEFS, "integration_base_sha": SHA, "worker_source_sha": SHA,
        "roles": {"implementation": "blend05", "generated_tests": "blendit04", "independent_review": "blendit"},
        "files_allowlist": ["intern/cycles/scene/materialx.cpp", "intern/cycles/test/materialx_test.cpp"],
        "focused_test_commands": COMMANDS, "generated_evidence_tier": "generated_semantic_template",
        "exception_budget": 0, "red_test": "", "approval_record": "",
    }
    manifest.update(overrides)
    return manifest


def make_completion_manifest(**overrides: object) -> dict[str, object]:
    completion: dict[str, object] = {
        "schema_version": 2, "batch_id": "native-add-float-001", "base_sha": SHA, "head_sha": "b" * 40,
        "node_defs": FAMILY_NODE_DEFS, "rejected_node_defs": [],
        "changed_files": ["intern/cycles/scene/materialx.cpp", "intern/cycles/test/materialx_test.cpp"],
        "tests": [{"command": COMMANDS[0], "passed": 1, "failed": 0, "exit_code": 0}],
        "review_verdict": "pass",
        "role_evidence": {"implementation": "IMPLEMENTATION-1a2b3c4d", "generated_tests": "GENERATED_TESTS-1a2b3c4d", "independent_review": "INDEPENDENT_REVIEW-1a2b3c4d"},
    }
    completion.update(overrides)
    return completion


class MaterialXVelocityManifestTest(unittest.TestCase):
    def validate(self, manifest):
        return velocity_manifest.validate_batch_manifest(manifest, registered_families=REGISTERED_FAMILIES)

    def test_accepts_complete_family_manifest(self) -> None:
        normalized = self.validate(make_batch_manifest())
        self.assertEqual(normalized["batch_kind"], "family")
        self.assertEqual(normalized["template_signature"], SIGNATURE)
        self.assertEqual(list(normalized), sorted(normalized))

    def test_rejects_wrong_registered_family_contract_values(self) -> None:
        cases = (
            ("template_signature", {**SIGNATURE, "operation": "subtract"}, "template_signature"),
            ("generated_evidence_tier", "invented_evidence", "generated_evidence_tier"),
            ("focused_test_commands", ["ctest --test-dir build -R unrelated"], "focused_test_commands"),
            ("node_defs", [*FAMILY_NODE_DEFS[:-1], "ND_unregistered"], "node_defs"),
        )
        for field, value, message in cases:
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, message):
                self.validate(make_batch_manifest(**{field: value}))

    def test_rejects_family_command_reordering(self) -> None:
        commands = ["ctest --test-dir build", COMMANDS[0]]
        families = {
            "add-float": [{**REGISTERED_FAMILIES["add-float"][0], "focused_test_commands": commands}]
        }
        manifest = make_batch_manifest(focused_test_commands=list(reversed(commands)))
        with self.assertRaisesRegex(ValueError, "focused_test_commands"):
            velocity_manifest.validate_batch_manifest(manifest, registered_families=families)

    def test_selects_exact_contract_from_a_multi_contract_family(self) -> None:
        alternate_signature = {**SIGNATURE, "operation": "add_vector"}
        alternate_nodes = [f"ND_add_vector_{index}" for index in range(8)]
        families = {
            "add-float": [
                REGISTERED_FAMILIES["add-float"][0],
                {
                    "template_signature": alternate_signature,
                    "node_defs": alternate_nodes,
                    "generated_evidence_tier": "generated_semantic_template",
                    "focused_test_commands": COMMANDS,
                },
            ]
        }
        accepted = velocity_manifest.validate_batch_manifest(
            make_batch_manifest(template_signature=alternate_signature, node_defs=alternate_nodes),
            registered_families=families,
        )
        self.assertEqual(accepted["node_defs"], alternate_nodes)
        with self.assertRaisesRegex(ValueError, "node_defs"):
            velocity_manifest.validate_batch_manifest(
                make_batch_manifest(template_signature=alternate_signature), registered_families=families
            )

    def test_rejects_prompt_sha_layer_roles_and_invalid_family_fields(self) -> None:
        valid = make_batch_manifest()
        cases = (
            ({"batch_id": "native-add-float-001", "prompt": "implement this"}, "missing fields"),
            ({**valid, "worker_source_sha": "b" * 40}, "worker_source_sha must equal integration_base_sha"),
            ({**valid, "layer": "cycles"}, "unsupported layer"),
            ({**valid, "roles": {"implementation": "blend05"}}, "missing required roles"),
            ({**valid, "node_defs": [FAMILY_NODE_DEFS[0]] * 8}, "duplicate NodeDef"),
            ({**valid, "node_defs": FAMILY_NODE_DEFS[:-1]}, "8-16 NodeDefs"),
            ({**valid, "exception_budget": 1}, "exception_budget must be zero"),
            ({**valid, "family_id": "unregistered"}, "unregistered family_id"),
        )
        for manifest, message in cases:
            with self.subTest(message=message), self.assertRaisesRegex(ValueError, message):
                self.validate(manifest)

    def test_accepts_approved_complex_exception_and_rejects_incomplete_one(self) -> None:
        exception = make_batch_manifest(batch_kind="complex_exception", node_defs=FAMILY_NODE_DEFS[:-1], exception_budget=1, red_test="RED_TEST-1a2b3c4d", approval_record="APPROVAL-1a2b3c4d")
        self.assertEqual(self.validate(exception)["batch_kind"], "complex_exception")
        for field in ("red_test", "approval_record"):
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, field):
                self.validate({**exception, field: ""})

    def test_rejects_untrusted_batch_receipts(self) -> None:
        exception = make_batch_manifest(batch_kind="complex_exception", node_defs=FAMILY_NODE_DEFS[:-1], exception_budget=1, red_test="RED_TEST-1a2b3c4d", approval_record="APPROVAL-1a2b3c4d")
        for field, value in (("red_test", "RED TEST-1a2b3c4d"), ("approval_record", "APPROVAL-1a2b3c4d\nraw task log"), ("red_test", "NVIDIA_API_KEY=secret"), ("approval_record", "A" * 65), ("red_test", {"receipt": "RED_TEST-1a2b3c4d"})):
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, "safe receipt"):
                self.validate({**exception, field: value})

    def test_accepts_and_sanitizes_completion_for_assignment(self) -> None:
        assignment = make_batch_manifest()
        normalized = velocity_manifest.validate_completion_manifest(assignment, make_completion_manifest(changed_files=list(reversed(assignment["files_allowlist"])), untrusted_log="not-for-output"))
        self.assertEqual(list(normalized), sorted(normalized))
        self.assertEqual(normalized["changed_files"], sorted(assignment["files_allowlist"]))
        self.assertEqual([test["command"] for test in normalized["tests"]], assignment["focused_test_commands"])
        self.assertNotIn("untrusted_log", normalized)

    def test_rejects_completion_commands_outside_assignment_and_in_a_different_order(self) -> None:
        assignment = make_batch_manifest(focused_test_commands=["ctest --test-dir build", COMMANDS[0]])
        cases = (
            [{"command": "cycles_test --gtest_filter=MaterialXSemantic.add_float NVIDIA_API_KEY=secret", "passed": 1, "failed": 0, "exit_code": 0}],
            [{"command": COMMANDS[0], "passed": 1, "failed": 0, "exit_code": 0}, {"command": "ctest --test-dir build", "passed": 1, "failed": 0, "exit_code": 0}],
        )
        for tests in cases:
            with self.subTest(tests=tests), self.assertRaisesRegex(ValueError, "test commands do not match assignment"):
                velocity_manifest.validate_completion_manifest(assignment, make_completion_manifest(tests=tests))

    def test_rejects_untrusted_role_receipts_and_missing_zero_exit_completion(self) -> None:
        completion = make_completion_manifest(role_evidence={"implementation": "IMPLEMENTATION-1a2b3c4d", "generated_tests": "unsafe receipt", "independent_review": "INDEPENDENT_REVIEW-1a2b3c4d"})
        with self.assertRaisesRegex(ValueError, "safe receipt"):
            velocity_manifest.validate_completion_manifest(make_batch_manifest(), completion)
        with self.assertRaisesRegex(ValueError, "zero without a completion manifest"):
            velocity_manifest.validate_completion_result(make_batch_manifest(), 0, None)

    def test_rejects_missing_or_invalid_completion_evidence(self) -> None:
        assignment, valid = make_batch_manifest(), make_completion_manifest()
        cases = (
            ({}, "completion manifest is missing fields"), ({**valid, "base_sha": "invalid"}, "completion base_sha must be a 40-hex SHA"),
            ({**valid, "head_sha": SHA}, "completion head_sha must differ from completion base_sha"), ({**valid, "node_defs": ["ND_other"]}, "completion NodeDefs do not match assignment"),
            ({**valid, "rejected_node_defs": [FAMILY_NODE_DEFS[0]]}, "completion contains rejected NodeDefs"), ({**valid, "changed_files": ["outside/allowlist.cpp"]}, "outside allowlist"),
            ({**valid, "tests": [{"command": COMMANDS[0], "passed": 1, "failed": 0}]}, "missing numeric test fields"),
            ({**valid, "tests": [{"command": COMMANDS[0], "passed": 1, "failed": 1, "exit_code": 0}]}, "completion contains failed tests"),
            ({**valid, "tests": [{"command": COMMANDS[0], "passed": 1, "failed": 0, "exit_code": 1}]}, "completion contains failed tests"),
            ({**valid, "review_verdict": "needs_changes"}, "completion review_verdict is not pass"),
        )
        for completion, message in cases:
            with self.subTest(message=message), self.assertRaisesRegex(ValueError, message):
                velocity_manifest.validate_completion_manifest(assignment, completion)


if __name__ == "__main__":
    unittest.main()
