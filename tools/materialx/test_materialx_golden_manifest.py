#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import json
import unittest

import materialx_golden_manifest


class MaterialXGoldenManifestTest(unittest.TestCase):
    def test_manifest_is_canonical_and_uses_native_harness_ids(self):
        manifest = materialx_golden_manifest.build_manifest([
            {
                "id": "noise-ramp",
                "nodedefs": ["ND_ramplr_float", "ND_noise2d_float"],
                "reference_image": "goldens/noise-ramp-reference.png",
                "current_image": "results/noise-ramp-current.png",
                "render_settings": {"samples": 64, "resolution": [512, 512]},
                "metrics": {"max_abs_error": 0.01, "ssim_min": 0.99},
                "reviewer_status": "pending",
            },
            {
                "id": "constant",
                "nodedefs": ["ND_constant_float"],
                "reference_image": "goldens/constant-reference.png",
                "current_image": "results/constant-current.png",
                "render_settings": {"samples": 16, "resolution": [64, 64]},
                "metrics": {"max_abs_error": 0.0},
                "reviewer_status": "approved",
            },
        ])

        self.assertEqual(manifest["schema_version"], 1)
        self.assertEqual([fixture["id"] for fixture in manifest["fixtures"]], ["constant", "noise-ramp"])
        self.assertEqual(manifest["fixtures"][1]["nodedefs"], ["ND_noise2d_float", "ND_ramplr_float"])
        self.assertEqual(
            manifest["fixtures"][1]["harness_test_ids"],
            ["MaterialXNativeCycles.ND_noise2d_float", "MaterialXNativeCycles.ND_ramplr_float"],
        )
        encoded = materialx_golden_manifest.manifest_as_json(manifest)
        self.assertEqual(json.loads(encoded), manifest)
        self.assertTrue(encoded.endswith("\n"))

    def test_manifest_rejects_absolute_image_paths_and_unknown_fields(self):
        fixture = {
            "id": "invalid",
            "nodedefs": ["ND_constant_float"],
            "reference_image": "/goldens/reference.png",
            "current_image": "results/current.png",
            "render_settings": {},
            "metrics": {},
            "reviewer_status": "pending",
        }
        with self.assertRaisesRegex(ValueError, "relative"):
            materialx_golden_manifest.build_manifest([fixture])

        fixture["reference_image"] = "goldens/reference.png"
        fixture["unexpected"] = True
        with self.assertRaisesRegex(ValueError, "unsupported fields"):
            materialx_golden_manifest.build_manifest([fixture])


if __name__ == "__main__":
    unittest.main()
