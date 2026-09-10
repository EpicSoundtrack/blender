#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import json
from pathlib import Path
import unittest

import materialx_nodedef_ledger


class MaterialXNodeDefLedgerTest(unittest.TestCase):
    def setUp(self):
        self.catalog = [
            {
                "id": "ND_zebra_float",
                "category": "procedural2d",
                "types": ["float", "vector2"],
                "source": "libraries/stdlib/zebra.mtlx",
            },
            {
                "id": "ND_absval_float",
                "category": "math",
                "types": ["float"],
                "source": "libraries/stdlib/math.mtlx",
            },
        ]

    def test_builds_deterministic_rows_with_all_required_authority_fields(self):
        document = materialx_nodedef_ledger.build_ledger(
            self.catalog,
            {
                "schema_version": 1,
                "rows": {
                    "ND_absval_float": {
                        "cycles_reader": "implemented",
                        "cycles_lowering": "implemented",
                        "hydra": "tested",
                        "disposition": "supported",
                        "evidence": ["tests/native_absval.json"],
                        "owner": "cycles",
                        "next_action": "gpu_parity",
                    }
                },
            },
        )

        self.assertEqual(document["schema_version"], 1)
        self.assertEqual([row["id"] for row in document["rows"]], ["ND_absval_float", "ND_zebra_float"])
        self.assertEqual(
            set(document["rows"][0]),
            set(materialx_nodedef_ledger.LEDGER_FIELDS),
        )
        self.assertEqual(document["rows"][0]["evidence"], ["tests/native_absval.json"])
        self.assertEqual(document["rows"][1]["cycles_reader"], "unclassified")
        self.assertEqual(document["rows"][1]["cycles_lowering"], "unclassified")
        self.assertEqual(document["rows"][1]["hydra"], "unclassified")
        self.assertEqual(document["rows"][1]["disposition"], "unclassified")
        self.assertEqual(document["rows"][1]["cycles_render_verified_status"], "NOT_YET_ATTEMPTED")
        self.assertEqual(document["rows"][1]["evidence"], [])
        self.assertEqual(document["rows"][1]["owner"], "unassigned")
        self.assertEqual(document["rows"][1]["next_action"], "classify")
        self.assertEqual(
            document["summary"],
            {
                "total": 2,
                "cycles_reader": {"implemented": 1, "unclassified": 1},
                "cycles_lowering": {"implemented": 1, "unclassified": 1},
                "hydra": {"tested": 1, "unclassified": 1},
                "disposition": {"supported": 1, "unclassified": 1},
            },
        )
        self.assertEqual(json.loads(materialx_nodedef_ledger.ledger_as_json(document)), document)

    def test_rejects_unknown_or_incomplete_authoritative_rows(self):
        with self.assertRaisesRegex(ValueError, "unknown NodeDef"):
            materialx_nodedef_ledger.build_ledger(
                self.catalog,
                {"schema_version": 1, "rows": {"ND_missing": {"owner": "cycles"}}},
            )

        document = materialx_nodedef_ledger.build_ledger(self.catalog)
        del document["rows"][0]["evidence"]
        with self.assertRaisesRegex(ValueError, "missing fields: evidence"):
            materialx_nodedef_ledger.validate_ledger(document, expected_count=2)

    def test_remaining_node_ids_excludes_explicitly_owned_rows(self):
        ledger = materialx_nodedef_ledger.build_ledger(
            [
                {
                    "id": node_id,
                    "category": "math",
                    "types": ["float"],
                    "source": "libraries/stdlib/math.mtlx",
                }
                for node_id in ("ND_active", "ND_completed", "ND_phase2", "ND_remaining")
            ]
        )

        self.assertEqual(
            materialx_nodedef_ledger.remaining_node_ids(
                ledger,
                completed_ids={"ND_completed"},
                phase2_ids={"ND_phase2"},
                active_ids={"ND_active"},
            ),
            ["ND_remaining"],
        )

    def test_wave31_draft_projection_is_explicitly_non_authoritative(self):
        overrides = json.loads(
            Path(materialx_nodedef_ledger.DEFAULT_OVERRIDES_PATH).read_text(encoding="utf-8")
        )
        override_ids = sorted(overrides["rows"])
        catalog = [
            {
                "id": node_id,
                "category": "ledger-test",
                "types": [],
                "source": "ledger-test",
            }
            for node_id in override_ids
        ]
        catalog.extend(
            {
                "id": f"ND_wave25_unclassified_{index:04d}",
                "category": "ledger-test",
                "types": [],
                "source": "ledger-test",
            }
            for index in range(materialx_nodedef_ledger.materialx_catalog.EXPECTED_NODEDEF_COUNT -
                               len(catalog))
        )

        document = materialx_nodedef_ledger.build_ledger(catalog, overrides)
        self.assertEqual(
            document["summary"],
            {
                "total": 802,
                "cycles_reader": {"tested": 781, "unsupported_by_design": 21},
                "cycles_lowering": {"tested": 781, "unsupported_by_design": 21},
                "hydra": {"tested": 217, "unclassified": 585},
                "disposition": {
                    "native_and_hydra_cpu_tested": 217,
                    "native_cycles_cpu_tested": 564,
                    "unsupported_by_design_cpu_tested": 21,
                },
            },
        )

        unsupported_by_design_rows = {
            node_id
            for node_id, row in overrides["rows"].items()
            if row.get("disposition") == "unsupported_by_design_cpu_tested"
        }
        self.assertEqual(
            unsupported_by_design_rows,
            {
                "ND_UsdPrimvarReader_filename",
                "ND_UsdPrimvarReader_string",
                "ND_bitangent_vector3",
                "ND_conical_edf",
                "ND_constant_filename",
                "ND_constant_string",
                "ND_dot_filename",
                "ND_dot_string",
                "ND_geompropvalueuniform_filename",
                "ND_geompropvalueuniform_string",
                "ND_hextiledimage_color3",
                "ND_hextiledimage_color4",
                "ND_hextilednormalmap_vector3",
                "ND_lama_dielectric",
                "ND_lama_generalized_schlick",
                "ND_lama_layer_bsdf",
                "ND_layer_bsdf",
                "ND_layer_vdf",
                "ND_measured_edf",
                "ND_worleynoise2d_vector3",
                "ND_worleynoise3d_vector3",
            },
        )

        hydra_extension_rows = {
            node_id
            for node_id, row in overrides["rows"].items()
            if any("Linux CPU-only cycles_hydra_test" in evidence for evidence in row["evidence"])
        }
        self.assertEqual(
            hydra_extension_rows,
            {
                "ND_power_float",
                "ND_smoothstep_float",
                "ND_smoothstep_vector2",
                "ND_smoothstep_vector2FA",
                "ND_smoothstep_vector3",
                "ND_smoothstep_vector3FA",
            },
        )

        wave25_draft_rows = {
            node_id: row
            for node_id, row in overrides["rows"].items()
            if any("WAVE25 DRAFT" in evidence for evidence in row["evidence"])
        }
        self.assertEqual(wave25_draft_rows, {})

        wave31_draft_rows = {
            node_id: row
            for node_id, row in overrides["rows"].items()
            if any("WAVE31 DRAFT" in evidence for evidence in row["evidence"])
        }
        self.assertEqual(wave31_draft_rows, {})

        for node_id in ("ND_ramplr_float", "ND_ramptb_color3", "ND_ramptb_float"):
            evidence = "\n".join(overrides["rows"][node_id]["evidence"])
            self.assertNotIn("WAVE31 DRAFT", evidence)


if __name__ == "__main__":
    unittest.main()
