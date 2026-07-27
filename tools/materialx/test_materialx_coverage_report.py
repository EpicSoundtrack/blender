#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import csv
import io
import json
import unittest

import materialx_coverage_report


class MaterialXCoverageReportTest(unittest.TestCase):
    def test_status_skeleton_is_sorted_and_tracks_renderers_separately(self):
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

        report = materialx_coverage_report.build_status_skeleton(catalog)

        self.assertEqual([entry["id"] for entry in report], ["ND_absval_float", "ND_zebra_float"])
        self.assertEqual(report[0]["hydra_status"], "unclassified")
        self.assertEqual(report[0]["native_cycles_status"], "unclassified")
        self.assertEqual(report[0]["types"], ["float"])
        self.assertEqual(
            set(report[0]),
            {"id", "category", "types", "source", "hydra_status", "native_cycles_status"},
        )

    def test_json_and_csv_are_deterministic_and_preserve_independent_statuses(self):
        catalog = [
            {
                "id": "ND_mix_float",
                "category": "compositing",
                "types": ["float"],
                "source": "libraries/stdlib/stdlib_defs.mtlx",
            }
        ]
        report = materialx_coverage_report.build_status_skeleton(
            catalog,
            {
                "ND_mix_float": {
                    "hydra_status": "tested",
                    "native_cycles_status": "planned",
                }
            },
        )

        self.assertEqual(json.loads(materialx_coverage_report.report_as_json(report)), report)
        rows = list(csv.DictReader(io.StringIO(materialx_coverage_report.report_as_csv(report))))
        self.assertEqual(rows, [{
            "id": "ND_mix_float",
            "category": "compositing",
            "types": "float",
            "source": "libraries/stdlib/stdlib_defs.mtlx",
            "hydra_status": "tested",
            "native_cycles_status": "planned",
        }])


if __name__ == "__main__":
    unittest.main()
