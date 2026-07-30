#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Create deterministic, fail-closed MaterialX template batch assignments."""

__all__ = (
    "build_batch_schedule",
    "build_template_candidates",
    "main",
    "schedule_as_json",
    "validate_batch_schedule",
)

import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any, Mapping, Sequence

import materialx_catalog
import materialx_nodedef_ledger
import materialx_semantic_registry


SCHEMA_VERSION = 1
BATCH_MINIMUM = 8
BATCH_MAXIMUM = 16
TEMPLATE_PRIORITY = {"direct_template": 0, "composed_template": 1}
CLASSIFICATION_METADATA_FIELDS = {"id", "classification", "next_action"}
ACTIVE_LAYERS = {"native_cycles", "hydra_ovrtx", "blender_authoring"}
BATCH_FIELDS = {
    "batch_id",
    "worker_id",
    "node_defs",
    "template_classification",
    "exception_budget",
    "focused_test_command",
    "generated_evidence_tier",
}
SCHEDULE_FIELDS = {"schema_version", "ledger_rows", "workers", "batches"}


def _healthy_workers(capacity: Mapping[str, Any]) -> list[str]:
    workers = capacity.get("healthy_workers") if isinstance(capacity, Mapping) else None
    if not isinstance(workers, list) or not workers:
        raise ValueError("Capacity state requires at least one healthy worker")
    result = []
    for worker in workers:
        if not isinstance(worker, Mapping) or set(worker) != {"id", "state"}:
            raise ValueError("healthy_workers entries must contain only id and state")
        worker_id = worker["id"]
        state = worker["state"]
        if not isinstance(worker_id, str) or not worker_id or not isinstance(state, str) or not state:
            raise ValueError("healthy_workers entries require non-empty id and state")
        if state != "active":
            raise ValueError(f"healthy_workers: {worker_id} is {state}, not active")
        result.append(worker_id)
    if len(result) != len(set(result)):
        raise ValueError("healthy_workers contains duplicate ids")
    return sorted(result)


def _semantic_registry(
    ledger_rows: Sequence[Mapping[str, Any]], registrations: Sequence[Mapping[str, Any]]
) -> dict[str, dict[str, Any]]:
    catalog = [
        {field: row[field] for field in ("id", "category", "types", "source")}
        for row in ledger_rows
    ]
    validated = materialx_semantic_registry.validate_registry(catalog, registrations)
    return {row["id"]: row for row in validated}


def _active_node_ids(active_manifests: Sequence[Mapping[str, Any]], ledger_ids: set[str]) -> set[str]:
    if not isinstance(active_manifests, Sequence) or isinstance(active_manifests, (str, bytes)):
        raise ValueError("active_manifests must be a sequence of manifests")
    active_ids = set()
    for manifest in active_manifests:
        if not isinstance(manifest, Mapping):
            raise ValueError("active_manifests entries must be mappings")
        layer = manifest.get("layer")
        node_defs = manifest.get("node_defs")
        if layer not in ACTIVE_LAYERS:
            raise ValueError("active manifest has unsupported layer")
        if not isinstance(node_defs, Sequence) or isinstance(node_defs, (str, bytes)) or not node_defs:
            raise ValueError("active manifest node_defs must be a non-empty list of NodeDefs")
        if not all(isinstance(node_id, str) and node_id for node_id in node_defs):
            raise ValueError("active manifest node_defs must be a non-empty list of NodeDefs")
        if len(node_defs) != len(set(node_defs)):
            raise ValueError("active manifest contains duplicate NodeDef")
        unknown = sorted(set(node_defs).difference(ledger_ids))
        if unknown:
            raise ValueError(f"active manifest references unknown ledger row: {', '.join(unknown)}")
        overlap = sorted(active_ids.intersection(node_defs))
        if overlap:
            raise ValueError(f"active manifest overlap for NodeDefs: {', '.join(overlap)}")
        active_ids.update(node_defs)
    return active_ids


