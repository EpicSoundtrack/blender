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

        exit_code = run_supervisor(
            SupervisorConfig(1),
            controller=FakeController([cycle]),
            state_store=store,
            clock=FakeClock(10),
            sleeper=FakeSleeper(),
            once=True,
        )

        self.assertEqual(exit_code, 1)
        state = store.commits[0]
        self.assertEqual(
            sum(worker["state"] == "active" for worker in state["workers"]), 4
        )
        self.assertIn("worker_blocked", state["failure_classifications"])
        self.assertIn("proxy_failure", state["failure_classifications"])

    def test_queue_worker_count_and_empty_queue_fail_closed_categorically(self):
        cases = [
            (healthy_cycle(queue_depth=4), "queue_below_watermark"),
            (healthy_cycle(queue_depth=0), "queue_empty"),
        ]
        short = healthy_cycle()
        short["workers"].pop()
        cases.append((short, "worker_count"))
        long = healthy_cycle()
        long["workers"].append({"id": "extra", "state": "idle"})
        cases.append((long, "worker_count"))

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
            once=True,
        )

        self.assertEqual(exit_code, 1)
        state = store.commits[0]
        self.assertEqual(state["cycle_sequence"], 5)
        self.assertEqual(state["last_successful_poll"], 10.0)
        self.assertEqual(
            state["failure_classifications"],
            ["controller_exception", "stale_poll"],
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

        exit_code = run_supervisor(
            SupervisorConfig(1),
            controller=FakeController([cycle]),
            state_store=store,
            clock=FakeClock(10),
            sleeper=FakeSleeper(),
            once=True,
        )

        self.assertEqual(exit_code, 1)
        state = store.commits[0]
        self.assertEqual(len(state["integration_receipts"]), 2)
        self.assertIn("integration_failure", state["failure_classifications"])
        self.assertIn("merge_failure", state["failure_classifications"])

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
        self.assertIn("invalid_clock", store.commits[0]["failure_classifications"])
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
