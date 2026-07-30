"""Tests for the fail-closed MaterialX velocity manifests."""

from __future__ import annotations

import unittest

try:
    import materialx_velocity_manifest as velocity_manifest
except ModuleNotFoundError:
    from tools.materialx import materialx_velocity_manifest as velocity_manifest


SHA = "a" * 40
REGISTERED_FAMILIES = {"add-float"}


def make_batch_manifest(**overrides: object) -> dict[str, object]:
    manifest: dict[str, object] = {
        "schema_version": 2,
        "batch_id": "native-add-float-001",
        "family_id": "add-float",
        "layer": "native_cycles",
        "node_defs": [f"ND_add_float_{index}" for index in range(8)],
        "integration_base_sha": SHA,
        "worker_source_sha": SHA,
        "roles": {
            "implementation": "blend05",
            "generated_tests": "blendit04",
            "independent_review": "blendit",
        },
        "files_allowlist": [
            "intern/cycles/scene/materialx.cpp",
            "intern/cycles/test/materialx_test.cpp",
        ],
        "complex_exception": False,
        "exception_budget": 0,
        "red_test": "",
        "approval_record": "",
    }
    manifest.update(overrides)
    return manifest


def make_completion_manifest(**overrides: object) -> dict[str, object]:
    completion: dict[str, object] = {
        "schema_version": 2,
        "batch_id": "native-add-float-001",
        "base_sha": SHA,
        "head_sha": "b" * 40,
        "node_defs": [f"ND_add_float_{index}" for index in range(8)],
        "rejected_node_defs": [],
        "changed_files": [
            "intern/cycles/scene/materialx.cpp",
            "intern/cycles/test/materialx_test.cpp",
        ],
        "tests": [
            {
                "command": "cycles_test --gtest_filter=MaterialXSemantic.add_float",
                "passed": 1,
                "failed": 0,
                "exit_code": 0,
            }
        ],
        "review_verdict": "pass",
        "role_evidence": {
            "implementation": "IMPLEMENTATION-1a2b3c4d",
            "generated_tests": "GENERATED_TESTS-1a2b3c4d",
            "independent_review": "INDEPENDENT_REVIEW-1a2b3c4d",
        },
    }
    completion.update(overrides)
    return completion


