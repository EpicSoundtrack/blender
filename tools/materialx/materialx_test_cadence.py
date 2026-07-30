#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Evidence-bound MaterialX test cadence decisions and bounded execution."""

from __future__ import annotations

from collections.abc import Mapping, Sequence
import hashlib
import re
import shlex
from typing import Any, Callable

from materialx_project_state import validate_project_state
from materialx_velocity_manifest import validate_batch_manifest


__all__ = ("build_cadence_decision", "execute_cadence")

LAYERS = ("native_cycles", "hydra_ovrtx", "blender_authoring")
LANES = ("golden_review", "local_cpu", "local_cuda", "windows_a40_cuda")
CONFIG_FIELDS = {
    "schema_version",
    "full_suite_interval",
    "full_suite_commands",
    "lane_commands",
}
INTEGRATION_FIELDS = {"assignment", "receipt"}
RECEIPT_FIELDS = {
    "batch_id",
    "layer",
    "base_sha",
    "head_sha",
    "focused_commands",
    "numeric_exits",
    "final_state",
}
DECISION_FIELDS = {
    "schema_version",
    "evidence_tier",
    "reason",
    "affected_layers",
    "families",
    "node_defs",
    "commands",
    "milestone_generation",
}
COMMAND_FIELDS = {"command_id", "tier", "scope", "argv"}
_TIER_ORDER = {
    "focused": 0,
    "full": 1,
    "golden_review": 2,
    "local_cpu": 3,
    "local_cuda": 4,
    "windows_a40_cuda": 5,
}
_UNSAFE_SHELL_TOKENS = {"|", "||", "&", "&&", ";", ">", ">>", "<", "<<"}
_SECRET_LIKE = re.compile(
    r"(?:NVIDIA_API_KEY|api[_-]?key|secret|password|credential|token)\s*[:=]",
    re.I,
)
COMMAND_TIMEOUT_SECONDS = 900


def _argv(value: Any, field: str) -> list[str]:
    if (
        isinstance(value, (str, bytes))
        or not isinstance(value, Sequence)
        or not value
        or any(not isinstance(item, str) or not item for item in value)
        or any(item in _UNSAFE_SHELL_TOKENS for item in value)
        or any(_SECRET_LIKE.search(item) for item in value)
    ):
        raise ValueError(f"{field} must be a safe non-empty argv")
    return list(value)


def _command_argv(command: str) -> list[str]:
    if (
        not isinstance(command, str)
        or not command
        or any(character in command for character in ("\n", "\r", "\0"))
    ):
        raise ValueError("focused command is invalid")
    try:
        argv = shlex.split(command, posix=True)
    except ValueError as ex:
        raise ValueError("focused command cannot be parsed as argv") from ex
    return _argv(argv, "focused command")


def _commands_mapping(value: Any, keys: Sequence[str], field: str) -> dict[str, list[list[str]]]:
    if not isinstance(value, Mapping) or set(value) != set(keys):
        raise ValueError(f"{field} must contain exactly {sorted(keys)}")
    result = {}
    for key in keys:
        commands = value[key]
        if (
            isinstance(commands, (str, bytes))
            or not isinstance(commands, Sequence)
            or not commands
        ):
            raise ValueError(f"{field}.{key} must be a non-empty command list")
        result[key] = [
            _argv(command, f"{field}.{key}")
            for command in commands
        ]
    return result


def _config(value: Any) -> dict[str, Any]:
    if not isinstance(value, Mapping) or set(value) != CONFIG_FIELDS:
        raise ValueError("cadence config must contain exactly the canonical fields")
    interval = value["full_suite_interval"]
    if isinstance(interval, bool) or not isinstance(interval, int) or not 3 <= interval <= 5:
        raise ValueError("full_suite_interval must be an integer from 3 through 5")
    if value["schema_version"] != 1 or isinstance(value["schema_version"], bool):
        raise ValueError("cadence config schema_version must be one")
    return {
        "schema_version": 1,
        "full_suite_interval": interval,
        "full_suite_commands": _commands_mapping(
            value["full_suite_commands"], LAYERS, "full_suite_commands"
        ),
        "lane_commands": _commands_mapping(
            value["lane_commands"], LANES, "lane_commands"
        ),
    }


