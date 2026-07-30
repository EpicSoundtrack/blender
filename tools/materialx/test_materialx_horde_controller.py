"""Tests for the fail-closed Horde harvest-and-refill controller."""

from __future__ import annotations

import copy
import unittest

from materialx_horde_controller import build_refill_cycle, run_controller_cycle
from materialx_horde_dispatch import _dispatch_id
from materialx_horde_dispatch_plan import build_dispatch_plan
from materialx_velocity_manifest import validate_batch_manifest
from test_materialx_batch_scheduler import call_schedule, make_inputs, registered_families
from test_materialx_horde_dispatch_plan import REGISTERED_FAMILIES, make_manifest


ALL_WORKERS = ("blend05", "blendit04", "blendit", "blendit2", "blendit3")


def second_manifest(batch_id="next-b"):
    return make_manifest(
        batch_id,
        node_start=8,
        roles={
            "implementation": "blendit2",
            "generated_tests": "blendit3",
            "independent_review": "blend05",
        },
    )


def independent_second_manifest(batch_id="next-b"):
    manifest = second_manifest(batch_id)
    manifest["roles"]["independent_review"] = "blendit"
    return manifest


def queue_entry(manifest=None):
    manifest = manifest or make_manifest("next-a")
    return {"worker_id": manifest["roles"]["implementation"], "manifest": manifest}


def idle_workers(worker_ids=ALL_WORKERS):
    return [
        {"id": worker_id, "state": "idle", "batch_id": f"finished-{index}"}
        for index, worker_id in enumerate(worker_ids)
    ]


class FakeControllerBackend:
    def __init__(self, harvests=None, dispatch_result=None):
        self.harvests = harvests or {}
        self.dispatch_result = dispatch_result
        self.harvest_calls = []
        self.dispatch_calls = []

    def harvest_finished(self, worker_id, batch_id):
        self.harvest_calls.append((worker_id, batch_id))
        return self.harvests.get(
            (worker_id, batch_id),
            {"outcome": "success", "evidence": "task_log"},
        )

    def dispatch_batch(self, manifests):
        self.dispatch_calls.append(copy.deepcopy(list(manifests)))
        if isinstance(self.dispatch_result, Exception):
            raise self.dispatch_result
        if self.dispatch_result is not None:
            return self.dispatch_result
        workers = sorted({
            worker
            for manifest in manifests
            for worker in manifest["roles"].values()
        })
        return {
            "outcome": "success",
            "worker_states": {worker: "active" for worker in workers},
            "dispatch_id": _dispatch_id([
                manifest["batch_id"] for manifest in manifests
            ]),
        }


