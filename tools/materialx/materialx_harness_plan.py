#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Create deterministic per-NodeDef renderer harness-plan scaffolding."""

__all__ = (
    "build_harness_plan",
    "ingest_targeted_results",
    "main",
    "plan_as_json",
)

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Mapping, Sequence

import materialx_catalog
import materialx_coverage_report


PLAN_FIELDS = (
    "id",
    "category",
    "types",
    "source",
    "renderer",
    "status",
    "test_id",
)
RENDERERS = (
    ("hydra", "hydra_status", "MaterialXHydra"),
    ("native_cycles", "native_cycles_status", "MaterialXNativeCycles"),
)
RESULT_FIELDS = (
    "id",
    "renderer",
    "test_id",
    "status",
    "cpu_result",
    "gpu_result",
    "parity",
    "evidence",
)
EVIDENCE_FIELDS = ("cpu_result", "gpu_result", "parity", "evidence")


def build_harness_plan(
    catalog: Sequence[Mapping[str, Any]],
    overrides: Mapping[str, Mapping[str, str]] | None = None,
) -> list[dict[str, Any]]:
    """Return two deterministic renderer-harness rows for every catalog NodeDef.

    This is intentionally scaffolding only: it copies explicit coverage statuses
    from the existing parity matrix but does not infer or assign renderer support.
    """
    matrix = materialx_coverage_report.build_status_skeleton(catalog, overrides)
    plan = []
    for entry in matrix:
        for renderer, status_field, suite in RENDERERS:
            plan.append(
                {
                    "id": entry["id"],
                    "category": entry["category"],
                    "types": entry["types"],
                    "source": entry["source"],
                    "renderer": renderer,
                    "status": entry[status_field],
                    "test_id": f"{suite}.{entry['id']}",
                }
            )
    return plan


def plan_as_json(plan: Sequence[Mapping[str, Any]]) -> str:
    """Serialize harness-plan rows deterministically."""
    return json.dumps(list(plan), indent=2, sort_keys=True) + "\n"


def ingest_targeted_results(
    plan: Sequence[Mapping[str, Any]], results: Sequence[Mapping[str, Any]]
) -> list[dict[str, Any]]:
    """Attach explicit CPU/GPU parity evidence to exactly matching plan rows.

    Results carry their own renderer status and parity finding.  This function
    merely records those submitted values; it never derives coverage or parity.
    """
    rows = [dict(row) for row in plan]
    by_key = {(row["id"], row["renderer"]): row for row in rows}
    if len(by_key) != len(rows):
        raise ValueError("Harness plan contains duplicate NodeDef renderer rows")

    seen = set()
    for result in results:
        missing = sorted(set(RESULT_FIELDS).difference(result))
        if missing:
            raise ValueError(f"Targeted result is missing fields: {', '.join(missing)}")
        unexpected = sorted(set(result).difference(RESULT_FIELDS))
        if unexpected:
            raise ValueError(f"Targeted result uses unsupported fields: {', '.join(unexpected)}")
        if not all(isinstance(result[field], str) and result[field] for field in RESULT_FIELDS):
            raise ValueError("Targeted result fields must be non-empty strings")

        key = (result["id"], result["renderer"])
        row = by_key.get(key)
        if row is None:
            raise ValueError(f"Targeted result references unknown plan row: {key!r}")
        if key in seen:
            raise ValueError(f"Targeted result is duplicated for plan row: {key!r}")
        if result["test_id"] != row["test_id"]:
            raise ValueError(f"Targeted result test_id does not match plan row: {key!r}")
        seen.add(key)
        row["status"] = result["status"]
        for field in EVIDENCE_FIELDS:
            row[field] = result[field]

    for row in rows:
        for field in EVIDENCE_FIELDS:
            row.setdefault(field, None)
    return rows


def _load_source(args: argparse.Namespace) -> list[dict[str, Any]]:
    if args.catalog:
        return materialx_coverage_report.load_catalog(args.catalog)
    return materialx_catalog.build_catalog(args.library_root)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--catalog", type=Path, help="JSON output from materialx_catalog.py")
    source.add_argument("--library-root", type=Path, help="MaterialX source root or libraries directory")
    parser.add_argument("--overrides", type=Path, help="Optional explicit renderer status JSON")
    parser.add_argument("--results", type=Path, help="Optional explicit targeted CPU/GPU result JSON")
    parser.add_argument("--output", type=Path, default=None)
    parser.add_argument("--validate", action="store_true")
    parser.add_argument(
        "--expected-count",
        type=int,
        default=materialx_catalog.EXPECTED_NODEDEF_COUNT,
        help=argparse.SUPPRESS,
    )
    args = parser.parse_args(argv)

    try:
        catalog = _load_source(args)
        overrides = materialx_coverage_report.load_overrides(args.overrides) if args.overrides else None
        plan = build_harness_plan(catalog, overrides)
        if args.results:
            results = json.loads(args.results.read_text(encoding="utf-8"))
            if not isinstance(results, list) or not all(isinstance(result, dict) for result in results):
                raise ValueError("Targeted result JSON must be a list of objects")
            plan = ingest_targeted_results(plan, results)
        if args.validate and len(plan) != args.expected_count * len(RENDERERS):
            raise ValueError(
                f"Expected {args.expected_count * len(RENDERERS)} harness rows, found {len(plan)}"
            )
    except (FileNotFoundError, json.JSONDecodeError, ValueError) as ex:
        print(f"materialx_harness_plan.py: error: {ex}", file=sys.stderr)
        return 1

    output = plan_as_json(plan)
    if args.output is None:
        sys.stdout.write(output)
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8", newline="")
    if args.validate:
        print(f"Validated {len(plan)} MaterialX renderer harness rows", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