def _integration(
    value: Any,
    *,
    registered_families: Mapping[str, Any],
) -> dict[str, Any]:
    if not isinstance(value, Mapping) or set(value) != INTEGRATION_FIELDS:
        raise ValueError("integration evidence requires exactly assignment and receipt")
    assignment = validate_batch_manifest(
        value["assignment"], registered_families=registered_families
    )
    if value["assignment"] != assignment:
        raise ValueError("integration assignment must be canonical")
    receipt = value["receipt"]
    if not isinstance(receipt, Mapping) or set(receipt) != RECEIPT_FIELDS:
        raise ValueError("integration receipt must be canonical integrated Task 7 evidence")
    if (
        receipt["final_state"] != "integrated"
        or receipt["batch_id"] != assignment["batch_id"]
        or receipt["layer"] != assignment["layer"]
        or receipt["base_sha"] != assignment["integration_base_sha"]
        or receipt["head_sha"] == receipt["base_sha"]
        or receipt["focused_commands"] != assignment["focused_test_commands"]
    ):
        raise ValueError("integration receipt does not match its assignment")
    for sha_field in ("base_sha", "head_sha"):
        sha = receipt[sha_field]
        if (
            not isinstance(sha, str)
            or len(sha) != 40
            or any(character not in "0123456789abcdef" for character in sha)
        ):
            raise ValueError("integration receipt SHA is invalid")
    exits = receipt["numeric_exits"]
    if (
        isinstance(exits, (str, bytes))
        or not isinstance(exits, Sequence)
        or len(exits) != len(receipt["focused_commands"])
        or any(isinstance(code, bool) or not isinstance(code, int) or code != 0 for code in exits)
    ):
        raise ValueError("integrated receipt requires exact zero exits")
    return {"assignment": assignment, "receipt": dict(receipt)}


def _command(tier: str, scope: str, argv: Sequence[str]) -> dict[str, Any]:
    normalized_argv = _argv(argv, "command argv")
    identity = "\0".join((tier, scope, *normalized_argv)).encode("utf-8")
    return {
        "command_id": "cadence-" + hashlib.sha256(identity).hexdigest()[:24],
        "tier": tier,
        "scope": scope,
        "argv": normalized_argv,
    }


