"""Deterministically classify every MaterialX NodeDef by explicit template manifest."""

from __future__ import annotations

import json
from collections.abc import Mapping, Sequence
from pathlib import Path
from typing import Any

import materialx_catalog

CLASSES = {"direct_template", "composed_template", "renderer_specific", "explicit_rejection"}
DEFAULT_MANIFEST = Path(__file__).with_name("materialx_template_manifest.json")


def load_manifest(path: Path = DEFAULT_MANIFEST) -> dict[str, dict[str, str]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError("Template manifest must be an object keyed by NodeDef id")
    return data


def build_backlog(catalog: Sequence[Mapping[str, Any]], manifest: Mapping[str, Mapping[str, str]] | None = None) -> list[dict[str, Any]]:
    """Return exactly one conservative classification row for each catalog NodeDef."""
    manifest = {} if manifest is None else manifest
    ids = [str(row["id"]) for row in catalog]
    if len(ids) != len(set(ids)):
        raise ValueError("Catalog contains duplicate NodeDef ids")
    unknown = sorted(set(manifest) - set(ids))
    if unknown:
        raise ValueError(f"Manifest references unknown NodeDefs: {', '.join(unknown)}")
    rows: list[dict[str, Any]] = []
    for node_id in sorted(ids):
        entry = dict(manifest.get(node_id, {}))
        classification = entry.pop("classification", "renderer_specific")
        if classification not in CLASSES:
            raise ValueError(f"Unknown classification for {node_id}: {classification}")
        if entry:
            raise ValueError(f"Unknown manifest fields for {node_id}: {', '.join(sorted(entry))}")
        rows.append({"id": node_id, "classification": classification,
                     "next_action": "classify" if classification == "renderer_specific" else "template"})
    return rows


def backlog_as_json(rows: Sequence[Mapping[str, Any]]) -> str:
    return json.dumps(list(rows), indent=2, sort_keys=True) + "\n"


if __name__ == "__main__":
    print(backlog_as_json(build_backlog(materialx_catalog.build_catalog(), load_manifest())))
