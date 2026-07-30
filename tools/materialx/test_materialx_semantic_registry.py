# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest
from pathlib import Path

try:
    import materialx_semantic_registry as registry
except ModuleNotFoundError:
    from tools.materialx import materialx_semantic_registry as registry


class SemanticRegistryTest(unittest.TestCase):
    catalog = [{"id": "ND_add_float", "types": ["float"]}]
    signature = {
        "operation": "add",
        "input_types": ["float", "float"],
        "output_type": "float",
        "broadcast_policy": "none",
        "output_socket_class": "float",
    }
    row = {
        "id": "ND_add_float",
        "template": "binary_componentwise",
        "types": ["float"],
        "input_order": ["in1", "in2"],
        "broadcast": False,
        "family_id": "add-float",
        "template_signature": signature,
    }

    def test_consumers_share_validated_semantics(self):
        cycles = registry.backend_records(self.catalog, [self.row], "cycles")
        hydra = registry.backend_records(self.catalog, [self.row], "hydra")
        self.assertEqual(
            {k: v for k, v in cycles[0].items() if k != "backend"},
            {k: v for k, v in hydra[0].items() if k != "backend"},
        )

    def test_rejects_duplicate_unknown_template_and_catalog(self):
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            registry.validate_registry(self.catalog, [self.row, self.row])
        with self.assertRaisesRegex(ValueError, "Unknown semantic template"):
            registry.validate_registry(self.catalog, [dict(self.row, template="bad")])
        with self.assertRaisesRegex(ValueError, "Unknown catalog"):
            registry.validate_registry(self.catalog, [dict(self.row, id="ND_missing")])

    def test_rejects_missing_or_nondeterministic_family_signature(self):
        for row, message in (
            (dict(self.row, family_id=""), "family_id"),
            (dict(self.row, template_signature={}), "template_signature"),
            (dict(self.row, template_signature={**self.signature, "input_types": []}), "input_types"),
            (dict(self.row, template_signature={**self.signature, "extra": "no"}), "template_signature"),
        ):
            with self.subTest(row=row), self.assertRaisesRegex(ValueError, message):
                registry.validate_registry(self.catalog, [row])

    def test_requires_family_metadata_for_schedulable_ids_but_keeps_legacy_rows(self):
        legacy = {key: value for key, value in self.row.items() if key not in {"family_id", "template_signature"}}
        self.assertEqual(registry.validate_registry(self.catalog, [legacy]), [legacy])
        with self.assertRaisesRegex(ValueError, "schedulable"):
            registry.validate_registry(self.catalog, [legacy], schedulable_ids={"ND_add_float"})
        with self.assertRaisesRegex(ValueError, "Unknown catalog"):
            registry.validate_registry(self.catalog, [self.row], schedulable_ids={"ND_missing"})

    def test_remap_manifest_exposes_truthful_deterministic_signatures(self):
        rows = registry.load_registry(Path(__file__).with_name("materialx_semantic_registry.json"))
        catalog = [{"id": row["id"], "types": row["types"]} for row in rows]
        validated = registry.validate_registry(catalog, rows)
        expected = {
            "ND_remap_vector2": (["vector2"] * 5, "vector2", "none", "vector2"),
            "ND_remap_vector2FA": (["vector2", "float", "float", "float", "float"], "vector2", "float_to_vector", "vector2"),
            "ND_remap_vector3": (["vector3"] * 5, "vector3", "none", "vector3"),
            "ND_remap_vector3FA": (["vector3", "float", "float", "float", "float"], "vector3", "float_to_vector", "vector3"),
        }
        self.assertEqual({row["id"] for row in validated}, set(expected))
        for row in validated:
            signature = row["template_signature"]
            with self.subTest(node_def=row["id"]):
                self.assertEqual(row["family_id"], "remap")
                self.assertEqual(signature["operation"], "remap")
                self.assertEqual(tuple(signature[field] for field in ("input_types", "output_type", "broadcast_policy", "output_socket_class")), expected[row["id"]])


if __name__ == "__main__":
    unittest.main()
