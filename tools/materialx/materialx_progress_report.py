#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Auditable MaterialX progress reports derived only from canonical evidence."""

from __future__ import annotations

from collections import Counter
from collections.abc import Mapping, Sequence
import json
from typing import Any

from materialx_nodedef_ledger import validate_ledger
from materialx_project_state import validate_project_state
from materialx_velocity_manifest import (
    validate_batch_manifest,
    validate_completion_manifest,
)


__all__ = ("build_progress_report", "progress_report_json", "progress_report_text")

LAYERS = ("native_cycles", "hydra_ovrtx", "blender_authoring")
TRAIN_STATES = frozenset(("idle", "validating", "integrated", "blocked"))
CREDIT_FIELDS = {
    "ledger_delta",
    "assignment",
    "completion",
    "integration_receipt",
}
DELTA_FIELDS = {"receipt_id", "node_defs"}
INTEGRATION_FIELDS = {
    "batch_id",
    "layer",
    "base_sha",
    "head_sha",
    "focused_commands",
    "numeric_exits",
    "final_state",
}
CADENCE_RECEIPT_FIELDS = {
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
RECOVERY_ACTIONS = {
    "cadence_failure": "rerun_failed_cadence",
    "integration_train_blocked": "repair_integration_train",
    "local_cpu_failed": "rerun_local_cpu",
    "local_cuda_failed": "rerun_local_cuda",
    "windows_a40_cuda_failed": "rerun_windows_a40_cuda",
    "golden_review_failed": "repeat_golden_review",
    "auth_failure": "refresh_horde_credentials",
    "proxy_failure": "retry_unrestricted_horde_probe",
    "stale_source": "synchronize_worker_source",
    "queue_empty": "generate_validated_batch_manifests",
    "capacity_loss": "restore_five_worker_capacity",
    "integration_failure": "repair_integration_train",
    "invalid_completion": "rerun_worker_completion",
    "transport_failure": "retry_transport",
}
REPORT_FIELDS = {
    "schema_version",
    "total",
    "credited",
    "remaining",
    "phase2",
    "workers",
    "integration_trains",
    "evidence_tier_counts",
    "cadence",
    "blockers",
}


def _strings(value: Any, field: str, *, allow_empty: bool = True) -> list[str]:
    if (
        isinstance(value, (str, bytes))
        or not isinstance(value, Sequence)
        or (not allow_empty and not value)
        or any(not isinstance(item, str) or not item for item in value)
    ):
        raise ValueError(f"{field} must be a string list")
    if len(value) != len(set(value)):
        raise ValueError(f"{field} contains duplicates")
    return sorted(value)


def _integration_receipt(
    value: Any,
    assignment: Mapping[str, Any],
    completion: Mapping[str, Any],
) -> dict[str, Any]:
    if not isinstance(value, Mapping) or set(value) != INTEGRATION_FIELDS:
        raise ValueError("integration receipt must be canonical integrated Task 7 evidence")
    expected = {
        "batch_id": assignment["batch_id"],
        "layer": assignment["layer"],
        "base_sha": completion["base_sha"],
        "head_sha": completion["head_sha"],
        "focused_commands": assignment["focused_test_commands"],
        "numeric_exits": [0] * len(assignment["focused_test_commands"]),
        "final_state": "integrated",
    }
    if value != expected:
        raise ValueError("integration receipt does not match completion and assignment")
    return dict(expected)


def _cadence_evidence(
    decision: Any,
    receipts: Any,
) -> tuple[dict[str, dict[str, Any]], set[str], int, int]:
    if not isinstance(decision, Mapping):
        raise ValueError("cadence decision must be a mapping")
    expected_decision_fields = {
        "schema_version",
        "evidence_tier",
        "reason",
        "affected_layers",
        "families",
        "node_defs",
        "commands",
        "milestone_generation",
    }
    if (
        set(decision) != expected_decision_fields
        or decision["schema_version"] != 1
        or decision["evidence_tier"] != "generated_due_decision"
        or isinstance(decision["milestone_generation"], bool)
        or not isinstance(decision["milestone_generation"], int)
    ):
        raise ValueError("cadence decision is noncanonical")
    commands = decision["commands"]
    if isinstance(commands, (str, bytes)) or not isinstance(commands, Sequence):
        raise ValueError("cadence decision commands are invalid")
    command_by_id = {}
    for command in commands:
        if (
            not isinstance(command, Mapping)
            or set(command) != {"command_id", "tier", "scope", "argv"}
            or not isinstance(command["command_id"], str)
            or not command["command_id"]
            or command["command_id"] in command_by_id
            or isinstance(command["argv"], (str, bytes))
            or not isinstance(command["argv"], Sequence)
            or not command["argv"]
            or any(not isinstance(item, str) or not item for item in command["argv"])
        ):
            raise ValueError("cadence command is noncanonical")
        command_by_id[command["command_id"]] = dict(command)
    if isinstance(receipts, (str, bytes)) or not isinstance(receipts, Sequence):
        raise ValueError("cadence receipts must be a sequence")
    receipt_by_id = {}
    failed_scopes: set[str] = set()
    green = 0
    for receipt in receipts:
        if (
            not isinstance(receipt, Mapping)
            or set(receipt) != CADENCE_RECEIPT_FIELDS
            or receipt["command_id"] not in command_by_id
            or receipt["command_id"] in receipt_by_id
        ):
            raise ValueError("cadence receipt is noncanonical or duplicated")
        command = command_by_id[receipt["command_id"]]
        if (
            receipt["argv"] != command["argv"]
            or receipt["tier"] != command["tier"]
            or receipt["milestone_generation"] != decision["milestone_generation"]
            or isinstance(receipt["exit_code"], bool)
            or not isinstance(receipt["exit_code"], int)
            or isinstance(receipt["passed"], bool)
            or not isinstance(receipt["passed"], int)
            or isinstance(receipt["failed"], bool)
            or not isinstance(receipt["failed"], int)
            or receipt["passed"] not in {0, 1}
            or receipt["failed"] not in {0, 1}
            or receipt["passed"] + receipt["failed"] != 1
        ):
            raise ValueError("cadence receipt does not match its due command")
        is_green = (
            receipt["classification"] == "green"
            and receipt["exit_code"] == 0
            and receipt["passed"] == 1
            and receipt["failed"] == 0
        )
        is_failure = (
            receipt["classification"] in {
                "missing_runner",
                "runner_exception",
                "malformed_result",
                "nonzero_exit",
            }
            and receipt["exit_code"] != 0
            and receipt["passed"] == 0
            and receipt["failed"] == 1
        )
        if not is_green and not is_failure:
            raise ValueError("cadence receipt classification contradicts numeric evidence")
        if is_green:
            green += 1
        else:
            failed_scopes.add(command["scope"])
        receipt_by_id[receipt["command_id"]] = dict(receipt)
    missing = set(command_by_id).difference(receipt_by_id)
    for command_id in missing:
        failed_scopes.add(command_by_id[command_id]["scope"])
    return receipt_by_id, failed_scopes, len(command_by_id), green


def _credit_record(
    value: Any,
    *,
    registered_families: Mapping[str, Any],
) -> dict[str, Any]:
    if not isinstance(value, Mapping) or set(value) != CREDIT_FIELDS:
        raise ValueError("credit record requires exactly canonical correlated evidence")
    assignment = validate_batch_manifest(
        value["assignment"], registered_families=registered_families
    )
    if value["assignment"] != assignment:
        raise ValueError("credit assignment must be canonical")
    completion = validate_completion_manifest(assignment, value["completion"])
    if value["completion"] != completion:
        raise ValueError("credit completion must be canonical")
    integration = _integration_receipt(
        value["integration_receipt"], assignment, completion
    )
    delta = value["ledger_delta"]
    if not isinstance(delta, Mapping) or set(delta) != DELTA_FIELDS:
        raise ValueError("ledger delta fields are invalid")
    if delta["receipt_id"] != f"ledger-delta-{assignment['batch_id']}":
        raise ValueError("ledger delta receipt identity is not bound to batch")
    delta_nodes = _strings(delta["node_defs"], "ledger delta node_defs", allow_empty=False)
    if delta_nodes != assignment["node_defs"]:
        raise ValueError("ledger delta NodeDefs do not match assignment")
    return {
        "assignment": assignment,
        "completion": completion,
        "integration_receipt": integration,
        "ledger_delta": {
            "receipt_id": delta["receipt_id"],
            "node_defs": delta_nodes,
        },
    }


def build_progress_report(
    *,
    ledger: Mapping[str, Any],
    phase2_ids: Sequence[str],
    credit_records: Sequence[Mapping[str, Any]],
    cadence_decision: Mapping[str, Any],
    cadence_receipts: Sequence[Mapping[str, Any]],
    project_state: Mapping[str, Any],
    integration_train_states: Mapping[str, str],
    registered_families: Mapping[str, Any],
) -> dict[str, Any]:
    """Reconcile exactly 802 ledger rows without crediting operational activity."""
    validate_ledger(ledger, expected_count=802)
    ledger_ids = {row["id"] for row in ledger["rows"]}
    phase2 = set(_strings(phase2_ids, "phase2_ids"))
    unknown_phase2 = phase2.difference(ledger_ids)
    if unknown_phase2:
        raise ValueError("Phase-2 ownership references unknown NodeDefs")
    if (
        isinstance(credit_records, (str, bytes))
        or not isinstance(credit_records, Sequence)
    ):
        raise ValueError("credit_records must be a sequence")
    credits = [
        _credit_record(record, registered_families=registered_families)
        for record in credit_records
    ]
    receipt_by_id, failed_scopes, due_count, green_count = _cadence_evidence(
        cadence_decision, cadence_receipts
    )
    state = validate_project_state(project_state)
    if (
        not isinstance(integration_train_states, Mapping)
        or set(integration_train_states) != set(LAYERS)
        or any(value not in TRAIN_STATES for value in integration_train_states.values())
    ):
        raise ValueError("integration train states must be the exact three canonical lanes")

    credited_ids: set[str] = set()
    tiers = Counter()
    batch_ids = set()
    for credit in credits:
        assignment = credit["assignment"]
        batch_id = assignment["batch_id"]
        if batch_id in batch_ids:
            raise ValueError("duplicate credit batch")
        batch_ids.add(batch_id)
        nodes = set(assignment["node_defs"])
        if not nodes.issubset(ledger_ids):
            raise ValueError("credit references unknown NodeDefs")
        if credited_ids.intersection(nodes):
            raise ValueError("credit records contain duplicate NodeDefs")
        focused_commands = [
            command
            for command in cadence_decision["commands"]
            if command["tier"] == "focused" and command["scope"] == batch_id
        ]
        focused_green = (
            len(focused_commands) == len(assignment["focused_test_commands"])
            and all(
                command["command_id"] in receipt_by_id
                and receipt_by_id[command["command_id"]]["classification"] == "green"
                for command in focused_commands
            )
        )
        layer_full_green = not any(
            scope.startswith(assignment["layer"] + ":")
            for scope in failed_scopes
        )
        if not focused_green or not layer_full_green or batch_id in failed_scopes:
            continue
        credited_ids.update(nodes)
        count = len(nodes)
        tiers["completion_manifest_v2"] += count
        tiers[assignment["generated_evidence_tier"]] += count
        tiers["integrated"] += count
        tiers["focused_green"] += count

    overlap = credited_ids.intersection(phase2)
    if overlap:
        raise ValueError("credited and Phase-2 NodeDefs overlap")
    remaining_ids = ledger_ids.difference(credited_ids).difference(phase2)
    if len(credited_ids) + len(remaining_ids) + len(phase2) != 802:
        raise ValueError("ledger reconciliation does not total 802")

    assignments = {
        item["worker_id"]: item["batch_id"]
        for item in state["assigned_batches"]
    }
    workers = [
        {
            "id": worker["id"],
            "state": worker["state"],
            "assignment": assignments.get(worker["id"], ""),
        }
        for worker in state["workers"]
    ]
    blockers = []
    for layer, train_state in sorted(integration_train_states.items()):
        if train_state == "blocked":
            blockers.append({
                "classification": "integration_train_blocked",
                "subject": layer,
                "recovery_action": RECOVERY_ACTIONS["integration_train_blocked"],
            })
    if green_count != due_count:
        blockers.append({
            "classification": "cadence_failure",
            "subject": "due_commands",
            "recovery_action": RECOVERY_ACTIONS["cadence_failure"],
        })
    for lane in ("local_cpu", "local_cuda", "windows_a40_cuda", "golden_review"):
        if state["lanes"][lane]["state"] == "failed":
            classification = f"{lane}_failed"
            blockers.append({
                "classification": classification,
                "subject": lane,
                "recovery_action": RECOVERY_ACTIONS[classification],
            })
        elif state["lanes"][lane]["state"] == "green":
            tiers[f"{lane}_green"] += 1
    for classification in state["failure_classifications"]:
        blockers.append({
            "classification": classification,
            "subject": "project_state",
            "recovery_action": RECOVERY_ACTIONS[classification],
        })
    blockers.sort(key=lambda item: (item["classification"], item["subject"]))

    return {
        "schema_version": 1,
        "total": 802,
        "credited": len(credited_ids),
        "remaining": len(remaining_ids),
        "phase2": len(phase2),
        "workers": workers,
        "integration_trains": {
            layer: integration_train_states[layer] for layer in LAYERS
        },
        "evidence_tier_counts": dict(sorted(tiers.items())),
        "cadence": {
            "due": due_count,
            "executed": len(cadence_receipts),
            "executed_green": green_count,
            "state": "green" if due_count == green_count else "failed",
        },
        "blockers": blockers,
    }


def _validated_report(report: Mapping[str, Any]) -> dict[str, Any]:
    if (
        not isinstance(report, Mapping)
        or set(report) != REPORT_FIELDS
        or report["schema_version"] != 1
        or isinstance(report["schema_version"], bool)
    ):
        raise ValueError("progress report fields are invalid")
    counts = ("total", "credited", "remaining", "phase2")
    if any(
        isinstance(report[field], bool)
        or not isinstance(report[field], int)
        or report[field] < 0
        for field in counts
    ):
        raise ValueError("progress report counts are invalid")
    if (
        report["total"] != 802
        or report["credited"] + report["remaining"] + report["phase2"] != 802
    ):
        raise ValueError("progress report does not reconcile to 802")
    workers = report["workers"]
    if (
        not isinstance(workers, Sequence)
        or len(workers) != 5
        or any(
            not isinstance(item, Mapping)
            or set(item) != {"id", "state", "assignment"}
            or not all(isinstance(item[field], str) for field in item)
            for item in workers
        )
    ):
        raise ValueError("progress report workers are invalid")
    trains = report["integration_trains"]
    if (
        not isinstance(trains, Mapping)
        or set(trains) != set(LAYERS)
        or any(state not in TRAIN_STATES for state in trains.values())
    ):
        raise ValueError("progress report integration trains are invalid")
    tiers = report["evidence_tier_counts"]
    if (
        not isinstance(tiers, Mapping)
        or any(
            not isinstance(key, str)
            or not key
            or isinstance(value, bool)
            or not isinstance(value, int)
            or value < 0
            for key, value in tiers.items()
        )
    ):
        raise ValueError("progress report evidence tier counts are invalid")
    cadence = report["cadence"]
    if (
        not isinstance(cadence, Mapping)
        or set(cadence) != {"due", "executed", "executed_green", "state"}
        or cadence["state"] not in {"green", "failed"}
        or any(
            isinstance(cadence[field], bool)
            or not isinstance(cadence[field], int)
            or cadence[field] < 0
            for field in ("due", "executed", "executed_green")
        )
        or cadence["executed_green"] > cadence["executed"]
        or cadence["executed"] > cadence["due"]
        or (cadence["state"] == "green") != (
            cadence["executed_green"] == cadence["due"]
        )
    ):
        raise ValueError("progress report cadence is invalid")
    blockers = report["blockers"]
    if isinstance(blockers, (str, bytes)) or not isinstance(blockers, Sequence):
        raise ValueError("progress report blockers are invalid")
    for blocker in blockers:
        if (
            not isinstance(blocker, Mapping)
            or set(blocker) != {
                "classification", "subject", "recovery_action"
            }
            or blocker["classification"] not in RECOVERY_ACTIONS
            or blocker["recovery_action"]
            != RECOVERY_ACTIONS[blocker["classification"]]
            or not isinstance(blocker["subject"], str)
            or not blocker["subject"]
        ):
            raise ValueError("progress report blocker is invalid")
    return dict(report)


def progress_report_json(report: Mapping[str, Any]) -> str:
    """Serialize an already-built sanitized report deterministically."""
    return json.dumps(_validated_report(report), indent=2, sort_keys=True) + "\n"


def progress_report_text(report: Mapping[str, Any]) -> str:
    """Render a concise human summary without embedding raw evidence."""
    report = _validated_report(report)
    worker_states = Counter(item["state"] for item in report["workers"])
    train_states = ", ".join(
        f"{layer}={state}" for layer, state in report["integration_trains"].items()
    )
    blockers = ", ".join(
        item["classification"] for item in report["blockers"]
    ) or "none"
    return "\n".join((
        (
            f"MaterialX: {report['total']} total; {report['credited']} credited; "
            f"{report['remaining']} remaining; {report['phase2']} Phase-2"
        ),
        "Workers: " + ", ".join(
            f"{state}={count}" for state, count in sorted(worker_states.items())
        ),
        f"Integration trains: {train_states}",
        (
            f"Cadence: {report['cadence']['state']} "
            f"({report['cadence']['executed_green']}/{report['cadence']['due']} green)"
        ),
        f"Blockers: {blockers}",
    )) + "\n"
