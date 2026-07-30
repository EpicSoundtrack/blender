#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Recurring, fail-closed supervision for bounded MaterialX Horde cycles."""

from __future__ import annotations

__all__ = (
    "AtomicJSONStateStore",
    "Clock",
    "Sleeper",
    "StateStore",
    "SupervisorConfig",
    "SupervisorController",
    "run_supervisor",
)

from dataclasses import dataclass
import json
import math
import os
from pathlib import Path
import re
import tempfile
from typing import Any, Mapping, Protocol, Sequence

from materialx_alert_sink import (
    ALLOWED_FAILURE_CLASSES,
    SanitizedAlertSink,
)
from materialx_velocity_manifest import EXPECTED_HORDE_WORKERS


_IDENTIFIER = re.compile(r"^[A-Za-z0-9_.-]{1,128}$")
_CATEGORY = re.compile(r"^[a-z][a-z0-9_]{0,63}$")
_RECEIPT_ID = re.compile(r"^[A-Za-z0-9_.:-]{1,128}$")
_WORKER_STATES = frozenset(("active", "idle", "blocked"))
_RECEIPT_STATES = frozenset(("integrated", "rejected"))
_LAYER_ORDER = {
    "native_cycles": 0,
    "hydra_ovrtx": 1,
    "blender_authoring": 2,
}
_CONTROLLER_ALERT_MAP = {
    "queue_empty": ("queue_empty", "queue"),
    "source_preflight_failure": ("stale_source", "worker"),
    "source_sync_failure": ("stale_source", "worker"),
    "stale_source": ("stale_source", "worker"),
    "auth_failure": ("auth_failure", "worker"),
    "proxy_failure": ("proxy_failure", "worker"),
    "harvest_failure": ("invalid_completion", "worker"),
    "harvest_missing": ("invalid_completion", "worker"),
    "invalid_completion": ("invalid_completion", "worker"),
    "invalid_exit": ("invalid_completion", "worker"),
    "invalid_json": ("invalid_completion", "worker"),
    "missing": ("invalid_completion", "worker"),
    "nonzero_exit": ("invalid_completion", "worker"),
    "oversized_line": ("invalid_completion", "worker"),
    "oversized_log_window": ("invalid_completion", "worker"),
    "oversized_payload": ("invalid_completion", "worker"),
    "secret_like_key": ("invalid_completion", "worker"),
    "unsupported_schema": ("invalid_completion", "worker"),
    "integration_failure": ("integration_failure", "worker"),
    "role_worker_unavailable": ("capacity_loss", "worker"),
    "dispatch_failure": ("capacity_loss", "worker"),
    "process_missing": ("capacity_loss", "worker"),
}


@dataclass(frozen=True)
class SupervisorConfig:
    """Strict recurring-supervisor timing and capacity thresholds."""

    poll_interval_seconds: float
    stale_intervals: int = 2
    queue_watermark: int = 5

    def __post_init__(self) -> None:
        interval = self.poll_interval_seconds
        if (
            isinstance(interval, bool)
            or not isinstance(interval, (int, float))
            or not math.isfinite(interval)
            or interval <= 0
        ):
            raise ValueError("poll_interval_seconds must be positive and finite")
        if (
            isinstance(self.stale_intervals, bool)
            or not isinstance(self.stale_intervals, int)
            or self.stale_intervals < 1
        ):
            raise ValueError("stale_intervals must be an integer of at least one")
        if (
            isinstance(self.queue_watermark, bool)
            or not isinstance(self.queue_watermark, int)
            or self.queue_watermark < 5
        ):
            raise ValueError("queue_watermark must be an integer of at least five")


class SupervisorController(Protocol):
    """One existing bounded operational/controller cycle."""

    def run_cycle(self) -> Mapping[str, Any]:
        """Poll, harvest, integrate, and refill once."""


class StateStore(Protocol):
    """Canonical supervisor state persistence."""

    def load(self) -> Mapping[str, Any] | None:
        """Load the last committed state, if any."""

    def commit(self, state: Mapping[str, Any]) -> None:
        """Atomically commit one sanitized cycle state."""


