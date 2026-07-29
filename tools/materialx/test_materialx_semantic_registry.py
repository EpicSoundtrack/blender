import unittest
from pathlib import Path
import materialx_semantic_registry as registry

class SemanticRegistryTest(unittest.TestCase):
    catalog = [{"id": "ND_add_float", "types": ["float"]}]
    row = {"id":"ND_add_float", "template":"binary_componentwise", "types":["float"], "input_order":["in1","in2"], "broadcast":False}
    def test_consumers_share_validated_semantics(self):
        cycles = registry.backend_records(self.catalog, [self.row], "cycles")
        hydra = registry.backend_records(self.catalog, [self.row], "hydra")
        self.assertEqual({k:v for k,v in cycles[0].items() if k != "backend"}, {k:v for k,v in hydra[0].items() if k != "backend"})
    def test_rejects_duplicate_unknown_template_and_catalog(self):
        with self.assertRaisesRegex(ValueError, "Duplicate"): registry.validate_registry(self.catalog, [self.row, self.row])
        bad = dict(self.row, template="bad")
        with self.assertRaisesRegex(ValueError, "Unknown semantic template"): registry.validate_registry(self.catalog, [bad])
        bad = dict(self.row, id="ND_missing")
        with self.assertRaisesRegex(ValueError, "Unknown catalog"): registry.validate_registry(self.catalog, [bad])

    def test_remap_manifest_preserves_order_and_scalar_broadcast(self):
        rows = registry.load_registry(Path(__file__).with_name("materialx_semantic_registry.json"))
        catalog = [{"id": row["id"], "types": row["types"]} for row in rows]
        cycles = registry.backend_records(catalog, rows, "cycles")
        hydra = registry.backend_records(catalog, rows, "hydra")
        expected = ["in", "inlow", "inhigh", "outlow", "outhigh"]
        self.assertTrue(all(row["template"] == "remap" and row["input_order"] == expected for row in cycles))
        self.assertEqual([row["broadcast"] for row in cycles], [False, True, False, True])
        self.assertEqual([{k: v for k, v in row.items() if k != "backend"} for row in cycles],
                         [{k: v for k, v in row.items() if k != "backend"} for row in hydra])
