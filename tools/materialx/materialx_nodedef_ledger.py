#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Generate and validate the authoritative MaterialX NodeDef ledger."""

__all__ = (
    "LEDGER_FIELDS",
    "build_ledger",
    "ledger_as_json",
    "main",
    "remaining_node_ids",
    "validate_ledger",
)

import argparse
import json
import sys
from collections import Counter
from collections.abc import Collection
from pathlib import Path
from typing import Any, Mapping, Sequence

import materialx_catalog


SCHEMA_VERSION = 1
LEDGER_FIELDS = (
    "id",
    "category",
    "types",
    "source",
    "cycles_reader",
    "cycles_lowering",
    "hydra",
    "disposition",
    "evidence",
    "owner",
    "next_action",
)
CATALOG_FIELDS = ("id", "category", "types", "source")
OVERRIDABLE_FIELDS = LEDGER_FIELDS[len(CATALOG_FIELDS):]
STATUS_FIELDS = ("cycles_reader", "cycles_lowering", "hydra", "disposition")
DEFAULT_OVERRIDES_PATH = Path(__file__).with_name("materialx_nodedef_ledger_overrides.json")
DEFAULTS = {
    "cycles_reader": "unclassified",
    "cycles_lowering": "unclassified",
    "hydra": "unclassified",
    "disposition": "unclassified",
    "evidence": [],
    "owner": "unassigned",
    "next_action": "classify",
}


def _catalog_row(entry: Mapping[str, Any]) -> dict[str, Any]:
    missing = sorted(set(CATALOG_FIELDS).difference(entry))
    if missing:
        raise ValueError(f"Catalog entry is missing fields: {', '.join(missing)}")
    unexpected = sorted(set(entry).difference(CATALOG_FIELDS))
    if unexpected:
        raise ValueError(f"Catalog entry uses unsupported fields: {', '.join(unexpected)}")
    if not isinstance(entry["id"], str) or not entry["id"]:
        raise ValueError("Catalog entry id must be a non-empty string")
    if not isinstance(entry["types"], list) or not all(isinstance(item, str) and item for item in entry["types"]):
        raise ValueError(f"Catalog entry {entry['id']!r} types must be a list of non-empty strings")
    return {
        "id": entry["id"],
        "category": str(entry["category"]),
        "types": sorted(set(entry["types"])),
        "source": str(entry["source"]),
    }


def _overrides_document(overrides: Mapping[str, Any] | None) -> Mapping[str, Mapping[str, Any]]:
    if overrides is None:
        return {}
    if set(overrides) != {"schema_version", "rows"}:
        raise ValueError("Ledger overrides must contain only schema_version and rows")
    if overrides["schema_version"] != SCHEMA_VERSION:
        raise ValueError(f"Ledger overrides require schema_version {SCHEMA_VERSION}")
    rows = overrides["rows"]
    if not isinstance(rows, dict) or not all(isinstance(node_id, str) and isinstance(row, dict)
                                              for node_id, row in rows.items()):
        raise ValueError("Ledger overrides rows must be an object of NodeDef objects")
    return rows


