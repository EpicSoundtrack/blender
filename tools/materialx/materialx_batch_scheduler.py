#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Create deterministic, fail-closed MaterialX Batch Manifest v2 schedules."""

__all__ = ("build_batch_schedule", "build_template_candidates", "main", "schedule_as_json", "validate_batch_schedule")

import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any, Mapping, Sequence

import materialx_catalog
import materialx_nodedef_ledger
import materialx_semantic_registry
from materialx_velocity_manifest import EXPECTED_HORDE_WORKERS, LAYERS, REQUIRED_ROLES, validate_batch_manifest


SCHEMA_VERSION = 2
BATCH_MINIMUM = 8
BATCH_MAXIMUM = 16
TEMPLATE_PRIORITY = {"direct_template": 0, "composed_template": 1}
CLASSIFICATION_METADATA_FIELDS = {"id", "classification", "next_action"}
SCHEDULE_FIELDS = {"schema_version", "ledger_rows", "assignments"}
EXCEPTION_FIELDS = {
    "batch_id", "batch_kind", "family_id", "template_signature", "node_defs",
    "focused_test_commands", "generated_evidence_tier", "exception_budget", "red_test", "approval_record",
}
SHA_PATTERN = re.compile(r"^[0-9a-f]{40}$")


def _healthy_workers(capacity: Mapping[str, Any]) -> tuple[str, ...]:
    workers = capacity.get("healthy_workers") if isinstance(capacity, Mapping) else None
    if not isinstance(workers, list):
        raise ValueError("capacity must declare exactly the five expected Horde workers")
    found: list[str] = []
    for worker in workers:
        if not isinstance(worker, Mapping) or set(worker) != {"id", "state"}:
            raise ValueError("capacity healthy_workers entries must contain only id and state")
        if not isinstance(worker["id"], str) or worker["state"] != "active":
            raise ValueError("capacity requires all expected Horde workers active")
        found.append(worker["id"])
    if len(found) != len(set(found)) or set(found) != set(EXPECTED_HORDE_WORKERS) or len(found) != len(EXPECTED_HORDE_WORKERS):
        raise ValueError("capacity must contain exactly the five expected Horde workers")
    return EXPECTED_HORDE_WORKERS


def _semantic_registry(ledger_rows: Sequence[Mapping[str, Any]], registrations: Sequence[Mapping[str, Any]]) -> dict[str, dict[str, Any]]:
    catalog = [{field: row[field] for field in ("id", "category", "types", "source")} for row in ledger_rows]
    return {row["id"]: row for row in materialx_semantic_registry.validate_registry(catalog, registrations)}


def _active_node_ids(active_manifests: Sequence[Mapping[str, Any]], ledger_ids: set[str]) -> set[str]:
    if not isinstance(active_manifests, Sequence) or isinstance(active_manifests, (str, bytes)):
        raise ValueError("active_manifests must be a sequence of manifests")
    active_ids: set[str] = set()
    for manifest in active_manifests:
        if not isinstance(manifest, Mapping) or manifest.get("layer") not in LAYERS:
            raise ValueError("active manifest has unsupported layer")
        node_defs = manifest.get("node_defs")
        if not isinstance(node_defs, Sequence) or isinstance(node_defs, (str, bytes)) or not node_defs or any(not isinstance(node_id, str) or not node_id for node_id in node_defs):
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