def build_cadence_decision(
    *,
    integrations: Sequence[Mapping[str, Any]],
    project_state: Mapping[str, Any],
    cadence_config: Mapping[str, Any],
    registered_families: Mapping[str, Any],
) -> dict[str, Any]:
    """Build one deterministic decision from canonical integration/state evidence."""
    state = validate_project_state(project_state)
    config = _config(cadence_config)
    if (
        isinstance(integrations, (str, bytes))
        or not isinstance(integrations, Sequence)
    ):
        raise ValueError("integrations must be a sequence")
    normalized = []
    for item in integrations:
        try:
            candidate = _integration(
                item, registered_families=registered_families
            )
        except (TypeError, ValueError):
            continue
        normalized.append(candidate)
    normalized.sort(key=lambda item: (
        LAYERS.index(item["assignment"]["layer"]),
        item["assignment"]["batch_id"],
    ))
    batch_ids: set[str] = set()
    node_defs: set[str] = set()
    existing_batches = {
        item["batch_id"]
        for item in state["integration_receipts"]
    }
    for item in normalized:
        assignment = item["assignment"]
        if assignment["batch_id"] in batch_ids or assignment["batch_id"] in existing_batches:
            raise ValueError("duplicate or already credited integration batch")
        overlap = node_defs.intersection(assignment["node_defs"])
        if overlap:
            raise ValueError("integration evidence contains duplicate NodeDefs")
        batch_ids.add(assignment["batch_id"])
        node_defs.update(assignment["node_defs"])

    commands = []
    full_due_layers: set[str] = set()
    interval = config["full_suite_interval"]
    existing_counts = {
        layer: sum(
            receipt["final_state"] == "integrated" and receipt["layer"] == layer
            for receipt in state["integration_receipts"]
        )
        for layer in LAYERS
    }
    incoming_counts = {layer: 0 for layer in LAYERS}
    for item in normalized:
        assignment = item["assignment"]
        layer = assignment["layer"]
        incoming_counts[layer] += 1
        for command in assignment["focused_test_commands"]:
            commands.append(_command(
                "focused",
                assignment["batch_id"],
                _command_argv(command),
            ))
    for layer in LAYERS:
        before = existing_counts[layer]
        after = before + incoming_counts[layer]
        boundaries = range(
            ((before // interval) + 1) * interval,
            after + 1,
            interval,
        )
        for boundary in boundaries:
            full_due_layers.add(layer)
            for argv in config["full_suite_commands"][layer]:
                commands.append(_command(
                    "full", f"{layer}:{boundary}", argv
                ))

    for lane in LANES:
        if state["lanes"][lane]["state"] == "due":
            for argv in config["lane_commands"][lane]:
                commands.append(_command(lane, lane, argv))

    command_ids = [item["command_id"] for item in commands]
    if len(command_ids) != len(set(command_ids)):
        raise ValueError("cadence decision contains duplicate command identity")
    commands.sort(key=lambda item: (
        _TIER_ORDER[item["tier"]],
        item["scope"],
        item["command_id"],
    ))
    reasons = []
    if full_due_layers:
        reasons.append("full_suite_interval")
    if normalized:
        reasons.append("new_integrated_family")
    if any(state["lanes"][lane]["state"] == "due" for lane in LANES):
        if state["lanes"]["golden_review"]["state"] == "due":
            reasons.append("explicit_release_gate")
        reasons.append("project_lane_due")
    reasons = sorted(set(reasons))
    return {
        "schema_version": 1,
        "evidence_tier": "generated_due_decision",
        "reason": reasons,
        "affected_layers": sorted({
            item["assignment"]["layer"] for item in normalized
        }.union(full_due_layers)),
        "families": sorted({
            item["assignment"]["family_id"] for item in normalized
        }),
        "node_defs": sorted(node_defs),
        "commands": commands,
        "milestone_generation": state["milestones"]["generation"],
    }


def _validate_decision(value: Any) -> dict[str, Any]:
    if not isinstance(value, Mapping) or set(value) != DECISION_FIELDS:
        raise ValueError("cadence decision fields are invalid")
    if (
        value["schema_version"] != 1
        or value["evidence_tier"] != "generated_due_decision"
        or isinstance(value["milestone_generation"], bool)
        or not isinstance(value["milestone_generation"], int)
        or value["milestone_generation"] < 0
    ):
        raise ValueError("cadence decision metadata is invalid")
    commands = value["commands"]
    if isinstance(commands, (str, bytes)) or not isinstance(commands, Sequence):
        raise ValueError("cadence commands must be a sequence")
    normalized_commands = []
    seen = set()
    for item in commands:
        if (
            not isinstance(item, Mapping)
            or set(item) != COMMAND_FIELDS
            or item["tier"] not in _TIER_ORDER
            or not isinstance(item["scope"], str)
            or not item["scope"]
        ):
            raise ValueError("cadence command fields are invalid")
        expected = _command(item["tier"], item["scope"], item["argv"])
        if item != expected or item["command_id"] in seen:
            raise ValueError("cadence command identity is invalid")
        seen.add(item["command_id"])
        normalized_commands.append(expected)
    normalized = dict(value)
    normalized["commands"] = normalized_commands
    return normalized


def execute_cadence(
    decision: Mapping[str, Any],
    *,
    runner: Callable[[Sequence[str]], Mapping[str, Any]] | None,
) -> list[dict[str, Any]]:
    """Execute all due argv independently and return only sanitized evidence."""
    normalized = _validate_decision(decision)
    receipts = []
    generation = normalized["milestone_generation"]
    for command in normalized["commands"]:
        classification = "missing_runner"
        exit_code = -1
        if runner is not None:
            try:
                result = runner(
                    tuple(command["argv"]),
                    timeout_seconds=COMMAND_TIMEOUT_SECONDS,
                )
            except Exception:
                classification = "runner_exception"
            else:
                if (
                    isinstance(result, Mapping)
                    and set(result) == {"exit_code"}
                    and isinstance(result["exit_code"], int)
                    and not isinstance(result["exit_code"], bool)
                ):
                    exit_code = result["exit_code"]
                    classification = "green" if exit_code == 0 else "nonzero_exit"
                else:
                    classification = "malformed_result"
        green = classification == "green"
        receipt_identity = (
            f"{command['command_id']}\0{generation}\0{exit_code}\0{classification}"
        ).encode("utf-8")
        receipts.append({
            "receipt_id": "cadence-receipt-" + hashlib.sha256(receipt_identity).hexdigest()[:24],
            "command_id": command["command_id"],
            "argv": list(command["argv"]),
            "tier": command["tier"],
            "milestone_generation": generation,
            "exit_code": exit_code,
            "passed": 1 if green else 0,
            "failed": 0 if green else 1,
            "classification": classification,
        })
    return receipts
