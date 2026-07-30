"""Tests for the recurring MaterialX Horde supervisor."""

from __future__ import annotations

import copy
import json
import math
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from materialx_horde_supervisor import (
    AtomicJSONStateStore,
    SupervisorConfig,
    run_supervisor,
)
from materialx_alert_sink import SanitizedAlertSink


WORKERS = ("blend05", "blendit04", "blendit", "blendit2", "blendit3")
SECRET = "private prompt and credential"


def integrated(batch_id: str, layer: str = "native_cycles"):
    return {
        "batch_id": batch_id,
        "layer": layer,
        "base_sha": "a" * 40,
        "head_sha": "b" * 40,
        "focused_commands": ["python private-command.py"],
        "numeric_exits": [0],
        "final_state": "integrated",
    }


def healthy_cycle(*, queue_depth=5):
    return {
        "workers": [
            {"id": worker, "state": "active", "batch_id": f"dispatch-{worker}"}
            for worker in WORKERS
        ],
        "assigned_batches": [
            {"worker_id": "blend05", "batch_id": "next-a"},
            {"worker_id": "blendit2", "batch_id": "next-b"},
        ],
        "artifacts": [{"raw": SECRET}],
        "integration_receipts": [
            integrated("finished-a"),
            integrated("finished-b", "hydra_ovrtx"),
        ],
        "alerts": [],
        "journal": [{"evidence": SECRET}],
        "queue_depth": queue_depth,
    }


class FakeController:
    def __init__(self, results):
        self.results = list(results)
        self.calls = 0

    def run_cycle(self):
        result = self.results[self.calls]
        self.calls += 1
        if isinstance(result, Exception):
            raise result
        return copy.deepcopy(result)


class FakeStateStore:
    def __init__(self, initial=None, failure=None):
        self.initial = copy.deepcopy(initial)
        self.failure = failure
        self.commits = []

    def load(self):
        return copy.deepcopy(self.initial)

    def commit(self, state):
        if self.failure is not None:
            raise self.failure
        self.commits.append(copy.deepcopy(state))


class FakeClock:
    def __init__(self, *values):
        self.values = list(values)
        self.calls = 0

    def now(self):
        value = self.values[self.calls]
        self.calls += 1
        return value


class FakeSleeper:
    def __init__(self):
        self.calls = []

    def sleep(self, seconds):
        self.calls.append(seconds)


class FakeAlertTransport:
    def __init__(self, events=None, fail=False):
        self.events = events
        self.fail = fail
        self.messages = []

    def send(self, message):
        self.messages.append(copy.deepcopy(message))
        if self.events is not None:
            self.events.append(("send", message["failure_class"], message["subject"]))
        if self.fail:
            raise RuntimeError(SECRET)
        return f"delivery-{len(self.messages)}"


class OrderedStateStore(FakeStateStore):
    def __init__(self, events):
        super().__init__()
        self.events = events

    def commit(self, state):
        self.events.append(("commit",))
        super().commit(state)


class SupervisorConfigTest(unittest.TestCase):
    def test_strictly_validates_interval_staleness_and_watermark(self):
        for interval in (0, -1, math.inf, math.nan, True, "1"):
            with self.subTest(interval=interval):
                with self.assertRaises(ValueError):
                    SupervisorConfig(interval)
        for stale in (0, -1, True, 1.0):
            with self.subTest(stale=stale):
                with self.assertRaises(ValueError):
                    SupervisorConfig(1, stale_intervals=stale)
        for watermark in (0, 4, True, 5.0):
            with self.subTest(watermark=watermark):
                with self.assertRaises(ValueError):
                    SupervisorConfig(1, queue_watermark=watermark)

        self.assertEqual(
            SupervisorConfig(0.25, stale_intervals=3, queue_watermark=7),
            SupervisorConfig(0.25, stale_intervals=3, queue_watermark=7),
        )


