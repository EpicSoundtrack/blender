#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Isolated, fail-closed integration trains for validated MaterialX artifacts."""

from __future__ import annotations

from collections.abc import Mapping, Sequence
from typing import Any, Protocol
import re

from materialx_velocity_manifest import (
    validate_batch_manifest,
    validate_completion_manifest,
)


__all__ = ("IntegrationBackend", "run_integration_trains")

LAYERS = ("native_cycles", "hydra_ovrtx", "blender_authoring")
ROLE_EVIDENCE_PREFIXES = {
    "implementation": "IMPLEMENTATION-",
    "generated_tests": "GENERATED_TESTS-",
    "independent_review": "INDEPENDENT_REVIEW-",
}
ARTIFACT_FIELDS = frozenset(
    ("worker_id", "batch_id", "layer", "assignment", "completion")
)
_SHA_PATTERN = re.compile(r"^[0-9a-f]{40}$")
_STATE_TRANSITIONS = {
    "queued": frozenset(("validating",)),
    "validating": frozenset(("integrated", "rejected")),
}


class IntegrationBackend(Protocol):
    """Injected bounded operations for one MaterialX integration artifact."""

    def prepare_worktree(
        self, layer: str, batch_id: str, base_sha: str
    ) -> Mapping[str, Any]:
        """Prepare an isolated worktree at exactly the declared base."""

    def apply_artifact(
        self, worktree: str, head_sha: str, changed_files: Sequence[str]
    ) -> Mapping[str, Any]:
        """Apply exactly the completed head and its declared changed files."""

    def run_commands(
        self, worktree: str, focused_commands: Sequence[str]
    ) -> Mapping[str, Any]:
        """Run exactly the completion's focused commands."""

    def merge_commit(
        self, worktree: str, layer: str, batch_id: str, head_sha: str
    ) -> Mapping[str, Any]:
        """Merge one validated artifact into its isolated integration lane."""


def _safe_string(value: Any, *, maximum: int = 256) -> str:
    if (
        not isinstance(value, str)
        or not value
        or len(value) > maximum
        or any(ord(character) < 32 for character in value)
    ):
        return ""
    return value


def _safe_sha(value: Any) -> str:
    return value if isinstance(value, str) and _SHA_PATTERN.fullmatch(value) else ""


def _receipt(
    *,
    batch_id: str,
    layer: str,
    base_sha: str,
    head_sha: str,
    focused_commands: Sequence[str],
    numeric_exits: Sequence[int],
    final_state: str,
    failure_classification: str | None = None,
) -> dict[str, Any]:
    receipt = {
        "batch_id": batch_id,
        "layer": layer,
        "base_sha": base_sha,
        "head_sha": head_sha,
        "focused_commands": list(focused_commands),
        "numeric_exits": list(numeric_exits),
        "final_state": final_state,
    }
    if failure_classification is not None:
        receipt["failure_classification"] = failure_classification
    return receipt


def _transition_state(state: str, target: str) -> str:
    if target not in _STATE_TRANSITIONS.get(state, ()):
        raise RuntimeError("invalid integration artifact state transition")
    return target


def _prevalidation_classification(artifact: Any) -> str | None:
    if not isinstance(artifact, Mapping) or set(artifact) != ARTIFACT_FIELDS:
        return "invalid_artifact"
    assignment = artifact.get("assignment")
    completion = artifact.get("completion")
    if not isinstance(assignment, Mapping):
        return "invalid_assignment"
    if not isinstance(completion, Mapping):
        return "invalid_completion"
    if assignment.get("layer") not in LAYERS or artifact.get("layer") not in LAYERS:
        return "invalid_assignment"
    if (
        artifact.get("batch_id") != assignment.get("batch_id")
        or artifact.get("layer") != assignment.get("layer")
        or artifact.get("worker_id")
        != (
            assignment.get("roles", {}).get("implementation")
            if isinstance(assignment.get("roles"), Mapping)
            else None
        )
    ):
        return "ownership_mismatch"
    tier = assignment.get("generated_evidence_tier")
    if isinstance(tier, str) and "approx" in tier.casefold():
        return "approximation_evidence"
    if completion.get("batch_id") != assignment.get("batch_id"):
        return "ownership_mismatch"
    if completion.get("base_sha") != assignment.get("integration_base_sha"):
        return "stale_base"
    if completion.get("head_sha") == completion.get("base_sha"):
        return "stale_head"
    changed_files = completion.get("changed_files")
    allowlist = assignment.get("files_allowlist")
    if (
        isinstance(changed_files, Sequence)
        and not isinstance(changed_files, (str, bytes))
        and isinstance(allowlist, Sequence)
        and not isinstance(allowlist, (str, bytes))
        and all(isinstance(item, str) for item in changed_files)
        and all(isinstance(item, str) for item in allowlist)
        and set(changed_files).difference(allowlist)
    ):
        return "changed_file_escape"
    if completion.get("review_verdict") != "pass":
        return "invalid_review_evidence"
    evidence = completion.get("role_evidence")
    if (
        not isinstance(evidence, Mapping)
        or set(evidence) != set(ROLE_EVIDENCE_PREFIXES)
    ):
        return "invalid_review_evidence"
    if isinstance(evidence, Mapping) and any(
        isinstance(value, str) and "approx" in value.casefold()
        for value in evidence.values()
    ):
        return "approximation_evidence"
    if any(
        not isinstance(evidence[role], str)
        or not evidence[role].startswith(prefix)
        for role, prefix in ROLE_EVIDENCE_PREFIXES.items()
    ):
        return "invalid_review_evidence"
    return None


