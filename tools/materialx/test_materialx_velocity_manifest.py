"""Tests for the fail-closed MaterialX velocity manifests."""

from __future__ import annotations

import unittest

try:
    import materialx_velocity_manifest as velocity_manifest
except ModuleNotFoundError:
    from tools.materialx import materialx_velocity_manifest as velocity_manifest


SHA = "a" * 40
REGISTERED_FAMILIES = {"add-float"}
SIGNATURE = {
    "operation": "add",
    "input_types": ["float", "float"],
    "output_type": "float",
    "broadcast_policy": "none",
    "output_socket_class": "float",
}


def make_batch_manifest(**overrides: object) -> dict[str, object]:
    manifest: dict[str, object] = {
        "schema_version": 2,
        "batch_id": "native-add-float-001",
        "batch_kind": "family",
        "family_id": "add-float",
        "template_signature": SIGNATURE,
        "layer": "native_cycles",
        "node_defs": [f"ND_add_float_{index}" for index in range(8)],
        "integration_base_sha": SHA,
        "worker_source_sha": SHA,
        "roles": {"implementation": "blend05", "generated_tests": "blendit04", "independent_review": "blendit"},
        "files_allowlist": ["intern/cycles/scene/materialx.cpp", "intern/cycles/test/materialx_test.cpp"],
        "focused_test_commands": ["cycles_test --gtest_filter=MaterialXSemantic.add_float"],
        "generated_evidence_tier": "generated_semantic_template",
        "exception_budget": 0,
        "red_test": "",
        "approval_record": "",
    }
    manifest.update(overrides)
    return manifest


def make_completion_manifest(**overrides: object) -> dict[str, object]:
    completion: dict[str, object] = {
        "schema_version": 2, "batch_id": "native-add-float-001", "base_sha": SHA, "head_sha": "b" * 40,
        "node_defs": [f"ND_add_float_{index}" for index in range(8)], "rejected_node_defs": [],
        "changed_files": ["intern/cycles/scene/materialx.cpp", "intern/cycles/test/materialx_test.cpp"],
        "tests": [{"command": "cycles_test --gtest_filter=MaterialXSemantic.add_float", "passed": 1, "failed": 0, "exit_code": 0}],
        "review_verdict": "pass",
        "role_evidence": {"implementation": "IMPLEMENTATION-1a2b3c4d", "generated_tests": "GENERATED_TESTS-1a2b3c4d", "independent_review": "INDEPENDENT_REVIEW-1a2b3c4d"},
    }
    completion.update(overrides)
    return completion


class MaterialXVelocityManifestTest(unittest.TestCase):
    def test_accepts_complete_family_manifest(self) -> None:
        normalized = velocity_manifest.validate_batch_manifest(make_batch_manifest(), registered_families=REGISTERED_FAMILIES)
        self.assertEqual(normalized["batch_kind"], "family")
        self.assertEqual(normalized["template_signature"], SIGNATURE)
        self.assertEqual(list(normalized), sorted(normalized))

    def test_rejects_invalid_family_contract(self) -> None:
        valid = make_batch_manifest()
        cases = (
            ({**valid, "batch_kind": "unknown"}, "batch_kind"),
            ({**valid, "template_signature": {**SIGNATURE, "input_types": []}}, "input_types"),
            ({**valid, "generated_evidence_tier": ""}, "generated_evidence_tier"),
            ({**valid, "node_defs": ["ND_add_float_0"] * 8}, "duplicate NodeDef"),
            ({**valid, "exception_budget": 1}, "exception_budget must be zero"),
            ({**valid, "red_test": "RED_TEST-1a2b3c4d"}, "cannot carry exception evidence"),
        )
        for manifest, message in cases:
            with self.subTest(message=message), self.assertRaisesRegex(ValueError, message):
                velocity_manifest.validate_batch_manifest(manifest, registered_families=REGISTERED_FAMILIES)

    def test_accepts_approved_complex_exception(self) -> None:
        exception = make_batch_manifest(
            batch_kind="complex_exception", node_defs=[f"ND_add_float_{index}" for index in range(7)],
            exception_budget=1, red_test="RED_TEST-1a2b3c4d", approval_record="APPROVAL-1a2b3c4d",
        )
        normalized = velocity_manifest.validate_batch_manifest(exception, registered_families=REGISTERED_FAMILIES)
        self.assertEqual(normalized["batch_kind"], "complex_exception")

    def test_rejects_incomplete_or_untrusted_complex_exception(self) -> None:
        valid = make_batch_manifest(
            batch_kind="complex_exception", node_defs=[f"ND_add_float_{index}" for index in range(7)],
            exception_budget=1, red_test="RED_TEST-1a2b3c4d", approval_record="APPROVAL-1a2b3c4d",
        )
        for field, value, message in (("red_test", "", "red_test"), ("approval_record", "unsafe receipt", "safe receipt")):
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, message):
                velocity_manifest.validate_batch_manifest({**valid, field: value}, registered_families=REGISTERED_FAMILIES)

    def test_accepts_and_sanitizes_completion_for_assignment(self) -> None:
        assignment = make_batch_manifest()
        normalized = velocity_manifest.validate_completion_manifest(
            assignment, make_completion_manifest(changed_files=list(reversed(assignment["files_allowlist"])), untrusted_log="not-for-output")
        )
        self.assertEqual(normalized["changed_files"], sorted(assignment["files_allowlist"]))
        self.assertNotIn("untrusted_log", normalized)

    def test_rejects_completion_outside_assignment_and_failed_evidence(self) -> None:
        assignment = make_batch_manifest()
        cases = (
            (make_completion_manifest(changed_files=["outside/allowlist.cpp"]), "outside allowlist"),
            (make_completion_manifest(tests=[{"command": "unrelated", "passed": 1, "failed": 0, "exit_code": 0}]), "test commands do not match assignment"),
            (make_completion_manifest(tests=[{"command": "cycles_test --gtest_filter=MaterialXSemantic.add_float", "passed": 1, "failed": 1, "exit_code": 0}]), "completion contains failed tests"),
        )
        for completion, message in cases:
            with self.subTest(message=message), self.assertRaisesRegex(ValueError, message):
                velocity_manifest.validate_completion_manifest(assignment, completion)

    def test_rejects_untrusted_role_receipts_and_missing_zero_exit_completion(self) -> None:
        completion = make_completion_manifest(role_evidence={"implementation": "IMPLEMENTATION-1a2b3c4d", "generated_tests": "unsafe receipt", "independent_review": "INDEPENDENT_REVIEW-1a2b3c4d"})
        with self.assertRaisesRegex(ValueError, "safe receipt"):
            velocity_manifest.validate_completion_manifest(make_batch_manifest(), completion)
        with self.assertRaisesRegex(ValueError, "zero without a completion manifest"):
            velocity_manifest.validate_completion_result(make_batch_manifest(), 0, None)
