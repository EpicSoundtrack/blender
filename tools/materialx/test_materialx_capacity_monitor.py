#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import copy
import unittest

import materialx_capacity_monitor
import materialx_project_state


WORKERS = ("blend05", "blendit04", "blendit", "blendit2", "blendit3")


def make_capacity():
    return materialx_project_state.update_horde_observation(
        materialx_project_state.new_project_state(),
        worker_states={worker: "active" for worker in WORKERS},
        evidence_receipt="cycle-previous",
    )


class FakeProbe:
    def __init__(
        self,
        worker_state="active",
        *,
        worker_states=None,
        log="private probe detail",
        windows_receipt=None,
    ):
        self.worker_state = worker_state
        self.worker_states = dict(worker_states or {})
        self.log = log
        self.windows_receipt = copy.deepcopy(windows_receipt)
        self.worker_calls = []
        self.windows_calls = 0

    def worker_process_log_state(self, worker_id):
        self.worker_calls.append(worker_id)
        return {
            "state": self.worker_states.get(worker_id, self.worker_state),
            "log": self.log,
            "evidence_receipt": "cycle-current",
        }

    def windows_a40_cuda_evidence(self):
        self.windows_calls += 1
        return copy.deepcopy(self.windows_receipt)


class MaterialXCapacityMonitorTest(unittest.TestCase):
    def test_records_exact_workers_and_horde_lane_without_changing_other_lanes(self):
        previous = make_capacity()
        previous_lanes = copy.deepcopy(previous["lanes"])

        result = materialx_capacity_monitor.poll_capacity(previous, FakeProbe())

        state = result["capacity_state"]
        self.assertTrue(all(worker["state"] == "active" for worker in state["workers"]))
        self.assertTrue(all(
            worker["last_evidence_id"] == "cycle-current"
            for worker in state["workers"]
        ))
        self.assertEqual(state["lanes"]["horde"], {
            "state": "active",
            "last_evidence_id": "cycle-current",
        })
        for lane in ("local_cpu", "local_cuda", "windows_a40_cuda", "golden_review"):
            self.assertEqual(state["lanes"][lane], previous_lanes[lane])
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
            next(worker for worker in state["workers"] if worker["id"] == "blend05")["state"],
            "exited",
        )
        self.assertEqual(result["new_alerts"], [{
            "failure_class": "capacity_loss",
            "subject": "worker:blend05",
        }])
        self.assertNotIn("secret", materialx_capacity_monitor.monitor_as_json(result))

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

    def test_windows_green_requires_own_matching_current_generation_receipt(self):
        state = materialx_project_state.credit_integration(
            make_capacity(),
            newly_integrated_nodedefs=128,
            render_path_edit=False,
            batch_id="windows-due",
        )
        receipt = {
            "schema_version": 1,
            "receipt_id": "windows-a40-0001",
            "lane": "windows_a40_cuda",
            "evidence_type": "windows_a40_cuda_render",
            "milestone_generation": state["milestones"]["generation"],
            "numeric_exits": [0],
        }

        result = materialx_capacity_monitor.poll_capacity(
            state,
            FakeProbe(windows_receipt=receipt),
        )

        self.assertEqual(result["capacity_state"]["lanes"]["windows_a40_cuda"], {
            "state": "green",
            "last_evidence_id": "windows-a40-0001",
        })
        self.assertEqual(result["capacity_state"]["lanes"]["local_cpu"]["state"], "due")
        self.assertEqual(result["capacity_state"]["lanes"]["local_cuda"]["state"], "due")

    def test_invalid_windows_receipt_fails_before_state_is_returned(self):
        state = materialx_project_state.credit_integration(
            make_capacity(),
            newly_integrated_nodedefs=128,
            render_path_edit=False,
            batch_id="windows-due",
        )
        receipt = {
            "schema_version": 1,
            "receipt_id": "windows-a40-bad",
            "lane": "windows_a40_cuda",
            "evidence_type": "windows_a40_cuda_render",
            "milestone_generation": state["milestones"]["generation"],
            "numeric_exits": [1],
        }
        with self.assertRaises(ValueError):
            materialx_capacity_monitor.poll_capacity(
                state,
                FakeProbe(windows_receipt=receipt),
            )

    def test_translates_stale_source_auth_and_proxy_without_raw_details(self):
        for state, failure_class in (
            ("stale_source", "stale_source"),
            ("auth_failure", "auth_failure"),
            ("proxy_failure", "proxy_failure"),
        ):
            with self.subTest(state=state):
                result = materialx_capacity_monitor.poll_capacity(
                    make_capacity(),
                    FakeProbe(
                        worker_states={"blend05": state},
                        log="credential=never-copy",
                    ),
                )
                self.assertEqual(result["new_alerts"], [{
                    "failure_class": failure_class,
                    "subject": "worker:blend05",
                }])
                self.assertNotIn("credential", materialx_capacity_monitor.monitor_as_json(result))

    def test_rejects_unknown_worker_set_before_any_probe_side_effect(self):
        capacity = make_capacity()
        capacity["workers"][0] = {
            "id": "rogue",
            "state": "active",
            "last_evidence_id": "cycle",
        }
        probe = FakeProbe()

        with self.assertRaises(ValueError):
            materialx_capacity_monitor.poll_capacity(capacity, probe)

        self.assertEqual(probe.worker_calls, [])
        self.assertEqual(probe.windows_calls, 0)


if __name__ == "__main__":
    unittest.main()