def _validate_artifact(
    artifact: Mapping[str, Any],
    *,
    registered_families: Mapping[str, Any],
) -> tuple[dict[str, Any] | None, str | None]:
    classification = _prevalidation_classification(artifact)
    if classification is not None:
        return None, classification
    try:
        assignment = validate_batch_manifest(
            artifact["assignment"],
            registered_families=registered_families,
        )
    except (TypeError, ValueError):
        return None, "invalid_assignment"
    if artifact["assignment"] != assignment:
        return None, "noncanonical_assignment"
    try:
        completion = validate_completion_manifest(assignment, artifact["completion"])
    except (TypeError, ValueError):
        return None, "invalid_completion"
    if artifact["completion"] != completion:
        return None, "noncanonical_completion"
    if (
        artifact["batch_id"] != assignment["batch_id"]
        or artifact["layer"] != assignment["layer"]
        or artifact["worker_id"] != assignment["roles"]["implementation"]
    ):
        return None, "ownership_mismatch"
    return {
        "worker_id": artifact["worker_id"],
        "batch_id": assignment["batch_id"],
        "layer": assignment["layer"],
        "assignment": assignment,
        "completion": completion,
    }, None


def _rejected_receipt(artifact: Any, classification: str) -> dict[str, Any]:
    artifact = artifact if isinstance(artifact, Mapping) else {}
    assignment = artifact.get("assignment")
    completion = artifact.get("completion")
    assignment = assignment if isinstance(assignment, Mapping) else {}
    completion = completion if isinstance(completion, Mapping) else {}
    state = _transition_state("queued", "validating")
    state = _transition_state(state, "rejected")
    return _receipt(
        batch_id=_safe_string(artifact.get("batch_id")) or "invalid",
        layer=_safe_string(artifact.get("layer")) or "invalid",
        base_sha=_safe_sha(completion.get("base_sha"))
        or _safe_sha(assignment.get("integration_base_sha")),
        head_sha=_safe_sha(completion.get("head_sha")),
        focused_commands=[],
        numeric_exits=[],
        final_state=state,
        failure_classification=classification,
    )


def _ambiguous_artifacts(artifacts: Sequence[Mapping[str, Any]]) -> set[int]:
    owners: dict[tuple[str, str], list[int]] = {}
    for index, artifact in enumerate(artifacts):
        owners.setdefault(("batch", artifact["batch_id"]), []).append(index)
        owners.setdefault(("worker", artifact["worker_id"]), []).append(index)
        assignment = artifact["assignment"]
        for node_def in assignment["node_defs"]:
            owners.setdefault(
                (f"node:{artifact['layer']}", node_def), []
            ).append(index)
        for changed_file in assignment["files_allowlist"]:
            owners.setdefault(
                (f"file:{artifact['layer']}", changed_file), []
            ).append(index)
    return {
        index
        for indices in owners.values()
        if len(indices) > 1
        for index in indices
    }


