#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest

import materialx_nodedef_ledger
import materialx_project_preflight
import materialx_project_state


def make_ledger(count=802, completed_ids=(), *, include_ledger_evidence=True):
    completed_ids = set(completed_ids)
    catalog = [
        {
            "id": f"ND_test_{index:04d}",
            "category": "math",
            "types": ["float"],
            "source": "libraries/stdlib/test.mtlx",
        }
        for index in range(count)
    ]
    overrides = {
        "schema_version": 1,
        "rows": {
            node_id: {
                "cycles_reader": "tested",
                "cycles_lowering": "tested",
                "disposition": "native_cycles_cpu_tested",
                "evidence": [f"evidence/{node_id}.json"] if include_ledger_evidence else [],
                "owner": "cycles",
                "next_action": "gpu_parity",
            }
            for node_id in completed_ids
        },
    }
    return materialx_nodedef_ledger.build_ledger(catalog, overrides)


def make_capacity(completed_ids=(), *, worker_state="active", windows_state="green"):
    state = materialx_project_state.new_project_state()
    state["workers"] = [
        {
            "id": worker["id"],
            "state": worker_state,
            "last_evidence_id": (
                "preflight-cycle" if worker_state != "unknown" else ""
            ),
        }
        for worker in state["workers"]
    ]
    state["lanes"]["horde"] = {
        "state": "active" if worker_state == "active" else "blocked",
        "last_evidence_id": (
            "preflight-cycle" if worker_state != "unknown" else ""
        ),
    }
    state["healthy"] = worker_state == "active"
    state["lanes"]["windows_a40_cuda"] = {
        "state": windows_state,
        "last_evidence_id": "windows-green" if windows_state == "green" else "",
    }
    state["completed_rows"] = sorted(completed_ids)
    state["evidence_records"] = [
            {"row_id": node_id, "record": f"evidence/{node_id}.json"}
            for node_id in completed_ids
        ]
    state["journal_records"] = [
            {"row_id": node_id, "record": f"journal/{node_id}.md"}
            for node_id in completed_ids
        ]
    return materialx_project_state.validate_project_state(state)


class MaterialXProjectPreflightTest(unittest.TestCase):
    def test_accepts_unclassified_rows_and_emits_deterministic_summary(self):
        ledger = make_ledger()
        capacity = make_capacity(windows_state="failed")

        summary = materialx_project_preflight.evaluate_preflight(ledger, capacity)

        self.assertTrue(summary["ok"])
        self.assertEqual(summary["ledger_rows"], 802)
        self.assertEqual(summary["completed_rows"], [])
        self.assertEqual(summary["failures"], [])
        self.assertEqual(
            materialx_project_preflight.preflight_as_json(summary),
            materialx_project_preflight.preflight_as_json(summary),
        )

    def test_fails_closed_when_ledger_does_not_validate_to_802_rows(self):
        summary = materialx_project_preflight.evaluate_preflight(make_ledger(801), make_capacity())

        self.assertFalse(summary["ok"])
        self.assertEqual(summary["failures"], ["ledger: Expected 802 NodeDefs, found 801"])

    def test_fails_closed_when_a_healthy_worker_is_not_active(self):
        summary = materialx_project_preflight.evaluate_preflight(
            make_ledger(), make_capacity(worker_state="blocked")
        )

        self.assertFalse(summary["ok"])
        self.assertEqual(
            summary["failures"],
            [
                "workers: blend05 is blocked, not active",
                "workers: blendit is blocked, not active",
                "workers: blendit04 is blocked, not active",
                "workers: blendit2 is blocked, not active",
                "workers: blendit3 is blocked, not active",
            ],
        )

    def test_accepts_alerted_worker_blocker_until_capacity_recovers(self):
        summary = materialx_project_preflight.evaluate_preflight(
            make_ledger(), make_capacity(worker_state="exited")
        )

        self.assertFalse(summary["ok"])

    def test_fails_closed_when_completed_rows_lack_evidence_or_journal_records(self):
        completed_id = "ND_test_0000"
        capacity = make_capacity([completed_id])
        capacity["evidence_records"] = []
        capacity["journal_records"] = []

        summary = materialx_project_preflight.evaluate_preflight(make_ledger(completed_ids=[completed_id]), capacity)

        self.assertFalse(summary["ok"])
        self.assertEqual(
            summary["failures"],
            [
                "completed_rows: ND_test_0000 is missing an evidence record",
                "completed_rows: ND_test_0000 is missing a journal record",
            ],
        )

    def test_fails_closed_when_completed_rows_lack_ledger_evidence(self):
        completed_id = "ND_test_0000"
        summary = materialx_project_preflight.evaluate_preflight(
            make_ledger(completed_ids=[completed_id], include_ledger_evidence=False),
            make_capacity([completed_id]),
        )

        self.assertFalse(summary["ok"])
        self.assertEqual(
            summary["failures"],
            ["completed_rows: ND_test_0000 is missing ledger evidence"],
        )

    def test_reports_independent_lane_readiness_without_legacy_names(self):
        capacity = make_capacity(windows_state="due")
        capacity["lanes"]["local_cpu"] = {"state": "due", "last_evidence_id": ""}
        summary = materialx_project_preflight.evaluate_preflight(make_ledger(), capacity)

        self.assertEqual(summary["lanes"]["local_cpu"]["state"], "due")
        self.assertEqual(summary["lanes"]["windows_a40_cuda"]["state"], "due")
        self.assertNotIn("windows_local_build", summary)

    def test_explicitly_migrates_v1_only_at_input_boundary(self):
        legacy = {
            "schema_version": 1,
            "healthy_workers": [],
            "completed_rows": [],
            "evidence_records": [],
            "journal_records": [],
            "lanes": {"windows_local_build": {"state": "ready", "alerted": False}},
            "capacity_journal": [],
            "alerts": [],
        }

        summary = materialx_project_preflight.evaluate_preflight(make_ledger(), legacy)

        self.assertFalse(summary["ok"])
        self.assertEqual(summary["lanes"]["windows_a40_cuda"]["state"], "due")


if __name__ == "__main__":
    unittest.main()
