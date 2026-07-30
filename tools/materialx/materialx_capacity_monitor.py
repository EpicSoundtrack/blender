#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Poll injected MaterialX capacity probes and record sanitized readiness state."""

__all__ = (
    "main",
    "monitor_as_json",
    "poll_capacity",
)

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Mapping, Protocol, Sequence

import materialx_project_preflight
from materialx_velocity_manifest import EXPECTED_HORDE_WORKERS


SCHEMA_VERSION = 1
LEGACY_ALERT_MESSAGES = {
    "worker_exit": "worker process is not active",
    "windows_local_build_blocked": "windows local build lane is blocked",
}
WORKER_FAILURE_CLASSES = {
    "exited": "capacity_loss",
    "stale_source": "stale_source",
    "auth_failure": "auth_failure",
    "proxy_failure": "proxy_failure",
}


class CapacityProbe(Protocol):
    def worker_process_log_state(self, worker_id: str) -> Mapping[str, Any]: ...

    def windows_local_build_log_state(self) -> Mapping[str, Any]: ...


def _probe_state(result: Mapping[str, Any], subject: str) -> tuple[str, bool]:
    if not isinstance(result, Mapping) or set(result) != {"state", "log"}:
        raise ValueError(f"Probe for {subject} must return only state and log")
    state = result["state"]
    log = result["log"]
    if not isinstance(state, str) or not state or not isinstance(log, str):
        raise ValueError(f"Probe for {subject} requires non-empty state and string log")
    return state, bool(log)


def poll_capacity(previous_state: Mapping[str, Any], probe: CapacityProbe) -> dict[str, Any]:
    """Return a sanitized current state plus only alerts newly raised this poll."""
    state = materialx_project_preflight.validate_capacity_state(previous_state)
    if set(state["worker_states"]) != set(EXPECTED_HORDE_WORKERS):
        raise ValueError("capacity state must contain the exact five Horde workers")
    workers = []
    journal = []
    current_failures = set()
    previous_failures = set()
    for record in state["capacity_journal"]:
        if record["kind"] == "worker_process":
            failure_class = WORKER_FAILURE_CLASSES.get(record["state"])
            if failure_class is not None:
                previous_failures.add((
                    failure_class,
                    f"worker:{record['subject']}",
                ))
        elif (
            record["kind"] == "windows_local_build"
            and record["state"] == "blocked"
        ):
            previous_failures.add(("capacity_loss", "lane:windows_local_build"))

    for worker_id in sorted(state["worker_states"]):
        worker_state, log_observed = _probe_state(probe.worker_process_log_state(worker_id), worker_id)
        if worker_state not in {"active", *WORKER_FAILURE_CLASSES}:
            raise ValueError(f"Probe for {worker_id} returned unsupported worker state {worker_state!r}")
        workers.append({"id": worker_id, "state": worker_state})
        journal.append({
            "kind": "worker_process",
            "subject": worker_id,
            "state": worker_state,
            "log_observed": log_observed,
        })
        failure_class = WORKER_FAILURE_CLASSES.get(worker_state)
        if failure_class is not None:
            current_failures.add((failure_class, f"worker:{worker_id}"))

    build_state, build_log_observed = _probe_state(
        probe.windows_local_build_log_state(), "windows_local_build"
    )
    if build_state not in {"ready", "blocked"}:
        raise ValueError(f"Probe for windows_local_build returned unsupported build state {build_state!r}")
    journal.append({
        "kind": "windows_local_build",
        "subject": "windows_local_build",
        "state": build_state,
        "log_observed": build_log_observed,
    })
    if build_state == "blocked":
        current_failures.add(("capacity_loss", "lane:windows_local_build"))

    legacy_failure_classes = set()
    if any(worker["state"] != "active" for worker in workers):
        legacy_failure_classes.add("worker_exit")
    if build_state == "blocked":
        legacy_failure_classes.add("windows_local_build_blocked")
    alerts = [
        {
            "failure_class": failure_class,
            "message": LEGACY_ALERT_MESSAGES[failure_class],
        }
        for failure_class in sorted(legacy_failure_classes)
    ]
    new_alerts = [
        {"failure_class": failure_class, "subject": subject}
        for failure_class, subject in sorted(current_failures.difference(previous_failures))
    ]
    current_alerts = [
        {"failure_class": failure_class, "subject": subject}
        for failure_class, subject in sorted(current_failures)
    ]
    journal.sort(key=lambda record: (record["kind"], record["subject"]))
    capacity_state = {
        "schema_version": SCHEMA_VERSION,
        "healthy_workers": workers,
        "completed_rows": state["completed_rows"],
        "evidence_records": [
            {"row_id": row_id, "record": record}
            for row_id, record in sorted(state["evidence_records"].items())
        ],
        "journal_records": [
            {"row_id": row_id, "record": record}
            for row_id, record in sorted(state["journal_records"].items())
        ],
        "lanes": {
            "windows_local_build": {
                "state": build_state,
                "alerted": build_state == "blocked",
            }
        },
        "capacity_journal": journal,
        "alerts": alerts,
    }
    materialx_project_preflight.validate_capacity_state(capacity_state)
    return {
        "schema_version": SCHEMA_VERSION,
        "capacity_state": capacity_state,
        "current_alerts": current_alerts,
        "new_alerts": new_alerts,
    }


def monitor_as_json(result: Mapping[str, Any]) -> str:
    """Serialize monitor output deterministically without including raw probe logs."""
    return json.dumps(result, indent=2, sort_keys=True) + "\n"


class JsonProbe:
    def __init__(self, snapshot: Mapping[str, Any]):
        if not isinstance(snapshot, Mapping) or set(snapshot) != {"workers", "windows_local_build"}:
            raise ValueError("Probe snapshot must contain only workers and windows_local_build")
        if not isinstance(snapshot["workers"], Mapping):
            raise ValueError("Probe snapshot workers must be an object")
        self.snapshot = snapshot

    def worker_process_log_state(self, worker_id: str) -> Mapping[str, Any]:
        try:
            return self.snapshot["workers"][worker_id]
        except KeyError as ex:
            raise ValueError(f"Probe snapshot is missing worker {worker_id!r}") from ex

    def windows_local_build_log_state(self) -> Mapping[str, Any]:
        return self.snapshot["windows_local_build"]


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capacity", required=True, type=Path)
    parser.add_argument("--probe-state", required=True, type=Path)
    parser.add_argument("--capacity-output", type=Path)
    parser.add_argument("--journal-output", type=Path)
    args = parser.parse_args(argv)
    try:
        previous = json.loads(args.capacity.read_text(encoding="utf-8"))
        snapshot = json.loads(args.probe_state.read_text(encoding="utf-8"))
        result = poll_capacity(previous, JsonProbe(snapshot))
    except (FileNotFoundError, json.JSONDecodeError, ValueError) as ex:
        print(f"materialx_capacity_monitor.py: error: {ex}", file=sys.stderr)
        return 1
    if args.capacity_output is not None:
        args.capacity_output.write_text(monitor_as_json(result["capacity_state"]), encoding="utf-8", newline="")
    if args.journal_output is not None:
        args.journal_output.write_text(monitor_as_json(result["capacity_state"]["capacity_journal"]), encoding="utf-8", newline="")
    sys.stdout.write(monitor_as_json(result))
    return 0


if __name__ == "__main__":
    sys.exit(main())