class MaterialXHordeControllerTest(unittest.TestCase):
    def test_assigns_validated_manifest_when_every_role_worker_is_eligible(self) -> None:
        manifest = make_manifest("next-a")
        result = build_refill_cycle(
            workers=[
                {"id": worker, "state": "idle", "harvest": "success"}
                for worker in manifest["roles"].values()
            ],
            queued_batches=[queue_entry(manifest)],
            registered_families=REGISTERED_FAMILIES,
        )

        self.assertEqual(result["completed_workers"], ["blend05", "blendit04", "blendit"])
        self.assertEqual(result["refills"], [{
            "worker_id": "blend05",
            "manifest": validate_batch_manifest(
                manifest, registered_families=REGISTERED_FAMILIES
            ),
        }])
        self.assertEqual(result["blocked_workers"], [])
        self.assertEqual(result["alerts"], [])

    def test_role_worker_unavailable_defers_only_dependent_manifest(self) -> None:
        dependent = make_manifest("next-a")
        independent = second_manifest()
        backend = FakeControllerBackend()
        workers = idle_workers()
        workers[1] = {"id": "blendit04", "state": "active", "batch_id": "running"}

        result = run_controller_cycle(
            workers=workers,
            queued_batches=[queue_entry(dependent), queue_entry(independent)],
            registered_families=REGISTERED_FAMILIES,
            backend=backend,
        )

        self.assertEqual(len(backend.dispatch_calls), 1)
        self.assertEqual(
            [manifest["batch_id"] for manifest in backend.dispatch_calls[0]],
            ["next-b"],
        )
        self.assertIn({
            "worker_id": "blend05",
            "batch_id": "next-a",
            "classification": "role_worker_unavailable",
        }, result["alerts"])
        self.assertEqual(result["assigned_batches"], [
            {"worker_id": "blendit2", "batch_id": "next-b"},
        ])
        self.assertIn({
            "id": "blendit04",
            "state": "active",
            "batch_id": "running",
        }, result["workers"])

    def test_cycle_batches_selected_manifests_into_one_backend_call(self) -> None:
        first = make_manifest("next-a")
        second = second_manifest()
        backend = FakeControllerBackend()

        result = run_controller_cycle(
            workers=idle_workers(),
            queued_batches=[queue_entry(first), queue_entry(second)],
            registered_families=REGISTERED_FAMILIES,
            backend=backend,
        )

        self.assertEqual(len(backend.dispatch_calls), 1)
        self.assertEqual(
            [manifest["batch_id"] for manifest in backend.dispatch_calls[0]],
            ["next-a", "next-b"],
        )
        self.assertEqual(result["workers"], [
            {
                "id": worker,
                "state": "active",
                "batch_id": _dispatch_id(["next-a", "next-b"]),
            }
            for worker in sorted(ALL_WORKERS)
        ])
        self.assertEqual(result["assigned_batches"], [
            {"worker_id": "blend05", "batch_id": "next-a"},
            {"worker_id": "blendit2", "batch_id": "next-b"},
        ])
        self.assertNotIn("instruction", repr(result).lower())

    def test_partial_dispatch_updates_each_role_worker_and_each_batch_truthfully(self) -> None:
        first = make_manifest("next-a")
        second = independent_second_manifest()
        worker_states = {worker: "active" for worker in ALL_WORKERS}
        worker_states["blend05"] = "failure"
        backend = FakeControllerBackend(dispatch_result={
            "outcome": "partial",
            "worker_states": worker_states,
            "dispatch_id": _dispatch_id(["next-a", "next-b"]),
        })

        result = run_controller_cycle(
            workers=idle_workers(),
            queued_batches=[queue_entry(first), queue_entry(second)],
            registered_families=REGISTERED_FAMILIES,
            backend=backend,
        )

        self.assertEqual(len(backend.dispatch_calls), 1)
        self.assertEqual(result["assigned_batches"], [
            {"worker_id": "blendit2", "batch_id": "next-b"},
        ])
        self.assertIn({"id": "blend05", "state": "blocked"}, result["workers"])
        self.assertIn({
            "id": "blendit2",
            "state": "active",
            "batch_id": _dispatch_id(["next-a", "next-b"]),
        }, result["workers"])
        self.assertIn(
            {"worker_id": "blend05", "classification": "dispatch_failure"},
            result["alerts"],
        )
        self.assertIn({
            "worker_id": "blend05",
            "batch_id": "next-a",
            "event": "dispatch_failure",
            "evidence": "command_result",
        }, result["journal"])
        self.assertIn({
            "worker_id": "blendit2",
            "batch_id": "next-b",
            "event": "dispatch_success",
            "evidence": "command_result",
        }, result["journal"])

    def test_failed_or_invalid_dispatch_evidence_blocks_selected_role_workers(self) -> None:
        cases = (
            (RuntimeError("private dispatch detail"), "missing"),
            ({"outcome": "success", "log": "private"}, "invalid"),
        )
        for dispatch_result, evidence in cases:
            with self.subTest(evidence=evidence):
                backend = FakeControllerBackend(dispatch_result=dispatch_result)
                result = run_controller_cycle(
                    workers=idle_workers(("blend05", "blendit04", "blendit")),
                    queued_batches=[queue_entry()],
                    registered_families=REGISTERED_FAMILIES,
                    backend=backend,
                )

                self.assertEqual(
                    result["workers"],
                    [
                        {"id": "blend05", "state": "blocked"},
                        {"id": "blendit", "state": "blocked"},
                        {"id": "blendit04", "state": "blocked"},
                    ],
                )
                self.assertEqual(result["assigned_batches"], [])
                self.assertIn({
                    "worker_id": "blend05",
                    "batch_id": "next-a",
                    "event": "dispatch_failure",
                    "evidence": evidence,
                }, result["journal"])
                self.assertNotIn("private", repr(result))

    def test_failed_harvest_and_queue_empty_remain_fail_closed(self) -> None:
        backend = FakeControllerBackend(harvests={
            ("blend05", "finished-0"): {"outcome": "failure", "evidence": "task_log"},
        })
        result = run_controller_cycle(
            workers=idle_workers(("blend05", "blendit04")),
            queued_batches=[],
            registered_families=REGISTERED_FAMILIES,
            backend=backend,
        )

        self.assertEqual(backend.dispatch_calls, [])
        self.assertEqual(result["workers"], [
            {"id": "blend05", "state": "blocked"},
            {"id": "blendit04", "state": "idle"},
        ])
        self.assertEqual(result["alerts"], [
            {"worker_id": "blend05", "classification": "harvest_failure"},
            {"worker_id": "blendit04", "classification": "queue_empty"},
        ])

    def test_queue_empty_and_deferred_outputs_reenter_without_reharvest(self) -> None:
        queue_empty = run_controller_cycle(
            workers=idle_workers(("blend05",)),
            queued_batches=[],
            registered_families=REGISTERED_FAMILIES,
            backend=FakeControllerBackend(),
        )
        queue_empty_backend = FakeControllerBackend()
        queue_empty_next = run_controller_cycle(
            workers=queue_empty["workers"],
            queued_batches=[],
            registered_families=REGISTERED_FAMILIES,
            backend=queue_empty_backend,
        )
        self.assertEqual(queue_empty_next["workers"], [
            {"id": "blend05", "state": "idle"}
        ])
        self.assertEqual(queue_empty_backend.harvest_calls, [])

        failed_role = "blend05"
        deferred = run_controller_cycle(
            workers=idle_workers(("blend05", "blendit04", "blendit")),
            queued_batches=[queue_entry()],
            registered_families=REGISTERED_FAMILIES,
            backend=FakeControllerBackend(harvests={
                (failed_role, "finished-0"): {
                    "outcome": "failure",
                    "evidence": "task_log",
                },
            }),
        )
        deferred_backend = FakeControllerBackend()
        deferred_next = run_controller_cycle(
            workers=deferred["workers"],
            queued_batches=[queue_entry()],
            registered_families=REGISTERED_FAMILIES,
            backend=deferred_backend,
        )
        self.assertEqual(deferred_backend.harvest_calls, [])
        self.assertEqual(deferred_backend.dispatch_calls, [])
        self.assertIn({
            "worker_id": "blend05",
            "batch_id": "next-a",
            "classification": "role_worker_unavailable",
        }, deferred_next["alerts"])

    def test_prompt_only_wrong_worker_and_unexpected_queue_fields_fail_before_backend(self) -> None:
        invalid_entries = (
            {"worker_id": "blend05", "manifest": {"batch_id": "legacy", "prompt": "private"}},
            {"worker_id": "blendit", "manifest": make_manifest("next-a")},
            {**queue_entry(), "prompt": "private"},
        )
        for entry in invalid_entries:
            with self.subTest(entry=set(entry)):
                backend = FakeControllerBackend()
                with self.assertRaises(ValueError):
                    run_controller_cycle(
                        workers=idle_workers(("blend05", "blendit04", "blendit")),
                        queued_batches=[entry],
                        registered_families=REGISTERED_FAMILIES,
                        backend=backend,
                    )
                self.assertEqual(backend.harvest_calls, [])
                self.assertEqual(backend.dispatch_calls, [])

    def test_duplicate_queue_ownership_fails_before_backend(self) -> None:
        first = queue_entry()
        duplicate_batch = queue_entry(second_manifest("next-a"))
        duplicate_nodes_manifest = second_manifest()
        duplicate_nodes_manifest["node_defs"] = list(first["manifest"]["node_defs"])
        duplicate_nodes_manifest["files_allowlist"] = ["intern/cycles/other.cpp"]
        duplicate_nodes = queue_entry(duplicate_nodes_manifest)
        duplicate_files_manifest = second_manifest()
        duplicate_files_manifest["files_allowlist"] = list(first["manifest"]["files_allowlist"])
        duplicate_files = queue_entry(duplicate_files_manifest)
        duplicate_worker = queue_entry(make_manifest("next-b", node_start=8))

        for entries, message in (
            ([first, duplicate_batch], "batch"),
            ([first, duplicate_nodes], "NodeDef"),
            ([first, duplicate_files], "file"),
            ([first, duplicate_worker], "worker"),
        ):
            with self.subTest(message=message):
                backend = FakeControllerBackend()
                with self.assertRaisesRegex(ValueError, message):
                    run_controller_cycle(
                        workers=idle_workers(),
                        queued_batches=entries,
                        registered_families=REGISTERED_FAMILIES,
                        backend=backend,
                    )
                self.assertEqual(backend.harvest_calls, [])

    def test_attached_queue_batch_fails_before_backend(self) -> None:
        backend = FakeControllerBackend()
        with self.assertRaisesRegex(ValueError, "already attached"):
            run_controller_cycle(
                workers=[
                    {"id": "blend05", "state": "active", "batch_id": "next-a"}
                ],
                queued_batches=[queue_entry()],
                registered_families=REGISTERED_FAMILIES,
                backend=backend,
            )
        self.assertEqual(backend.harvest_calls, [])

    def test_active_role_workers_may_share_one_dispatch_identity(self) -> None:
        backend = FakeControllerBackend()
        result = run_controller_cycle(
            workers=[
                {"id": "blend05", "state": "active", "batch_id": "dispatch-running"},
                {"id": "blendit", "state": "active", "batch_id": "dispatch-running"},
            ],
            queued_batches=[],
            registered_families=REGISTERED_FAMILIES,
            backend=backend,
        )

        self.assertEqual(result["workers"], [
            {"id": "blend05", "state": "active", "batch_id": "dispatch-running"},
            {"id": "blendit", "state": "active", "batch_id": "dispatch-running"},
        ])
        self.assertEqual(backend.harvest_calls, [])

    def test_scheduler_assignments_cross_dispatch_and_controller_once(self) -> None:
        inputs = make_inputs()
        schedule = call_schedule(inputs)
        families = registered_families(inputs)
        manifests = list(schedule["assignments"].values())
        original = copy.deepcopy(manifests)
        plan = build_dispatch_plan(manifests, "horde.env", registered_families=families)
        backend = FakeControllerBackend()

        result = run_controller_cycle(
            workers=idle_workers(tuple(schedule["assignments"])),
            queued_batches=[
                {"worker_id": worker_id, "manifest": manifest}
                for worker_id, manifest in schedule["assignments"].items()
            ],
            registered_families=families,
            backend=backend,
        )

        self.assertEqual(len(plan["assignments"]), 5)
        self.assertEqual(len(backend.dispatch_calls), 1)
        self.assertEqual(len(backend.dispatch_calls[0]), 5)
        self.assertEqual(len(result["assigned_batches"]), 5)
        self.assertEqual(manifests, original)

    def test_prompt_only_fails_dispatch_plan_and_controller_before_backend(self) -> None:
        inputs = make_inputs()
        families = registered_families(inputs)
        prompt_only = {"batch_id": "legacy", "prompt": "private"}
        with self.assertRaises(ValueError):
            build_dispatch_plan([prompt_only], "horde.env", registered_families=families)
        backend = FakeControllerBackend()
        with self.assertRaises(ValueError):
            run_controller_cycle(
                workers=idle_workers(("blend05", "blendit04", "blendit")),
                queued_batches=[{"worker_id": "blend05", "manifest": prompt_only}],
                registered_families=families,
                backend=backend,
            )
        self.assertEqual(backend.harvest_calls, [])
        self.assertEqual(backend.dispatch_calls, [])


if __name__ == "__main__":
    unittest.main()
