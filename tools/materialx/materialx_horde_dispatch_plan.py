#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Create deterministic, non-executing Horde dispatch guardrail plans."""

__all__ = (
    "build_dispatch_plan",
    "main",
    "plan_as_json",
    "validate_credential_file",
)

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any, Mapping, Sequence

from materialx_velocity_manifest import validate_batch_manifest


REQUIRED_CREDENTIAL_KEY = "NVIDIA_API_KEY"


def validate_credential_file(credential_file: str | Path) -> dict[str, str]:
    """Validate required credential-file structure without exposing its value."""
    path = Path(credential_file)
    try:
        content = path.read_text(encoding="utf-8")
    except OSError as ex:
        raise ValueError(f"Credential file is unavailable: {path}") from ex

    raw_tokens = content.split()
    if len(raw_tokens) == 3 and all("=" not in token for token in raw_tokens):
        return {
            "path": str(path),
            "required_key": REQUIRED_CREDENTIAL_KEY,
            "format": "raw_three_token",
            "status": "valid",
        }

    key_count = 0
    for line in content.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if "=" not in stripped:
            raise ValueError("Credential file entries must use KEY=VALUE structure")
        key, value = stripped.split("=", 1)
        if not key or not value:
            raise ValueError("Credential file entries must use KEY=VALUE structure")
        if key == REQUIRED_CREDENTIAL_KEY:
            key_count += 1

    if key_count != 1:
        raise ValueError("Credential file must contain exactly one NVIDIA_API_KEY entry")
    return {
        "path": str(path),
        "required_key": REQUIRED_CREDENTIAL_KEY,
        "format": "assignment",
        "status": "valid",
    }


def build_dispatch_plan(
    manifests: Sequence[Mapping[str, Any]],
    credential_file: str | Path,
    *,
    registered_families: Mapping[str, Any],
) -> dict[str, Any]:
    """Return a deterministic plan; it performs neither remote work nor credential writes."""
    if isinstance(manifests, (str, bytes)) or not isinstance(manifests, Sequence) or not manifests:
        raise ValueError("Dispatch plan requires at least one batch manifest")
    assignments = sorted(
        (
            validate_batch_manifest(manifest, registered_families=registered_families)
            for manifest in manifests
        ),
        key=lambda assignment: assignment["batch_id"],
    )

    batch_ids: set[str] = set()
    node_defs: set[str] = set()
    files: set[str] = set()
    integration_bases: set[str] = set()
    for assignment in assignments:
        batch_id = assignment["batch_id"]
        if batch_id in batch_ids:
            raise ValueError(f"duplicate batch_id ownership: {batch_id}")
        overlapping_node_defs = node_defs.intersection(assignment["node_defs"])
        if overlapping_node_defs:
            raise ValueError(f"duplicate NodeDef ownership: {sorted(overlapping_node_defs)}")
        overlapping_files = files.intersection(assignment["files_allowlist"])
        if overlapping_files:
            raise ValueError(f"duplicate file ownership: {sorted(overlapping_files)}")
        batch_ids.add(batch_id)
        node_defs.update(assignment["node_defs"])
        files.update(assignment["files_allowlist"])
        integration_bases.add(assignment["integration_base_sha"])
    if len(integration_bases) != 1:
        raise ValueError("all integration_base_sha values must match")

    workers = sorted({
        worker
        for assignment in assignments
        for worker in assignment["roles"].values()
    })
    worker_tasks = {worker: [] for worker in workers}
    for assignment in assignments:
        for role, worker in assignment["roles"].items():
            worker_tasks[worker].append({"batch_id": assignment["batch_id"], "role": role})
    for tasks in worker_tasks.values():
        tasks.sort(key=lambda task: (task["batch_id"], task["role"]))

    credential_path = str(credential_file)
    if not credential_path:
        raise ValueError("Credential file path must be non-empty")
    failure_alert = {
        "batch_ids": sorted(batch_ids),
        "kind": "capacity_alert",
        "timing": "immediate",
        "workers": workers,
    }
    dispatch_id = "dispatch-" + hashlib.sha256(
        json.dumps(sorted(batch_ids), separators=(",", ":")).encode("utf-8")
    ).hexdigest()[:24]
    return {
        "schema_version": 2,
        "dispatch_id": dispatch_id,
        "assignments": assignments,
        "workers": workers,
        "worker_tasks": worker_tasks,
        "credential_file": credential_path,
        "required_steps": [
            {
                "id": "structural_credential_validation",
                "kind": "credential_validation",
                "required_key": REQUIRED_CREDENTIAL_KEY,
                "value_handling": "never_emit",
            },
            {
                "id": "no_write_probe",
                "kind": "connectivity_probe",
                "write_policy": "forbidden",
            },
            {
                "id": "persist_nvidia_api_key",
                "kind": "environment_assignment",
                "variable": REQUIRED_CREDENTIAL_KEY,
                "persistence": "exactly_once",
            },
            {
                "id": "hermes_process_check",
                "kind": "process_check",
                "process": "Hermes",
                "scope": "real_process",
                "required": True,
            },
            {
                "id": "immediate_capacity_alert",
                "kind": "capacity_alert",
                "on": "failure",
                "record": failure_alert,
            },
        ],
        "failure_alert": failure_alert,
    }


def plan_as_json(plan: Mapping[str, Any]) -> str:
    """Serialize a dispatch plan deterministically."""
    return json.dumps(plan, indent=2, sort_keys=True) + "\n"


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--credential-file", type=Path, required=True)
    parser.add_argument("--batch-manifests", type=Path, required=True, help="Batch Manifest v2 JSON array")
    parser.add_argument("--registered-families", type=Path, required=True, help="Registered family contract JSON mapping")
    parser.add_argument("--output", type=Path, default=None)
    args = parser.parse_args(argv)

    try:
        manifests = json.loads(args.batch_manifests.read_text(encoding="utf-8"))
        registered_families = json.loads(args.registered_families.read_text(encoding="utf-8"))
        plan = build_dispatch_plan(
            manifests,
            args.credential_file,
            registered_families=registered_families,
        )
    except (OSError, json.JSONDecodeError, ValueError) as ex:
        print(f"materialx_horde_dispatch_plan.py: error: {ex}", file=sys.stderr)
        return 1

    output = plan_as_json(plan)
    if args.output is None:
        sys.stdout.write(output)
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8", newline="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
