#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Tests for isolated, fail-closed MaterialX integration trains."""

from __future__ import annotations

import copy
import unittest

from materialx_integration_train import run_integration_trains
from test_materialx_completion_harvest import make_completion
from test_materialx_horde_dispatch_plan import REGISTERED_FAMILIES, make_manifest


LAYERS = ("native_cycles", "hydra_ovrtx", "blender_authoring")


def make_artifact(
    batch_id: str = "next-a",
    *,
    layer: str = "native_cycles",
    worker_id: str = "blend05",
    roles: dict[str, str] | None = None,
) -> dict:
    assignment = make_manifest(batch_id)
    assignment["layer"] = layer
    if roles is not None:
        assignment["roles"] = roles
    completion = make_completion(batch_id)
    completion.update({
        "base_sha": assignment["integration_base_sha"],
        "node_defs": list(assignment["node_defs"]),
        "changed_files": list(assignment["files_allowlist"]),
        "tests": [
            {"command": command, "passed": 1, "failed": 0, "exit_code": 0}
            for command in assignment["focused_test_commands"]
        ],
    })
    return {
        "worker_id": worker_id,
        "batch_id": batch_id,
        "layer": layer,
        "assignment": assignment,
        "completion": completion,
    }


class FakeIntegrationBackend:
    def __init__(self, overrides=None, exceptions=None):
        self.overrides = overrides or {}
        self.exceptions = exceptions or {}
        self.calls = []

    def _result(self, operation, batch_id, default):
        self.calls.append((operation, batch_id))
        if (operation, batch_id) in self.exceptions:
            raise self.exceptions[(operation, batch_id)]
        return copy.deepcopy(self.overrides.get((operation, batch_id), default))

    def prepare_worktree(self, layer, batch_id, base_sha):
        return self._result("prepare", batch_id, {
            "worktree": f"worktree:{layer}:{batch_id}",
            "base_sha": base_sha,
        })

    def apply_artifact(self, worktree, head_sha, changed_files):
        batch_id = worktree.rsplit(":", 1)[-1]
        return self._result("apply", batch_id, {
            "status": "applied",
            "head_sha": head_sha,
            "changed_files": list(changed_files),
        })

    def run_commands(self, worktree, focused_commands):
        batch_id = worktree.rsplit(":", 1)[-1]
        return self._result("commands", batch_id, {
            "commands": [
                {"command": command, "exit_code": 0}
                for command in focused_commands
            ],
        })

    def merge_commit(self, worktree, layer, batch_id, head_sha):
        return self._result("merge", batch_id, {
            "status": "merged",
            "head_sha": head_sha,
        })


