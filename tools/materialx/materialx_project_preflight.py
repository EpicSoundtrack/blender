#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Fail-closed project readiness checks for MaterialX authoring work."""

__all__ = (
    "evaluate_preflight",
    "main",
    "preflight_as_json",
    "validate_capacity_state",
)

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Mapping, Sequence

import materialx_catalog
import materialx_nodedef_ledger


SCHEMA_VERSION = 1
CAPACITY_FIELDS = {
    "schema_version",
    "healthy_workers",
    "completed_rows",
    "evidence_records",
    "journal_records",
    "lanes",
    "capacity_journal",
    "alerts",
}
DEFAULT_CAPACITY_PATH = Path(__file__).with_name("materialx_project_capacity_state.json")


def _records_by_row(records: Any, field: str) -> dict[str, str]:
    if not isinstance(records, list):
        raise ValueError(f"{field} must be a list")
    result = {}
    for record in records:
        if not isinstance(record, dict) or set(record) != {"row_id", "record"}:
            raise ValueError(f"{field} entries must contain only row_id and record")
        row_id = record["row_id"]
        value = record["record"]
        if not isinstance(row_id, str) or not row_id or not isinstance(value, str) or not value:
            raise ValueError(f"{field} entries require non-empty row_id and record")
        if row_id in result:
            raise ValueError(f"{field} contains duplicate row_id {row_id!r}")
        result[row_id] = value
    return result


def _capacity_state(capacity: Mapping[str, Any]) -> dict[str, Any]:
    if not isinstance(capacity, dict) or set(capacity) != CAPACITY_FIELDS:
        raise ValueError("capacity state must contain only the required project-state fields")
    if capacity["schema_version"] != SCHEMA_VERSION:
        raise ValueError(f"capacity state requires schema_version {SCHEMA_VERSION}")

    workers = capacity["healthy_workers"]
    if not isinstance(workers, list):
        raise ValueError("healthy_workers must be a list")
    worker_states = {}
    for worker in workers:
        if not isinstance(worker, dict) or set(worker) != {"id", "state"}:
            raise ValueError("healthy_workers entries must contain only id and state")
        worker_id = worker["id"]
        state = worker["state"]
        if not isinstance(worker_id, str) or not worker_id or not isinstance(state, str) or not state:
            raise ValueError("healthy_workers entries require non-empty id and state")
        if worker_id in worker_states:
            raise ValueError(f"healthy_workers contains duplicate id {worker_id!r}")
        worker_states[worker_id] = state

    completed_rows = capacity["completed_rows"]
    if not isinstance(completed_rows, list) or not all(
        isinstance(row_id, str) and row_id for row_id in completed_rows
    ):
        raise ValueError("completed_rows must be a list of non-empty row ids")
    if len(completed_rows) != len(set(completed_rows)):
        raise ValueError("completed_rows contains duplicate row ids")

    lanes = capacity["lanes"]
    if not isinstance(lanes, dict) or set(lanes) != {"windows_local_build"}:
        raise ValueError("lanes must contain only windows_local_build")
    windows = lanes["windows_local_build"]
    if not isinstance(windows, dict) or set(windows) != {"state", "alerted"}:
        raise ValueError("windows_local_build must contain only state and alerted")
    if not isinstance(windows["state"], str) or not windows["state"] or not isinstance(windows["alerted"], bool):
        raise ValueError("windows_local_build requires non-empty state and boolean alerted")

    capacity_journal = capacity["capacity_journal"]
    if not isinstance(capacity_journal, list):
        raise ValueError("capacity_journal must be a list")
    journal_rows = []
    for record in capacity_journal:
        if not isinstance(record, dict) or set(record) != {"kind", "subject", "state", "log_observed"}:
            raise ValueError("capacity_journal entries must contain only kind, subject, state, and log_observed")
        if not all(isinstance(record[field], str) and record[field] for field in ("kind", "subject", "state")):
            raise ValueError("capacity_journal entries require non-empty kind, subject, and state")
        if not isinstance(record["log_observed"], bool):
            raise ValueError("capacity_journal entries require boolean log_observed")
        journal_rows.append(dict(record))
    if journal_rows != sorted(journal_rows, key=lambda row: (row["kind"], row["subject"])):
        raise ValueError("capacity_journal entries must be sorted")

    alerts = capacity["alerts"]
    if not isinstance(alerts, list):
        raise ValueError("alerts must be a list")
    alert_rows = []
    for alert in alerts:
        if not isinstance(alert, dict) or set(alert) != {"failure_class", "message"}:
            raise ValueError("alerts entries must contain only failure_class and message")
        if not all(isinstance(alert[field], str) and alert[field] for field in ("failure_class", "message")):
            raise ValueError("alerts entries require non-empty failure_class and message")
        alert_rows.append(dict(alert))
    if len({alert["failure_class"] for alert in alert_rows}) != len(alert_rows):
        raise ValueError("alerts contains duplicate failure_class")
    if alert_rows != sorted(alert_rows, key=lambda alert: alert["failure_class"]):
        raise ValueError("alerts entries must be sorted")

    return {
        "worker_states": worker_states,
        "completed_rows": sorted(completed_rows),
        "evidence_records": _records_by_row(capacity["evidence_records"], "evidence_records"),
        "journal_records": _records_by_row(capacity["journal_records"], "journal_records"),
        "windows_local_build": dict(windows),
        "capacity_journal": journal_rows,
        "alerts": alert_rows,
    }


