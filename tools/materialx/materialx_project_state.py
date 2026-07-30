#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Canonical, evidence-bound MaterialX project state."""

from __future__ import annotations

__all__ = (
    "EXPECTED_HORDE_WORKERS",
    "SCHEMA_VERSION",
    "apply_lane_evidence",
    "assert_journal_extension",
    "credit_integration",
    "load_project_state",
    "migrate_project_state",
    "new_project_state",
    "serialize_project_state",
    "set_release_gate",
    "update_horde_dispatch",
    "update_horde_observation",
    "validate_project_state",
    "write_project_state",
)

import copy
from contextlib import contextmanager
import hashlib
import json
import math
import os
from pathlib import Path
import re
import tempfile
from typing import Any, Mapping, Sequence

from materialx_velocity_manifest import EXPECTED_HORDE_WORKERS

if os.name == "nt":
    import msvcrt
else:
    import fcntl


SCHEMA_VERSION = 2
LANE_IDS = (
    "golden_review",
    "horde",
    "local_cpu",
    "local_cuda",
    "windows_a40_cuda",
)
PROJECT_FIELDS = {
    "schema_version",
    "workers",
    "lanes",
    "milestones",
    "completed_rows",
    "evidence_records",
    "journal_records",
    "assigned_batches",
    "integration_receipts",
    "alerts",
    "failure_classifications",
    "queue_depth",
    "cycle_sequence",
    "checked_at",
    "last_successful_poll",
    "healthy",
    "semantic_journal",
}
MILESTONE_FIELDS = {
    "generation",
    "integrated_nodedefs",
    "local_green_threshold",
    "windows_green_threshold",
    "local_last_green_count",
    "windows_last_green_count",
    "render_path_revision",
    "release_gate",
}
WORKER_STATES = frozenset((
    "unknown",
    "active",
    "idle",
    "blocked",
    "exited",
    "failure",
    "stale_source",
    "auth_failure",
    "proxy_failure",
    "source_preflight_failure",
    "source_sync_failure",
    "missing_repository",
    "missing_required_file",
    "invalid_probe",
    "probe_failure",
    "credential_persistence_failure",
    "launch_failure",
    "process_missing",
    "ready",
))
LANE_STATES = {
    "horde": frozenset(("unknown", "active", "degraded", "blocked")),
    "local_cpu": frozenset(("not_due", "due", "green", "failed", "unknown")),
    "local_cuda": frozenset(("not_due", "due", "green", "failed", "unknown")),
    "windows_a40_cuda": frozenset(("not_due", "due", "green", "failed", "unknown")),
    "golden_review": frozenset(("not_due", "due", "green", "failed", "unknown")),
}
JOURNAL_EVENT_KINDS = frozenset((
    "dispatch",
    "horde_lane",
    "horde_worker",
    "integration_credit",
    "lane_due",
    "lane_evidence",
    "release_gate",
))
JOURNAL_REASONS = frozenset((
    "dispatch_failure",
    "dispatch_success",
    "explicit_release_gate",
    "integration_milestone",
    "matching_evidence",
    "probe_failure",
    "probe_observation",
    "render_path_edit",
))
JOURNAL_STATES = frozenset((
    "none",
    *WORKER_STATES,
    *(state for states in LANE_STATES.values() for state in states),
    "credited",
    "disabled",
    "enabled",
))
LANE_EVIDENCE_TYPES = {
    "local_cpu": "local_cpu_render",
    "local_cuda": "local_cuda_render",
    "windows_a40_cuda": "windows_a40_cuda_render",
    "golden_review": "golden_review",
}
ALLOWED_FAILURE_CLASSIFICATIONS = frozenset((
    "auth_failure",
    "capacity_loss",
    "integration_failure",
    "invalid_completion",
    "proxy_failure",
    "queue_empty",
    "stale_source",
    "transport_failure",
))
_INTEGRATION_LAYER_ORDER = {
    "native_cycles": 0,
    "hydra_ovrtx": 1,
    "blender_authoring": 2,
}
_IDENTIFIER = re.compile(r"^[A-Za-z0-9_.:/ -]{1,128}$")
_CATEGORY = re.compile(r"^[a-z][a-z0-9_]{0,63}$")
_SECRET_KEY = re.compile(r"(?:secret|password|credential|api[_-]?key|token)", re.I)
_SECRET_VALUE = re.compile(
    r"(?:NVIDIA_API_KEY|(?:secret|password|credential|token)\s*[:=])",
    re.I,
)


def _bounded_string(value: Any, field: str, *, allow_empty: bool = False) -> str:
    if (
        not isinstance(value, str)
        or (not value and not allow_empty)
        or len(value) > 128
        or (value and not _IDENTIFIER.fullmatch(value))
    ):
        raise ValueError(f"{field} must be a bounded identifier")
    return value