class Clock(Protocol):
    def now(self) -> float:
        """Return the current finite monotonic or epoch time."""


class Sleeper(Protocol):
    def sleep(self, seconds: float) -> None:
        """Sleep for exactly one configured interval."""


class _UnavailableAlertTransport:
    def send(self, message: Mapping[str, Any]) -> str:
        raise RuntimeError("alert transport is not configured")


class AtomicJSONStateStore:
    """Write one canonical JSON state through an atomic same-directory replace."""

    def __init__(self, path: str | Path):
        self.path = Path(path)

    def load(self) -> Mapping[str, Any] | None:
        if not self.path.exists():
            return None
        document = json.loads(self.path.read_text(encoding="utf-8"))
        if not isinstance(document, Mapping):
            raise ValueError("supervisor state must be a mapping")
        return document

    def commit(self, state: Mapping[str, Any]) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        temporary_path: Path | None = None
        try:
            with tempfile.NamedTemporaryFile(
                mode="w",
                encoding="utf-8",
                newline="",
                dir=self.path.parent,
                prefix=f".{self.path.name}.",
                suffix=".tmp",
                delete=False,
            ) as temporary:
                temporary_path = Path(temporary.name)
                json.dump(
                    state,
                    temporary,
                    allow_nan=False,
                    indent=2,
                    sort_keys=True,
                )
                temporary.write("\n")
                temporary.flush()
                os.fsync(temporary.fileno())
            os.replace(temporary_path, self.path)
            temporary_path = None
        finally:
            if temporary_path is not None:
                try:
                    temporary_path.unlink(missing_ok=True)
                except OSError:
                    pass


def _identifier(value: Any) -> str:
    return value if isinstance(value, str) and _IDENTIFIER.fullmatch(value) else ""


def _category(value: Any) -> str:
    return value if isinstance(value, str) and _CATEGORY.fullmatch(value) else ""


def _sha(value: Any) -> str:
    return (
        value
        if isinstance(value, str)
        and len(value) == 40
        and all(character in "0123456789abcdef" for character in value)
        else ""
    )


def _sequence(value: Any) -> int:
    return value if isinstance(value, int) and not isinstance(value, bool) and value >= 0 else 0


def _prior_state(state_store: StateStore) -> tuple[Mapping[str, Any], list[str]]:
    try:
        prior = state_store.load()
    except Exception:
        return {}, ["state_store_failure"]
    return (prior, []) if isinstance(prior, Mapping) else ({}, [])


def _sanitize_workers(raw: Any) -> tuple[list[dict[str, str]], bool]:
    if isinstance(raw, (str, bytes)) or not isinstance(raw, Sequence):
        return [], False
    workers = []
    seen = set()
    valid = True
    for value in raw:
        if not isinstance(value, Mapping):
            valid = False
            continue
        worker_id = _identifier(value.get("id"))
        state = value.get("state")
        batch_id = _identifier(value.get("batch_id")) if "batch_id" in value else ""
        if (
            not worker_id
            or worker_id not in EXPECTED_HORDE_WORKERS
            or worker_id in seen
            or state not in _WORKER_STATES
        ):
            valid = False
            continue
        record = {"id": worker_id, "state": state}
        if batch_id:
            record["batch_id"] = batch_id
        workers.append(record)
        seen.add(worker_id)
    return sorted(workers, key=lambda worker: worker["id"]), valid


def _sanitize_assignments(raw: Any) -> tuple[list[dict[str, str]], bool]:
    if isinstance(raw, (str, bytes)) or not isinstance(raw, Sequence):
        return [], False
    assignments = []
    valid = True
    for value in raw:
        if not isinstance(value, Mapping):
            valid = False
            continue
        worker_id = _identifier(value.get("worker_id"))
        batch_id = _identifier(value.get("batch_id"))
        if (
            not worker_id
            or worker_id not in EXPECTED_HORDE_WORKERS
            or not batch_id
        ):
            valid = False
            continue
        assignments.append({"worker_id": worker_id, "batch_id": batch_id})
    return sorted(
        assignments,
        key=lambda assignment: (assignment["worker_id"], assignment["batch_id"]),
    ), valid