def _integrate_artifact(
    artifact: dict[str, Any],
    *,
    backend: IntegrationBackend,
) -> dict[str, Any]:
    state = "queued"
    assignment = artifact["assignment"]
    completion = artifact["completion"]
    batch_id = artifact["batch_id"]
    layer = artifact["layer"]
    base_sha = completion["base_sha"]
    head_sha = completion["head_sha"]
    commands = [test["command"] for test in completion["tests"]]
    numeric_exits: list[int] = []
    state = _transition_state(state, "validating")

    def reject(classification: str) -> dict[str, Any]:
        rejected_state = _transition_state(state, "rejected")
        return _receipt(
            batch_id=batch_id,
            layer=layer,
            base_sha=base_sha,
            head_sha=head_sha,
            focused_commands=commands,
            numeric_exits=numeric_exits,
            final_state=rejected_state,
            failure_classification=classification,
        )

    try:
        prepared = backend.prepare_worktree(layer, batch_id, base_sha)
        if (
            not isinstance(prepared, Mapping)
            or set(prepared) != {"worktree", "base_sha"}
        ):
            return reject("invalid_backend_shape")
        worktree = _safe_string(prepared["worktree"], maximum=1_024)
        prepared_base = prepared["base_sha"]
        if not worktree:
            return reject("invalid_backend_shape")
        if prepared_base != base_sha:
            return reject("worktree_base_mismatch")

        applied = backend.apply_artifact(
            worktree,
            head_sha,
            completion["changed_files"],
        )
        if isinstance(applied, Mapping) and set(applied) == {"status"}:
            if applied["status"] == "conflict":
                return reject("apply_conflict")
            return reject("invalid_backend_shape")
        if (
            not isinstance(applied, Mapping)
            or set(applied) != {"status", "head_sha", "changed_files"}
            or applied["status"] != "applied"
        ):
            return reject("invalid_backend_shape")
        if applied["head_sha"] != head_sha:
            return reject("stale_head")
        if applied["changed_files"] != completion["changed_files"]:
            return reject("changed_file_escape")
        if set(applied["changed_files"]).difference(assignment["files_allowlist"]):
            return reject("changed_file_escape")

        command_result = backend.run_commands(worktree, commands)
        if (
            not isinstance(command_result, Mapping)
            or set(command_result) != {"commands"}
            or isinstance(command_result["commands"], (str, bytes))
            or not isinstance(command_result["commands"], Sequence)
            or len(command_result["commands"]) != len(commands)
        ):
            return reject("invalid_backend_shape")
        for index, result in enumerate(command_result["commands"]):
            if (
                not isinstance(result, Mapping)
                or set(result) != {"command", "exit_code"}
                or result["command"] != commands[index]
                or not isinstance(result["exit_code"], int)
                or isinstance(result["exit_code"], bool)
            ):
                return reject("invalid_backend_shape")
            numeric_exits.append(result["exit_code"])
        if any(exit_code != 0 for exit_code in numeric_exits):
            return reject("focused_test_failure")

        merged = backend.merge_commit(worktree, layer, batch_id, head_sha)
        if isinstance(merged, Mapping) and set(merged) == {"status"}:
            if merged["status"] == "failure":
                return reject("merge_failure")
            return reject("invalid_backend_shape")
        if (
            not isinstance(merged, Mapping)
            or set(merged) != {"status", "head_sha"}
            or merged["status"] != "merged"
            or merged["head_sha"] != head_sha
        ):
            return reject("invalid_backend_shape")
    except Exception:
        return reject("backend_exception")

    state = _transition_state(state, "integrated")
    return _receipt(
        batch_id=batch_id,
        layer=layer,
        base_sha=base_sha,
        head_sha=head_sha,
        focused_commands=commands,
        numeric_exits=numeric_exits,
        final_state=state,
    )


def run_integration_trains(
    artifacts: Sequence[Mapping[str, Any]],
    *,
    registered_families: Mapping[str, Any],
    backend: IntegrationBackend,
) -> list[dict[str, Any]]:
    """Validate and independently integrate sanitized completion artifacts."""
    if isinstance(artifacts, (str, bytes)) or not isinstance(artifacts, Sequence):
        raise ValueError("artifacts must be a sequence")
    queued: dict[str, list[dict[str, Any]]] = {
        layer: [] for layer in LAYERS
    }
    rejected: list[tuple[Any, str]] = []
    validated: list[dict[str, Any]] = []
    for artifact in artifacts:
        normalized, classification = _validate_artifact(
            artifact,
            registered_families=registered_families,
        )
        if normalized is None:
            rejected.append((artifact, classification or "invalid_artifact"))
            continue
        validated.append(normalized)

    ambiguous = _ambiguous_artifacts(validated)
    for index, normalized in enumerate(validated):
        if index in ambiguous:
            rejected.append((normalized, "ambiguous_ownership"))
        else:
            queued[normalized["layer"]].append(normalized)

    receipts = [
        _rejected_receipt(artifact, classification)
        for artifact, classification in rejected
    ]
    for layer in LAYERS:
        for artifact in sorted(
            queued[layer],
            key=lambda item: item["batch_id"],
        ):
            receipts.append(_integrate_artifact(artifact, backend=backend))
    order = {layer: index for index, layer in enumerate(LAYERS)}
    return sorted(
        receipts,
        key=lambda receipt: (
            order.get(receipt["layer"], len(order)),
            receipt["batch_id"],
        ),
    )
