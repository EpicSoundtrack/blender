#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import json
import unittest

import materialx_semantic_test_plan


class MaterialXSemanticTestPlanTest(unittest.TestCase):
    catalog = [
        {"id": "ND_add_float", "types": ["float"]},
        {"id": "ND_subtract_float", "types": ["float"]},
        {"id": "ND_convert_float_color3", "types": ["float", "color3"]},
    ]

    def test_siblings_share_one_parameterized_template_plan(self):
        registrations = [
            {
                "id": "ND_subtract_float",
                "template": "binary_componentwise",
                "types": ["float"],
                "input_order": ["in1", "in2"],
                "broadcast": False,
            },
            {
                "id": "ND_add_float",
                "template": "binary_componentwise",
                "types": ["float"],
                "input_order": ["in1", "in2"],
                "broadcast": False,
            },
        ]

        plan = materialx_semantic_test_plan.build_family_test_plan(self.catalog, registrations)

        self.assertEqual(plan, [
            {
                "template": "binary_componentwise",
                "registered_ids": ["ND_add_float", "ND_subtract_float"],
                "cases": [
                    {"input_class": "literal_linked", "ids": ["ND_add_float", "ND_subtract_float"]},
                    {"input_class": "scalar_broadcast", "ids": []},
                    {"input_class": "type_rejection", "ids": ["ND_add_float", "ND_subtract_float"]},
                ],
                "golden_fixture": {
                    "representative_id": "ND_add_float",
                    "cpu": "required",
                    "gpu": "required",
                },
            }
        ])
        self.assertEqual(json.loads(materialx_semantic_test_plan.plan_as_json(plan)), plan)

    def test_new_template_emits_one_new_family_plan_with_broadcast_coverage(self):
        registrations = [
            {
                "id": "ND_add_float",
                "template": "binary_componentwise",
                "types": ["float"],
                "input_order": ["in1", "in2"],
                "broadcast": False,
            },
            {
                "id": "ND_convert_float_color3",
                "template": "conversion",
                "types": ["float", "color3"],
                "input_order": ["in"],
                "broadcast": True,
            },
        ]

        plan = materialx_semantic_test_plan.build_family_test_plan(self.catalog, registrations)

        self.assertEqual([row["template"] for row in plan], ["binary_componentwise", "conversion"])
        conversion = plan[1]
        self.assertEqual(conversion["registered_ids"], ["ND_convert_float_color3"])
        self.assertEqual(conversion["cases"][1], {
            "input_class": "scalar_broadcast",
            "ids": ["ND_convert_float_color3"],
        })
        self.assertEqual(conversion["golden_fixture"], {
            "representative_id": "ND_convert_float_color3",
            "cpu": "required",
            "gpu": "required",
        })

    def test_registry_validation_is_authoritative(self):
        registration = {
            "id": "ND_missing",
            "template": "conversion",
            "types": ["float"],
            "input_order": ["in"],
            "broadcast": False,
        }
        with self.assertRaisesRegex(ValueError, "Unknown catalog NodeDef"):
            materialx_semantic_test_plan.build_family_test_plan(self.catalog, [registration])


if __name__ == "__main__":
    unittest.main()
