#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Render explicit MaterialX parity evidence as a deterministic local HTML index."""

__all__ = (
    "build_dashboard_rows",
    "dashboard_as_html",
    "main",
)

import argparse
import html
import json
import sys
from pathlib import Path
from typing import Any, Mapping, Sequence

import materialx_harness_plan


IMAGE_FIELDS = ("reference_image", "current_image", "diff_image")
OPTIONAL_FIELDS = (*IMAGE_FIELDS, "metrics", "human_review")


def build_dashboard_rows(evidence_rows: Sequence[Mapping[str, Any]]) -> list[dict[str, Any]]:
    """Validate and sort submitted evidence rows without deriving parity state."""
    rows = []
    for evidence in evidence_rows:
        missing = sorted(set(materialx_harness_plan.RESULT_FIELDS).difference(evidence))
        if missing:
            raise ValueError(f"Evidence row is missing fields: {', '.join(missing)}")
        unexpected = sorted(
            set(evidence).difference((*materialx_harness_plan.RESULT_FIELDS, *OPTIONAL_FIELDS))
        )
        if unexpected:
            raise ValueError(f"Evidence row uses unsupported fields: {', '.join(unexpected)}")
        if not all(
            isinstance(evidence[field], str) and evidence[field]
            for field in materialx_harness_plan.RESULT_FIELDS
        ):
            raise ValueError("Evidence row required fields must be non-empty strings")
        for field in IMAGE_FIELDS + ("human_review",):
            if field in evidence and (not isinstance(evidence[field], str) or not evidence[field]):
                raise ValueError(f"Evidence row {field} must be a non-empty string when provided")
        metrics = evidence.get("metrics")
        if metrics is not None and (
            not isinstance(metrics, Mapping)
            or not all(isinstance(name, str) and isinstance(value, (int, float, str)) for name, value in metrics.items())
        ):
            raise ValueError("Evidence row metrics must map string names to scalar values")

        row = {field: evidence[field] for field in materialx_harness_plan.RESULT_FIELDS}
        for field in IMAGE_FIELDS + ("human_review",):
            row[field] = evidence.get(field)
        row["metrics"] = dict(sorted(metrics.items())) if metrics is not None else None
        rows.append(row)
    return sorted(rows, key=lambda row: (row["id"], row["renderer"], row["test_id"]))


def _link(path: str | None) -> str:
    if path is None:
        return "&mdash;"
    escaped = html.escape(path, quote=True)
    return f'<a href="{escaped}">{escaped}</a>'


def _text(value: object | None) -> str:
    return "&mdash;" if value is None else html.escape(str(value), quote=True)


def _metrics(metrics: Mapping[str, Any] | None) -> str:
    if metrics is None:
        return "&mdash;"
    return html.escape("; ".join(f"{name}={value}" for name, value in metrics.items()), quote=True)


def dashboard_as_html(rows: Sequence[Mapping[str, Any]]) -> str:
    """Render deterministic, self-contained HTML for already validated dashboard rows."""
    body = []
    for row in rows:
        body.append(
            "<tr>"
            f"<td>{_text(row['id'])}</td>"
            f"<td>{_text(row['renderer'])}</td>"
            f"<td>{_text(row['status'])}</td>"
            f"<td>{_text(row['cpu_result'])}</td>"
            f"<td>{_text(row['gpu_result'])}</td>"
            f"<td>{_text(row['parity'])}</td>"
            f"<td>{_link(row['evidence'])}</td>"
            f"<td>{_link(row['reference_image'])}</td>"
            f"<td>{_link(row['current_image'])}</td>"
            f"<td>{_link(row['diff_image'])}</td>"
            f"<td>{_metrics(row['metrics'])}</td>"
            f"<td>{_text(row['human_review'])}</td>"
            "</tr>"
        )
    return (
        "<!doctype html>\n"
        "<html lang=\"en\"><head><meta charset=\"utf-8\">"
        "<title>MaterialX parity dashboard</title>"
        "<style>body{font-family:sans-serif;margin:1rem}table{border-collapse:collapse}"
        "th,td{border:1px solid #bbb;padding:.35rem;text-align:left;vertical-align:top}"
        "th{background:#eee}</style></head><body>"
        "<h1>MaterialX parity dashboard</h1>"
        "<p>Values are explicit submitted evidence; blank fields are not inferred.</p>"
        "<table><thead><tr><th>NodeDef</th><th>Renderer</th><th>Status</th>"
        "<th>CPU</th><th>GPU</th><th>Parity</th><th>Evidence</th>"
        "<th>Reference</th><th>Current</th><th>Diff</th><th>Metrics</th>"
        "<th>Human review</th></tr></thead><tbody>"
        + "".join(body)
        + "</tbody></table></body></html>\n"
    )


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", required=True, type=Path, help="Explicit harness evidence JSON list")
    parser.add_argument("--output", required=True, type=Path, help="Local HTML dashboard path")
    args = parser.parse_args(argv)
    try:
        evidence_rows = json.loads(args.evidence.read_text(encoding="utf-8"))
        if not isinstance(evidence_rows, list) or not all(isinstance(row, dict) for row in evidence_rows):
            raise ValueError("Evidence JSON must be a list of objects")
        dashboard = dashboard_as_html(build_dashboard_rows(evidence_rows))
    except (FileNotFoundError, json.JSONDecodeError, ValueError) as ex:
        print(f"materialx_parity_dashboard.py: error: {ex}", file=sys.stderr)
        return 1
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(dashboard, encoding="utf-8", newline="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
