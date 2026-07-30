#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest

import materialx_capacity_monitor


def make_capacity():
    return {
        "schema_version": 1,
        "healthy_workers": [{"id": "worker-a", "state": "active"}],
        "completed_rows": [],
        "evidence_records": [],
        "journal_records": [],
        "lanes": {"windows_local_build": {"state": "ready", "alerted": False}},
        "capacity_journal": [],
        "alerts": [],
    }


class FakeProbe:
    def __init__(self, worker_state="active", build_state="ready", log="private probe detail"):
        self.worker_state = worker_state
        self.build_state = build_state
        self.log = log

    def worker_process_log_state(self, worker_id):
        return {"state": self.worker_state, "log": self.log}

    def windows_local_build_log_state(self):
        return {"state": self.build_state, "log": self.log}


class MaterialXCapacityMonitorTest(unittest.TestCase):
    def test_records_healthy_worker_and_build_lane_without_alerts(self):
        result = materialx_capacity_monitor.poll_capacity(make_capacity(), FakeProbe())

        state = result["capacity_state"]
        self.assertEqual(state["healthy_workers"], [{"id": "worker-a", "state": "active"}])
        self.assertEqual(state["lanes"]["windows_local_build"], {"state": "ready", "alerted": False})
        self.assertEqual(state["capacity_journal"], [
            {"kind": "windows_local_build", "log_observed": True, "state": "ready", "subject": "windows_local_build"},
            {"kind": "worker_process", "log_observed": True, "state": "active", "subject": "worker-a"},
        ])
        self.assertEqual(result["new_alerts"], [])

    def test_records_sanitized_alert_when_worker_process_exits(self):
        result = materialx_capacity_monitor.poll_capacity(make_capacity(), FakeProbe(worker_state="exited", log="secret=never-copy"))

        state = result["capacity_state"]
        self.assertEqual(state["healthy_workers"], [{"id": "worker-a", "state": "exited"}])
        self.assertEqual(result["new_alerts"], [{
            "failure_class": "capacity_loss",
            "subject": "worker:worker-a",
        }])
        self.assertNotIn("secret", materialx_capacity_monitor.monitor_as_json(result))

    def test_retains_each_worker_identity_instead_of_collapsing_capacity_loss(self):
        capacity = make_capacity()
        capacity["healthy_workers"].append({"id": "worker-b", "state": "active"})
        result = materialx_capacity_monitor.poll_capacity(
            capacity, FakeProbe(worker_state="exited")
        )

        self.assertEqual(result["new_alerts"], [
            {"failure_class": "capacity_loss", "subject": "worker:worker-a"},
            {"failure_class": "capacity_loss", "subject": "worker:worker-b"},
        ])

    def test_existing_capacity_alert_deduplicates_by_exact_class_and_subject(self):
        first = materialx_capacity_monitor.poll_capacity(
            make_capacity(), FakeProbe(worker_state="exited")
        )
        second = materialx_capacity_monitor.poll_capacity(
            first["capacity_state"], FakeProbe(worker_state="exited")
        )

        self.assertEqual(second["new_alerts"], [])
        self.assertEqual(second["current_alerts"], [{
            "failure_class": "capacity_loss",
            "subject": "worker:worker-a",
        }])

    def test_marks_blocked_build_lane_alerted_and_journals_it(self):
        result = materialx_capacity_monitor.poll_capacity(make_capacity(), FakeProbe(build_state="blocked"))

        self.assertEqual(result["capacity_state"]["lanes"]["windows_local_build"], {"state": "blocked", "alerted": True})
        self.assertEqual(result["new_alerts"], [{
            "failure_class": "capacity_loss",
            "subject": "lane:windows_local_build",
        }])

    def test_translates_stale_source_auth_and_proxy_without_raw_details(self):
        class DetailedProbe(FakeProbe):
            def __init__(self, state):
                super().__init__()
                self.state = state

            def worker_process_log_state(self, worker_id):
                return {"state": self.state, "log": "credential=never-copy"}

        for state, failure_class in (
            ("stale_source", "stale_source"),
            ("auth_failure", "auth_failure"),
            ("proxy_failure", "proxy_failure"),
        ):
            with self.subTest(state=state):
                result = materialx_capacity_monitor.poll_capacity(
                    make_capacity(), DetailedProbe(state)
                )
                self.assertEqual(result["new_alerts"], [{
                    "failure_class": failure_class,
                    "subject": "worker:worker-a",
                }])
                self.assertNotIn("credential", materialx_capacity_monitor.monitor_as_json(result))


if __name__ == "__main__":
    unittest.main()
