"""Tests for bounded MaterialX Completion Manifest v2 harvesting."""

from __future__ import annotations

import json
import unittest

from materialx_completion_harvest import (
    COMPLETION_PREFIX,
    EXIT_PREFIX,
    MAX_LOG_WINDOW_BYTES,
    MAX_PAYLOAD_BYTES,
    parse_completion_evidence,
)
from materialx_velocity_manifest import validate_completion_result
from test_materialx_horde_dispatch_plan import make_manifest


def make_completion(batch_id: str = "next-a") -> dict:
    assignment = make_manifest(batch_id)
    return {
        "schema_version": 2,
        "batch_id": batch_id,
        "base_sha": assignment["integration_base_sha"],
        "head_sha": "b" * 40,
        "node_defs": list(assignment["node_defs"]),
        "rejected_node_defs": [],
        "changed_files": list(assignment["files_allowlist"]),
        "tests": [
            {"command": command, "passed": 1, "failed": 0, "exit_code": 0}
            for command in assignment["focused_test_commands"]
        ],
        "review_verdict": "pass",
        "role_evidence": {
            "implementation": "IMPLEMENTATION-12345678",
            "generated_tests": "GENERATED_TESTS-12345678",
            "independent_review": "INDEPENDENT_REVIEW-12345678",
        },
    }


def evidence(completion: dict | None = None, exit_code: int = 0) -> str:
    lines = []
    if completion is not None:
        lines.append(COMPLETION_PREFIX + json.dumps(completion, separators=(",", ":")))
    lines.append(f"{EXIT_PREFIX}{exit_code}")
    return "\n".join(lines)


class MaterialXCompletionHarvestTest(unittest.TestCase):
    def test_accepts_exact_completion_and_exit_lines(self) -> None:
        completion = make_completion()
        result = parse_completion_evidence(evidence(completion))
        self.assertEqual(result, {
            "classification": "completion",
            "process_exit": 0,
            "completion": completion,
        })
        validated = validate_completion_result(
            make_manifest("next-a"), result["process_exit"], result["completion"]
        )
        self.assertEqual(validated["batch_id"], "next-a")

    def test_exit_zero_without_completion_is_invalid(self) -> None:
        self.assertEqual(
            parse_completion_evidence(f"{EXIT_PREFIX}0"),
            {"classification": "invalid_completion"},
        )

    def test_rejects_missing_duplicate_and_invalid_sentinels_categorically(self) -> None:
        completion_line, exit_line = evidence(make_completion()).splitlines()
        cases = (
            ("", "invalid_completion"),
            (completion_line, "invalid_exit"),
            (completion_line + "\n" + completion_line + "\n" + exit_line, "invalid_completion"),
            (completion_line + "\n" + exit_line + "\n" + exit_line, "invalid_exit"),
            (completion_line + "\nMATERIALX_HORDE_EXIT:-1", "invalid_exit"),
            (completion_line + "\nMATERIALX_HORDE_EXIT:not-a-number", "invalid_exit"),
            (completion_line + "\nnoise\n" + exit_line, "invalid_completion"),
        )
        for raw, classification in cases:
            with self.subTest(raw=raw[-40:]):
                self.assertEqual(
                    parse_completion_evidence(raw),
                    {"classification": classification},
                )

    def test_rejects_invalid_json_non_object_and_unsupported_schema(self) -> None:
        cases = (
            ("{", "invalid_json"),
            ("[]", "invalid_completion"),
            (json.dumps({**make_completion(), "schema_version": 3}), "unsupported_schema"),
        )
        for payload, classification in cases:
            with self.subTest(classification=classification):
                self.assertEqual(
                    parse_completion_evidence(
                        COMPLETION_PREFIX + payload + "\n" + EXIT_PREFIX + "0"
                    ),
                    {"classification": classification},
                )

    def test_rejects_oversized_payload_and_window(self) -> None:
        self.assertEqual(
            parse_completion_evidence("x" * (MAX_LOG_WINDOW_BYTES + 1)),
            {"classification": "oversized_log_window"},
        )
        oversized_payload = "{}" + (" " * (MAX_PAYLOAD_BYTES - 1))
        self.assertEqual(
            parse_completion_evidence(
                COMPLETION_PREFIX + oversized_payload + "\n" + EXIT_PREFIX + "0"
            ),
            {"classification": "oversized_payload"},
        )
        oversized_line = "{}" + (" " * (MAX_PAYLOAD_BYTES + 1_024))
        self.assertEqual(
            parse_completion_evidence(
                COMPLETION_PREFIX + oversized_line + "\n" + EXIT_PREFIX + "0"
            ),
            {"classification": "oversized_line"},
        )

    def test_pathologically_nested_json_is_categorical(self) -> None:
        payload = (
            '{"schema_version":2,"nested":'
            + ("[" * 1_200)
            + "0"
            + ("]" * 1_200)
            + "}"
        )
        self.assertEqual(
            parse_completion_evidence(
                COMPLETION_PREFIX + payload + "\n" + EXIT_PREFIX + "0"
            ),
            {"classification": "invalid_json"},
        )

    def test_rejects_secret_like_keys_at_any_depth_without_leaking_values(self) -> None:
        for completion in (
            {**make_completion(), "api_key": "do-not-return"},
            {
                **make_completion(),
                "role_evidence": {
                    **make_completion()["role_evidence"],
                    "nested": {"authorization": "do-not-return"},
                },
            },
        ):
            result = parse_completion_evidence(evidence(completion))
            self.assertEqual(result, {"classification": "secret_like_key"})
            self.assertNotIn("do-not-return", repr(result))

    def test_nonzero_exit_is_categorical_and_drops_completion(self) -> None:
        result = parse_completion_evidence(evidence(make_completion(), exit_code=7))
        self.assertEqual(result, {"classification": "nonzero_exit"})
        self.assertNotIn("completion", result)

    def test_accepts_only_fixed_remote_failure_classifications(self) -> None:
        for category in ("missing", "auth_failure", "proxy_failure", "oversized_log_window"):
            self.assertEqual(
                parse_completion_evidence("MATERIALX_HARVEST_V2:" + category),
                {"classification": category},
            )
        self.assertEqual(
            parse_completion_evidence("MATERIALX_HARVEST_V2:private failure"),
            {"classification": "invalid_completion"},
        )


if __name__ == "__main__":
    unittest.main()