class MaterialXHordeSupervisorTest(unittest.TestCase):
    def test_once_commits_same_cycle_harvest_integration_and_refill_evidence(self):
        controller = FakeController([healthy_cycle()])
        store = FakeStateStore()

        exit_code = run_supervisor(
            SupervisorConfig(5),
            controller=controller,
            state_store=store,
            clock=FakeClock(100),
            sleeper=FakeSleeper(),
            once=True,
        )

        self.assertEqual(exit_code, 0)
        self.assertEqual(controller.calls, 1)
        self.assertEqual(len(store.commits), 1)
        state = store.commits[0]
        self.assertTrue(state["healthy"])
        self.assertEqual(
            [receipt["batch_id"] for receipt in state["integration_receipts"]],
            ["finished-a", "finished-b"],
        )
        self.assertEqual(
            [assignment["batch_id"] for assignment in state["assigned_batches"]],
            ["next-a", "next-b"],
        )
        self.assertEqual(len(state["workers"]), 5)
        self.assertNotIn(SECRET, json.dumps(state))
        self.assertNotIn("private-command", json.dumps(state))

    def test_recurring_mode_runs_once_per_interval_and_stops_boundedly(self):
        controller = FakeController([healthy_cycle()] * 3)
        store = FakeStateStore()
        sleeper = FakeSleeper()

        exit_code = run_supervisor(
            SupervisorConfig(2.5),
            controller=controller,
            state_store=store,
            clock=FakeClock(1, 2, 3),
            sleeper=sleeper,
            max_cycles=3,
        )

        self.assertEqual(exit_code, 0)
        self.assertEqual(controller.calls, 3)
        self.assertEqual(len(store.commits), 3)
        self.assertEqual(sleeper.calls, [2.5, 2.5])

    def test_one_blocked_worker_is_unhealthy_while_other_four_are_preserved(self):
        cycle = healthy_cycle()
        cycle["workers"][0] = {"id": "blend05", "state": "blocked"}
        cycle["alerts"] = [
            {"worker_id": "blend05", "classification": "proxy_failure"}
        ]
        store = FakeStateStore()

        transport = FakeAlertTransport()
        exit_code = run_supervisor(
            SupervisorConfig(1),
            controller=FakeController([cycle]),
            state_store=store,
            clock=FakeClock(10),
            sleeper=FakeSleeper(),
            alert_sink=SanitizedAlertSink(transport),
            once=True,
        )

        self.assertEqual(exit_code, 1)
        state = store.commits[0]
        self.assertEqual(
            sum(worker["state"] == "active" for worker in state["workers"]), 4
        )
        self.assertIn("capacity_loss", state["failure_classifications"])
        self.assertIn("proxy_failure", state["failure_classifications"])
        self.assertEqual(
            [(item["failure_class"], item["subject"]) for item in state["alerts"]],
            [
                ("capacity_loss", "worker:blend05"),
                ("proxy_failure", "worker:blend05"),
            ],
        )

    def test_queue_worker_count_and_empty_queue_fail_closed_categorically(self):
        cases = [
            (healthy_cycle(queue_depth=4), "capacity_loss"),
            (healthy_cycle(queue_depth=0), "queue_empty"),
        ]
        short = healthy_cycle()
        short["workers"].pop()
        cases.append((short, "capacity_loss"))
        long = healthy_cycle()
        long["workers"].append({"id": "extra", "state": "idle"})
        cases.append((long, "capacity_loss"))

        for cycle, classification in cases:
            with self.subTest(classification=classification):
                store = FakeStateStore()
                exit_code = run_supervisor(
                    SupervisorConfig(1),
                    controller=FakeController([cycle]),
                    state_store=store,
                    clock=FakeClock(10),
                    sleeper=FakeSleeper(),
                    once=True,
                )
                self.assertEqual(exit_code, 1)
                self.assertIn(
                    classification,
                    store.commits[0]["failure_classifications"],
                )

    def test_stale_previous_success_and_controller_exception_are_finite(self):
        store = FakeStateStore(
            {"last_successful_poll": 10.0, "cycle_sequence": 4}
        )

        exit_code = run_supervisor(
            SupervisorConfig(5, stale_intervals=2),
            controller=FakeController([RuntimeError(SECRET)]),
            state_store=store,
            clock=FakeClock(20),
            sleeper=FakeSleeper(),
            alert_sink=SanitizedAlertSink(FakeAlertTransport()),
            once=True,
        )

        self.assertEqual(exit_code, 1)
        state = store.commits[0]
        self.assertEqual(state["cycle_sequence"], 5)
        self.assertEqual(state["last_successful_poll"], 10.0)
        self.assertEqual(
            state["failure_classifications"],
            ["capacity_loss"],
        )
        self.assertNotIn(SECRET, json.dumps(state))

    def test_integration_rejection_is_unhealthy_but_other_receipt_survives(self):
        cycle = healthy_cycle()
        cycle["integration_receipts"][1] = {
            **integrated("finished-b", "hydra_ovrtx"),
            "final_state": "rejected",
            "failure_classification": "merge_failure",
        }
        store = FakeStateStore()

        transport = FakeAlertTransport()
        exit_code = run_supervisor(
            SupervisorConfig(1),
            controller=FakeController([cycle]),
            state_store=store,
            clock=FakeClock(10),
            sleeper=FakeSleeper(),
            alert_sink=SanitizedAlertSink(transport),
            once=True,
        )

        self.assertEqual(exit_code, 1)
        state = store.commits[0]
        self.assertEqual(len(state["integration_receipts"]), 2)
        self.assertIn("integration_failure", state["failure_classifications"])
        self.assertNotIn("merge_failure", state["failure_classifications"])
        self.assertEqual(
            state["alerts"][0]["subject"],
            "lane:hydra_ovrtx",
        )

    def test_explicit_finite_mapping_delivers_before_atomic_commit(self):
        events = []
        cycle = healthy_cycle(queue_depth=0)
        cycle["workers"][0] = {"id": "blend05", "state": "blocked"}
        cycle["alerts"] = [
            {"worker_id": "blend05", "classification": "source_sync_failure"},
            {"worker_id": "blendit04", "classification": "auth_failure"},
            {"worker_id": "blendit", "classification": "proxy_failure"},
            {"worker_id": "blendit2", "classification": "invalid_completion"},
            {"worker_id": "blendit3", "classification": "process_missing"},
            {"worker_id": "blend05", "classification": "queue_empty"},
        ]
        transport = FakeAlertTransport(events)
        store = OrderedStateStore(events)

        exit_code = run_supervisor(
            SupervisorConfig(1),
            controller=FakeController([cycle]),
            state_store=store,
            clock=FakeClock(10),
            sleeper=FakeSleeper(),
            alert_sink=SanitizedAlertSink(transport),
            once=True,
        )

        self.assertEqual(exit_code, 1)
        self.assertEqual(events[-1], ("commit",))
        self.assertTrue(all(event[0] == "send" for event in events[:-1]))
        pairs = {
            (alert["failure_class"], alert["subject"])
            for alert in store.commits[0]["alerts"]
        }
        self.assertEqual(pairs, {
            ("stale_source", "worker:blend05"),
            ("auth_failure", "worker:blendit04"),
            ("proxy_failure", "worker:blendit"),
            ("invalid_completion", "worker:blendit2"),
            ("capacity_loss", "worker:blendit3"),
            ("queue_empty", "queue"),
            ("capacity_loss", "worker:blend05"),
        })
        self.assertTrue(all(
            set(alert) == {
                "failure_class",
                "subject",
                "timestamp",
                "delivery_state",
                "receipt_id",
            }
            for alert in store.commits[0]["alerts"]
        ))

    def test_transport_failure_is_visible_and_does_not_discard_cycle_work(self):
        cycle = healthy_cycle(queue_depth=0)
        transport = FakeAlertTransport(fail=True)
        store = FakeStateStore()

        exit_code = run_supervisor(
            SupervisorConfig(1),
            controller=FakeController([cycle]),
            state_store=store,
            clock=FakeClock(10),
            sleeper=FakeSleeper(),
            alert_sink=SanitizedAlertSink(transport),
            once=True,
        )

        self.assertEqual(exit_code, 1)
        state = store.commits[0]
        self.assertEqual(len(state["integration_receipts"]), 2)
        self.assertEqual(len(state["assigned_batches"]), 2)
        self.assertIn("transport_failure", state["failure_classifications"])
        self.assertIn({
            "failure_class": "queue_empty",
            "subject": "queue",
            "timestamp": 10.0,
            "delivery_state": "unsent",
        }, state["alerts"])
        self.assertIn({
            "failure_class": "transport_failure",
            "subject": "runtime",
            "timestamp": 10.0,
            "delivery_state": "unsent",
        }, state["alerts"])
        self.assertNotIn(SECRET, json.dumps(state))

    def test_unknown_worker_identity_fails_closed_without_reaching_transport(self):
        cycle = healthy_cycle()
        cycle["workers"][0] = {"id": "rogue", "state": "blocked"}
        cycle["alerts"] = [
            {"worker_id": "rogue", "classification": "proxy_failure"},
        ]
        transport = FakeAlertTransport()
        store = FakeStateStore()

        exit_code = run_supervisor(
            SupervisorConfig(1),
            controller=FakeController([cycle]),
            state_store=store,
            clock=FakeClock(10),
            sleeper=FakeSleeper(),
            alert_sink=SanitizedAlertSink(transport),
            once=True,
        )

        self.assertEqual(exit_code, 1)
        self.assertNotIn("rogue", json.dumps(store.commits[0]))
        self.assertNotIn("rogue", json.dumps(transport.messages))
        self.assertEqual(
            {(item["failure_class"], item["subject"]) for item in store.commits[0]["alerts"]},
            {("capacity_loss", "runtime")},
        )

    def test_unknown_integration_layer_fails_closed_without_reaching_transport(self):
        cycle = healthy_cycle()
        cycle["integration_receipts"][0]["layer"] = "rogue"
        transport = FakeAlertTransport()
        store = FakeStateStore()

        exit_code = run_supervisor(
            SupervisorConfig(1),
            controller=FakeController([cycle]),
            state_store=store,
            clock=FakeClock(10),
            sleeper=FakeSleeper(),
            alert_sink=SanitizedAlertSink(transport),
            once=True,
        )

        self.assertEqual(exit_code, 1)
        self.assertNotIn("rogue", json.dumps(store.commits[0]))
        self.assertNotIn("rogue", json.dumps(transport.messages))
        self.assertIn(
            {
                "failure_class": "invalid_completion",
                "subject": "runtime",
                "timestamp": 10.0,
                "delivery_state": "sent",
                "receipt_id": "delivery-1",
            },
            store.commits[0]["alerts"],
        )

    def test_unchanged_failure_sends_once_and_recovery_then_recurrence_resends(self):
        empty = healthy_cycle(queue_depth=0)
        transport = FakeAlertTransport()
        store = FakeStateStore()
        sink = SanitizedAlertSink(transport)

        run_supervisor(
            SupervisorConfig(1),
            controller=FakeController([empty, healthy_cycle(), empty]),
            state_store=store,
            clock=FakeClock(1, 2, 3),
            sleeper=FakeSleeper(),
            alert_sink=sink,
            max_cycles=3,
        )

        self.assertEqual(len(transport.messages), 2)
        self.assertEqual(store.commits[1]["alerts"], [])
        self.assertEqual(store.commits[2]["alerts"][0]["timestamp"], 3.0)

    def test_state_store_failure_makes_cycle_unhealthy_without_retry_loop(self):
        controller = FakeController([healthy_cycle()])
        store = FakeStateStore(failure=OSError(SECRET))

        exit_code = run_supervisor(
            SupervisorConfig(1),
            controller=controller,
            state_store=store,
            clock=FakeClock(10),
            sleeper=FakeSleeper(),
            once=True,
        )

        self.assertEqual(exit_code, 1)
        self.assertEqual(controller.calls, 1)

    def test_nonfinite_clock_is_categorical_and_never_persisted(self):
        store = FakeStateStore()

        exit_code = run_supervisor(
            SupervisorConfig(1),
            controller=FakeController([healthy_cycle()]),
            state_store=store,
            clock=FakeClock(math.inf),
            sleeper=FakeSleeper(),
            once=True,
        )

        self.assertEqual(exit_code, 1)
        self.assertIn("capacity_loss", store.commits[0]["failure_classifications"])
        self.assertNotIn("Infinity", json.dumps(store.commits[0]))


class AtomicJSONStateStoreTest(unittest.TestCase):
    def test_commit_atomically_replaces_target_and_round_trips(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "state.json"
            path.write_text('{"old":true}\n', encoding="utf-8")
            store = AtomicJSONStateStore(path)
            state = {"schema_version": 2, "healthy": True}

            store.commit(state)

            self.assertEqual(store.load(), state)
            self.assertEqual(json.loads(path.read_text(encoding="utf-8")), state)
            self.assertEqual(list(path.parent.glob(f".{path.name}.*.tmp")), [])

    def test_replace_failure_preserves_old_target_and_cleans_temp(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "state.json"
            path.write_text('{"old":true}\n', encoding="utf-8")
            store = AtomicJSONStateStore(path)

            with mock.patch.object(
                os, "replace", side_effect=OSError("replace failed")
            ):
                with self.assertRaises(OSError):
                    store.commit({"schema_version": 2})

            self.assertEqual(json.loads(path.read_text(encoding="utf-8")), {"old": True})
            self.assertEqual(list(path.parent.glob(f".{path.name}.*.tmp")), [])


if __name__ == "__main__":
    unittest.main()