def build_template_candidates(ledger: Mapping[str, Any], semantic_registry: Sequence[Mapping[str, Any]], classification_metadata: Sequence[Mapping[str, Any]], *, completed_ids: Sequence[str] = (), phase2_ids: Sequence[str] = (), active_manifests: Sequence[Mapping[str, Any]] = ()) -> list[dict[str, Any]]:
    """Build ledger-positive, semantically complete candidates for family batching."""
    materialx_nodedef_ledger.validate_ledger(ledger, expected_count=materialx_catalog.EXPECTED_NODEDEF_COUNT)
    ledger_ids = {row["id"] for row in ledger["rows"]}
    active_ids = _active_node_ids(active_manifests, ledger_ids)
    remaining_ids = set(materialx_nodedef_ledger.remaining_node_ids(ledger, completed_ids=completed_ids, phase2_ids=phase2_ids, active_ids=active_ids))
    registry = _semantic_registry(ledger["rows"], semantic_registry)
    if not isinstance(classification_metadata, Sequence) or isinstance(classification_metadata, (str, bytes)):
        raise ValueError("classification metadata must be a list")
    template_ids = [row["id"] for row in ledger["rows"] if row["id"] in remaining_ids and row["next_action"] == "template"]
    metadata_by_id: dict[str, Mapping[str, Any]] = {}
    for row in classification_metadata:
        if not isinstance(row, Mapping) or set(row) != CLASSIFICATION_METADATA_FIELDS:
            raise ValueError("classification metadata entries must contain only id, classification, and next_action")
        node_id = row["id"]
        if not isinstance(node_id, str) or not node_id or node_id in metadata_by_id:
            raise ValueError("classification metadata requires unique non-empty ids")
        if node_id not in ledger_ids:
            raise ValueError(f"classification metadata references unknown ledger row {node_id!r}")
        if node_id not in remaining_ids:
            raise ValueError(f"classification metadata references non-remaining NodeDef {node_id!r}")
        if node_id not in template_ids:
            raise ValueError(f"classification metadata references non-template ledger row {node_id!r}")
        if row["classification"] not in TEMPLATE_PRIORITY or row["next_action"] != "template":
            raise ValueError(f"classification metadata {node_id!r} has unsupported template classification")
        metadata_by_id[node_id] = row
    missing = sorted(set(template_ids).difference(metadata_by_id))
    if missing:
        raise ValueError(f"ledger template NodeDefs are missing classification metadata: {', '.join(missing)}")
    candidates: list[dict[str, Any]] = []
    for node_id in template_ids:
        semantic = registry.get(node_id)
        if semantic is None:
            raise ValueError(f"ledger template NodeDef {node_id!r} is missing semantic registry metadata")
        if "family_id" not in semantic or "template_signature" not in semantic:
            raise ValueError(f"ledger template NodeDef {node_id!r} is missing schedulable family metadata")
        candidates.append({
            "id": node_id, "classification": metadata_by_id[node_id]["classification"],
            "family_id": semantic["family_id"], "template_signature": semantic["template_signature"],
            "focused_test_commands": [f"cycles_test --gtest_filter=MaterialXSemantic.{semantic['template']}"],
            "generated_evidence_tier": "generated_semantic_template",
        })
    return sorted(candidates, key=lambda item: (TEMPLATE_PRIORITY[item["classification"]], item["family_id"], item["id"]))


def _partition(candidates: Sequence[Mapping[str, Any]]) -> list[list[Mapping[str, Any]]]:
    if len(candidates) < BATCH_MINIMUM:
        raise ValueError(f"incomplete family queue has {len(candidates)} NodeDefs; require between 8 and 16")
    result: list[list[Mapping[str, Any]]] = []
    start = 0
    while len(candidates) - start > BATCH_MAXIMUM:
        size = BATCH_MAXIMUM
        if 0 < len(candidates) - start - size < BATCH_MINIMUM:
            size = len(candidates) - start - BATCH_MINIMUM
        result.append(list(candidates[start:start + size]))
        start += size
    result.append(list(candidates[start:]))
    return result


def _require_sha_map(probed_worker_shas: Mapping[str, Any], integration_base_sha: str) -> dict[str, str]:
    if not isinstance(integration_base_sha, str) or not SHA_PATTERN.fullmatch(integration_base_sha):
        raise ValueError("integration_base_sha must be a 40-hex SHA")
    if not isinstance(probed_worker_shas, Mapping) or set(probed_worker_shas) != set(EXPECTED_HORDE_WORKERS):
        raise ValueError("probed_worker_shas must contain all five expected Horde workers")
    result = {}
    for worker in EXPECTED_HORDE_WORKERS:
        source_sha = probed_worker_shas[worker]
        if not isinstance(source_sha, str) or not SHA_PATTERN.fullmatch(source_sha):
            raise ValueError(f"probed_worker_shas[{worker}] must be a 40-hex SHA")
        if source_sha != integration_base_sha:
            raise ValueError(f"probed_worker_shas[{worker}] must equal integration_base_sha")
        result[worker] = source_sha
    return result


def _require_assignments(role_allocations: Mapping[str, Any], files_allowlists: Mapping[str, Any]) -> tuple[dict[str, dict[str, str]], dict[str, list[str]]]:
    if not isinstance(role_allocations, Mapping) or set(role_allocations) != set(EXPECTED_HORDE_WORKERS):
        raise ValueError("role_allocations must contain every primary Horde worker")
    if not isinstance(files_allowlists, Mapping) or set(files_allowlists) != set(EXPECTED_HORDE_WORKERS):
        raise ValueError("files_allowlists must contain every primary Horde worker")
    roles: dict[str, dict[str, str]] = {}
    allowlists: dict[str, list[str]] = {}
    seen_files: set[str] = set()
    for worker in EXPECTED_HORDE_WORKERS:
        allocation = role_allocations[worker]
        if not isinstance(allocation, Mapping) or set(allocation) != REQUIRED_ROLES or any(not isinstance(value, str) or value not in EXPECTED_HORDE_WORKERS for value in allocation.values()):
            raise ValueError(f"role allocation for {worker} must contain the three Horde roles")
        if allocation["implementation"] != worker:
            raise ValueError(f"role allocation key {worker} must equal roles implementation")
        if len(set(allocation.values())) != len(REQUIRED_ROLES):
            raise ValueError(f"role allocation for {worker} must use independent workers")
        files = files_allowlists[worker]
        if not isinstance(files, Sequence) or isinstance(files, (str, bytes)) or not files or any(not isinstance(path, str) or not path for path in files) or len(files) != len(set(files)):
            raise ValueError(f"files allowlist for {worker} must be a non-empty unique list")
        if seen_files.intersection(files):
            raise ValueError("files allowlists must be pairwise disjoint")
        seen_files.update(files)
        roles[worker] = {role: allocation[role] for role in sorted(REQUIRED_ROLES)}
        allowlists[worker] = sorted(files)
    return roles, allowlists