def build_template_candidates(
    ledger: Mapping[str, Any],
    semantic_registry: Sequence[Mapping[str, Any]],
    classification_metadata: Sequence[Mapping[str, Any]],
    *,
    completed_ids: Sequence[str] = (),
    phase2_ids: Sequence[str] = (),
    active_manifests: Sequence[Mapping[str, Any]] = (),
) -> list[dict[str, Any]]:
    """Build candidates from remaining ledger rows annotated by template metadata."""
    materialx_nodedef_ledger.validate_ledger(ledger, expected_count=materialx_catalog.EXPECTED_NODEDEF_COUNT)
    ledger_ids = {row["id"] for row in ledger["rows"]}
    active_ids = _active_node_ids(active_manifests, ledger_ids)
    remaining_ids = set(materialx_nodedef_ledger.remaining_node_ids(
        ledger,
        completed_ids=completed_ids,
        phase2_ids=phase2_ids,
        active_ids=active_ids,
    ))
    registry = _semantic_registry(ledger["rows"], semantic_registry)
    if not isinstance(classification_metadata, Sequence) or isinstance(classification_metadata, (str, bytes)):
        raise ValueError("Classification metadata must be a list")
    candidates = []
    seen = set()
    for row in classification_metadata:
        if not isinstance(row, Mapping) or set(row) != CLASSIFICATION_METADATA_FIELDS:
            raise ValueError("Classification metadata entries must contain only id, classification, and next_action")
        node_id = row["id"]
        classification = row["classification"]
        next_action = row["next_action"]
        if not isinstance(node_id, str) or not node_id:
            raise ValueError("Classification metadata id must be a non-empty string")
        if node_id in seen:
            raise ValueError(f"Classification metadata contains duplicate id {node_id!r}")
        seen.add(node_id)
        if node_id not in ledger_ids:
            raise ValueError(f"Classification metadata references unknown ledger row {node_id!r}")
        if node_id not in remaining_ids:
            if node_id in completed_ids:
                raise ValueError(f"Classification metadata references completed NodeDef {node_id!r}")
            if node_id in phase2_ids:
                raise ValueError(f"Classification metadata references Phase-2 NodeDef {node_id!r}")
            raise ValueError(f"Classification metadata references active NodeDef {node_id!r}")
        if not isinstance(classification, str) or not isinstance(next_action, str):
            raise ValueError(f"Classification metadata {node_id!r} fields must be strings")
        if classification not in TEMPLATE_PRIORITY:
            continue
        if next_action != "template":
            raise ValueError(f"Classification metadata {node_id!r} must use next_action template")
        semantic = registry.get(node_id)
        if semantic is None:
            raise ValueError(f"Classification metadata {node_id!r} is missing semantic registry metadata")
        template = semantic["template"]
        candidates.append(
            {
                "id": node_id,
                "classification": classification,
                "semantic_template": template,
                "focused_test_command": f"cycles_test --gtest_filter=MaterialXSemantic.{template}",
                "generated_evidence_tier": "generated_semantic_template",
                "exception_budget": 0,
            }
        )
    return sorted(candidates, key=lambda item: (TEMPLATE_PRIORITY[item["classification"]], item["id"]))


def _partition(candidates: Sequence[Mapping[str, Any]]) -> list[list[Mapping[str, Any]]]:
    if len(candidates) < BATCH_MINIMUM:
        raise ValueError(f"Incomplete template batch has {len(candidates)} NodeDefs; require between 8 and 16")
    batches = []
    start = 0
    while len(candidates) - start > BATCH_MAXIMUM:
        remaining = len(candidates) - start
        size = BATCH_MAXIMUM
        if 0 < remaining - size < BATCH_MINIMUM:
            size = remaining - BATCH_MINIMUM
        batches.append(list(candidates[start:start + size]))
        start += size
    batches.append(list(candidates[start:]))
    return batches


def validate_batch_schedule(schedule: Mapping[str, Any], healthy_workers: Sequence[str]) -> None:
    """Reject malformed, overlapping, incomplete, or idle-worker batch plans."""
    if not isinstance(schedule, Mapping) or set(schedule) != SCHEDULE_FIELDS:
        raise ValueError("Batch schedule must contain only schema_version, ledger_rows, workers, and batches")
    if schedule["schema_version"] != SCHEMA_VERSION:
        raise ValueError(f"Batch schedule requires schema_version {SCHEMA_VERSION}")
    if not isinstance(schedule["ledger_rows"], int) or schedule["ledger_rows"] != materialx_catalog.EXPECTED_NODEDEF_COUNT:
        raise ValueError(f"Batch schedule requires {materialx_catalog.EXPECTED_NODEDEF_COUNT} validated ledger rows")
    expected_workers = sorted(healthy_workers)
    if schedule["workers"] != expected_workers:
        raise ValueError("Batch schedule workers do not match healthy workers")
    batches = schedule["batches"]
    if not isinstance(batches, list) or not batches:
        raise ValueError("Batch schedule requires at least one complete batch")

    assigned_nodes = set()
    assigned_workers = set()
    batch_ids = set()
    for batch in batches:
        if not isinstance(batch, Mapping):
            raise ValueError("Batch record must be an object")
        missing = sorted(BATCH_FIELDS.difference(batch))
        if missing:
            raise ValueError(f"Batch record is missing fields: {', '.join(missing)}")
        unexpected = sorted(set(batch).difference(BATCH_FIELDS))
        if unexpected:
            raise ValueError(f"Batch record uses unsupported fields: {', '.join(unexpected)}")
        batch_id = batch["batch_id"]
        worker_id = batch["worker_id"]
        node_defs = batch["node_defs"]
        if not isinstance(batch_id, str) or not batch_id or batch_id in batch_ids:
            raise ValueError("Batch records require unique non-empty batch_id values")
        batch_ids.add(batch_id)
        if worker_id not in expected_workers:
            raise ValueError(f"Batch {batch_id!r} has unknown worker ownership {worker_id!r}")
        assigned_workers.add(worker_id)
        if not isinstance(node_defs, list) or not BATCH_MINIMUM <= len(node_defs) <= BATCH_MAXIMUM:
            raise ValueError(f"Batch {batch_id!r} must contain between 8 and 16 NodeDefs")
        if not all(isinstance(node_id, str) and node_id for node_id in node_defs):
            raise ValueError(f"Batch {batch_id!r} NodeDefs must be non-empty strings")
        if len(node_defs) != len(set(node_defs)):
            raise ValueError(f"Batch {batch_id!r} contains overlapping NodeDefs")
        overlap = sorted(assigned_nodes.intersection(node_defs))
        if overlap:
            raise ValueError(f"Batch schedule contains overlap for NodeDefs: {', '.join(overlap)}")
        assigned_nodes.update(node_defs)
        if batch["template_classification"] not in TEMPLATE_PRIORITY:
            raise ValueError(f"Batch {batch_id!r} has unsupported template classification")
        if not isinstance(batch["exception_budget"], int) or isinstance(batch["exception_budget"], bool) or batch["exception_budget"] < 0:
            raise ValueError(f"Batch {batch_id!r} requires a non-negative exception_budget")
        for field in ("focused_test_command", "generated_evidence_tier"):
            if not isinstance(batch[field], str) or not batch[field]:
                raise ValueError(f"Batch {batch_id!r} requires a non-empty {field}")
    idle_workers = sorted(set(expected_workers).difference(assigned_workers))
    if idle_workers:
        raise ValueError(f"Batch schedule leaves idle healthy worker(s): {', '.join(idle_workers)}")


