#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import json
import tempfile
import unittest
from pathlib import Path

import materialx_catalog


class MaterialXCatalogTest(unittest.TestCase):
    def test_catalog_entries_are_deterministic_and_deduplicated(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            library_root = Path(temp_dir)
            defs = library_root / "libraries" / "stdlib" / "stdlib_defs.mtlx"
            duplicate = library_root / "libraries" / "stdlib" / "genglsl" / "stdlib_impl.mtlx"
            defs.parent.mkdir(parents=True)
            duplicate.parent.mkdir(parents=True)
            defs.write_text(
                """<?xml version=\"1.0\"?>
<materialx version=\"1.39\">
  <nodedef name=\"ND_noise3d_float\" node=\"noise3d\" nodegroup=\"procedural3d\">
    <input name=\"amplitude\" type=\"float\" value=\"1.0\" />
    <input name=\"pivot\" type=\"vector3\" />
    <output name=\"out\" type=\"float\" />
  </nodedef>
  <nodedef name=\"ND_image_color3\" node=\"image\" nodegroup=\"texture2d\">
    <input name=\"file\" type=\"filename\" />
    <input name=\"default\" type=\"color3\" />
    <output name=\"out\" type=\"color3\" />
  </nodedef>
</materialx>
""",
                encoding="utf-8",
            )
            duplicate.write_text(
                """<?xml version=\"1.0\"?>
<materialx version=\"1.39\">
  <nodedef name=\"ND_noise3d_float\" node=\"noise3d\" nodegroup=\"procedural3d\">
    <input name=\"amplitude\" type=\"float\" value=\"1.0\" />
    <input name=\"pivot\" type=\"vector3\" />
    <output name=\"out\" type=\"float\" />
  </nodedef>
</materialx>
""",
                encoding="utf-8",
            )

            catalog = materialx_catalog.build_catalog(library_root)
            encoded = materialx_catalog.catalog_as_json(catalog)

        self.assertEqual([entry["id"] for entry in catalog], ["ND_image_color3", "ND_noise3d_float"])
        self.assertEqual(catalog[0]["category"], "texture2d")
        self.assertEqual(catalog[0]["source"], "libraries/stdlib/stdlib_defs.mtlx")
        self.assertEqual(catalog[0]["types"], ["color3", "filename"])
        self.assertEqual(json.loads(encoded), catalog)
        self.assertTrue(encoded.endswith("\n"))


if __name__ == "__main__":
    unittest.main()
