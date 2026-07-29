"""Tests for the bounded operational Horde controller adapter."""

from __future__ import annotations

import base64
import json
from pathlib import Path
import tempfile
import unittest

from materialx_horde_dispatch import CommandResult, HordeBackend
from materialx_horde_operational import HordeOperationalAdapter, run_operational_controller_cycle


PROMPT = "private MaterialX prompt"
CREDENTIAL = "private-credential"
RAW_LOG = "private remote log"
ENCODED_PROMPT = base64.b64encode(PROMPT.encode("utf-8")).decode("ascii")
COMMAND_LINE = f"123 python3 hermes_runner.py {ENCODED_PROMPT}"


class FakeRunner:
    def __init__(self, results):
        self.results = results
        self.calls = []

    def __call__(self, command, *, env, input_text, timeout):
        self.calls.append(command)
        return self.results.get(command, CommandResult(0, "absent", ""))


class FakeDispatcher:
    def __init__(self, outcomes):
        self.outcomes = outcomes
        self.calls = []

    def __call__(self, worker_id, batch):
        self.calls.append((worker_id, batch["batch_id"]))
        return self.outcomes[(worker_id, batch["batch_id"])]


class MaterialXHordeOperationalTest(unittest.TestCase):
    @staticmethod
    def backend():
        return HordeBackend({"first": {"host": "first"}, "second": {"host": "second"}, "active": {"host": "active"}})

    def test_active_worker_is_retained_without_harvest_or_dispatch(self):
        backend = self.backend()
        runner = FakeRunner({backend.process_command("active"): CommandResult(0, "active:321", "")})
        adapter = HordeOperationalAdapter(backend=backend, runner=runner, dispatch_one=FakeDispatcher({}))

        result = run_operational_controller_cycle(
            workers=[{"id": "active", "batch_id": "running"}], queued_batches=[], adapter=adapter
        )

        self.assertEqual(result["workers"], [{"id": "active", "state": "active"}])
        self.assertEqual(len(runner.calls), 1)
        self.assertEqual(result["journal"], [{"worker_id": "active", "event": "process_active", "evidence": "pid"}])

    def test_absent_workers_harvest_exact_success_and_failure_sentinels(self):
        backend = self.backend()
        runner = FakeRunner({
            backend.process_command("first"): CommandResult(0, "absent", ""),
            backend.harvest_command("first", "finished-success"): CommandResult(0, "success", ""),
            backend.process_command("second"): CommandResult(0, "absent", ""),
            backend.harvest_command("second", "finished-failure"): CommandResult(0, "failure", ""),
        })
        dispatcher = FakeDispatcher({("first", "next"): {"outcome": "success"}})
        adapter = HordeOperationalAdapter(backend=backend, runner=runner, dispatch_one=dispatcher)

        result = run_operational_controller_cycle(
            workers=[{"id": "first", "batch_id": "finished-success"}, {"id": "second", "batch_id": "finished-failure"}],
            queued_batches=[{"batch_id": "next", "prompt": PROMPT}], adapter=adapter,
        )

        self.assertEqual(dispatcher.calls, [("first", "next")])
        self.assertEqual(result["workers"], [{"id": "first", "state": "active"}, {"id": "second", "state": "blocked"}])
        self.assertIn({"worker_id": "second", "classification": "harvest_failure"}, result["alerts"])

    def test_missing_malformed_or_active_harvest_evidence_blocks_only_that_worker(self):
        backend = self.backend()
        runner = FakeRunner({
            backend.process_command("first"): CommandResult(0, "absent", ""),
            backend.harvest_command("first", "finished-first"): CommandResult(0, "invalid", RAW_LOG),
            backend.process_command("second"): CommandResult(0, COMMAND_LINE, ""),
        })
        adapter = HordeOperationalAdapter(backend=backend, runner=runner, dispatch_one=FakeDispatcher({}))

        result = run_operational_controller_cycle(
            workers=[{"id": "first", "batch_id": "finished-first"}, {"id": "second", "batch_id": "finished-second"}],
            queued_batches=[], adapter=adapter,
        )

        self.assertEqual(result["workers"], [{"id": "first", "state": "blocked"}, {"id": "second", "state": "blocked"}])
        self.assertNotIn(backend.harvest_command("second", "finished-second"), runner.calls)
        rendered = json.dumps(result, sort_keys=True)
        self.assertNotIn(COMMAND_LINE, rendered)
        self.assertNotIn(ENCODED_PROMPT, rendered)

    def test_distinct_idle_workers_dispatch_distinct_batches_and_continue_after_failure(self):
        backend = self.backend()
        runner = FakeRunner({
            backend.process_command("first"): CommandResult(0, "absent", ""),
            backend.harvest_command("first", "finished-first"): CommandResult(0, "success", ""),
            backend.process_command("second"): CommandResult(0, "absent", ""),
            backend.harvest_command("second", "finished-second"): CommandResult(0, "success", ""),
        })
        dispatcher = FakeDispatcher({
            ("first", "next-first"): {"outcome": "failure"},
            ("second", "next-second"): {"outcome": "success"},
        })
        adapter = HordeOperationalAdapter(backend=backend, runner=runner, dispatch_one=dispatcher)

        result = run_operational_controller_cycle(
            workers=[{"id": "first", "batch_id": "finished-first"}, {"id": "second", "batch_id": "finished-second"}],
            queued_batches=[{"batch_id": "next-first", "prompt": PROMPT}, {"batch_id": "next-second", "prompt": PROMPT}],
            adapter=adapter,
        )

        self.assertEqual(dispatcher.calls, [("first", "next-first"), ("second", "next-second")])
        self.assertEqual(result["workers"], [{"id": "first", "state": "blocked"}, {"id": "second", "state": "active"}])
        rendered = json.dumps(result, sort_keys=True)
        for private_value in (PROMPT, CREDENTIAL, RAW_LOG, COMMAND_LINE, ENCODED_PROMPT):
            self.assertNotIn(private_value, rendered)

    def test_persisted_aggregate_is_sanitized(self):
        backend = self.backend()
        runner = FakeRunner({backend.process_command("first"): CommandResult(0, "absent", "")})
        adapter = HordeOperationalAdapter(backend=backend, runner=runner, dispatch_one=FakeDispatcher({}))
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            result = run_operational_controller_cycle(
                workers=[{"id": "first", "batch_id": "finished"}], queued_batches=[], adapter=adapter,
                state_path=root / "state.json", journal_path=root / "journal.json",
            )
            rendered = (root / "state.json").read_text() + (root / "journal.json").read_text() + json.dumps(result)
            for private_value in (PROMPT, CREDENTIAL, RAW_LOG, COMMAND_LINE, ENCODED_PROMPT):
                self.assertNotIn(private_value, rendered)

    def test_unassigned_absent_worker_is_safely_blocked(self):
        backend = self.backend()
        runner = FakeRunner({backend.process_command("first"): CommandResult(0, "absent", "")})
        adapter = HordeOperationalAdapter(backend=backend, runner=runner, dispatch_one=FakeDispatcher({}))

        result = run_operational_controller_cycle(workers=[{"id": "first"}], queued_batches=[], adapter=adapter)

        self.assertEqual(result["workers"], [{"id": "first", "state": "blocked"}])
        self.assertEqual(result["alerts"], [{"worker_id": "first", "classification": "process_missing"}])


if __name__ == "__main__":
    unittest.main()
