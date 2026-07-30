#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Poll bounded capacity evidence into the canonical MaterialX state."""

__all__ = ("main", "monitor_as_json", "poll_capacity")

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Mapping, Protocol, Sequence

import materialx_project_state
from materialx_velocity_manifest import EXPECTED_HORDE_WORKERS


SCHEMA_VERSION = materialx_project_state.SCHEMA_VERSION
WORKER_FAILURE_CLASSES = {
    "exited": "capacity_loss",
    "stale_source": "stale_source",
    "auth_failure": "auth_failure",
    "proxy_failure": "proxy_failure",
}


class CapacityProbe(Protocol):
    def worker_process_log_state(self, worker_id: str) -> Mapping[str, Any]: ...

    def windows_a40_cuda_evidence(self) -> Mapping[str, Any] | None: ...


def _probe_state(
    result: Mapping[str, Any],
    subject: str,
) -> tuple[str, bool, str]:
    if (
        not isinstance(result, Mapping)
        or set(result) != {"state", "log", "evidence_receipt"}
    ):
        raise ValueError(
            f"Probe for {subject} must return state, log, and evidence_receipt"
        )
    state = result["state"]
    log = result["log"]
    evidence = result["evidence_receipt"]
    if (
        not isinstance(state, str)
        or not state
        or not isinstance(log, str)
        or not isinstance(evidence, str)
        or not evidence
        or len(evidence) > 128
    ):
        raise ValueError(f"Probe for {subject} returned invalid bounded evidence")
    return state, bool(log), evidence


def _failure_pairs(state: Mapping[str, Any]) -> set[tuple[str, str]]:
    return {
        (WORKER_FAILURE_CLASSES[worker["state"]], f"worker:{worker['id']}")
        for worker in state["workers"]
        if worker["state"] in WORKER_FAILURE_CLASSES
    }


def poll_capacity(
    previous_state: Mapping[str, Any],
    probe: CapacityProbe,
) -> dict[str, Any]:
    """Update only observed Horde state and explicit Windows A40 evidence."""
    if (
        isinstance(previous_state, Mapping)
        and previous_state.get("schema_version") == 1
    ):
        state = materialx_project_state.migrate_project_state(previous_state)
    else:
        state = materialx_project_state.validate_project_state(previous_state)
    if {worker["id"] for worker in state["workers"]} != set(EXPECTED_HORDE_WORKERS):
        raise ValueError("project state must contain the exact five Horde workers")

    previous_failures = _failure_pairs(state)
    worker_states = {}
    evidence_receipts = set()
    for worker_id in sorted(EXPECTED_HORDE_WORKERS):
        worker_state, _, evidence = _probe_state(
            probe.worker_process_log_state(worker_id),
            worker_id,
        )
        if worker_state not in {"active", *WORKER_FAILURE_CLASSES}:
            raise ValueError(
                f"Probe for {worker_id} returned unsupported worker state "
                f"{worker_state!r}"
            )
        worker_states[worker_id] = worker_state
        evidence_receipts.add(evidence)
    if len(evidence_receipts) != 1:
        raise ValueError("one capacity poll requires one shared Horde evidence receipt")
    horde_evidence = next(iter(evidence_receipts))
    state = materialx_project_state.update_horde_observation(
        state,
        worker_states=worker_states,
        evidence_receipt=horde_evidence,
        reason=(
            "probe_observation"
            if set(worker_states.values()) == {"active"}
            else "probe_failure"
        ),
    )

    windows_receipt = probe.windows_a40_cuda_evidence()
    if windows_receipt is not None:
        state = materialx_project_state.apply_lane_evidence(
            state,
            windows_receipt,
        )

    current_failures = _failure_pairs(state)
    return {
        "schema_version": SCHEMA_VERSION,
        "capacity_state": state,
        "current_alerts": [
            {"failure_class": failure_class, "subject": subject}
            for failure_class, subject in sorted(current_failures)
        ],
        "new_alerts": [
            {"failure_class": failure_class, "subject": subject}
            for failure_class, subject in sorted(
                current_failures.difference(previous_failures)
            )
        ],
    }


def monitor_as_json(result: Mapping[str, Any]) -> str:
    return json.dumps(result, allow_nan=False, indent=2, sort_keys=True) + "\n"


class JsonProbe:
    def __init__(self, snapshot: Mapping[str, Any]):
        if (
            not isinstance(snapshot, Mapping)
            or set(snapshot) != {"workers", "windows_a40_cuda_evidence"}
            or not isinstance(snapshot["workers"], Mapping)
        ):
            raise ValueError(
                "Probe snapshot requires workers and windows_a40_cuda_evidence"
            )
        self.snapshot = snapshot

    def worker_process_log_state(self, worker_id: str) -> Mapping[str, Any]:
        try:
            return self.snapshot["workers"][worker_id]
        except KeyError as ex:
            raise ValueError(
                f"Probe snapshot is missing worker {worker_id!r}"
            ) from ex

    def windows_a40_cuda_evidence(self) -> Mapping[str, Any] | None:
        return self.snapshot["windows_a40_cuda_evidence"]


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
        if args.capacity_output is not None:
            materialx_project_state.write_project_state(
                args.capacity_output,
                result["capacity_state"],
            )
        if args.journal_output is not None:
            args.journal_output.write_text(
                json.dumps(
                    result["capacity_state"]["semantic_journal"],
                    allow_nan=False,
                    indent=2,
                    sort_keys=True,
                ) + "\n",
                encoding="utf-8",
                newline="",
            )
    except (FileNotFoundError, json.JSONDecodeError, ValueError) as ex:
        print(f"materialx_capacity_monitor.py: error: {ex}", file=sys.stderr)
        return 1
    sys.stdout.write(monitor_as_json(result))
    return 0


if __name__ == "__main__":
    sys.exit(main())
