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


_IDENTIFIER = re.compile(r"^[A-Za-z0-9_.-]{1,128}$")
_CATEGORY = re.compile(r"^[a-z][a-z0-9_]{0,63}$")
_WORKER_STATES = frozenset(("active", "idle", "blocked"))
_RECEIPT_STATES = frozenset(("integrated", "rejected"))
_LAYER_ORDER = {
    "native_cycles": 0,
    "hydra_ovrtx": 1,
    "blender_authoring": 2,
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
        if not worker_id or worker_id in seen or state not in _WORKER_STATES:
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
        if not worker_id or not batch_id:
            valid = False
            continue
        assignments.append({"worker_id": worker_id, "batch_id": batch_id})
    return sorted(
        assignments,
        key=lambda assignment: (assignment["worker_id"], assignment["batch_id"]),
    ), valid


def _sanitize_receipts(raw: Any) -> tuple[list[dict[str, Any]], list[str]]:
    if isinstance(raw, (str, bytes)) or not isinstance(raw, Sequence):
        return [], ["invalid_integration_receipt"]
    receipts = []
    failures = []
    for value in raw:
        if not isinstance(value, Mapping):
            failures.append("invalid_integration_receipt")
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
            or final_state not in _RECEIPT_STATES
            or not base_sha
            or not head_sha
            or (final_state == "rejected" and not failure)
            or (final_state == "integrated" and "failure_classification" in value)
        ):
            failures.append("invalid_integration_receipt")
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
            failures.extend(("integration_failure", failure))
        receipts.append(receipt)
    return sorted(
        receipts,
        key=lambda receipt: (
            _LAYER_ORDER.get(receipt["layer"], len(_LAYER_ORDER)),
            receipt["batch_id"],
        ),
    ), failures


def _sanitize_alerts(raw: Any) -> tuple[list[dict[str, str]], list[str]]:
    if isinstance(raw, (str, bytes)) or not isinstance(raw, Sequence):
        return [], ["invalid_alert_evidence"]
    alerts = []
    failures = []
    for value in raw:
        if not isinstance(value, Mapping):
            failures.append("invalid_alert_evidence")
            continue
        classification = _category(value.get("classification"))
        worker_id = _identifier(value.get("worker_id"))
        batch_id = _identifier(value.get("batch_id")) if "batch_id" in value else ""
        if not classification or not worker_id:
            failures.append("invalid_alert_evidence")
            continue
        alert = {"worker_id": worker_id, "classification": classification}
        if batch_id:
            alert["batch_id"] = batch_id
        alerts.append(alert)
        failures.append(classification)
    return sorted(
        alerts,
        key=lambda alert: (
            alert["worker_id"],
            alert.get("batch_id", ""),
            alert["classification"],
        ),
    ), failures


def _build_cycle_state(
    raw: Mapping[str, Any] | None,
    *,
    config: SupervisorConfig,
    checked_at: float,
    clock_valid: bool,
    prior: Mapping[str, Any],
    initial_failures: Sequence[str],
    controller_failed: bool,
) -> dict[str, Any]:
    sequence = _sequence(prior.get("cycle_sequence")) + 1
    previous_poll = prior.get("last_successful_poll")
    if (
        isinstance(previous_poll, bool)
        or not isinstance(previous_poll, (int, float))
        or not math.isfinite(previous_poll)
    ):
        previous_poll = None
    failures = list(initial_failures)
    if not clock_valid:
        failures.append("invalid_clock")

    if controller_failed or raw is None:
        failures.append("controller_exception")
        workers: list[dict[str, str]] = []
        assignments: list[dict[str, str]] = []
        receipts: list[dict[str, Any]] = []
        alerts: list[dict[str, str]] = []
        queue_depth = 0
        last_poll = previous_poll
    else:
        workers, workers_valid = _sanitize_workers(raw.get("workers"))
        assignments, assignments_valid = _sanitize_assignments(
            raw.get("assigned_batches")
        )
        receipts, receipt_failures = _sanitize_receipts(
            raw.get("integration_receipts")
        )
        alerts, alert_failures = _sanitize_alerts(raw.get("alerts"))
        failures.extend(receipt_failures)
        failures.extend(alert_failures)
        if not workers_valid or not assignments_valid:
            failures.append("invalid_controller_result")
        if len(workers) != 5:
            failures.append("worker_count")
        elif clock_valid:
            previous_poll = checked_at
        last_poll = previous_poll
        if any(worker["state"] == "blocked" for worker in workers):
            failures.append("worker_blocked")
        queue_depth = raw.get("queue_depth")
        if (
            isinstance(queue_depth, bool)
            or not isinstance(queue_depth, int)
            or queue_depth < 0
        ):
            failures.append("invalid_queue_depth")
            queue_depth = 0
        if queue_depth == 0:
            failures.append("queue_empty")
        elif queue_depth < config.queue_watermark:
            failures.append("queue_below_watermark")

    if (
        clock_valid
        and last_poll is not None
        and checked_at - last_poll
        >= config.stale_intervals * config.poll_interval_seconds
    ):
        failures.append("stale_poll")

    failure_classifications = sorted(set(failures))
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
        "alerts": alerts,
        "failure_classifications": failure_classifications,
    }


def run_supervisor(
    config: SupervisorConfig,
    *,
    controller: SupervisorController,
    state_store: StateStore,
    clock: Clock,
    sleeper: Sleeper,
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
        state = _build_cycle_state(
            raw_cycle,
            config=config,
            checked_at=checked_at,
            clock_valid=clock_valid,
            prior=prior,
            initial_failures=load_failures if cycle_index == 0 else (),
            controller_failed=controller_failed,
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
