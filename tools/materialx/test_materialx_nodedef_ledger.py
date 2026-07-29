#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import json
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


if __name__ == "__main__":
    unittest.main()