def _sanitize_receipts(
    raw: Any,
) -> tuple[list[dict[str, Any]], list[dict[str, str]]]:
    if isinstance(raw, (str, bytes)) or not isinstance(raw, Sequence):
        return [], [{
            "failure_class": "invalid_completion",
            "subject": "runtime",
        }]
    receipts = []
    alerts = []
    for value in raw:
        if not isinstance(value, Mapping):
            alerts.append({
                "failure_class": "invalid_completion",
                "subject": "runtime",
            })
            continue
        batch_id = _identifier(value.get("batch_id"))
        layer = _identifier(value.get("layer"))
        final_state = value.get("final_state")
        base_sha = _sha(value.get("base_sha"))
        head_sha = _sha(value.get("head_sha"))
        failure = _category(value.get("failure_classification"))
        if (
            not batch_id
            or not layer
            or layer not in _LAYER_ORDER
            or final_state not in _RECEIPT_STATES
            or not base_sha
            or not head_sha
            or (final_state == "rejected" and not failure)
            or (final_state == "integrated" and "failure_classification" in value)
        ):
            alerts.append({
                "failure_class": "invalid_completion",
                "subject": "runtime",
            })
            continue
        receipt = {
            "batch_id": batch_id,
            "layer": layer,
            "base_sha": base_sha,
            "head_sha": head_sha,
            "final_state": final_state,
        }
        if failure:
            receipt["failure_classification"] = failure
            alerts.append({
                "failure_class": "integration_failure",
                "subject": f"lane:{layer}",
            })
        receipts.append(receipt)
    return sorted(
        receipts,
        key=lambda receipt: (
            _LAYER_ORDER.get(receipt["layer"], len(_LAYER_ORDER)),
            receipt["batch_id"],
        ),
    ), alerts


def _public_controller_alert(
    classification: str,
    worker_id: str,
) -> dict[str, str] | None:
    mapped = _CONTROLLER_ALERT_MAP.get(classification)
    if mapped is None:
        return None
    failure_class, subject_kind = mapped
    return {
        "failure_class": failure_class,
        "subject": (
            "queue" if subject_kind == "queue" else f"worker:{worker_id}"
        ),
    }


def _sanitize_alerts(raw: Any) -> tuple[list[dict[str, str]], bool]:
    if isinstance(raw, (str, bytes)) or not isinstance(raw, Sequence):
        return [], False
    alerts = []
    valid = True
    for value in raw:
        if not isinstance(value, Mapping):
            valid = False
            continue
        classification = _category(value.get("classification"))
        worker_id = _identifier(value.get("worker_id"))
        if (
            not classification
            or not worker_id
            or worker_id not in EXPECTED_HORDE_WORKERS
        ):
            valid = False
            continue
        alert = _public_controller_alert(classification, worker_id)
        if alert is None:
            valid = False
            continue
        alerts.append(alert)
    return sorted(
        alerts,
        key=lambda alert: (
            alert["failure_class"],
            alert["subject"],
        ),
    ), valid


