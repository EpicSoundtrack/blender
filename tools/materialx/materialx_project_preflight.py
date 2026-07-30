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
import materialx_project_state


SCHEMA_VERSION = materialx_project_state.SCHEMA_VERSION
DEFAULT_CAPACITY_PATH = Path(__file__).with_name("materialx_project_capacity_state.json")


def validate_capacity_state(capacity: Mapping[str, Any]) -> dict[str, Any]:
    """Compatibility name for the one canonical state validator."""
    return materialx_project_state.validate_project_state(capacity)


def _input_state(capacity: Mapping[str, Any]) -> dict[str, Any]:
    if isinstance(capacity, Mapping) and capacity.get("schema_version") == 1:
        return materialx_project_state.migrate_project_state(capacity)
    return materialx_project_state.validate_project_state(capacity)


def evaluate_preflight(
    ledger: Mapping[str, Any],
    capacity: Mapping[str, Any],
    *,
    expected_count: int = materialx_catalog.EXPECTED_NODEDEF_COUNT,
) -> dict[str, Any]:
    """Return deterministic readiness from a ledger and canonical state."""
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
        state = _input_state(capacity)
    except ValueError as ex:
        failures.append(f"capacity: {ex}")

    if state is not None:
        evidence_rows = {
            record["row_id"] for record in state["evidence_records"]
        }
        journal_rows = {
            record["row_id"] for record in state["journal_records"]
        }
        for worker in state["workers"]:
            if worker["state"] != "active":
                failures.append(
                    f"workers: {worker['id']} is {worker['state']}, not active"
                )
        for row_id in state["completed_rows"]:
            row = rows_by_id.get(row_id)
            if row is None:
                failures.append(
                    f"completed_rows: {row_id} is not a validated ledger row"
                )
                continue
            if not row["evidence"]:
                failures.append(
                    f"completed_rows: {row_id} is missing ledger evidence"
                )
            if row_id not in evidence_rows:
                failures.append(
                    f"completed_rows: {row_id} is missing an evidence record"
                )
            if row_id not in journal_rows:
                failures.append(
                    f"completed_rows: {row_id} is missing a journal record"
                )

    return {
        "schema_version": SCHEMA_VERSION,
        "ok": not failures,
        "ledger_rows": len(ledger_rows) if isinstance(ledger_rows, list) else 0,
        "completed_rows": state["completed_rows"] if state is not None else [],
        "active_workers": [
            worker["id"] for worker in state["workers"]
            if worker["state"] == "active"
        ] if state is not None else [],
        "lanes": state["lanes"] if state is not None else None,
        "failures": failures,
    }


def preflight_as_json(summary: Mapping[str, Any]) -> str:
    return json.dumps(summary, allow_nan=False, indent=2, sort_keys=True) + "\n"


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ledger", required=True, type=Path)
    parser.add_argument("--capacity", type=Path, default=DEFAULT_CAPACITY_PATH)
    parser.add_argument(
        "--expected-count",
        type=int,
        default=materialx_catalog.EXPECTED_NODEDEF_COUNT,
    )
    args = parser.parse_args(argv)
    try:
        ledger = json.loads(args.ledger.read_text(encoding="utf-8"))
        capacity = json.loads(args.capacity.read_text(encoding="utf-8"))
        summary = evaluate_preflight(
            ledger,
            capacity,
            expected_count=args.expected_count,
        )
    except (FileNotFoundError, json.JSONDecodeError, ValueError) as ex:
        summary = {
            "schema_version": SCHEMA_VERSION,
            "ok": False,
            "ledger_rows": 0,
            "completed_rows": [],
            "active_workers": [],
            "lanes": None,
            "failures": [f"input: {ex}"],
        }
    sys.stdout.write(preflight_as_json(summary))
    return 0 if summary["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
