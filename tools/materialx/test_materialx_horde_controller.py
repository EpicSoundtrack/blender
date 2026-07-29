"""Tests for the fail-closed Horde harvest-and-refill controller."""

from __future__ import annotations

import unittest

from materialx_horde_controller import build_refill_cycle, run_controller_cycle


class FakeControllerBackend:
    def __init__(self, harvests, dispatches):
        self.harvests = harvests
        self.dispatches = dispatches
        self.harvest_calls = []
        self.dispatch_calls = []

    def harvest_finished(self, worker_id, batch_id):
        self.harvest_calls.append((worker_id, batch_id))
        return self.harvests[(worker_id, batch_id)]

    def dispatch_batch(self, worker_id, batch):
        self.dispatch_calls.append((worker_id, batch["batch_id"]))
        result = self.dispatches[(worker_id, batch["batch_id"])]
        if isinstance(result, Exception):
            raise result
        return result


class MaterialXHordeControllerTest(unittest.TestCase):
    def test_assigns_next_distinct_batches_to_all_healthy_idle_workers(self) -> None:
        result = build_refill_cycle(
            workers=[
                {"id": "blend05", "state": "idle", "harvest": "success"},
                {"id": "blendit", "state": "active", "harvest": "pending"},
                {"id": "blendit2", "state": "idle", "harvest": "success"},
            ],
            queued_batches=[
                {"batch_id": "batch-a", "prompt": "MaterialX task A"},
                {"batch_id": "batch-b", "prompt": "MaterialX task B"},
            ],
        )

        self.assertEqual(result["completed_workers"], ["blend05", "blendit2"])
        self.assertEqual(
            result["refills"],
            [
                {"worker_id": "blend05", "batch_id": "batch-a", "prompt": "MaterialX task A"},
                {"worker_id": "blendit2", "batch_id": "batch-b", "prompt": "MaterialX task B"},
            ],
        )
        self.assertEqual(result["blocked_workers"], [])

    def test_fails_closed_when_a_healthy_idle_worker_has_no_harvest_evidence(self) -> None:
        result = build_refill_cycle(
            workers=[{"id": "blend05", "state": "idle", "harvest": "missing"}],
            queued_batches=[{"batch_id": "batch-a", "prompt": "MaterialX task A"}],
        )

        self.assertEqual(result["refills"], [])
        self.assertEqual(result["blocked_workers"], ["blend05"])
        self.assertEqual(result["alerts"][0]["classification"], "harvest_missing")

    def test_cycle_harvests_and_refills_distinct_batches(self) -> None:
        backend = FakeControllerBackend(
            {
                ("blend05", "finished-a"): {"outcome": "success", "evidence": "task_log"},
                ("blendit2", "finished-b"): {"outcome": "success", "evidence": "task_log"},
            },
            {
                ("blend05", "next-a"): {"outcome": "success"},
                ("blendit2", "next-b"): {"outcome": "success"},
            },
        )

        result = run_controller_cycle(
            workers=[
                {"id": "blend05", "state": "idle", "batch_id": "finished-a"},
                {"id": "blendit2", "state": "idle", "batch_id": "finished-b"},
            ],
            queued_batches=[
                {"batch_id": "next-a", "prompt": "private task A"},
                {"batch_id": "next-b", "prompt": "private task B"},
            ],
            backend=backend,
        )

        self.assertEqual(backend.harvest_calls, [("blend05", "finished-a"), ("blendit2", "finished-b")])
        self.assertEqual(backend.dispatch_calls, [("blend05", "next-a"), ("blendit2", "next-b")])
        self.assertEqual(result["workers"], [
            {"id": "blend05", "state": "active"},
            {"id": "blendit2", "state": "active"},
        ])
        self.assertEqual(result["assigned_batches"], [
            {"worker_id": "blend05", "batch_id": "next-a"},
            {"worker_id": "blendit2", "batch_id": "next-b"},
        ])
        self.assertEqual(result["journal"], [
            {"worker_id": "blend05", "batch_id": "finished-a", "event": "harvest_success", "evidence": "task_log"},
            {"worker_id": "blend05", "batch_id": "next-a", "event": "dispatch_success", "evidence": "command_result"},
            {"worker_id": "blendit2", "batch_id": "finished-b", "event": "harvest_success", "evidence": "task_log"},
            {"worker_id": "blendit2", "batch_id": "next-b", "event": "dispatch_success", "evidence": "command_result"},
        ])
        self.assertNotIn("private task", repr(result))

    def test_cycle_leaves_active_worker_untouched(self) -> None:
        backend = FakeControllerBackend(
            {("idle", "finished"): {"outcome": "success", "evidence": "task_log"}},
            {("idle", "next"): {"outcome": "success"}},
        )

        result = run_controller_cycle(
            workers=[
                {"id": "active", "state": "active", "batch_id": "running"},
                {"id": "idle", "state": "idle", "batch_id": "finished"},
            ],
            queued_batches=[{"batch_id": "next", "prompt": "private task"}],
            backend=backend,
        )

        self.assertNotIn(("active", "running"), backend.harvest_calls)
        self.assertNotIn(("active", "next"), backend.dispatch_calls)
        self.assertIn({"id": "active", "state": "active"}, result["workers"])

    def test_cycle_blocks_only_workers_with_missing_failed_or_invalid_harvest(self) -> None:
        backend = FakeControllerBackend(
            {
                ("missing", "finished-missing"): {"outcome": "missing", "evidence": "none"},
                ("failed", "finished-failed"): {"outcome": "failure", "evidence": "task_log"},
                ("invalid", "finished-invalid"): {"outcome": "success", "evidence": "task_log", "log": "private"},
                ("healthy", "finished-healthy"): {"outcome": "success", "evidence": "task_log"},
            },
            {("healthy", "next"): {"outcome": "success"}},
        )

        result = run_controller_cycle(
            workers=[
                {"id": "missing", "state": "idle", "batch_id": "finished-missing"},
                {"id": "failed", "state": "idle", "batch_id": "finished-failed"},
                {"id": "invalid", "state": "idle", "batch_id": "finished-invalid"},
                {"id": "healthy", "state": "idle", "batch_id": "finished-healthy"},
            ],
            queued_batches=[{"batch_id": "next", "prompt": "private task"}],
            backend=backend,
        )

        self.assertEqual(backend.dispatch_calls, [("healthy", "next")])
        self.assertEqual(result["workers"], [
            {"id": "failed", "state": "blocked"},
            {"id": "healthy", "state": "active"},
            {"id": "invalid", "state": "blocked"},
            {"id": "missing", "state": "blocked"},
        ])
        self.assertEqual(result["alerts"], [
            {"worker_id": "failed", "classification": "harvest_failure"},
            {"worker_id": "invalid", "classification": "harvest_missing"},
            {"worker_id": "missing", "classification": "harvest_missing"},
        ])
        self.assertEqual(result["journal"], [
            {"worker_id": "failed", "batch_id": "finished-failed", "event": "harvest_failure", "evidence": "task_log"},
            {"worker_id": "healthy", "batch_id": "finished-healthy", "event": "harvest_success", "evidence": "task_log"},
            {"worker_id": "healthy", "batch_id": "next", "event": "dispatch_success", "evidence": "command_result"},
            {"worker_id": "invalid", "batch_id": "finished-invalid", "event": "harvest_missing", "evidence": "invalid"},
            {"worker_id": "missing", "batch_id": "finished-missing", "event": "harvest_missing", "evidence": "none"},
        ])
        self.assertNotIn("private", repr(result))

    def test_cycle_records_dispatch_failure_and_continues(self) -> None:
        backend = FakeControllerBackend(
            {
                ("first", "finished-first"): {"outcome": "success", "evidence": "task_log"},
                ("second", "finished-second"): {"outcome": "success", "evidence": "task_log"},
            },
            {
                ("first", "next-first"): {"outcome": "failure"},
                ("second", "next-second"): {"outcome": "success"},
            },
        )

        result = run_controller_cycle(
            workers=[
                {"id": "first", "state": "idle", "batch_id": "finished-first"},
                {"id": "second", "state": "idle", "batch_id": "finished-second"},
            ],
            queued_batches=[
                {"batch_id": "next-first", "prompt": "private task A"},
                {"batch_id": "next-second", "prompt": "private task B"},
            ],
            backend=backend,
        )

        self.assertEqual(backend.dispatch_calls, [("first", "next-first"), ("second", "next-second")])
        self.assertEqual(result["workers"], [
            {"id": "first", "state": "blocked"},
            {"id": "second", "state": "active"},
        ])
        self.assertEqual(result["assigned_batches"], [{"worker_id": "second", "batch_id": "next-second"}])
        self.assertEqual(result["alerts"], [{"worker_id": "first", "classification": "dispatch_failure"}])
        self.assertEqual(result["journal"], [
            {"worker_id": "first", "batch_id": "finished-first", "event": "harvest_success", "evidence": "task_log"},
            {"worker_id": "first", "batch_id": "next-first", "event": "dispatch_failure", "evidence": "command_result"},
            {"worker_id": "second", "batch_id": "finished-second", "event": "harvest_success", "evidence": "task_log"},
            {"worker_id": "second", "batch_id": "next-second", "event": "dispatch_success", "evidence": "command_result"},
        ])

    def test_cycle_keeps_harvested_idle_worker_reusable_after_queue_exhaustion(self) -> None:
        backend = FakeControllerBackend(
            {("idle", "finished"): {"outcome": "success", "evidence": "task_log"}},
            {},
        )

        result = run_controller_cycle(
            workers=[{"id": "idle", "state": "idle", "batch_id": "finished"}],
            queued_batches=[],
            backend=backend,
        )

        self.assertEqual(backend.dispatch_calls, [])
        self.assertEqual(result["workers"], [{"id": "idle", "state": "idle"}])
        self.assertEqual(result["alerts"], [{"worker_id": "idle", "classification": "queue_empty"}])
        self.assertEqual(result["journal"], [{
            "worker_id": "idle", "batch_id": "finished", "event": "harvest_success", "evidence": "task_log",
        }])

        backend.dispatches[("idle", "next")] = {"outcome": "success"}
        refill = run_controller_cycle(
            workers=[{"id": "idle", "state": "idle", "batch_id": "finished"}],
            queued_batches=[{"batch_id": "next", "prompt": "private next task"}],
            backend=backend,
        )

        self.assertEqual(refill["workers"], [{"id": "idle", "state": "active"}])
        self.assertEqual(refill["assigned_batches"], [{"worker_id": "idle", "batch_id": "next"}])

    def test_cycle_journals_missing_evidence_when_dispatch_raises(self) -> None:
        backend = FakeControllerBackend(
            {("idle", "finished"): {"outcome": "success", "evidence": "task_log"}},
            {("idle", "next"): RuntimeError("private dispatch detail")},
        )

        result = run_controller_cycle(
            workers=[{"id": "idle", "state": "idle", "batch_id": "finished"}],
            queued_batches=[{"batch_id": "next", "prompt": "private task"}],
            backend=backend,
        )

        self.assertEqual(result["workers"], [{"id": "idle", "state": "blocked"}])
        self.assertEqual(result["journal"][-1], {
            "worker_id": "idle", "batch_id": "next", "event": "dispatch_failure", "evidence": "missing",
        })
        self.assertNotIn("private", repr(result))

    def test_cycle_journals_invalid_evidence_for_malformed_dispatch(self) -> None:
        backend = FakeControllerBackend(
            {("idle", "finished"): {"outcome": "success", "evidence": "task_log"}},
            {("idle", "next"): {"outcome": "success", "log": "private dispatch detail"}},
        )

        result = run_controller_cycle(
            workers=[{"id": "idle", "state": "idle", "batch_id": "finished"}],
            queued_batches=[{"batch_id": "next", "prompt": "private task"}],
            backend=backend,
        )

        self.assertEqual(result["workers"], [{"id": "idle", "state": "blocked"}])
        self.assertEqual(result["journal"][-1], {
            "worker_id": "idle", "batch_id": "next", "event": "dispatch_failure", "evidence": "invalid",
        })
        self.assertNotIn("private", repr(result))

    def test_cycle_rejects_a_batch_already_attached_to_a_worker(self) -> None:
        backend = FakeControllerBackend({}, {})

        with self.assertRaisesRegex(ValueError, "already attached"):
            run_controller_cycle(
                workers=[
                    {"id": "active", "state": "active", "batch_id": "running"},
                    {"id": "idle", "state": "idle", "batch_id": "finished"},
                ],
                queued_batches=[{"batch_id": "running", "prompt": "private task"}],
                backend=backend,
            )

        self.assertEqual(backend.harvest_calls, [])
        self.assertEqual(backend.dispatch_calls, [])

    def test_cycle_rejects_duplicate_current_batch_ids(self) -> None:
        backend = FakeControllerBackend({}, {})

        with self.assertRaisesRegex(ValueError, "duplicate non-empty batch_id"):
            run_controller_cycle(
                workers=[
                    {"id": "first", "state": "active", "batch_id": "running"},
                    {"id": "second", "state": "active", "batch_id": "running"},
                ],
                queued_batches=[],
                backend=backend,
            )

        self.assertEqual(backend.harvest_calls, [])
        self.assertEqual(backend.dispatch_calls, [])


if __name__ == "__main__":
    unittest.main()