def _nonnegative_integer(value: Any, field: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise ValueError(f"{field} must be a non-negative integer")
    return value


def _finite_number(value: Any, field: str, *, optional: bool = False) -> float | None:
    if optional and value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError(f"{field} must be finite")
    return float(value)


def _safe_json(value: Any, field: str) -> Any:
    def visit(item: Any) -> None:
        if isinstance(item, str):
            if _SECRET_VALUE.search(item):
                raise ValueError(f"{field} contains secret-like content")
            return
        if item is None or isinstance(item, (bool, int)):
            return
        if isinstance(item, float):
            if not math.isfinite(item):
                raise ValueError(f"{field} contains a non-finite number")
            return
        if isinstance(item, list):
            for child in item:
                visit(child)
            return
        if isinstance(item, dict):
            for key, child in item.items():
                if not isinstance(key, str) or _SECRET_KEY.search(key):
                    raise ValueError(f"{field} contains an unsafe key")
                visit(child)
            return
        raise ValueError(f"{field} contains unsupported JSON")

    visit(value)
    if len(json.dumps(value, allow_nan=False, sort_keys=True)) > 8192:
        raise ValueError(f"{field} is oversized")
    return copy.deepcopy(value)


def _record_list(value: Any, field: str) -> list[dict[str, Any]]:
    if isinstance(value, (str, bytes)) or not isinstance(value, Sequence):
        raise ValueError(f"{field} must be a list")
    result = []
    seen = set()
    for item in value:
        if not isinstance(item, Mapping) or set(item) != {"row_id", "record"}:
            raise ValueError(f"{field} records require row_id and record")
        row_id = _bounded_string(item["row_id"], f"{field}.row_id")
        if row_id in seen:
            raise ValueError(f"{field} contains duplicate row_id")
        seen.add(row_id)
        result.append({"row_id": row_id, "record": _safe_json(item["record"], field)})
    return sorted(result, key=lambda item: item["row_id"])


def _journal_record(
    sequence: int,
    *,
    event_kind: str,
    subject: str,
    previous_state: str,
    new_state: str,
    reason: str,
    evidence_receipt: str,
    batch_id: str | None = None,
) -> dict[str, Any]:
    record = {
        "sequence": sequence,
        "event_kind": event_kind,
        "subject": subject,
        "previous_state": previous_state,
        "new_state": new_state,
        "reason": reason,
        "evidence_receipt": evidence_receipt,
    }
    if batch_id is not None:
        record["batch_id"] = batch_id
    return record


def _append_event(state: dict[str, Any], **values: Any) -> None:
    state["semantic_journal"].append(_journal_record(
        len(state["semantic_journal"]) + 1,
        **values,
    ))


def new_project_state(
    *,
    local_green_threshold: int = 32,
    windows_green_threshold: int = 128,
) -> dict[str, Any]:
    """Return the single empty canonical project state."""
    if (
        isinstance(local_green_threshold, bool)
        or not isinstance(local_green_threshold, int)
        or not 32 <= local_green_threshold <= 64
    ):
        raise ValueError("local_green_threshold must be an integer from 32 through 64")
    if (
        isinstance(windows_green_threshold, bool)
        or not isinstance(windows_green_threshold, int)
        or windows_green_threshold <= local_green_threshold
    ):
        raise ValueError("windows_green_threshold must be larger than local_green_threshold")
    state = {
        "schema_version": SCHEMA_VERSION,
        "workers": [
            {"id": worker, "state": "unknown", "last_evidence_id": ""}
            for worker in sorted(EXPECTED_HORDE_WORKERS)
        ],
        "lanes": {
            "golden_review": {"state": "not_due", "last_evidence_id": ""},
            "horde": {"state": "unknown", "last_evidence_id": ""},
            "local_cpu": {"state": "not_due", "last_evidence_id": ""},
            "local_cuda": {"state": "not_due", "last_evidence_id": ""},
            "windows_a40_cuda": {"state": "not_due", "last_evidence_id": ""},
        },
        "milestones": {
            "generation": 0,
            "integrated_nodedefs": 0,
            "local_green_threshold": local_green_threshold,
            "windows_green_threshold": windows_green_threshold,
            "local_last_green_count": 0,
            "windows_last_green_count": 0,
            "render_path_revision": 0,
            "release_gate": False,
        },
        "completed_rows": [],
        "evidence_records": [],
        "journal_records": [],
        "assigned_batches": [],
        "integration_receipts": [],
        "alerts": [],
        "failure_classifications": [],
        "queue_depth": 0,
        "cycle_sequence": 0,
        "checked_at": 0.0,
        "last_successful_poll": None,
        "healthy": False,
        "semantic_journal": [],
    }
    return validate_project_state(state)


def _validate_semantic_journal(value: Any) -> list[dict[str, Any]]:
    if isinstance(value, (str, bytes)) or not isinstance(value, Sequence):
        raise ValueError("semantic_journal must be a list")
    result = []
    for expected_sequence, item in enumerate(value, 1):
        if not isinstance(item, Mapping):
            raise ValueError("semantic journal records must be mappings")
        expected_fields = {
            "sequence",
            "event_kind",
            "subject",
            "previous_state",
            "new_state",
            "reason",
            "evidence_receipt",
        }
        if "batch_id" in item:
            expected_fields.add("batch_id")
        if set(item) != expected_fields or item["sequence"] != expected_sequence:
            raise ValueError("semantic journal sequence or fields are invalid")
        if item["event_kind"] not in JOURNAL_EVENT_KINDS:
            raise ValueError("semantic journal event_kind is unsupported")
        if item["previous_state"] not in JOURNAL_STATES or item["new_state"] not in JOURNAL_STATES:
            raise ValueError("semantic journal state is unsupported")
        if item["reason"] not in JOURNAL_REASONS:
            raise ValueError("semantic journal reason is unsupported")
        record = dict(item)
        _bounded_string(record["subject"], "semantic_journal.subject")
        _bounded_string(record["evidence_receipt"], "semantic_journal.evidence_receipt")
        if "batch_id" in record:
            _bounded_string(record["batch_id"], "semantic_journal.batch_id")
        result.append(record)
    return result


def _validate_alerts(value: Any) -> list[dict[str, Any]]:
    if isinstance(value, (str, bytes)) or not isinstance(value, Sequence):
        raise ValueError("alerts must be a list")
    result = []
    seen = set()
    for item in value:
        if not isinstance(item, Mapping):
            raise ValueError("alerts must contain mappings")
        expected = {"failure_class", "subject", "timestamp", "delivery_state"}
        if item.get("delivery_state") == "sent":
            expected.add("receipt_id")
        if set(item) != expected or item["delivery_state"] not in {"sent", "unsent"}:
            raise ValueError("alert fields are invalid")
        failure_class = _bounded_string(item["failure_class"], "alert.failure_class")
        subject = _bounded_string(item["subject"], "alert.subject")
        timestamp = _finite_number(item["timestamp"], "alert.timestamp")
        key = (failure_class, subject)
        if key in seen:
            raise ValueError("alerts contain duplicate class and subject")
        seen.add(key)
        record = {
            "failure_class": failure_class,
            "subject": subject,
            "timestamp": timestamp,
            "delivery_state": item["delivery_state"],
        }
        if "receipt_id" in item:
            record["receipt_id"] = _bounded_string(item["receipt_id"], "alert.receipt_id")
        result.append(record)
    return sorted(result, key=lambda item: (item["failure_class"], item["subject"]))


def validate_project_state(document: Mapping[str, Any]) -> dict[str, Any]:
    """Validate and normalize the only supported project-state schema."""
    if not isinstance(document, Mapping) or set(document) != PROJECT_FIELDS:
        raise ValueError("project state must contain exactly the canonical fields")
    if document["schema_version"] != SCHEMA_VERSION:
        raise ValueError(f"project state requires schema_version {SCHEMA_VERSION}")

    workers = document["workers"]
    if isinstance(workers, (str, bytes)) or not isinstance(workers, Sequence):
        raise ValueError("workers must be a list")
    normalized_workers = []
    for item in workers:
        if not isinstance(item, Mapping) or set(item) != {"id", "state", "last_evidence_id"}:
            raise ValueError("worker fields are invalid")
        worker_id = _bounded_string(item["id"], "worker.id")
        if item["state"] not in WORKER_STATES:
            raise ValueError("worker state is unsupported")
        evidence = _bounded_string(item["last_evidence_id"], "worker.last_evidence_id", allow_empty=True)
        if item["state"] == "active" and not evidence:
            raise ValueError("active worker requires matching evidence")
        normalized_workers.append({
            "id": worker_id,
            "state": item["state"],
            "last_evidence_id": evidence,
        })
    if [item["id"] for item in normalized_workers] != sorted(EXPECTED_HORDE_WORKERS):
        raise ValueError("workers must be the exact sorted five Horde workers")

    lanes = document["lanes"]
    if not isinstance(lanes, Mapping) or set(lanes) != set(LANE_IDS):
        raise ValueError("lanes must be the exact independent lane set")
    normalized_lanes = {}
    for lane in LANE_IDS:
        item = lanes[lane]
        if not isinstance(item, Mapping) or set(item) != {"state", "last_evidence_id"}:
            raise ValueError(f"{lane} fields are invalid")
        if item["state"] not in LANE_STATES[lane]:
            raise ValueError(f"{lane} state is unsupported")
        evidence = _bounded_string(item["last_evidence_id"], f"{lane}.last_evidence_id", allow_empty=True)
        if item["state"] in {"active", "green"} and not evidence:
            raise ValueError(f"{lane} {item['state']} requires matching evidence")
        normalized_lanes[lane] = {"state": item["state"], "last_evidence_id": evidence}

    milestones = document["milestones"]
    if not isinstance(milestones, Mapping) or set(milestones) != MILESTONE_FIELDS:
        raise ValueError("milestones fields are invalid")
    normalized_milestones = {
        field: _nonnegative_integer(milestones[field], f"milestones.{field}")
        for field in MILESTONE_FIELDS - {"release_gate"}
    }
    if not isinstance(milestones["release_gate"], bool):
        raise ValueError("milestones.release_gate must be boolean")
    normalized_milestones["release_gate"] = milestones["release_gate"]
    local_threshold = normalized_milestones["local_green_threshold"]
    windows_threshold = normalized_milestones["windows_green_threshold"]
    if not 32 <= local_threshold <= 64 or windows_threshold <= local_threshold:
        raise ValueError("milestone thresholds are invalid")
    integrated = normalized_milestones["integrated_nodedefs"]
    if (
        normalized_milestones["local_last_green_count"] > integrated
        or normalized_milestones["windows_last_green_count"] > integrated
    ):
        raise ValueError("milestone green baselines exceed integrated count")
    golden_state = normalized_lanes["golden_review"]["state"]
    if (
        (not normalized_milestones["release_gate"] and golden_state != "not_due")
        or (
            normalized_milestones["release_gate"]
            and golden_state not in {"due", "green", "failed"}
        )
    ):
        raise ValueError("golden_review state contradicts the explicit release gate")

    horde_state = normalized_lanes["horde"]["state"]
    horde_evidence = normalized_lanes["horde"]["last_evidence_id"]
    active_workers = [
        worker for worker in normalized_workers if worker["state"] == "active"
    ]
    unknown_workers = [
        worker for worker in normalized_workers if worker["state"] == "unknown"
    ]
    if horde_state == "active":
        if len(active_workers) != len(normalized_workers) or any(
            worker["last_evidence_id"] != horde_evidence
            for worker in normalized_workers
        ):
            raise ValueError("active Horde lane contradicts worker evidence")
    elif horde_state == "unknown":
        if len(unknown_workers) != len(normalized_workers) or horde_evidence:
            raise ValueError("unknown Horde lane contradicts worker state")
    elif horde_state == "degraded":
        if (
            not active_workers
            or not horde_evidence
            or (
                len(active_workers) == len(normalized_workers)
                and len({
                    worker["last_evidence_id"]
                    for worker in normalized_workers
                }) == 1
            )
        ):
            raise ValueError("degraded Horde lane contradicts worker state")
    elif horde_state == "blocked":
        if active_workers or len(unknown_workers) == len(normalized_workers) or not horde_evidence:
            raise ValueError("blocked Horde lane contradicts worker state")
    if horde_state != "unknown" and any(
        worker["state"] != "unknown" and not worker["last_evidence_id"]
        for worker in normalized_workers
    ):
        raise ValueError("observed Horde worker state requires bounded evidence")

    completed_rows = document["completed_rows"]
    if (
        isinstance(completed_rows, (str, bytes))
        or not isinstance(completed_rows, Sequence)
        or any(not isinstance(item, str) or not item or len(item) > 128 for item in completed_rows)
        or list(completed_rows) != sorted(set(completed_rows))
    ):
        raise ValueError("completed_rows must be sorted unique bounded strings")
    evidence_records = _record_list(document["evidence_records"], "evidence_records")
    journal_records = _record_list(document["journal_records"], "journal_records")

    assigned_batches = document["assigned_batches"]
    if isinstance(assigned_batches, (str, bytes)) or not isinstance(assigned_batches, Sequence):
        raise ValueError("assigned_batches must be a list")
    normalized_assignments = []
    for item in assigned_batches:
        if not isinstance(item, Mapping) or set(item) != {"worker_id", "batch_id"}:
            raise ValueError("assigned batch fields are invalid")
        worker_id = _bounded_string(item["worker_id"], "assigned_batch.worker_id")
        if worker_id not in EXPECTED_HORDE_WORKERS:
            raise ValueError("assigned batch worker is unknown")
        normalized_assignments.append({
            "worker_id": worker_id,
            "batch_id": _bounded_string(item["batch_id"], "assigned_batch.batch_id"),
        })
    normalized_assignments.sort(key=lambda item: (item["worker_id"], item["batch_id"]))
    if len({(item["worker_id"], item["batch_id"]) for item in normalized_assignments}) != len(normalized_assignments):
        raise ValueError("assigned_batches contain duplicates")

    integration_receipts = document["integration_receipts"]
    if isinstance(integration_receipts, (str, bytes)) or not isinstance(integration_receipts, Sequence):
        raise ValueError("integration_receipts must be a list")
    normalized_receipts = []
    seen_receipts = set()
    for item in integration_receipts:
        if not isinstance(item, Mapping):
            raise ValueError("integration receipts must be mappings")
        expected_fields = {
            "batch_id",
            "layer",
            "base_sha",
            "head_sha",
            "final_state",
        }
        if item.get("final_state") == "rejected":
            expected_fields.add("failure_classification")
        if set(item) != expected_fields:
            raise ValueError("integration receipt fields are invalid")
        batch_id = _bounded_string(item["batch_id"], "integration_receipt.batch_id")
        layer = item["layer"]
        final_state = item["final_state"]
        if layer not in _INTEGRATION_LAYER_ORDER or final_state not in {"integrated", "rejected"}:
            raise ValueError("integration receipt categories are invalid")
        for field in ("base_sha", "head_sha"):
            if (
                not isinstance(item[field], str)
                or not re.fullmatch(r"[0-9a-f]{40}", item[field])
            ):
                raise ValueError("integration receipt SHA is invalid")
        key = (batch_id, layer)
        if key in seen_receipts:
            raise ValueError("integration receipts contain duplicates")
        seen_receipts.add(key)
        normalized = {
            "batch_id": batch_id,
            "layer": layer,
            "base_sha": item["base_sha"],
            "head_sha": item["head_sha"],
            "final_state": final_state,
        }
        if final_state == "rejected":
            normalized["failure_classification"] = _bounded_string(
                item["failure_classification"],
                "integration_receipt.failure_classification",
            )
        normalized_receipts.append(normalized)
    normalized_receipts.sort(key=lambda item: (
        _INTEGRATION_LAYER_ORDER.get(
            str(item.get("layer", "")),
            len(_INTEGRATION_LAYER_ORDER),
        ),
        str(item.get("batch_id", "")),
    ))

    failure_classifications = document["failure_classifications"]
    if (
        isinstance(failure_classifications, (str, bytes))
        or not isinstance(failure_classifications, Sequence)
        or any(item not in ALLOWED_FAILURE_CLASSIFICATIONS for item in failure_classifications)
        or list(failure_classifications) != sorted(set(failure_classifications))
    ):
        raise ValueError("failure_classifications must be sorted unique categories")
    if not isinstance(document["healthy"], bool):
        raise ValueError("healthy must be boolean")
    expected_healthy = (
        normalized_lanes["horde"]["state"] == "active"
        and not failure_classifications
    )
    if document["healthy"] != expected_healthy:
        raise ValueError("healthy contradicts Horde lane or failure classifications")

    normalized = {
        "schema_version": SCHEMA_VERSION,
        "workers": normalized_workers,
        "lanes": normalized_lanes,
        "milestones": normalized_milestones,
        "completed_rows": list(completed_rows),
        "evidence_records": evidence_records,
        "journal_records": journal_records,
        "assigned_batches": normalized_assignments,
        "integration_receipts": normalized_receipts,
        "alerts": _validate_alerts(document["alerts"]),
        "failure_classifications": list(failure_classifications),
        "queue_depth": _nonnegative_integer(document["queue_depth"], "queue_depth"),
        "cycle_sequence": _nonnegative_integer(document["cycle_sequence"], "cycle_sequence"),
        "checked_at": _finite_number(document["checked_at"], "checked_at"),
        "last_successful_poll": _finite_number(
            document["last_successful_poll"],
            "last_successful_poll",
            optional=True,
        ),
        "healthy": document["healthy"],
        "semantic_journal": _validate_semantic_journal(document["semantic_journal"]),
    }
    return normalized


def serialize_project_state(document: Mapping[str, Any]) -> str:
    return json.dumps(
        validate_project_state(document),
        allow_nan=False,
        indent=2,
        sort_keys=True,
    ) + "\n"


def assert_journal_extension(previous: Mapping[str, Any], current: Mapping[str, Any]) -> None:
    old = validate_project_state(previous)["semantic_journal"]
    new = validate_project_state(current)["semantic_journal"]
    if len(new) < len(old) or new[:len(old)] != old:
        raise ValueError("semantic journal history is not an immutable prefix")


def update_horde_observation(
    document: Mapping[str, Any],
    *,
    worker_states: Mapping[str, str],
    evidence_receipt: str,
    batch_id: str | None = None,
    reason: str = "probe_observation",
) -> dict[str, Any]:
    state = validate_project_state(document)
    evidence = _bounded_string(evidence_receipt, "evidence_receipt")
    if set(worker_states) != set(EXPECTED_HORDE_WORKERS):
        raise ValueError("worker_states must contain exactly the five Horde workers")
    if reason not in {"probe_observation", "probe_failure", "dispatch_success", "dispatch_failure"}:
        raise ValueError("unsupported Horde observation reason")
    if batch_id is not None:
        _bounded_string(batch_id, "batch_id")
    result = copy.deepcopy(state)
    for worker in result["workers"]:
        previous = worker["state"]
        new = worker_states[worker["id"]]
        if new not in WORKER_STATES:
            raise ValueError("unsupported worker observation state")
        worker["state"] = new
        worker["last_evidence_id"] = evidence
        if new != previous:
            _append_event(
                result,
                event_kind="horde_worker",
                subject=f"worker:{worker['id']}",
                previous_state=previous,
                new_state=new,
                reason=reason,
                evidence_receipt=evidence,
                batch_id=batch_id,
            )
    previous_lane = result["lanes"]["horde"]["state"]
    states = set(worker_states.values())
    if states == {"active"}:
        lane_state = "active"
    elif "active" in states:
        lane_state = "degraded"
    else:
        lane_state = "blocked"
    result["lanes"]["horde"] = {"state": lane_state, "last_evidence_id": evidence}
    if lane_state != previous_lane:
        _append_event(
            result,
            event_kind="horde_lane",
            subject="lane:horde",
            previous_state=previous_lane,
            new_state=lane_state,
            reason=reason,
            evidence_receipt=evidence,
            batch_id=batch_id,
        )
    result["healthy"] = lane_state == "active" and not result["failure_classifications"]
    validated = validate_project_state(result)
    assert_journal_extension(state, validated)
    return validated


def update_horde_dispatch(
    document: Mapping[str, Any],
    *,
    worker_states: Mapping[str, str],
    evidence_receipt: str,
    dispatch_id: str,
    success: bool,
) -> dict[str, Any]:
    """Apply one dispatch's observed worker subset without touching other lanes."""
    state = validate_project_state(document)
    evidence = _bounded_string(evidence_receipt, "evidence_receipt")
    dispatch = _bounded_string(dispatch_id, "dispatch_id")
    if (
        not worker_states
        or not set(worker_states).issubset(EXPECTED_HORDE_WORKERS)
        or not isinstance(success, bool)
    ):
        raise ValueError("dispatch workers or success state are invalid")
    result = copy.deepcopy(state)
    reason = "dispatch_success" if success else "dispatch_failure"
    _append_event(
        result,
        event_kind="dispatch",
        subject="lane:horde",
        previous_state="none",
        new_state="credited",
        reason=reason,
        evidence_receipt=evidence,
        batch_id=dispatch,
    )
    for worker in result["workers"]:
        if worker["id"] not in worker_states:
            continue
        previous = worker["state"]
        new = worker_states[worker["id"]]
        if new not in WORKER_STATES:
            raise ValueError("unsupported dispatch worker state")
        worker["state"] = new
        worker["last_evidence_id"] = evidence
        if new != previous:
            _append_event(
                result,
                event_kind="horde_worker",
                subject=f"worker:{worker['id']}",
                previous_state=previous,
                new_state=new,
                reason=reason,
                evidence_receipt=evidence,
                batch_id=dispatch,
            )
    all_states = {worker["state"] for worker in result["workers"]}
    worker_evidence = {
        worker["last_evidence_id"] for worker in result["workers"]
    }
    if all_states == {"active"} and worker_evidence == {evidence}:
        lane_state = "active"
    elif "active" in all_states:
        lane_state = "degraded"
    else:
        lane_state = "blocked"
    previous_lane = result["lanes"]["horde"]["state"]
    result["lanes"]["horde"] = {
        "state": lane_state,
        "last_evidence_id": evidence,
    }
    if lane_state != previous_lane:
        _append_event(
            result,
            event_kind="horde_lane",
            subject="lane:horde",
            previous_state=previous_lane,
            new_state=lane_state,
            reason=reason,
            evidence_receipt=evidence,
            batch_id=dispatch,
        )
    result["healthy"] = lane_state == "active" and not result["failure_classifications"]
    validated = validate_project_state(result)
    assert_journal_extension(state, validated)
    return validated


def _mark_due(
    state: dict[str, Any],
    lane: str,
    *,
    reason: str,
    evidence_receipt: str,
    batch_id: str | None,
) -> None:
    previous = state["lanes"][lane]["state"]
    if previous == "due":
        return
    state["lanes"][lane] = {"state": "due", "last_evidence_id": ""}
    _append_event(
        state,
        event_kind="lane_due",
        subject=f"lane:{lane}",
        previous_state=previous,
        new_state="due",
        reason=reason,
        evidence_receipt=evidence_receipt,
        batch_id=batch_id,
    )


def credit_integration(
    document: Mapping[str, Any],
    *,
    newly_integrated_nodedefs: int,
    render_path_edit: bool,
    batch_id: str,
) -> dict[str, Any]:
    state = validate_project_state(document)
    count = _nonnegative_integer(newly_integrated_nodedefs, "newly_integrated_nodedefs")
    if not isinstance(render_path_edit, bool):
        raise ValueError("render_path_edit must be boolean")
    batch = _bounded_string(batch_id, "batch_id")
    if count == 0 and not render_path_edit:
        raise ValueError("integration credit requires NodeDefs or a render-path edit")
    result = copy.deepcopy(state)
    milestone = result["milestones"]
    milestone["generation"] += 1
    milestone["integrated_nodedefs"] += count
    if render_path_edit:
        milestone["render_path_revision"] += 1
    evidence = f"integration-generation-{milestone['generation']}"
    _append_event(
        result,
        event_kind="integration_credit",
        subject="milestone:integration",
        previous_state="none",
        new_state="credited",
        reason="render_path_edit" if render_path_edit else "integration_milestone",
        evidence_receipt=evidence,
        batch_id=batch,
    )
    if (
        render_path_edit
        or milestone["integrated_nodedefs"] - milestone["local_last_green_count"]
        >= milestone["local_green_threshold"]
    ):
        for lane in ("local_cpu", "local_cuda"):
            _mark_due(
                result,
                lane,
                reason="render_path_edit" if render_path_edit else "integration_milestone",
                evidence_receipt=evidence,
                batch_id=batch,
            )
    if (
        milestone["integrated_nodedefs"] - milestone["windows_last_green_count"]
        >= milestone["windows_green_threshold"]
    ):
        _mark_due(
            result,
            "windows_a40_cuda",
            reason="integration_milestone",
            evidence_receipt=evidence,
            batch_id=batch,
        )
    validated = validate_project_state(result)
    assert_journal_extension(state, validated)
    return validated


def set_release_gate(document: Mapping[str, Any], *, due: bool) -> dict[str, Any]:
    state = validate_project_state(document)
    if not isinstance(due, bool):
        raise ValueError("due must be boolean")
    result = copy.deepcopy(state)
    previous = "enabled" if result["milestones"]["release_gate"] else "disabled"
    result["milestones"]["release_gate"] = due
    lane_previous = result["lanes"]["golden_review"]["state"]
    lane_new = "due" if due else "not_due"
    result["lanes"]["golden_review"] = {"state": lane_new, "last_evidence_id": ""}
    if lane_new != lane_previous:
        _append_event(
            result,
            event_kind="release_gate",
            subject="lane:golden_review",
            previous_state=previous,
            new_state="enabled" if due else "disabled",
            reason="explicit_release_gate",
            evidence_receipt=f"release-gate-generation-{result['milestones']['generation']}",
        )
    return validate_project_state(result)


def apply_lane_evidence(
    document: Mapping[str, Any],
    receipt: Mapping[str, Any],
) -> dict[str, Any]:
    state = validate_project_state(document)
    fields = {
        "schema_version",
        "receipt_id",
        "lane",
        "evidence_type",
        "milestone_generation",
        "numeric_exits",
    }
    if not isinstance(receipt, Mapping) or set(receipt) != fields:
        raise ValueError("lane evidence receipt fields are invalid")
    receipt_schema_version = receipt["schema_version"]
    if (
        isinstance(receipt_schema_version, bool)
        or not isinstance(receipt_schema_version, int)
        or receipt_schema_version != 1
    ):
        raise ValueError("lane evidence receipt fields are invalid")
    lane = receipt["lane"]
    if lane not in LANE_EVIDENCE_TYPES or receipt["evidence_type"] != LANE_EVIDENCE_TYPES[lane]:
        raise ValueError("lane evidence type does not match lane")
    receipt_generation = receipt["milestone_generation"]
    if (
        isinstance(receipt_generation, bool)
        or not isinstance(receipt_generation, int)
        or receipt_generation != state["milestones"]["generation"]
    ):
        raise ValueError("lane evidence is not for the current milestone generation")
    exits = receipt["numeric_exits"]
    if (
        isinstance(exits, (str, bytes))
        or not isinstance(exits, Sequence)
        or not exits
        or any(isinstance(code, bool) or not isinstance(code, int) or code != 0 for code in exits)
    ):
        raise ValueError("lane evidence requires numeric zero exits")
    if state["lanes"][lane]["state"] != "due":
        raise ValueError("lane evidence can only satisfy a due lane")
    evidence = _bounded_string(receipt["receipt_id"], "receipt_id")
    result = copy.deepcopy(state)
    previous = result["lanes"][lane]["state"]
    result["lanes"][lane] = {"state": "green", "last_evidence_id": evidence}
    if lane in {"local_cpu", "local_cuda"} and all(
        result["lanes"][name]["state"] == "green"
        for name in ("local_cpu", "local_cuda")
    ):
        result["milestones"]["local_last_green_count"] = result["milestones"]["integrated_nodedefs"]
    elif lane == "windows_a40_cuda":
        result["milestones"]["windows_last_green_count"] = result["milestones"]["integrated_nodedefs"]
    _append_event(
        result,
        event_kind="lane_evidence",
        subject=f"lane:{lane}",
        previous_state=previous,
        new_state="green",
        reason="matching_evidence",
        evidence_receipt=evidence,
    )
    validated = validate_project_state(result)
    assert_journal_extension(state, validated)
    return validated


def migrate_project_state(document: Mapping[str, Any]) -> dict[str, Any]:
    """Explicitly migrate the supported schema-v1 capacity document."""
    if not isinstance(document, Mapping) or document.get("schema_version") != 1:
        raise ValueError("only schema-version-1 project state can be migrated")
    required = {
        "schema_version",
        "healthy_workers",
        "completed_rows",
        "evidence_records",
        "journal_records",
        "lanes",
    }
    optional = {"capacity_journal", "alerts"}
    if not required.issubset(document) or set(document) - required - optional:
        raise ValueError("legacy project state fields are invalid")
    result = new_project_state()
    legacy_workers = document["healthy_workers"]
    if isinstance(legacy_workers, (str, bytes)) or not isinstance(legacy_workers, Sequence):
        raise ValueError("legacy healthy_workers must be a list")
    seen = set()
    legacy_states = {}
    for item in legacy_workers:
        if not isinstance(item, Mapping) or set(item) != {"id", "state"}:
            raise ValueError("legacy worker fields are invalid")
        worker = item["id"]
        if worker not in EXPECTED_HORDE_WORKERS or worker in seen or item["state"] not in WORKER_STATES:
            raise ValueError("legacy worker identity or state is invalid")
        seen.add(worker)
        legacy_states[worker] = item["state"]

    lanes = document["lanes"]
    if not isinstance(lanes, Mapping) or set(lanes) != {"windows_local_build"}:
        raise ValueError("legacy lanes must contain only windows_local_build")
    windows = lanes["windows_local_build"]
    if not isinstance(windows, Mapping) or set(windows) != {"state", "alerted"}:
        raise ValueError("legacy windows lane fields are invalid")
    if not isinstance(windows["alerted"], bool):
        raise ValueError("legacy windows alerted must be boolean")
    mapping = {
        "ready": "due",
        "unknown": "unknown",
        "blocked": "failed",
    }
    if windows["state"] not in mapping:
        raise ValueError("legacy windows state is unsupported")
    result["lanes"]["windows_a40_cuda"] = {
        "state": "unknown",
        "last_evidence_id": "",
    }

    completed = document["completed_rows"]
    if (
        isinstance(completed, (str, bytes))
        or not isinstance(completed, Sequence)
        or any(not isinstance(item, str) or not item for item in completed)
        or len(completed) != len(set(completed))
    ):
        raise ValueError("legacy completed_rows are invalid")
    result["completed_rows"] = sorted(completed)
    result["evidence_records"] = _record_list(document["evidence_records"], "evidence_records")
    result["journal_records"] = _record_list(document["journal_records"], "journal_records")
    legacy_journal = document.get("capacity_journal", [])
    if (
        isinstance(legacy_journal, (str, bytes))
        or not isinstance(legacy_journal, Sequence)
    ):
        raise ValueError("legacy capacity_journal must be a list")
    worker_records_seen: set[str] = set()
    windows_records_seen = False
    latest_horde_evidence = ""
    for record_index, record in enumerate(legacy_journal):
        fields = {"kind", "subject", "state", "log_observed"}
        if not isinstance(record, Mapping) or set(record) != fields:
            raise ValueError("legacy capacity journal fields are invalid")
        if (
            record["kind"] not in {"worker_process", "windows_local_build"}
            or not isinstance(record["log_observed"], bool)
        ):
            raise ValueError("legacy capacity journal category is invalid")
        evidence = "legacy-" + hashlib.sha256(
            json.dumps(
                {"index": record_index, "record": dict(record)},
                separators=(",", ":"),
                sort_keys=True,
            ).encode("utf-8")
        ).hexdigest()[:20]
        if record["kind"] == "worker_process":
            worker_id = record["subject"]
            if worker_id not in EXPECTED_HORDE_WORKERS or record["state"] not in WORKER_STATES:
                raise ValueError("legacy worker journal record is invalid")
            worker = next(
                item for item in result["workers"] if item["id"] == worker_id
            )
            previous_state = worker["state"]
            preserved_state = record["state"]
            if preserved_state in {"active", "ready"} and not record["log_observed"]:
                current_state = "unknown"
            else:
                current_state = (
                    "active" if preserved_state == "ready" else preserved_state
                )
            worker["state"] = current_state
            worker["last_evidence_id"] = (
                evidence
                if record["log_observed"] or current_state != "unknown"
                else ""
            )
            worker_records_seen.add(worker_id)
            latest_horde_evidence = evidence
            _append_event(
                result,
                event_kind="horde_worker",
                subject=f"worker:{worker_id}",
                previous_state=previous_state,
                new_state=current_state,
                reason=(
                    "probe_observation"
                    if current_state == "active" and record["log_observed"]
                    else "probe_failure"
                ),
                evidence_receipt=evidence,
            )
        else:
            if (
                record["subject"] != "windows_local_build"
                or record["state"] not in mapping
            ):
                raise ValueError("legacy Windows journal record is invalid")
            lane_state = mapping[record["state"]]
            previous_state = result["lanes"]["windows_a40_cuda"]["state"]
            result["lanes"]["windows_a40_cuda"] = {
                "state": lane_state,
                "last_evidence_id": (
                    evidence if record["log_observed"] else ""
                ),
            }
            windows_records_seen = True
            _append_event(
                result,
                event_kind=(
                    "lane_due" if lane_state == "due" else "lane_evidence"
                ),
                subject="lane:windows_a40_cuda",
                previous_state=previous_state,
                new_state=lane_state,
                reason=(
                    "integration_milestone"
                    if lane_state == "due"
                    else "probe_failure"
                ),
                evidence_receipt=evidence,
            )

    for worker in result["workers"]:
        worker_id = worker["id"]
        legacy_state = legacy_states.get(worker_id, "unknown")
        if worker_id in worker_records_seen:
            expected_state = "active" if legacy_state == "ready" else legacy_state
            if (
                legacy_state in {"active", "ready"}
                and worker["state"] == "unknown"
            ):
                expected_state = "unknown"
            if worker["state"] != expected_state:
                raise ValueError(
                    "legacy worker summary contradicts final journal state"
                )
            continue
        if legacy_state in {"active", "ready"}:
            worker["state"] = "unknown"
            worker["last_evidence_id"] = ""
        elif legacy_state != "unknown":
            evidence = "legacy-current-" + hashlib.sha256(
                f"{worker_id}:{legacy_state}".encode("utf-8")
            ).hexdigest()[:20]
            worker["state"] = legacy_state
            worker["last_evidence_id"] = evidence
            latest_horde_evidence = evidence

    legacy_windows_state = mapping[windows["state"]]
    if windows_records_seen:
        if result["lanes"]["windows_a40_cuda"]["state"] != legacy_windows_state:
            raise ValueError(
                "legacy Windows summary contradicts final journal state"
            )
    else:
        result["lanes"]["windows_a40_cuda"] = {
            "state": legacy_windows_state,
            "last_evidence_id": "",
        }

    active_workers = [
        worker for worker in result["workers"] if worker["state"] == "active"
    ]
    if len(active_workers) == len(result["workers"]) and len({
        worker["last_evidence_id"] for worker in result["workers"]
    }) == 1:
        horde_state = "active"
        horde_evidence = active_workers[0]["last_evidence_id"]
    elif active_workers:
        horde_state = "degraded"
        horde_evidence = latest_horde_evidence or "legacy-horde-degraded"
    elif all(worker["state"] == "unknown" for worker in result["workers"]):
        horde_state = "unknown"
        horde_evidence = ""
    else:
        horde_state = "blocked"
        horde_evidence = latest_horde_evidence or "legacy-horde-blocked"
    result["lanes"]["horde"] = {
        "state": horde_state,
        "last_evidence_id": horde_evidence,
    }
    result["healthy"] = horde_state == "active"
    return validate_project_state(result)


def load_project_state(path: str | Path, *, migrate_v1: bool = False) -> dict[str, Any]:
    document = json.loads(Path(path).read_text(encoding="utf-8"))
    if migrate_v1 and isinstance(document, Mapping) and document.get("schema_version") == 1:
        return migrate_project_state(document)
    return validate_project_state(document)


@contextmanager
def _exclusive_state_path_lock(target: Path):
    """Hold one stable adjacent lock for the complete state transaction."""
    lock_path = target.with_name(f".{target.name}.lock")
    lock_file = lock_path.open("a+b")
    acquired = False
    try:
        if os.name == "nt":
            if os.fstat(lock_file.fileno()).st_size == 0:
                lock_file.write(b"\0")
                lock_file.flush()
                os.fsync(lock_file.fileno())
            lock_file.seek(0)
            msvcrt.locking(lock_file.fileno(), msvcrt.LK_LOCK, 1)
        else:
            fcntl.flock(lock_file.fileno(), fcntl.LOCK_EX)
        acquired = True
        yield
    finally:
        try:
            if acquired:
                if os.name == "nt":
                    lock_file.seek(0)
                    msvcrt.locking(lock_file.fileno(), msvcrt.LK_UNLCK, 1)
                else:
                    fcntl.flock(lock_file.fileno(), fcntl.LOCK_UN)
        finally:
            lock_file.close()


def write_project_state(path: str | Path, document: Mapping[str, Any]) -> None:
    target = Path(path).resolve()
    target.parent.mkdir(parents=True, exist_ok=True)
    with _exclusive_state_path_lock(target):
        candidate = validate_project_state(document)
        if target.exists():
            existing_document = json.loads(target.read_text(encoding="utf-8"))
            if (
                isinstance(existing_document, Mapping)
                and existing_document.get("schema_version") == 1
            ):
                existing = migrate_project_state(existing_document)
            else:
                existing = validate_project_state(existing_document)
            assert_journal_extension(existing, candidate)
        payload = serialize_project_state(candidate)
        temporary_path: Path | None = None
        try:
            with tempfile.NamedTemporaryFile(
                mode="w",
                encoding="utf-8",
                newline="",
                dir=target.parent,
                prefix=f".{target.name}.",
                suffix=".tmp",
                delete=False,
            ) as temporary:
                temporary_path = Path(temporary.name)
                temporary.write(payload)
                temporary.flush()
                os.fsync(temporary.fileno())
            os.replace(temporary_path, target)
            temporary_path = None
        finally:
            if temporary_path is not None:
                temporary_path.unlink(missing_ok=True)