def _exception_cores(complex_exceptions: Sequence[Mapping[str, Any]], ledger_ids: set[str], unavailable_ids: set[str]) -> list[dict[str, Any]]:
    if not isinstance(complex_exceptions, Sequence) or isinstance(complex_exceptions, (str, bytes)):
        raise ValueError("complex_exceptions must be a sequence")
    result: list[dict[str, Any]] = []
    used_ids: set[str] = set()
    for record in complex_exceptions:
        if not isinstance(record, Mapping) or set(record) != EXCEPTION_FIELDS:
            raise ValueError("complex exception must contain all non-scheduler manifest fields")
        core = dict(record)
        if core["batch_kind"] != "complex_exception":
            raise ValueError("complex exception batch_kind must be complex_exception")
        node_defs = core["node_defs"]
        if not isinstance(node_defs, Sequence) or isinstance(node_defs, (str, bytes)) or not 1 <= len(node_defs) <= 7 or any(not isinstance(node_id, str) or not node_id for node_id in node_defs) or len(node_defs) != len(set(node_defs)):
            raise ValueError("complex exception must contain 1-7 unique NodeDefs")
        unknown = set(node_defs).difference(ledger_ids)
        if unknown or set(node_defs).intersection(unavailable_ids) or set(node_defs).intersection(used_ids):
            raise ValueError("complex exception NodeDefs must be ledger-positive and unowned")
        used_ids.update(node_defs)
        result.append(core)
    return result


def validate_batch_schedule(schedule: Mapping[str, Any], *, registered_families: Sequence[str]) -> None:
    """Reject anything other than one valid manifest for each expected primary worker."""
    if not isinstance(schedule, Mapping) or set(schedule) != SCHEDULE_FIELDS:
        raise ValueError("batch schedule must contain only schema_version, ledger_rows, and assignments")
    if schedule["schema_version"] != SCHEMA_VERSION:
        raise ValueError("batch schedule requires schema_version 2")
    if schedule["ledger_rows"] != materialx_catalog.EXPECTED_NODEDEF_COUNT:
        raise ValueError(f"batch schedule requires {materialx_catalog.EXPECTED_NODEDEF_COUNT} validated ledger rows")
    assignments = schedule["assignments"]
    if not isinstance(assignments, Mapping) or set(assignments) != set(EXPECTED_HORDE_WORKERS) or len(assignments) != len(EXPECTED_HORDE_WORKERS):
        raise ValueError("schedule requires exactly five dispatchable assignments")
    node_ids: set[str] = set()
    files: set[str] = set()
    batch_ids: set[str] = set()
    for worker in EXPECTED_HORDE_WORKERS:
        assignment = validate_batch_manifest(assignments[worker], registered_families=registered_families)
        if assignment["roles"]["implementation"] != worker:
            raise ValueError("assignment key must equal roles implementation")
        if assignment["batch_id"] in batch_ids:
            raise ValueError("schedule contains duplicate batch_id")
        if node_ids.intersection(assignment["node_defs"]):
            raise ValueError("schedule contains overlapping NodeDefs")
        if files.intersection(assignment["files_allowlist"]):
            raise ValueError("schedule contains overlapping files allowlists")
        batch_ids.add(assignment["batch_id"])
        node_ids.update(assignment["node_defs"])
        files.update(assignment["files_allowlist"])


