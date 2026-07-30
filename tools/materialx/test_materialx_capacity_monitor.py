#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest

import materialx_capacity_monitor


WORKERS = ("blend05", "blendit04", "blendit", "blendit2", "blendit3")


def make_capacity():
    return {
        "schema_version": 1,
        "healthy_workers": [
            {"id": worker_id, "state": "active"} for worker_id in WORKERS
        ],
        "completed_rows": [],
        "evidence_records": [],
        "journal_records": [],
        "lanes": {"windows_local_build": {"state": "ready", "alerted": False}},
        "capacity_journal": [],
        "alerts": [],
    }


class FakeProbe:
    def __init__(
        self,
        worker_state="active",
        *,
        worker_states=None,
        build_state="ready",
        log="private probe detail",
    ):
        self.worker_state = worker_state
        self.worker_states = dict(worker_states or {})
        self.build_state = build_state
        self.log = log
        self.worker_calls = []
        self.build_calls = 0

    def worker_process_log_state(self, worker_id):
        self.worker_calls.append(worker_id)
        return {
            "state": self.worker_states.get(worker_id, self.worker_state),
            "log": self.log,
        }

    def windows_local_build_log_state(self):
        self.build_calls += 1
        return {"state": self.build_state, "log": self.log}


class MaterialXCapacityMonitorTest(unittest.TestCase):
    def test_records_healthy_worker_and_build_lane_without_alerts(self):
        result = materialx_capacity_monitor.poll_capacity(make_capacity(), FakeProbe())

        state = result["capacity_state"]
        self.assertEqual(
            state["healthy_workers"],
            [
                {"id": worker_id, "state": "active"}
                for worker_id in sorted(WORKERS)
            ],
        )
        self.assertEqual(state["lanes"]["windows_local_build"], {"state": "ready", "alerted": False})
        self.assertEqual(
            state["capacity_journal"],
            [{
                "kind": "windows_local_build",
                "log_observed": True,
                "state": "ready",
                "subject": "windows_local_build",
            }] + [{
                "kind": "worker_process",
                "log_observed": True,
                "state": "active",
                "subject": worker_id,
            } for worker_id in sorted(WORKERS)],
        )
        self.assertEqual(result["new_alerts"], [])

    def test_records_sanitized_alert_when_worker_process_exits(self):
        result = materialx_capacity_monitor.poll_capacity(
            make_capacity(),
            FakeProbe(
                worker_states={"blend05": "exited"},
                log="secret=never-copy",
            ),
        )

        state = result["capacity_state"]
        self.assertEqual(
            next(worker for worker in state["healthy_workers"] if worker["id"] == "blend05"),
            {"id": "blend05", "state": "exited"},
        )
        self.assertEqual(result["new_alerts"], [{
            "failure_class": "capacity_loss",
            "subject": "worker:blend05",
        }])
        self.assertNotIn("secret", materialx_capacity_monitor.monitor_as_json(result))

    def test_retains_each_worker_identity_instead_of_collapsing_capacity_loss(self):
        capacity = make_capacity()
        result = materialx_capacity_monitor.poll_capacity(
            capacity,
            FakeProbe(worker_states={
                "blend05": "exited",
                "blendit04": "exited",
            }),
        )

        self.assertEqual(result["new_alerts"], [
            {"failure_class": "capacity_loss", "subject": "worker:blend05"},
            {"failure_class": "capacity_loss", "subject": "worker:blendit04"},
        ])

    def test_existing_capacity_alert_deduplicates_by_exact_class_and_subject(self):
        first = materialx_capacity_monitor.poll_capacity(
            make_capacity(), FakeProbe(worker_states={"blend05": "exited"})
        )
        second = materialx_capacity_monitor.poll_capacity(
            first["capacity_state"],
            FakeProbe(worker_states={"blend05": "exited"}),
        )

        self.assertEqual(second["new_alerts"], [])
        self.assertEqual(second["current_alerts"], [{
            "failure_class": "capacity_loss",
            "subject": "worker:blend05",
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
                super().__init__(worker_states={"blend05": state})
                self.state = state

            def worker_process_log_state(self, worker_id):
                result = super().worker_process_log_state(worker_id)
                return {**result, "log": "credential=never-copy"}

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
                    "subject": "worker:blend05",
                }])
                self.assertNotIn("credential", materialx_capacity_monitor.monitor_as_json(result))

    def test_rejects_unknown_worker_set_before_any_probe_side_effect(self):
        capacity = make_capacity()
        capacity["healthy_workers"][0] = {"id": "rogue", "state": "active"}
        probe = FakeProbe()

        with self.assertRaises(ValueError):
            materialx_capacity_monitor.poll_capacity(capacity, probe)

        self.assertEqual(probe.worker_calls, [])
        self.assertEqual(probe.build_calls, 0)


if __name__ == "__main__":
    unittest.main()