def _summary(rows: Sequence[Mapping[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {"total": len(rows)}
    for field in STATUS_FIELDS:
        result[field] = dict(sorted(Counter(row[field] for row in rows).items()))
    return result


def build_ledger(
    catalog: Sequence[Mapping[str, Any]], overrides: Mapping[str, Any] | None = None
) -> dict[str, Any]:
    """Build one complete, sorted ledger row for every catalog NodeDef."""
    entries = [_catalog_row(entry) for entry in catalog]
    entries.sort(key=lambda entry: entry["id"])
    ids = [entry["id"] for entry in entries]
    if len(ids) != len(set(ids)):
        raise ValueError("Catalog contains duplicate NodeDef ids")

    override_rows = _overrides_document(overrides)
    unknown = sorted(set(override_rows).difference(ids))
    if unknown:
        raise ValueError(f"Ledger overrides reference unknown NodeDef: {', '.join(unknown)}")

    rows = []
    for entry in entries:
        row = dict(entry)
        row.update(DEFAULTS)
        override = override_rows.get(entry["id"], {})
        unexpected = sorted(set(override).difference(OVERRIDABLE_FIELDS))
        if unexpected:
            raise ValueError(f"Ledger override for {entry['id']!r} uses unsupported fields: {', '.join(unexpected)}")
        row.update(override)
        if isinstance(row["evidence"], list):
            row["evidence"] = sorted(set(row["evidence"]))
        rows.append(row)

    document = {"schema_version": SCHEMA_VERSION, "rows": rows, "summary": _summary(rows)}
    validate_ledger(document)
    return document


def validate_ledger(document: Mapping[str, Any], expected_count: int | None = None) -> None:
    """Require complete rows and a summary that exactly matches those rows."""
    if set(document) != {"schema_version", "rows", "summary"}:
        raise ValueError("Ledger document must contain only schema_version, rows, and summary")
    if document["schema_version"] != SCHEMA_VERSION:
        raise ValueError(f"Ledger requires schema_version {SCHEMA_VERSION}")
    rows = document["rows"]
    if not isinstance(rows, list):
        raise ValueError("Ledger rows must be a list")
    if expected_count is not None and len(rows) != expected_count:
        raise ValueError(f"Expected {expected_count} NodeDefs, found {len(rows)}")

    ids = []
    for row in rows:
        if not isinstance(row, dict):
            raise ValueError("Ledger rows must be objects")
        missing = sorted(set(LEDGER_FIELDS).difference(row))
        if missing:
            raise ValueError(f"Ledger row {row.get('id')!r} is missing fields: {', '.join(missing)}")
        unexpected = sorted(set(row).difference(LEDGER_FIELDS))
        if unexpected:
            raise ValueError(f"Ledger row {row.get('id')!r} uses unsupported fields: {', '.join(unexpected)}")
        if not isinstance(row["id"], str) or not row["id"]:
            raise ValueError("Ledger row id must be a non-empty string")
        if not isinstance(row["types"], list) or not all(isinstance(item, str) and item for item in row["types"]):
            raise ValueError(f"Ledger row {row['id']!r} types must be a list of non-empty strings")
        if not isinstance(row["evidence"], list) or not all(isinstance(item, str) and item for item in row["evidence"]):
            raise ValueError(f"Ledger row {row['id']!r} evidence must be a list of non-empty strings")
        for field in ("category", "source", *STATUS_FIELDS, "owner", "next_action"):
            if not isinstance(row[field], str) or not row[field]:
                raise ValueError(f"Ledger row {row['id']!r} {field} must be a non-empty string")
        ids.append(row["id"])
    if ids != sorted(ids):
        raise ValueError("Ledger rows are not sorted by id")
    if len(ids) != len(set(ids)):
        raise ValueError("Ledger contains duplicate NodeDef ids")
    if document["summary"] != _summary(rows):
        raise ValueError("Ledger summary does not match ledger rows")


def remaining_node_ids(
    ledger: Mapping[str, Any], *, completed_ids: Collection[str] = (), phase2_ids: Collection[str] = (),
    active_ids: Collection[str] = (),
) -> list[str]:
    """Return validated ledger IDs not explicitly owned by another scheduling layer."""
    validate_ledger(ledger)
    ledger_ids = {row["id"] for row in ledger["rows"]}
    ownership_ids = {}
    for name, ids in (
        ("completed", completed_ids),
        ("Phase-2", phase2_ids),
        ("active", active_ids),
    ):
        if isinstance(ids, (str, bytes)) or not isinstance(ids, Collection):
            raise ValueError(f"{name}_ids must be a collection of non-empty NodeDef ids")
        if not all(isinstance(node_id, str) and node_id for node_id in ids):
            raise ValueError(f"{name}_ids must be a collection of non-empty NodeDef ids")
        unknown = sorted(set(ids).difference(ledger_ids))
        if unknown:
            raise ValueError(f"{name}_ids reference unknown ledger row: {', '.join(unknown)}")
        ownership_ids[name] = set(ids)
    for first, second in (("completed", "Phase-2"), ("completed", "active"), ("Phase-2", "active")):
        overlap = sorted(ownership_ids[first].intersection(ownership_ids[second]))
        if overlap:
            raise ValueError(f"{first} and {second} NodeDef overlap: {', '.join(overlap)}")
    excluded_ids = set().union(*ownership_ids.values())
    return [row["id"] for row in ledger["rows"] if row["id"] not in excluded_ids]


def ledger_as_json(document: Mapping[str, Any]) -> str:
    """Serialize the ledger deterministically for review and automation."""
    validate_ledger(document)
    return json.dumps(document, indent=2, sort_keys=True) + "\n"


def _load_catalog(path: Path | None, library_root: Path | None) -> list[dict[str, Any]]:
    if path is not None:
        data = json.loads(path.read_text(encoding="utf-8"))
        if not isinstance(data, list):
            raise ValueError("Catalog JSON must be a list")
        return data
    return materialx_catalog.build_catalog(library_root)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--catalog", type=Path, help="JSON output from materialx_catalog.py")
    source.add_argument("--library-root", type=Path, help="MaterialX source root or libraries directory")
    parser.add_argument("--overrides", type=Path, default=DEFAULT_OVERRIDES_PATH)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--validate", action="store_true")
    parser.add_argument("--expected-count", type=int, default=materialx_catalog.EXPECTED_NODEDEF_COUNT,
                        help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    try:
        catalog = _load_catalog(args.catalog, args.library_root)
        overrides = json.loads(args.overrides.read_text(encoding="utf-8"))
        document = build_ledger(catalog, overrides)
        if args.validate:
            validate_ledger(document, args.expected_count)
    except (FileNotFoundError, json.JSONDecodeError, ValueError) as ex:
        print(f"materialx_nodedef_ledger.py: error: {ex}", file=sys.stderr)
        return 1

    output = ledger_as_json(document)
    if args.output is None:
        sys.stdout.write(output)
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8", newline="")
    print(json.dumps(document["summary"], sort_keys=True), file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