def build_batch_schedule(ledger: Mapping[str, Any], semantic_registry: Sequence[Mapping[str, Any]], classification_metadata: Sequence[Mapping[str, Any]], capacity: Mapping[str, Any], *, integration_base_sha: str, probed_worker_shas: Mapping[str, Any], layer: str, role_allocations: Mapping[str, Any], files_allowlists: Mapping[str, Any], completed_ids: Sequence[str] = (), phase2_ids: Sequence[str] = (), active_manifests: Sequence[Mapping[str, Any]] = (), complex_exceptions: Sequence[Mapping[str, Any]] = ()) -> dict[str, Any]:
    """Emit a complete v2 five-worker dispatch window from ledger-positive work."""
    _healthy_workers(capacity)
    if layer not in LAYERS:
        raise ValueError("layer is unsupported")
    source_shas = _require_sha_map(probed_worker_shas, integration_base_sha)
    roles, allowlists = _require_assignments(role_allocations, files_allowlists)
    candidates = build_template_candidates(ledger, semantic_registry, classification_metadata, completed_ids=completed_ids, phase2_ids=phase2_ids, active_manifests=active_manifests)
    registry = _semantic_registry(ledger["rows"], semantic_registry)
    registered_families = sorted({row["family_id"] for row in registry.values() if "family_id" in row})
    groups: dict[tuple[str, str, str, str, str], list[dict[str, Any]]] = defaultdict(list)
    for candidate in candidates:
        groups[(candidate["family_id"], json.dumps(candidate["template_signature"], sort_keys=True, separators=(",", ":")), candidate["classification"], candidate["focused_test_commands"][0], candidate["generated_evidence_tier"])].append(candidate)
    cores: list[dict[str, Any]] = []
    for key in sorted(groups, key=lambda item: (TEMPLATE_PRIORITY[item[2]], item)):
        family_id, _, _, command, evidence_tier = key
        for index, group in enumerate(_partition(groups[key]), start=1):
            cores.append({
                "batch_id": f"family-{family_id}-{index:03d}", "batch_kind": "family", "family_id": family_id,
                "template_signature": group[0]["template_signature"], "node_defs": [candidate["id"] for candidate in group],
                "focused_test_commands": [command], "generated_evidence_tier": evidence_tier,
                "exception_budget": 0, "red_test": "", "approval_record": "",
            })
    ledger_ids = {row["id"] for row in ledger["rows"]}
    unavailable_ids = set(completed_ids).union(phase2_ids).union(_active_node_ids(active_manifests, ledger_ids))
    exceptions = _exception_cores(complex_exceptions, ledger_ids, unavailable_ids)
    if cores and len(exceptions) > 1:
        raise ValueError("more than one complex exception is not allowed while normal family work remains queued")
    cores.extend(exceptions)
    if len(cores) != len(EXPECTED_HORDE_WORKERS):
        raise ValueError(f"queue has {len(cores)} dispatchable manifests; requires exactly five")
    assignments: dict[str, dict[str, Any]] = {}
    for worker, core in zip(EXPECTED_HORDE_WORKERS, cores, strict=True):
        assignments[worker] = {
            **core, "schema_version": SCHEMA_VERSION, "layer": layer, "integration_base_sha": integration_base_sha,
            "worker_source_sha": source_shas[worker], "roles": roles[worker], "files_allowlist": allowlists[worker],
        }
    schedule = {"schema_version": SCHEMA_VERSION, "ledger_rows": len(ledger["rows"]), "assignments": assignments}
    validate_batch_schedule(schedule, registered_families=registered_families)
    return schedule


def schedule_as_json(schedule: Mapping[str, Any]) -> str:
    return json.dumps(schedule, indent=2, sort_keys=True) + "\n"


def _node_id_array(value: Any, name: str) -> list[str]:
    if not isinstance(value, list) or any(not isinstance(node_id, str) or not node_id for node_id in value):
        raise ValueError(f"{name} must be a JSON array of non-empty NodeDef ids")
    return value


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("ledger", "semantic-registry", "classification-metadata", "capacity", "completed-ids", "phase2-ids", "active-manifests", "probed-worker-shas", "role-allocations", "files-allowlists"):
        parser.add_argument(f"--{option}", type=Path, required=True)
    parser.add_argument("--integration-base-sha", required=True)
    parser.add_argument("--layer", required=True)
    parser.add_argument("--complex-exceptions", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    try:
        read = lambda path: json.loads(path.read_text(encoding="utf-8"))
        schedule = build_batch_schedule(
            read(args.ledger), read(args.semantic_registry), read(args.classification_metadata), read(args.capacity),
            integration_base_sha=args.integration_base_sha, probed_worker_shas=read(args.probed_worker_shas), layer=args.layer,
            role_allocations=read(args.role_allocations), files_allowlists=read(args.files_allowlists),
            completed_ids=_node_id_array(read(args.completed_ids), "completed_ids"), phase2_ids=_node_id_array(read(args.phase2_ids), "phase2_ids"),
            active_manifests=read(args.active_manifests), complex_exceptions=[] if args.complex_exceptions is None else read(args.complex_exceptions),
        )
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
