#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Create an auditable MaterialX coverage skeleton for Cycles render paths.

The report deliberately keeps the Hydra and native Cycles lowerers distinct.  An
entry marked as supported by one renderer is never inferred to be supported by
the other.
"""

__all__ = (
    "build_status_skeleton",
    "load_catalog",
    "load_overrides",
    "main",
    "report_as_csv",
    "report_as_json",
)

import argparse
import csv
import io
import json
import sys
from pathlib import Path
from typing import Any, Mapping, Sequence

import materialx_catalog


REPORT_FIELDS = (
    "id",
    "category",
    "types",
    "source",
    "hydra_status",
    "native_cycles_status",
)
STATUS_FIELDS = ("hydra_status", "native_cycles_status")
DEFAULT_STATUS = "unclassified"


def _catalog_entry(entry: Mapping[str, Any]) -> dict[str, Any]:
    missing = {"id", "category", "types", "source"}.difference(entry)
    if missing:
        raise ValueError(f"Catalog entry is missing keys: {sorted(missing)}")
    node_id = entry["id"]
    if not isinstance(node_id, str) or not node_id:
        raise ValueError("Catalog entry id must be a non-empty string")
    types = entry["types"]
    if not isinstance(types, list) or not all(isinstance(value, str) for value in types):
        raise ValueError(f"Catalog entry {node_id!r} types must be a list of strings")
    return {
        "id": node_id,
        "category": str(entry["category"]),
        "types": sorted(types),
        "source": str(entry["source"]),
    }


def build_status_skeleton(
    catalog: Sequence[Mapping[str, Any]],
    overrides: Mapping[str, Mapping[str, str]] | None = None,
) -> list[dict[str, Any]]:
    """Return sorted, per-NodeDef renderer coverage rows.

    ``overrides`` may only set explicit status columns.  It is intentionally
    unable to merge renderer results, which prevents native coverage from being
    accidentally claimed from a Hydra test (or vice versa).
    """
    overrides = {} if overrides is None else overrides
    entries = [_catalog_entry(entry) for entry in catalog]
    ids = [entry["id"] for entry in entries]
    if len(ids) != len(set(ids)):
        raise ValueError("Catalog contains duplicate NodeDef ids")
    unknown = sorted(set(overrides).difference(ids))
    if unknown:
        raise ValueError(f"Overrides reference unknown NodeDefs: {', '.join(unknown)}")

    report = []
    for entry in sorted(entries, key=lambda value: value["id"]):
        node_id = entry["id"]
        row = dict(entry)
        row.update({field: DEFAULT_STATUS for field in STATUS_FIELDS})
        for field, status in overrides.get(node_id, {}).items():
            if field not in STATUS_FIELDS:
                raise ValueError(f"Override for {node_id!r} uses unsupported field {field!r}")
            if not isinstance(status, str) or not status:
                raise ValueError(f"Override for {node_id!r}/{field} must be a non-empty string")
            row[field] = status
        report.append(row)
    return report


def report_as_json(report: Sequence[Mapping[str, Any]]) -> str:
    """Serialize report rows deterministically."""
    return json.dumps(list(report), indent=2, sort_keys=True) + "\n"


def report_as_csv(report: Sequence[Mapping[str, Any]]) -> str:
    """Serialize report rows as deterministic spreadsheet-friendly CSV."""
    output = io.StringIO(newline="")
    writer = csv.DictWriter(output, fieldnames=REPORT_FIELDS, lineterminator="\n")
    writer.writeheader()
    for entry in report:
        row = dict(entry)
        row["types"] = ";".join(row["types"])
        writer.writerow({field: row[field] for field in REPORT_FIELDS})
    return output.getvalue()


def load_catalog(path: Path) -> list[dict[str, Any]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, list):
        raise ValueError("Catalog JSON must be a list")
    return data


def load_overrides(path: Path) -> Mapping[str, Mapping[str, str]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError("Override JSON must be an object keyed by NodeDef id")
    if not all(isinstance(node_id, str) and isinstance(value, dict) for node_id, value in data.items()):
        raise ValueError("Each override must map a NodeDef id to an object")
    return data


def _write_or_stdout(text: str, output: Path | None) -> None:
    if output is None:
        sys.stdout.write(text)
        return
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(text, encoding="utf-8", newline="")


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--catalog", type=Path, help="JSON output from materialx_catalog.py")
    source.add_argument("--library-root", type=Path, help="MaterialX source root or libraries directory")
    parser.add_argument("--overrides", type=Path, help="Optional explicit renderer status JSON")
    parser.add_argument("--json-output", type=Path)
    parser.add_argument("--csv-output", type=Path)
    parser.add_argument("--validate", action="store_true")
    parser.add_argument("--expected-count", type=int, default=materialx_catalog.EXPECTED_NODEDEF_COUNT,
                        help=argparse.SUPPRESS)
    args = parser.parse_args(argv)

    try:
        catalog = load_catalog(args.catalog) if args.catalog else materialx_catalog.build_catalog(args.library_root)
        overrides = load_overrides(args.overrides) if args.overrides else None
        report = build_status_skeleton(catalog, overrides)
        if args.validate and len(report) != args.expected_count:
            raise ValueError(f"Expected {args.expected_count} NodeDefs, found {len(report)}")
    except (FileNotFoundError, json.JSONDecodeError, ValueError) as ex:
        print(f"materialx_coverage_report.py: error: {ex}", file=sys.stderr)
        return 1

    json_report = report_as_json(report)
    if args.json_output:
        _write_or_stdout(json_report, args.json_output)
    if args.csv_output:
        _write_or_stdout(report_as_csv(report), args.csv_output)
    if not args.json_output and not args.csv_output:
        _write_or_stdout(json_report, None)
    if args.validate:
        print(f"Validated {len(report)} MaterialX NodeDefs in coverage skeleton", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