class MaterialXIntegrationTrainTest(unittest.TestCase):
    def test_routes_exactly_three_layers_in_deterministic_order(self) -> None:
        backend = FakeIntegrationBackend()
        artifacts = [
            make_artifact("native-a", layer="native_cycles"),
            make_artifact(
                "hydra-a",
                layer="hydra_ovrtx",
                worker_id="blendit2",
                roles={
                    "implementation": "blendit2",
                    "generated_tests": "blendit3",
                    "independent_review": "blend05",
                },
            ),
            make_artifact(
                "author-a",
                layer="blender_authoring",
                worker_id="blendit04",
                roles={
                    "implementation": "blendit04",
                    "generated_tests": "blendit",
                    "independent_review": "blendit3",
                },
            ),
        ]

        receipts = run_integration_trains(
            list(reversed(artifacts)),
            registered_families=REGISTERED_FAMILIES,
            backend=backend,
        )

        self.assertEqual(
            [(receipt["layer"], receipt["batch_id"]) for receipt in receipts],
            [
                ("native_cycles", "native-a"),
                ("hydra_ovrtx", "hydra-a"),
                ("blender_authoring", "author-a"),
            ],
        )
        self.assertTrue(all(receipt["final_state"] == "integrated" for receipt in receipts))
        self.assertEqual(
            [call for call in backend.calls if call[0] == "prepare"],
            [
                ("prepare", "native-a"),
                ("prepare", "hydra-a"),
                ("prepare", "author-a"),
            ],
        )

    def test_receipt_is_sanitized_and_contains_only_creditable_evidence(self) -> None:
        artifact = make_artifact()
        receipt = run_integration_trains(
            [artifact],
            registered_families=REGISTERED_FAMILIES,
            backend=FakeIntegrationBackend(),
        )[0]

        self.assertEqual(set(receipt), {
            "batch_id",
            "layer",
            "base_sha",
            "head_sha",
            "focused_commands",
            "numeric_exits",
            "final_state",
        })
        self.assertEqual(receipt["focused_commands"], artifact["assignment"]["focused_test_commands"])
        self.assertEqual(receipt["numeric_exits"], [0])
        self.assertEqual(receipt["final_state"], "integrated")
        self.assertNotIn("worker_id", receipt)
        self.assertNotIn("worktree", receipt)

    def test_revalidates_exact_artifact_and_rejects_untrusted_variants_before_backend(self) -> None:
        base = make_artifact()
        cases = {}
        extra = copy.deepcopy(base)
        extra["prompt"] = "do not persist"
        cases["invalid_artifact"] = extra
        wrong_owner = copy.deepcopy(base)
        wrong_owner["worker_id"] = "blendit2"
        cases["ownership_mismatch"] = wrong_owner
        stale_base = copy.deepcopy(base)
        stale_base["completion"]["base_sha"] = "c" * 40
        cases["stale_base"] = stale_base
        stale_head = copy.deepcopy(base)
        stale_head["completion"]["head_sha"] = stale_head["completion"]["base_sha"]
        cases["stale_head"] = stale_head
        escaped = copy.deepcopy(base)
        escaped["completion"]["changed_files"] = ["outside/allowlist.cpp"]
        cases["changed_file_escape"] = escaped
        rejected_review = copy.deepcopy(base)
        rejected_review["completion"]["review_verdict"] = "fail"
        cases["invalid_review_evidence"] = rejected_review
        approximation = copy.deepcopy(base)
        approximation["completion"]["role_evidence"]["independent_review"] = "APPROXIMATION-12345678"
        cases["approximation_evidence"] = approximation
        missing_review = copy.deepcopy(base)
        del missing_review["completion"]["role_evidence"]["independent_review"]
        cases["invalid_review_evidence"] = missing_review

        for expected, artifact in cases.items():
            with self.subTest(expected=expected):
                backend = FakeIntegrationBackend()
                receipt = run_integration_trains(
                    [artifact],
                    registered_families=REGISTERED_FAMILIES,
                    backend=backend,
                )[0]
                self.assertEqual(receipt["final_state"], "rejected")
                self.assertEqual(receipt["failure_classification"], expected)
                self.assertEqual(backend.calls, [])

    def test_rejects_noncanonical_assignment_and_completion_representations(self) -> None:
        canonical = make_artifact()
        assignment_extra = copy.deepcopy(canonical)
        assignment_extra["assignment"]["prompt"] = "not canonical"
        assignment_reordered = copy.deepcopy(canonical)
        assignment_reordered["assignment"]["node_defs"].reverse()
        completion_extra = copy.deepcopy(canonical)
        completion_extra["completion"]["stdout"] = "raw output"
        test_extra = copy.deepcopy(canonical)
        test_extra["completion"]["tests"][0]["stdout"] = "raw test output"
        completion_reordered = copy.deepcopy(canonical)
        completion_reordered["completion"]["node_defs"].reverse()
        changed_files_reordered = copy.deepcopy(canonical)
        changed_files_reordered["assignment"]["files_allowlist"] = sorted([
            "intern/cycles/extra.cpp",
            *changed_files_reordered["assignment"]["files_allowlist"],
        ])
        changed_files_reordered["completion"]["changed_files"] = list(
            reversed(changed_files_reordered["assignment"]["files_allowlist"])
        )

        cases = (
            (assignment_extra, "invalid_assignment"),
            (assignment_reordered, "noncanonical_assignment"),
            (completion_extra, "noncanonical_completion"),
            (test_extra, "noncanonical_completion"),
            (completion_reordered, "noncanonical_completion"),
            (changed_files_reordered, "noncanonical_completion"),
        )
        for artifact, classification in cases:
            with self.subTest(classification=classification):
                backend = FakeIntegrationBackend()
                receipt = run_integration_trains(
                    [artifact],
                    registered_families=REGISTERED_FAMILIES,
                    backend=backend,
                )[0]
                self.assertEqual(receipt["final_state"], "rejected")
                self.assertEqual(
                    receipt["failure_classification"], classification
                )
                self.assertEqual(backend.calls, [])

    def test_backend_failures_are_categorical_and_stop_only_that_artifact(self) -> None:
        failing = make_artifact("fail-a")
        passing = make_artifact(
            "pass-a",
            layer="hydra_ovrtx",
            worker_id="blendit2",
            roles={
                "implementation": "blendit2",
                "generated_tests": "blendit3",
                "independent_review": "blend05",
            },
        )
        cases = (
            (
                {("prepare", "fail-a"): {"worktree": "wrong", "base_sha": "c" * 40}},
                {},
                "worktree_base_mismatch",
            ),
            (
                {("apply", "fail-a"): {"status": "conflict"}},
                {},
                "apply_conflict",
            ),
            (
                {("commands", "fail-a"): {
                    "commands": [{"command": failing["assignment"]["focused_test_commands"][0], "exit_code": 7}],
                }},
                {},
                "focused_test_failure",
            ),
            (
                {("merge", "fail-a"): {"status": "failure"}},
                {},
                "merge_failure",
            ),
            (
                {("apply", "fail-a"): {"stdout": "secret-bearing raw log"}},
                {},
                "invalid_backend_shape",
            ),
            (
                {},
                {("prepare", "fail-a"): RuntimeError("remote detail must not persist")},
                "backend_exception",
            ),
        )
        for overrides, exceptions, classification in cases:
            with self.subTest(classification=classification):
                backend = FakeIntegrationBackend(overrides, exceptions)
                receipts = run_integration_trains(
                    [failing, passing],
                    registered_families=REGISTERED_FAMILIES,
                    backend=backend,
                )
                by_batch = {receipt["batch_id"]: receipt for receipt in receipts}
                self.assertEqual(by_batch["fail-a"]["final_state"], "rejected")
                self.assertEqual(
                    by_batch["fail-a"]["failure_classification"], classification
                )
                self.assertEqual(by_batch["pass-a"]["final_state"], "integrated")
                self.assertNotIn("remote detail", repr(receipts))
                self.assertNotIn("secret-bearing", repr(receipts))

    def test_malformed_ownership_is_rejected_without_blocking_a_valid_lane(self) -> None:
        malformed = make_artifact("malformed-a")
        malformed["assignment"]["node_defs"] = [["not", "hashable"]]
        malformed["completion"]["changed_files"] = [["not", "hashable"]]
        valid = make_artifact(
            "valid-a",
            layer="hydra_ovrtx",
            worker_id="blendit2",
            roles={
                "implementation": "blendit2",
                "generated_tests": "blendit3",
                "independent_review": "blend05",
            },
        )
        backend = FakeIntegrationBackend()

        receipts = run_integration_trains(
            [malformed, valid],
            registered_families=REGISTERED_FAMILIES,
            backend=backend,
        )

        by_batch = {receipt["batch_id"]: receipt for receipt in receipts}
        self.assertEqual(by_batch["malformed-a"]["final_state"], "rejected")
        self.assertEqual(by_batch["valid-a"]["final_state"], "integrated")

    def test_apply_must_confirm_exact_head_and_changed_files(self) -> None:
        artifact = make_artifact()
        cases = (
            ({"status": "applied", "head_sha": "c" * 40, "changed_files": artifact["completion"]["changed_files"]}, "stale_head"),
            ({"status": "applied", "head_sha": artifact["completion"]["head_sha"], "changed_files": ["escaped.cpp"]}, "changed_file_escape"),
        )
        for result, classification in cases:
            with self.subTest(classification=classification):
                backend = FakeIntegrationBackend({("apply", "next-a"): result})
                receipt = run_integration_trains(
                    [artifact],
                    registered_families=REGISTERED_FAMILIES,
                    backend=backend,
                )[0]
                self.assertEqual(receipt["final_state"], "rejected")
                self.assertEqual(receipt["failure_classification"], classification)
                self.assertFalse(any(call[0] == "merge" for call in backend.calls))

    def test_unknown_layer_and_duplicate_ownership_are_rejected_without_backend(self) -> None:
        unknown = make_artifact()
        unknown["layer"] = "default"
        unknown["assignment"]["layer"] = "default"
        backend = FakeIntegrationBackend()
        receipt = run_integration_trains(
            [unknown],
            registered_families=REGISTERED_FAMILIES,
            backend=backend,
        )[0]
        self.assertEqual(receipt["failure_classification"], "invalid_assignment")
        self.assertEqual(backend.calls, [])

        duplicate = make_artifact()
        duplicate_backend = FakeIntegrationBackend()
        duplicate_receipts = run_integration_trains(
            [duplicate, copy.deepcopy(duplicate)],
            registered_families=REGISTERED_FAMILIES,
            backend=duplicate_backend,
        )
        self.assertEqual(
            [receipt["failure_classification"] for receipt in duplicate_receipts],
            ["ambiguous_ownership", "ambiguous_ownership"],
        )
        self.assertEqual(duplicate_backend.calls, [])

        same_worker = make_artifact("native-worker")
        same_worker_other_lane = make_artifact(
            "hydra-worker",
            layer="hydra_ovrtx",
        )
        worker_backend = FakeIntegrationBackend()
        worker_receipts = run_integration_trains(
            [same_worker, same_worker_other_lane],
            registered_families=REGISTERED_FAMILIES,
            backend=worker_backend,
        )
        self.assertEqual(
            [receipt["failure_classification"] for receipt in worker_receipts],
            ["ambiguous_ownership", "ambiguous_ownership"],
        )
        self.assertEqual(worker_backend.calls, [])


if __name__ == "__main__":
    unittest.main()
