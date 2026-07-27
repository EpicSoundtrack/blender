#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import json
import unittest

import materialx_harness_plan


class MaterialXHarnessPlanTest(unittest.TestCase):
    def test_harness_plan_is_deterministic_and_keeps_renderer_statuses_separate(self):
        catalog = [
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

        plan = materialx_harness_plan.build_harness_plan(
            catalog,
            {"ND_absval_float": {"hydra_status": "tested"}},
        )

        self.assertEqual(
            plan,
            [
                {
                    "id": "ND_absval_float",
                    "category": "math",
                    "types": ["float"],
                    "source": "libraries/stdlib/math.mtlx",
                    "renderer": "hydra",
                    "status": "tested",
                    "test_id": "MaterialXHydra.ND_absval_float",
                },
                {
                    "id": "ND_absval_float",
                    "category": "math",
                    "types": ["float"],
                    "source": "libraries/stdlib/math.mtlx",
                    "renderer": "native_cycles",
                    "status": "unclassified",
                    "test_id": "MaterialXNativeCycles.ND_absval_float",
                },
                {
                    "id": "ND_zebra_float",
                    "category": "procedural2d",
                    "types": ["float", "vector2"],
                    "source": "libraries/stdlib/zebra.mtlx",
                    "renderer": "hydra",
                    "status": "unclassified",
                    "test_id": "MaterialXHydra.ND_zebra_float",
                },
                {
                    "id": "ND_zebra_float",
                    "category": "procedural2d",
                    "types": ["float", "vector2"],
                    "source": "libraries/stdlib/zebra.mtlx",
                    "renderer": "native_cycles",
                    "status": "unclassified",
                    "test_id": "MaterialXNativeCycles.ND_zebra_float",
                },
            ],
        )
        self.assertEqual(json.loads(materialx_harness_plan.plan_as_json(plan)), plan)

    def test_targeted_cpu_gpu_evidence_updates_only_its_explicit_renderer_row(self):
        catalog = [
            {
                "id": "ND_absval_float",
                "category": "math",
                "types": ["float"],
                "source": "libraries/stdlib/math.mtlx",
            }
        ]

        plan = materialx_harness_plan.build_harness_plan(catalog)
        updated = materialx_harness_plan.ingest_targeted_results(
            plan,
            [
                {
                    "id": "ND_absval_float",
                    "renderer": "hydra",
                    "test_id": "MaterialXHydra.ND_absval_float",
                    "status": "tested",
                    "cpu_result": "passed",
                    "gpu_result": "passed",
                    "parity": "matched",
                    "evidence": "tests/hydra_absval_cpu_gpu.json",
                }
            ],
        )

        self.assertEqual(updated[0], {
            "id": "ND_absval_float",
            "category": "math",
            "types": ["float"],
            "source": "libraries/stdlib/math.mtlx",
            "renderer": "hydra",
            "status": "tested",
            "test_id": "MaterialXHydra.ND_absval_float",
            "cpu_result": "passed",
            "gpu_result": "passed",
            "parity": "matched",
            "evidence": "tests/hydra_absval_cpu_gpu.json",
        })
        self.assertEqual(updated[1]["renderer"], "native_cycles")
        self.assertEqual(updated[1]["status"], "unclassified")
        self.assertIsNone(updated[1]["cpu_result"])
        self.assertIsNone(updated[1]["gpu_result"])
        self.assertIsNone(updated[1]["parity"])
        self.assertIsNone(updated[1]["evidence"])

    def test_targeted_evidence_rejects_unknown_rows_and_missing_cpu_gpu_fields(self):
        plan = materialx_harness_plan.build_harness_plan([
            {
                "id": "ND_absval_float",
                "category": "math",
                "types": ["float"],
                "source": "libraries/stdlib/math.mtlx",
            }
        ])
        result = {
            "id": "ND_absval_float",
            "renderer": "hydra",
            "test_id": "MaterialXHydra.ND_absval_float",
            "status": "tested",
            "cpu_result": "passed",
            "parity": "matched",
            "evidence": "tests/hydra_absval_cpu_gpu.json",
        }

        with self.assertRaisesRegex(ValueError, "missing fields: gpu_result"):
            materialx_harness_plan.ingest_targeted_results(plan, [result])

        result["gpu_result"] = "passed"
        result["renderer"] = "native_cycles"
        with self.assertRaisesRegex(ValueError, "test_id"):
            materialx_harness_plan.ingest_targeted_results(plan, [result])


if __name__ == "__main__":
    unittest.main()