def validate_capacity_state(capacity: Mapping[str, Any]) -> dict[str, Any]:
    """Validate the shared capacity-state contract used by monitors and preflight."""
    return _capacity_state(capacity)


def evaluate_preflight(
    ledger: Mapping[str, Any], capacity: Mapping[str, Any], *, expected_count: int = materialx_catalog.EXPECTED_NODEDEF_COUNT
) -> dict[str, Any]:
    """Return a deterministic project-readiness result without inferring coverage."""
    failures = []
    ledger_rows = ledger.get("rows") if isinstance(ledger, dict) else None
    rows_by_id = {}
    try:
        materialx_nodedef_ledger.validate_ledger(ledger, expected_count=expected_count)
        rows_by_id = {row["id"]: row for row in ledger["rows"]}
    except (TypeError, ValueError) as ex:
        failures.append(f"ledger: {ex}")

    state = None
    try:
        state = validate_capacity_state(capacity)
    except ValueError as ex:
        failures.append(f"capacity: {ex}")

    if state is not None:
        alert_classes = {alert["failure_class"] for alert in state["alerts"]}
        for worker_id, worker_state in sorted(state["worker_states"].items()):
            if worker_state != "active" and "worker_exit" not in alert_classes:
                failures.append(f"healthy_workers: {worker_id} is {worker_state}, not active")

        for row_id in state["completed_rows"]:
            row = rows_by_id.get(row_id)
            if row is None:
                failures.append(f"completed_rows: {row_id} is not a validated ledger row")
                continue
            if not row["evidence"]:
                failures.append(f"completed_rows: {row_id} is missing ledger evidence")
            if row_id not in state["evidence_records"]:
                failures.append(f"completed_rows: {row_id} is missing an evidence record")
            if row_id not in state["journal_records"]:
                failures.append(f"completed_rows: {row_id} is missing a journal record")

        windows = state["windows_local_build"]
        if windows["state"] == "blocked":
            if not windows["alerted"]:
                failures.append("windows_local_build: blocked lane requires alerted=true")
            elif "windows_local_build_blocked" not in alert_classes:
                failures.append("windows_local_build: blocked lane requires an alert record")

    return {
        "schema_version": SCHEMA_VERSION,
        "ok": not failures,
        "ledger_rows": len(ledger_rows) if isinstance(ledger_rows, list) else 0,
        "completed_rows": state["completed_rows"] if state is not None else [],
        "active_workers": sorted(
            worker_id for worker_id, worker_state in state["worker_states"].items() if worker_state == "active"
        ) if state is not None else [],
        "windows_local_build": state["windows_local_build"] if state is not None else None,
        "failures": failures,
    }


def preflight_as_json(summary: Mapping[str, Any]) -> str:
    """Serialize a readiness result deterministically for automation."""
    return json.dumps(summary, indent=2, sort_keys=True) + "\n"


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ledger", required=True, type=Path, help="Validated MaterialX NodeDef ledger JSON")
    parser.add_argument("--capacity", type=Path, default=DEFAULT_CAPACITY_PATH, help="Project capacity-state JSON")
    parser.add_argument("--expected-count", type=int, default=materialx_catalog.EXPECTED_NODEDEF_COUNT)
    args = parser.parse_args(argv)

    try:
        ledger = json.loads(args.ledger.read_text(encoding="utf-8"))
        capacity = json.loads(args.capacity.read_text(encoding="utf-8"))
    except (FileNotFoundError, json.JSONDecodeError) as ex:
        summary = {
            "schema_version": SCHEMA_VERSION,
            "ok": False,
            "ledger_rows": 0,
            "completed_rows": [],
            "active_workers": [],
            "windows_local_build": None,
            "failures": [f"input: {ex}"],
        }
    else:
        summary = evaluate_preflight(ledger, capacity, expected_count=args.expected_count)

    sys.stdout.write(preflight_as_json(summary))
    return 0 if summary["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
