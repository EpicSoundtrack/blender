#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest

import materialx_parity_dashboard


class MaterialXParityDashboardTest(unittest.TestCase):
    def test_dashboard_is_deterministic_and_renders_explicit_review_evidence(self):
        evidence_rows = [
            {
                "id": "ND_zebra_float",
                "renderer": "native_cycles",
                "test_id": "MaterialXNativeCycles.ND_zebra_float",
                "status": "tested",
                "cpu_result": "passed",
                "gpu_result": "passed",
                "parity": "matched",
                "evidence": "results/zebra.json",
            },
            {
                "id": "ND_absval_float",
                "renderer": "hydra",
                "test_id": "MaterialXHydra.ND_absval_float",
                "status": "needs_review",
                "cpu_result": "passed",
                "gpu_result": "passed",
                "parity": "different",
                "evidence": "results/absval.json",
                "reference_image": "images/reference/absval.png",
                "current_image": "images/current/absval.png",
                "diff_image": "images/diff/absval.png",
                "metrics": {"ssim": 0.98, "mae": 0.02},
                "human_review": "pending",
            },
        ]

        dashboard = materialx_parity_dashboard.build_dashboard_rows(evidence_rows)
        self.assertEqual([row["id"] for row in dashboard], ["ND_absval_float", "ND_zebra_float"])
        self.assertEqual(dashboard[0]["metrics"], {"mae": 0.02, "ssim": 0.98})
        self.assertEqual(dashboard[1]["human_review"], None)

        html = materialx_parity_dashboard.dashboard_as_html(dashboard)
        self.assertIn('href="images/reference/absval.png"', html)
        self.assertIn('href="images/current/absval.png"', html)
        self.assertIn('href="images/diff/absval.png"', html)
        self.assertIn("mae=0.02; ssim=0.98", html)
        self.assertIn("needs_review", html)
        self.assertIn("pending", html)
        self.assertLess(html.index("ND_absval_float"), html.index("ND_zebra_float"))

    def test_dashboard_rejects_non_explicit_evidence_schema(self):
        with self.assertRaisesRegex(ValueError, "missing fields: parity"):
            materialx_parity_dashboard.build_dashboard_rows([
                {
                    "id": "ND_absval_float",
                    "renderer": "hydra",
                    "test_id": "MaterialXHydra.ND_absval_float",
                    "status": "tested",
                    "cpu_result": "passed",
                    "gpu_result": "passed",
                    "evidence": "results/absval.json",
                }
            ])


if __name__ == "__main__":
    unittest.main()
