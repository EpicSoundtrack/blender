"""Validated, renderer-neutral MaterialX semantic template registry."""
from __future__ import annotations

import json
from collections.abc import Mapping, Sequence
from pathlib import Path
from typing import Any

TEMPLATES = {
    "unary_componentwise", "binary_componentwise", "scalar_broadcast", "min_max_clamp",
    "remap", "mix_subtract", "conversion", "compose_separate",
}
DEFAULT_REGISTRY = Path(__file__).with_name("materialx_semantic_registry.json")
BASE_REGISTRY_FIELDS = {"id", "template", "types", "input_order", "broadcast"}
REGISTRY_FIELDS = BASE_REGISTRY_FIELDS | {"family_id", "template_signature"}
TEMPLATE_SIGNATURE_FIELDS = {"operation", "input_types", "output_type", "broadcast_policy", "output_socket_class"}


def load_registry(path: Path = DEFAULT_REGISTRY) -> list[dict[str, Any]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, list):
        raise ValueError("Semantic registry must be a list")
    return data


def validate_registry(catalog: Sequence[Mapping[str, Any]], registrations: Sequence[Mapping[str, Any]]) -> list[dict[str, Any]]:
    catalog_rows = {str(row["id"]): row for row in catalog}
    result: list[dict[str, Any]] = []
    seen: set[str] = set()
    for raw in registrations:
        row = dict(raw)
        if set(row) not in (BASE_REGISTRY_FIELDS, REGISTRY_FIELDS):
            raise ValueError(f"Registry row fields must be {sorted(BASE_REGISTRY_FIELDS)} or {sorted(REGISTRY_FIELDS)}")
        node_id = str(row["id"])
        if node_id in seen:
            raise ValueError(f"Duplicate semantic registration: {node_id}")
        seen.add(node_id)
        if node_id not in catalog_rows:
            raise ValueError(f"Unknown catalog NodeDef: {node_id}")
        if row["template"] not in TEMPLATES:
            raise ValueError(f"Unknown semantic template: {row['template']}")
        if not isinstance(row["types"], list) or not all(isinstance(v, str) for v in row["types"]):
            raise ValueError(f"Invalid types for {node_id}")
        catalog_types = set(catalog_rows[node_id].get("types", []))
        if catalog_types and not set(row["types"]).issubset(catalog_types):
            raise ValueError(f"Registration types do not match catalog for {node_id}")
        if not isinstance(row["input_order"], list) or not all(isinstance(v, str) for v in row["input_order"]):
            raise ValueError(f"Invalid input_order for {node_id}")
        if not isinstance(row["broadcast"], bool):
            raise ValueError(f"Invalid broadcast for {node_id}")
        if "family_id" in row:
            if not isinstance(row["family_id"], str) or not row["family_id"]:
                raise ValueError(f"Invalid family_id for {node_id}")
            signature = row["template_signature"]
            if not isinstance(signature, Mapping) or set(signature) != TEMPLATE_SIGNATURE_FIELDS:
                raise ValueError(f"Invalid template_signature for {node_id}")
            if not all(isinstance(signature[field], str) and signature[field] for field in (
                "operation", "output_type", "broadcast_policy", "output_socket_class"
            )):
                raise ValueError(f"Invalid template_signature for {node_id}")
            input_types = signature["input_types"]
            if not isinstance(input_types, list) or not input_types or not all(isinstance(value, str) and value for value in input_types):
                raise ValueError(f"Invalid template_signature input_types for {node_id}")
            row["template_signature"] = {
                "operation": signature["operation"], "input_types": list(input_types),
                "output_type": signature["output_type"], "broadcast_policy": signature["broadcast_policy"],
                "output_socket_class": signature["output_socket_class"],
            }
        result.append(row)
    return sorted(result, key=lambda row: row["id"])


def backend_records(catalog: Sequence[Mapping[str, Any]], registrations: Sequence[Mapping[str, Any]], backend: str) -> list[dict[str, Any]]:
    if backend not in {"cycles", "hydra"}:
        raise ValueError(f"Unknown backend: {backend}")
    return [{"backend": backend, **row} for row in validate_registry(catalog, registrations)]