class MaterialXVelocityManifestTest(unittest.TestCase):
    def test_accepts_complete_family_manifest(self) -> None:
        manifest = make_batch_manifest(node_defs=[f"ND_add_float_{index}" for index in range(8)])

        normalized = velocity_manifest.validate_batch_manifest(
            manifest, registered_families=REGISTERED_FAMILIES
        )

        self.assertEqual(normalized["batch_id"], "native-add-float-001")
        self.assertEqual(list(normalized), sorted(normalized))
        self.assertNotIn("prompt", normalized)

    def test_rejects_prompt_only_and_invalid_batch_fields(self) -> None:
        valid = make_batch_manifest()
        cases = (
            ({"batch_id": "native-add-float-001", "prompt": "implement this"}, "missing fields"),
            ({**valid, "worker_source_sha": "b" * 40}, "worker_source_sha must equal integration_base_sha"),
            ({**valid, "layer": "cycles"}, "unsupported layer"),
            ({**valid, "roles": {"implementation": "blend05"}}, "missing required roles"),
            ({**valid, "node_defs": ["ND_add_float_0"] * 8}, "duplicate NodeDef"),
            ({**valid, "node_defs": [f"ND_add_float_{index}" for index in range(7)]}, "8-16 NodeDefs"),
            ({**valid, "exception_budget": 1}, "exception_budget must be zero"),
            ({**valid, "family_id": "unregistered"}, "unregistered family_id"),
        )

        for manifest, message in cases:
            with self.subTest(message=message), self.assertRaisesRegex(ValueError, message):
                velocity_manifest.validate_batch_manifest(
                    manifest, registered_families=REGISTERED_FAMILIES
                )

    def test_accepts_approved_complex_exception_and_rejects_incomplete_one(self) -> None:
        exception = make_batch_manifest(
            node_defs=[f"ND_add_float_{index}" for index in range(7)],
            complex_exception=True,
            exception_budget=1,
            red_test="RED_TEST-1a2b3c4d",
            approval_record="APPROVAL-1a2b3c4d",
        )

        normalized = velocity_manifest.validate_batch_manifest(
            exception, registered_families=REGISTERED_FAMILIES
        )
        self.assertEqual(normalized["exception_budget"], 1)

        for field in ("red_test", "approval_record"):
            incomplete = dict(exception)
            incomplete[field] = ""
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, field):
                velocity_manifest.validate_batch_manifest(
                    incomplete, registered_families=REGISTERED_FAMILIES
                )

    def test_accepts_and_sanitizes_completion_for_assignment(self) -> None:
        assignment = make_batch_manifest()
        completion = make_completion_manifest(
            changed_files=list(reversed(assignment["files_allowlist"])),
            untrusted_log="NVIDIA_API_KEY=not-for-output",
        )

        normalized = velocity_manifest.validate_completion_manifest(assignment, completion)

        self.assertEqual(list(normalized), sorted(normalized))
        self.assertEqual(normalized["changed_files"], sorted(assignment["files_allowlist"]))
        self.assertNotIn("untrusted_log", normalized)

    def test_rejects_untrusted_batch_receipts(self) -> None:
        exception = make_batch_manifest(
            node_defs=[f"ND_add_float_{index}" for index in range(7)],
            complex_exception=True,
            exception_budget=1,
            red_test="RED_TEST-1a2b3c4d",
            approval_record="APPROVAL-1a2b3c4d",
        )
        cases = (
            ("red_test", "RED TEST-1a2b3c4d"),
            ("approval_record", "APPROVAL-1a2b3c4d\nraw task log"),
            ("red_test", "NVIDIA_API_KEY=secret"),
            ("approval_record", "A" * 65),
            ("red_test", {"receipt": "RED_TEST-1a2b3c4d"}),
        )

        for field, value in cases:
            manifest = dict(exception)
            manifest[field] = value
            with self.subTest(field=field, value=value), self.assertRaisesRegex(ValueError, "safe receipt"):
                velocity_manifest.validate_batch_manifest(
                    manifest, registered_families=REGISTERED_FAMILIES
                )

    def test_rejects_untrusted_role_receipts(self) -> None:
        assignment = make_batch_manifest()
        valid = make_completion_manifest()
        cases = (
            "GENERATED TESTS-1a2b3c4d",
            "GENERATED_TESTS-1a2b3c4d\nMATERIALX_HORDE_EXIT:0",
            "NVIDIA_API_KEY=secret",
            "A" * 65,
            {"receipt": "GENERATED_TESTS-1a2b3c4d"},
        )

        for value in cases:
            role_evidence = dict(valid["role_evidence"])
            role_evidence["generated_tests"] = value
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, "safe receipt"):
                velocity_manifest.validate_completion_manifest(
                    assignment, {**valid, "role_evidence": role_evidence}
                )

    def test_rejects_zero_process_exit_without_completion_manifest(self) -> None:
        with self.assertRaisesRegex(ValueError, "zero without a completion manifest"):
            velocity_manifest.validate_completion_result(make_batch_manifest(), 0, None)

    def test_rejects_missing_or_invalid_completion_evidence(self) -> None:
        assignment = make_batch_manifest()
        valid = make_completion_manifest()
        cases = (
            ({}, "completion manifest is missing fields"),
            ({**valid, "base_sha": "invalid"}, "completion base_sha must be a 40-hex SHA"),
            ({**valid, "head_sha": SHA}, "completion head_sha must differ from completion base_sha"),
            ({**valid, "node_defs": ["ND_other"]}, "completion NodeDefs do not match assignment"),
            ({**valid, "rejected_node_defs": ["ND_add_float_0"]}, "completion contains rejected NodeDefs"),
            ({**valid, "changed_files": ["outside/allowlist.cpp"]}, "outside allowlist"),
            ({**valid, "tests": [{"command": "test", "passed": 1, "failed": 0}]}, "missing numeric test fields"),
            ({**valid, "tests": [{"command": "test", "passed": 1, "failed": 1, "exit_code": 0}]}, "completion contains failed tests"),
            ({**valid, "tests": [{"command": "test", "passed": 1, "failed": 0, "exit_code": 1}]}, "completion contains failed tests"),
            ({**valid, "review_verdict": "needs_changes"}, "completion review_verdict is not pass"),
        )

        for completion, message in cases:
            with self.subTest(message=message), self.assertRaisesRegex(ValueError, message):
                velocity_manifest.validate_completion_manifest(assignment, completion)


if __name__ == "__main__":
    unittest.main()
