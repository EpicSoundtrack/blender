#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Build deterministic golden-render fixture manifests for MaterialX NodeDefs."""

__all__ = (
    "build_manifest",
    "main",
    "manifest_as_json",
)

import argparse
import json
import sys
from pathlib import PurePosixPath
from typing import Any, Mapping, Sequence


SCHEMA_VERSION = 1
FIXTURE_FIELDS = (
    "id",
    "nodedefs",
    "reference_image",
    "current_image",
    "render_settings",
    "metrics",
    "reviewer_status",
)
MANIFEST_FIXTURE_FIELDS = FIXTURE_FIELDS + ("harness_test_ids",)
REVIEWER_STATUSES = ("pending", "approved", "rejected", "needs_review")


def _relative_image_path(value: object, field: str) -> str:
    if not isinstance(value, str) or not value:
        raise ValueError(f"Fixture {field} must be a non-empty string")
    path = PurePosixPath(value.replace("\\", "/"))
    if path.is_absolute() or ".." in path.parts:
        raise ValueError(f"Fixture {field} must be a relative path without '..'")
    return path.as_posix()


def _canonical_fixture(fixture: Mapping[str, Any]) -> dict[str, Any]:
    missing = sorted(set(FIXTURE_FIELDS).difference(fixture))
    if missing:
        raise ValueError(f"Fixture is missing fields: {', '.join(missing)}")
    unexpected = sorted(set(fixture).difference(FIXTURE_FIELDS))
    if unexpected:
        raise ValueError(f"Fixture uses unsupported fields: {', '.join(unexpected)}")

    fixture_id = fixture["id"]
    if not isinstance(fixture_id, str) or not fixture_id:
        raise ValueError("Fixture id must be a non-empty string")
    nodedefs = fixture["nodedefs"]
    if (not isinstance(nodedefs, list) or not nodedefs or
            not all(isinstance(nodedef, str) and nodedef.startswith("ND_") for nodedef in nodedefs)):
        raise ValueError("Fixture nodedefs must be a non-empty list of NodeDef IDs")
    if len(set(nodedefs)) != len(nodedefs):
        raise ValueError(f"Fixture {fixture_id!r} contains duplicate NodeDefs")
    if not isinstance(fixture["render_settings"], Mapping):
        raise ValueError("Fixture render_settings must be an object")
    if not isinstance(fixture["metrics"], Mapping):
        raise ValueError("Fixture metrics must be an object")
    reviewer_status = fixture["reviewer_status"]
    if reviewer_status not in REVIEWER_STATUSES:
        raise ValueError(
            f"Fixture reviewer_status must be one of: {', '.join(REVIEWER_STATUSES)}")

    nodedefs = sorted(nodedefs)
    return {
        "id": fixture_id,
        "nodedefs": nodedefs,
        "reference_image": _relative_image_path(fixture["reference_image"], "reference_image"),
        "current_image": _relative_image_path(fixture["current_image"], "current_image"),
        "render_settings": dict(fixture["render_settings"]),
        "metrics": dict(fixture["metrics"]),
        "reviewer_status": reviewer_status,
        # Matches materialx_harness_plan.RENDERERS' native_cycles suite convention.
        "harness_test_ids": [f"MaterialXNativeCycles.{nodedef}" for nodedef in nodedefs],
    }


def build_manifest(fixtures: Sequence[Mapping[str, Any]]) -> dict[str, Any]:
    """Return a canonical golden-fixture manifest for explicit fixture rows."""
    if not isinstance(fixtures, Sequence) or isinstance(fixtures, (str, bytes)):
        raise ValueError("Fixtures must be a list of objects")
    canonical = [_canonical_fixture(fixture) for fixture in fixtures]
    fixture_ids = [fixture["id"] for fixture in canonical]
    if len(set(fixture_ids)) != len(fixture_ids):
        raise ValueError("Manifest contains duplicate fixture IDs")
    return {"schema_version": SCHEMA_VERSION, "fixtures": sorted(canonical, key=lambda fixture: fixture["id"])}


def manifest_as_json(manifest: Mapping[str, Any]) -> str:
    """Serialize a canonical manifest deterministically."""
    return json.dumps(manifest, indent=2, sort_keys=True) + "\n"


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=str, required=True, help="JSON list of fixture objects")
    parser.add_argument("--output", type=str, default=None, help="Output manifest JSON path")
    args = parser.parse_args(argv)

    try:
        fixtures = json.loads(open(args.input, encoding="utf-8").read())
        manifest = build_manifest(fixtures)
    except (OSError, json.JSONDecodeError, ValueError) as ex:
        print(f"materialx_golden_manifest.py: error: {ex}", file=sys.stderr)
        return 1

    output = manifest_as_json(manifest)
    if args.output is None:
        sys.stdout.write(output)
    else:
        with open(args.output, "w", encoding="utf-8", newline="") as handle:
            handle.write(output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