def build_batch_schedule(
    ledger: Mapping[str, Any],
    semantic_registry: Sequence[Mapping[str, Any]],
    classification_metadata: Sequence[Mapping[str, Any]],
    capacity: Mapping[str, Any],
    *,
    completed_ids: Sequence[str] = (),
    phase2_ids: Sequence[str] = (),
    active_manifests: Sequence[Mapping[str, Any]] = (),
) -> dict[str, Any]:
    """Assign remaining ledger NodeDefs without inferring classification metadata."""
    workers = _healthy_workers(capacity)
    candidates = build_template_candidates(
        ledger,
        semantic_registry,
        classification_metadata,
        completed_ids=completed_ids,
        phase2_ids=phase2_ids,
        active_manifests=active_manifests,
    )

    groups = defaultdict(list)
    for candidate in candidates:
        groups[
            (
                candidate["classification"],
                candidate["focused_test_command"],
                candidate["generated_evidence_tier"],
                candidate["exception_budget"],
            )
        ].append(candidate)

    raw_batches = []
    for key in sorted(groups, key=lambda item: (TEMPLATE_PRIORITY[item[0]], item[1:])):
        for node_group in _partition(groups[key]):
            raw_batches.append((key, node_group))

    batches = []
    for index, (key, node_group) in enumerate(raw_batches, start=1):
        classification, command, evidence_tier, exception_budget = key
        batches.append(
            {
                "batch_id": f"template-{classification.removesuffix('_template')}-{index:03d}",
                "worker_id": workers[(index - 1) % len(workers)],
                "node_defs": [candidate["id"] for candidate in node_group],
                "template_classification": classification,
                "exception_budget": exception_budget,
                "focused_test_command": command,
                "generated_evidence_tier": evidence_tier,
            }
        )

    schedule = {
        "schema_version": SCHEMA_VERSION,
        "ledger_rows": len(ledger["rows"]),
        "workers": workers,
        "batches": batches,
    }
    validate_batch_schedule(schedule, workers)
    return schedule


def schedule_as_json(schedule: Mapping[str, Any]) -> str:
    """Serialize a batch schedule deterministically for review and automation."""
    return json.dumps(schedule, indent=2, sort_keys=True) + "\n"


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ledger", type=Path, required=True)
    parser.add_argument("--semantic-registry", type=Path, required=True)
    parser.add_argument("--classification-metadata", type=Path, required=True)
    parser.add_argument("--capacity", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    try:
        ledger = json.loads(args.ledger.read_text(encoding="utf-8"))
        semantic_registry = json.loads(args.semantic_registry.read_text(encoding="utf-8"))
        classification_metadata = json.loads(args.classification_metadata.read_text(encoding="utf-8"))
        capacity = json.loads(args.capacity.read_text(encoding="utf-8"))
        schedule = build_batch_schedule(ledger, semantic_registry, classification_metadata, capacity)
    except (FileNotFoundError, json.JSONDecodeError, ValueError) as ex:
        print(f"materialx_batch_scheduler.py: error: {ex}", file=sys.stderr)
        return 1
    output = schedule_as_json(schedule)
    if args.output is None:
        sys.stdout.write(output)
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8", newline="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
