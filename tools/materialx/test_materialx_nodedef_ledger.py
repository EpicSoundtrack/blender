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
                "cycles_reader": {"tested": 794, "unclassified": 8},
                "cycles_lowering": {"tested": 771, "unclassified": 8, "unsupported_verified": 23},
                "hydra": {"tested": 211, "unclassified": 591},
                "disposition": {
                    "native_and_hydra_cpu_tested": 211,
                    "native_cycles_cpu_tested": 560,
                    "unclassified": 8,
                    "unsupported_cycles_gap_verified": 23,
                },
            },
        )

        verified_gap_rows = {
            node_id: row
            for node_id, row in overrides["rows"].items()
            if row.get("disposition") == "unsupported_cycles_gap_verified"
        }
        self.assertEqual(
            set(verified_gap_rows),
            {
                "ND_bitangent_vector3",
                "ND_bump_vector3",
                "ND_burley_diffuse_bsdf",
                "ND_conical_edf",
                "ND_UsdPrimvarReader_filename",
                "ND_UsdPrimvarReader_string",
                "ND_generalized_schlick_bsdf",
                "ND_geompropvalueuniform_filename",
                "ND_geompropvalueuniform_string",
                "ND_heighttonormal_vector3",
                "ND_hextiledimage_color3",
                "ND_hextiledimage_color4",
                "ND_hextilednormalmap_vector3",
                "ND_lama_dielectric",
                "ND_lama_generalized_schlick",
                "ND_lama_layer_bsdf",
                "ND_lama_sheen",
                "ND_layer_bsdf",
                "ND_layer_vdf",
                "ND_measured_edf",
                "ND_tangent_vector3",
                "ND_worleynoise2d_vector3",
                "ND_worleynoise3d_vector3",
            },
        )
        for node_id, row in verified_gap_rows.items():
            self.assertEqual(row["cycles_reader"], "tested")
            self.assertEqual(row["cycles_lowering"], "unsupported_verified")
            evidence = "\n".join(row["evidence"])
            if node_id.startswith("ND_lama_"):
                self.assertIn("rejects_unsupported_lama_physical_nodes_without_mutation", evidence)
            elif node_id in {"ND_bitangent_vector3", "ND_bump_vector3", "ND_tangent_vector3"}:
                self.assertIn("rejects_tangent_bitangent_and_bump_without_mutating_graph", evidence)
            elif node_id == "ND_heighttonormal_vector3":
                self.assertIn("rejects_heighttonormal_without_mutating_graph", evidence)
            elif node_id == "ND_hextilednormalmap_vector3":
                self.assertIn("rejects_tangent_bitangent_and_bump_without_mutating_graph", evidence)
            elif node_id.startswith("ND_hextiledimage_"):
                self.assertIn("rejects_hextiledimage_without_mutating_graph", evidence)
            elif node_id.startswith("ND_geompropvalueuniform_"):
                self.assertIn("rejects_runtime_", evidence)
            elif node_id.startswith("ND_UsdPrimvarReader_"):
                self.assertIn("rejects_runtime_usd_primvar_", evidence)
            elif node_id.startswith("ND_worleynoise"):
                self.assertIn("rejects_worleynoise_vector3_without_mutating_graph", evidence)
            else:
                self.assertIn("rejects_unsupportable_requested_closures_by_name", evidence)
            self.assertIn("CPU-only structural gap verification", evidence)

        wave25_draft_rows = {
            node_id: row
            for node_id, row in overrides["rows"].items()
            if any("WAVE25 DRAFT" in evidence for evidence in row["evidence"])
        }
        self.assertEqual(len(wave25_draft_rows), 97)
        component_counts = {
            commit: sum(
                any(commit in evidence for evidence in row["evidence"])
                for row in wave25_draft_rows.values()
            )
            for commit in (
                "eae44d8e46b390c136229dc8f578fff2940710fe",
                "00fdbf7404bbf5fc9d8e7e9ee8524f9cb3202f57",
                "069b767034019a119f7da8f52bbbeeb59cd28cad",
                "0ac6a16ff7155be36dcadde91eba9c13babb8c62",
                "b7d59a4008e0e70413133f464ae4228f4091aed6",
            )
        }
        self.assertEqual(component_counts, {
            "eae44d8e46b390c136229dc8f578fff2940710fe": 36,
            "00fdbf7404bbf5fc9d8e7e9ee8524f9cb3202f57": 34,
            "069b767034019a119f7da8f52bbbeeb59cd28cad": 9,
            "0ac6a16ff7155be36dcadde91eba9c13babb8c62": 8,
            "b7d59a4008e0e70413133f464ae4228f4091aed6": 10,
        })
        for row in wave25_draft_rows.values():
            evidence = "\n".join(row["evidence"])
            self.assertIn("ec1fb36133eb1ebf48736f0aa929ec8b243e1fab: CPU GREEN", evidence)
            self.assertIn("FINAL COMPOSED TIP: PENDING", evidence)
            self.assertIn("GPU GATES: PENDING", evidence)

        wave31_draft_rows = {
            node_id: row
            for node_id, row in overrides["rows"].items()
            if any("WAVE31 DRAFT" in evidence for evidence in row["evidence"])
        }
        self.assertEqual(
            set(wave31_draft_rows),
            {
                "ND_rotate2d_vector2",
                "ND_rotate3d_vector3",
                "ND_cellnoise2d_float",
                "ND_cellnoise3d_float",
                "ND_fractal3d_color3",
                "ND_fractal3d_color3FA",
                "ND_fractal3d_float",
                "ND_fractal3d_vector2",
                "ND_fractal3d_vector2FA",
                "ND_fractal3d_vector3",
                "ND_fractal3d_vector3FA",
                "ND_ramplr_color3",
                "ND_ramplr_color4",
                "ND_ramptb_color4",
                "ND_splitlr_color3",
                "ND_splitlr_color4",
                "ND_splitlr_float",
                "ND_splittb_color3",
                "ND_splittb_color4",
                "ND_splittb_float",
            },
        )
        self.assertEqual(len(wave25_draft_rows) + len(wave31_draft_rows), 117)

        matrix_conditional_rows = {
            node_id: row
            for node_id, row in overrides["rows"].items()
            if node_id
            in {
                "ND_ifequal_matrix33",
                "ND_ifequal_matrix33B",
                "ND_ifequal_matrix33I",
                "ND_ifequal_matrix44",
                "ND_ifequal_matrix44B",
                "ND_ifequal_matrix44I",
                "ND_ifgreater_matrix33",
                "ND_ifgreater_matrix33I",
                "ND_ifgreater_matrix44",
                "ND_ifgreater_matrix44I",
                "ND_ifgreatereq_matrix33",
                "ND_ifgreatereq_matrix33I",
                "ND_ifgreatereq_matrix44",
                "ND_ifgreatereq_matrix44I",
            }
        }
        self.assertEqual(len(matrix_conditional_rows), 14)
        for row in matrix_conditional_rows.values():
            self.assertEqual(row["cycles_reader"], "tested")
            self.assertEqual(row["cycles_lowering"], "tested")
            self.assertEqual(row["disposition"], "native_cycles_cpu_tested")
            evidence = "\n".join(row["evidence"])
            self.assertIn("reads_and_lowers_literal_matrix_conditionals", evidence)
            self.assertIn("CPU-only structural/unit verification", evidence)

        switch_rows = {
            node_id: row
            for node_id, row in overrides["rows"].items()
            if node_id
            in {
                "ND_switch_color3",
                "ND_switch_color3I",
                "ND_switch_color4",
                "ND_switch_color4I",
                "ND_switch_float",
                "ND_switch_floatI",
                "ND_switch_matrix33",
                "ND_switch_matrix33I",
                "ND_switch_matrix44",
                "ND_switch_matrix44I",
                "ND_switch_vector2",
                "ND_switch_vector2I",
                "ND_switch_vector3",
                "ND_switch_vector3I",
                "ND_switch_vector4",
                "ND_switch_vector4I",
            }
        }
        self.assertEqual(len(switch_rows), 16)
        for row in switch_rows.values():
            self.assertEqual(row["cycles_reader"], "tested")
            self.assertEqual(row["cycles_lowering"], "tested")
            self.assertEqual(row["disposition"], "native_cycles_cpu_tested")
            evidence = "\n".join(row["evidence"])
            self.assertIn("reads_and_lowers_literal_switch_backlog_family", evidence)
            self.assertIn("CPU-only structural/unit verification", evidence)

        matrix44_add_subtract_rows = {
            node_id: row
            for node_id, row in overrides["rows"].items()
            if node_id in {"ND_add_matrix44", "ND_subtract_matrix44"}
        }
        self.assertEqual(len(matrix44_add_subtract_rows), 2)
        for row in matrix44_add_subtract_rows.values():
            self.assertEqual(row["cycles_reader"], "tested")
            self.assertEqual(row["cycles_lowering"], "tested")
            self.assertEqual(row["disposition"], "native_cycles_cpu_tested")
            evidence = "\n".join(row["evidence"])
            self.assertIn("literal affine Matrix44 add/subtract arithmetic", evidence)
            self.assertIn("CPU-only structural/unit verification", evidence)

        matrix_determinant_rows = {
            node_id: row
            for node_id, row in overrides["rows"].items()
            if node_id in {"ND_determinant_matrix33", "ND_determinant_matrix44"}
        }
        self.assertEqual(len(matrix_determinant_rows), 2)
        for row in matrix_determinant_rows.values():
            self.assertEqual(row["cycles_reader"], "tested")
            self.assertEqual(row["cycles_lowering"], "tested")
            self.assertEqual(row["disposition"], "native_cycles_cpu_tested")
            evidence = "\n".join(row["evidence"])
            self.assertIn("reads_and_lowers_literal_matrix_determinants", evidence)
            self.assertIn("CPU-only structural/unit verification", evidence)

        color4_blend_rows = {
            node_id: row
            for node_id, row in overrides["rows"].items()
            if node_id
            in {
                "ND_plus_color4",
                "ND_minus_color4",
                "ND_difference_color4",
                "ND_screen_color4",
                "ND_overlay_color4",
            }
        }
        self.assertEqual(len(color4_blend_rows), 5)
        for row in color4_blend_rows.values():
            self.assertEqual(row["cycles_reader"], "tested")
            self.assertEqual(row["cycles_lowering"], "tested")
            self.assertEqual(row["disposition"], "native_cycles_cpu_tested")
            evidence = "\n".join(row["evidence"])
            self.assertIn("reads_and_lowers_color4_compositing_blends", evidence)
            self.assertIn("CPU-only structural/unit verification", evidence)

        procedural_shape_rows = {
            node_id: row
            for node_id, row in overrides["rows"].items()
            if node_id in {"ND_circle_float", "ND_cloverleaf_float", "ND_hexagon_float", "ND_line_float"}
        }
        self.assertEqual(len(procedural_shape_rows), 4)
        for row in procedural_shape_rows.values():
            self.assertEqual(row["cycles_reader"], "tested")
            self.assertEqual(row["cycles_lowering"], "tested")
            self.assertEqual(row["disposition"], "native_cycles_cpu_tested")
            evidence = "\n".join(row["evidence"])
            self.assertIn("reads_and_lowers_procedural2d_circle_and_line_masks", evidence)
            self.assertIn("CPU-only structural/unit verification", evidence)

        ramp_gradient_row = overrides["rows"]["ND_ramp_gradient"]
        self.assertEqual(ramp_gradient_row["cycles_reader"], "tested")
        self.assertEqual(ramp_gradient_row["cycles_lowering"], "tested")
        self.assertEqual(ramp_gradient_row["disposition"], "native_cycles_cpu_tested")
        ramp_gradient_evidence = "\n".join(ramp_gradient_row["evidence"])
        self.assertIn("reads_and_lowers_literal_ramp_gradient", ramp_gradient_evidence)
        self.assertIn("CPU-only structural/unit verification", ramp_gradient_evidence)

        color4_burn_dodge_rows = {
            node_id: row
            for node_id, row in overrides["rows"].items()
            if node_id in {"ND_burn_color4", "ND_dodge_color4"}
        }
        self.assertEqual(len(color4_burn_dodge_rows), 2)
        for row in color4_burn_dodge_rows.values():
            self.assertEqual(row["cycles_reader"], "tested")
            self.assertEqual(row["cycles_lowering"], "tested")
            self.assertEqual(row["disposition"], "native_cycles_cpu_tested")
            evidence = "\n".join(row["evidence"])
            self.assertIn("reads_and_lowers_color4_burn_dodge_as_materialx_arithmetic", evidence)
            self.assertIn("CPU-only structural/unit verification", evidence)

        alpha_aware_color4_composite_rows = {
            node_id: row
            for node_id, row in overrides["rows"].items()
            if node_id
            in {
                "ND_in_color4",
                "ND_mask_color4",
                "ND_matte_color4",
                "ND_out_color4",
                "ND_over_color4",
            }
        }
        self.assertEqual(len(alpha_aware_color4_composite_rows), 5)
        for row in alpha_aware_color4_composite_rows.values():
            self.assertEqual(row["cycles_reader"], "tested")
            self.assertEqual(row["cycles_lowering"], "tested")
            self.assertEqual(row["disposition"], "native_cycles_cpu_tested")
            evidence = "\n".join(row["evidence"])
            self.assertIn("reads_and_lowers_alpha_aware_color4_compositing_operators", evidence)
            self.assertIn("CPU-only structural/unit verification", evidence)

        application_time_rows = {
            node_id: row
            for node_id, row in overrides["rows"].items()
            if node_id in {"ND_frame_float", "ND_time_float"}
        }
        self.assertEqual(len(application_time_rows), 2)
        for row in application_time_rows.values():
            self.assertEqual(row["cycles_reader"], "tested")
            self.assertEqual(row["cycles_lowering"], "tested")
            self.assertEqual(row["disposition"], "native_cycles_cpu_tested")
            evidence = "\n".join(row["evidence"])
            self.assertIn("reads_application_frame_and_time_float", evidence)
            self.assertIn("CPU-only structural/unit verification", evidence)

        self.assertEqual(
            sum(
                any("5ffea950510a114fc727fa0c8675a349799c3709" in evidence
                    for evidence in row["evidence"])
                for row in wave31_draft_rows.values()
            ),
            2,
        )
        self.assertEqual(
            sum(
                any("b4617f9dc37e94905e35b12285f12ca5aca86e81" in evidence
                    for evidence in row["evidence"])
                for row in wave31_draft_rows.values()
            ),
            18,
        )
        for row in wave31_draft_rows.values():
            self.assertEqual(row["cycles_reader"], "tested")
            self.assertEqual(row["cycles_lowering"], "tested")
            self.assertEqual(row["disposition"], "native_cycles_cpu_tested")
            evidence = "\n".join(row["evidence"])
            self.assertIn("FINAL COMPOSED TIP: PENDING", evidence)
            self.assertIn("CURRENT-TIP GPU GATES: PENDING", evidence)

        for node_id in (
            "ND_worleynoise2d_float",
            "ND_worleynoise2d_vector2",
            "ND_worleynoise3d_float",
            "ND_worleynoise3d_vector2",
        ):
            row = overrides["rows"][node_id]
            self.assertEqual(row["cycles_reader"], "tested")
            self.assertEqual(row["cycles_lowering"], "tested")
            self.assertEqual(row["disposition"], "native_cycles_cpu_tested")
            evidence = "\n".join(row["evidence"])
            self.assertIn("materialx_graph.*worley*", evidence)
            self.assertIn("CPU-only structural/unit verification", evidence)

        for node_id in (
            "ND_unifiednoise2d_float",
            "ND_unifiednoise3d_float",
        ):
            row = overrides["rows"][node_id]
            self.assertEqual(row["cycles_reader"], "tested")
            self.assertEqual(row["cycles_lowering"], "tested")
            self.assertEqual(row["disposition"], "native_cycles_cpu_tested")
            evidence = "\n".join(row["evidence"])
            self.assertIn("materialx_graph.*unifiednoise*", evidence)
            self.assertIn("CPU-only structural/unit verification", evidence)

        for node_id in ("ND_ramplr_float", "ND_ramptb_color3", "ND_ramptb_float"):
            evidence = "\n".join(overrides["rows"][node_id]["evidence"])
            self.assertNotIn("WAVE31 DRAFT", evidence)


if __name__ == "__main__":
    unittest.main()