def _build_cycle_state(
    raw: Mapping[str, Any] | None,
    *,
    config: SupervisorConfig,
    checked_at: float,
    clock_valid: bool,
    prior: Mapping[str, Any],
    initial_failures: Sequence[str],
    controller_failed: bool,
) -> tuple[dict[str, Any], list[dict[str, str]]]:
    sequence = _sequence(prior.get("cycle_sequence")) + 1
    previous_poll = prior.get("last_successful_poll")
    if (
        isinstance(previous_poll, bool)
        or not isinstance(previous_poll, (int, float))
        or not math.isfinite(previous_poll)
    ):
        previous_poll = None
    alerts: list[dict[str, str]] = []
    if initial_failures:
        alerts.append({
            "failure_class": "capacity_loss",
            "subject": "runtime",
        })
    if not clock_valid:
        alerts.append({
            "failure_class": "capacity_loss",
            "subject": "runtime",
        })

    if controller_failed or raw is None:
        alerts.append({
            "failure_class": "capacity_loss",
            "subject": "runtime",
        })
        workers: list[dict[str, str]] = []
        assignments: list[dict[str, str]] = []
        receipts: list[dict[str, Any]] = []
        queue_depth = 0
        last_poll = previous_poll
    else:
        workers, workers_valid = _sanitize_workers(raw.get("workers"))
        assignments, assignments_valid = _sanitize_assignments(
            raw.get("assigned_batches")
        )
        receipts, receipt_alerts = _sanitize_receipts(
            raw.get("integration_receipts")
        )
        controller_alerts, controller_alerts_valid = _sanitize_alerts(
            raw.get("alerts")
        )
        alerts.extend(receipt_alerts)
        alerts.extend(controller_alerts)
        if (
            not workers_valid
            or not assignments_valid
            or not controller_alerts_valid
        ):
            alerts.append({
                "failure_class": "capacity_loss",
                "subject": "runtime",
            })
        if len(workers) != 5:
            alerts.append({
                "failure_class": "capacity_loss",
                "subject": "runtime",
            })
        elif clock_valid:
            previous_poll = checked_at
        last_poll = previous_poll
        for worker in workers:
            if worker["state"] == "blocked":
                alerts.append({
                    "failure_class": "capacity_loss",
                    "subject": f"worker:{worker['id']}",
                })
        queue_depth = raw.get("queue_depth")
        if (
            isinstance(queue_depth, bool)
            or not isinstance(queue_depth, int)
            or queue_depth < 0
        ):
            alerts.append({
                "failure_class": "capacity_loss",
                "subject": "queue",
            })
            queue_depth = 0
        if queue_depth == 0:
            alerts.append({
                "failure_class": "queue_empty",
                "subject": "queue",
            })
        elif queue_depth < config.queue_watermark:
            alerts.append({
                "failure_class": "capacity_loss",
                "subject": "queue",
            })

    if (
        clock_valid
        and last_poll is not None
        and checked_at - last_poll
        >= config.stale_intervals * config.poll_interval_seconds
    ):
        alerts.append({
            "failure_class": "capacity_loss",
            "subject": "runtime",
        })

    current_alerts = [
        {"failure_class": failure_class, "subject": subject}
        for failure_class, subject in sorted({
            (alert["failure_class"], alert["subject"])
            for alert in alerts
        })
    ]
    failure_classifications = sorted({
        alert["failure_class"] for alert in current_alerts
    })
    return {
        "schema_version": 2,
        "cycle_sequence": sequence,
        "healthy": not failure_classifications,
        "checked_at": checked_at,
        "last_successful_poll": last_poll,
        "queue_depth": queue_depth,
        "workers": workers,
        "assigned_batches": assignments,
        "integration_receipts": receipts,
        "alerts": [],
        "failure_classifications": failure_classifications,
    }, current_alerts


def _validated_delivery_records(
    value: Any,
    current_alerts: Sequence[Mapping[str, str]],
    *,
    timestamp: float,
) -> list[dict[str, Any]]:
    expected = {
        (alert["failure_class"], alert["subject"]) for alert in current_alerts
    }
    if isinstance(value, (str, bytes)) or not isinstance(value, Sequence):
        raise ValueError("alert sink result must be a sequence")
    records = []
    found = set()
    for item in value:
        if not isinstance(item, Mapping):
            raise ValueError("alert delivery records must be mappings")
        delivery_state = item.get("delivery_state")
        expected_fields = {
            "failure_class",
            "subject",
            "timestamp",
            "delivery_state",
        }
        if delivery_state == "sent":
            expected_fields.add("receipt_id")
        if set(item) != expected_fields or delivery_state not in {"sent", "unsent"}:
            raise ValueError("alert delivery record has unsupported fields")
        failure_class = item["failure_class"]
        subject = item["subject"]
        key = (failure_class, subject)
        if (
            failure_class not in ALLOWED_FAILURE_CLASSES
            or key not in expected
            or key in found
            or isinstance(item["timestamp"], bool)
            or not isinstance(item["timestamp"], (int, float))
            or not math.isfinite(item["timestamp"])
            or item["timestamp"] > timestamp
        ):
            raise ValueError("alert delivery record is not current and sanitized")
        record = {
            "failure_class": failure_class,
            "subject": subject,
            "timestamp": float(item["timestamp"]),
            "delivery_state": delivery_state,
        }
        if delivery_state == "sent":
            receipt_id = item["receipt_id"]
            if (
                not isinstance(receipt_id, str)
                or not _RECEIPT_ID.fullmatch(receipt_id)
            ):
                raise ValueError("alert delivery receipt ID is invalid")
            record["receipt_id"] = receipt_id
        records.append(record)
        found.add(key)
    if found != expected:
        raise ValueError("alert sink omitted current failures")
    return sorted(
        records,
        key=lambda item: (item["failure_class"], item["subject"]),
    )


