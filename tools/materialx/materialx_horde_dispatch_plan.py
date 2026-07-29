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
import json
import sys
import re
from pathlib import Path
from typing import Any, Mapping, Sequence


REQUIRED_CREDENTIAL_KEY = "NVIDIA_API_KEY"
_BATCH_ID_PATTERN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")


def _canonical_workers(workers: Sequence[str]) -> list[str]:
    if not isinstance(workers, Sequence) or isinstance(workers, (str, bytes)) or not workers:
        raise ValueError("Dispatch plan requires at least one worker")
    if not all(isinstance(worker, str) and worker and worker.strip() == worker for worker in workers):
        raise ValueError("Worker IDs must be non-empty strings without surrounding whitespace")
    if len(set(workers)) != len(workers):
        raise ValueError("Dispatch plan contains duplicate worker IDs")
    return sorted(workers)


def _canonical_manifest(batch_manifest: Mapping[str, Any]) -> dict[str, Any]:
    if not isinstance(batch_manifest, Mapping):
        raise ValueError("Batch manifest must be an object")
    batch_id = batch_manifest.get("batch_id")
    if not isinstance(batch_id, str) or not _BATCH_ID_PATTERN.fullmatch(batch_id):
        raise ValueError("Batch manifest batch_id must contain only letters, digits, dot, underscore, or hyphen")
    try:
        return json.loads(json.dumps(dict(batch_manifest), sort_keys=True))
    except (TypeError, ValueError) as ex:
        raise ValueError("Batch manifest must contain JSON-compatible values") from ex


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
    workers: Sequence[str], credential_file: str | Path, batch_manifest: Mapping[str, Any]
) -> dict[str, Any]:
    """Return a deterministic plan; it performs neither remote work nor credential writes."""
    canonical_workers = _canonical_workers(workers)
    credential_path = str(credential_file)
    if not credential_path:
        raise ValueError("Credential file path must be non-empty")
    manifest = _canonical_manifest(batch_manifest)
    failure_alert = {
        "batch_id": manifest["batch_id"],
        "kind": "capacity_alert",
        "timing": "immediate",
        "workers": canonical_workers,
    }
    return {
        "schema_version": 1,
        "workers": canonical_workers,
        "credential_file": credential_path,
        "batch_manifest": manifest,
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
    parser.add_argument("--worker", action="append", required=True, help="Worker identifier; repeat per worker")
    parser.add_argument("--credential-file", type=Path, required=True)
    parser.add_argument("--batch-manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=None)
    args = parser.parse_args(argv)

    try:
        manifest = json.loads(args.batch_manifest.read_text(encoding="utf-8"))
        plan = build_dispatch_plan(args.worker, args.credential_file, manifest)
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
