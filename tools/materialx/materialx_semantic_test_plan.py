#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Build deterministic parameterized test families from semantic registrations."""

__all__ = (
    "build_family_test_plan",
    "main",
    "plan_as_json",
)

import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any, Mapping, Sequence

import materialx_semantic_registry


def build_family_test_plan(
    catalog: Sequence[Mapping[str, Any]], registrations: Sequence[Mapping[str, Any]]
) -> list[dict[str, Any]]:
    """Return one shared parameterized plan for each registered semantic template."""
    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for registration in materialx_semantic_registry.validate_registry(catalog, registrations):
        grouped[registration["template"]].append(registration)

    families = []
    for template in sorted(grouped):
        rows = grouped[template]
        registered_ids = sorted(row["id"] for row in rows)
        broadcast_ids = sorted(row["id"] for row in rows if row["broadcast"])
        families.append(
            {
                "template": template,
                "registered_ids": registered_ids,
                "cases": [
                    {"input_class": "literal_linked", "ids": registered_ids},
                    {"input_class": "scalar_broadcast", "ids": broadcast_ids},
                    {"input_class": "type_rejection", "ids": registered_ids},
                ],
                "golden_fixture": {
                    "representative_id": registered_ids[0],
                    "cpu": "required",
                    "gpu": "required",
                },
            }
        )
    return families


def plan_as_json(plan: Sequence[Mapping[str, Any]]) -> str:
    """Serialize semantic test-plan families deterministically."""
    return json.dumps(list(plan), indent=2, sort_keys=True) + "\n"


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--registry", type=Path, default=materialx_semantic_registry.DEFAULT_REGISTRY)
    parser.add_argument("--output", type=Path, default=None)
    args = parser.parse_args(argv)
    try:
        catalog = json.loads(args.catalog.read_text(encoding="utf-8"))
        registrations = materialx_semantic_registry.load_registry(args.registry)
        plan = build_family_test_plan(catalog, registrations)
    except (FileNotFoundError, json.JSONDecodeError, ValueError) as ex:
        print(f"materialx_semantic_test_plan.py: error: {ex}", file=sys.stderr)
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