def _failed_delivery_records(
    current_alerts: Sequence[Mapping[str, str]],
    *,
    timestamp: float,
) -> list[dict[str, Any]]:
    return [
        {
            "failure_class": alert["failure_class"],
            "subject": alert["subject"],
            "timestamp": timestamp,
            "delivery_state": "unsent",
        }
        for alert in current_alerts
    ]


def run_supervisor(
    config: SupervisorConfig,
    *,
    controller: SupervisorController,
    state_store: StateStore,
    clock: Clock,
    sleeper: Sleeper,
    alert_sink: Any | None = None,
    once: bool = False,
    max_cycles: int | None = None,
) -> int:
    """Run one or a bounded recurring sequence of operational cycles."""
    if max_cycles is not None and (
        isinstance(max_cycles, bool)
        or not isinstance(max_cycles, int)
        or max_cycles < 1
    ):
        raise ValueError("max_cycles must be an integer of at least one")
    limit = 1 if once else max_cycles
    prior, load_failures = _prior_state(state_store)
    if alert_sink is None:
        alert_sink = SanitizedAlertSink(_UnavailableAlertTransport())
    final_healthy = False
    cycle_index = 0
    while limit is None or cycle_index < limit:
        try:
            raw_time = clock.now()
            clock_valid = (
                not isinstance(raw_time, bool)
                and isinstance(raw_time, (int, float))
                and math.isfinite(raw_time)
            )
        except Exception:
            raw_time = 0.0
            clock_valid = False
        checked_at = float(raw_time) if clock_valid else 0.0
        try:
            raw_cycle = controller.run_cycle()
            controller_failed = not isinstance(raw_cycle, Mapping)
        except Exception:
            raw_cycle = None
            controller_failed = True
        state, current_alerts = _build_cycle_state(
            raw_cycle,
            config=config,
            checked_at=checked_at,
            clock_valid=clock_valid,
            prior=prior,
            initial_failures=load_failures if cycle_index == 0 else (),
            controller_failed=controller_failed,
        )
        try:
            raw_delivery_records = alert_sink.process(
                current_alerts,
                timestamp=checked_at,
            )
            delivery_records = _validated_delivery_records(
                raw_delivery_records,
                current_alerts,
                timestamp=checked_at,
            )
        except Exception:
            delivery_records = _failed_delivery_records(
                current_alerts,
                timestamp=checked_at,
            )
        if any(
            record["delivery_state"] == "unsent"
            for record in delivery_records
        ):
            transport_record = {
                "failure_class": "transport_failure",
                "subject": "runtime",
                "timestamp": checked_at,
                "delivery_state": "unsent",
            }
            if transport_record not in delivery_records:
                delivery_records.append(transport_record)
            state["failure_classifications"] = sorted({
                *state["failure_classifications"],
                "transport_failure",
            })
            state["healthy"] = False
        state["alerts"] = sorted(
            delivery_records,
            key=lambda item: (item["failure_class"], item["subject"]),
        )
        try:
            state_store.commit(state)
        except Exception:
            final_healthy = False
        else:
            final_healthy = state["healthy"]
            prior = state
        cycle_index += 1
        if limit is None or cycle_index < limit:
            sleeper.sleep(config.poll_interval_seconds)
    return 0 if final_healthy else 1
