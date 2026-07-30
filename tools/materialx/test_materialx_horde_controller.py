"""Tests for the fail-closed Horde harvest-and-refill controller."""

from __future__ import annotations

import copy
import unittest

from materialx_horde_controller import build_refill_cycle, run_controller_cycle
from materialx_horde_dispatch import _dispatch_id
from materialx_horde_dispatch_plan import build_dispatch_plan
from materialx_velocity_manifest import validate_batch_manifest
from test_materialx_completion_harvest import make_completion
from test_materialx_integration_train import FakeIntegrationBackend
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
        {"id": worker_id, "state": "idle"}
        for worker_id in worker_ids
    ]


def completed_worker(manifest, dispatch_id="dispatch-finished"):
    return {
        "id": manifest["roles"]["implementation"],
        "state": "idle",
        "batch_id": dispatch_id,
        "assignment": manifest,
    }


def parsed_completion(manifest, **changes):
    completion = make_completion(manifest["batch_id"])
    completion.update({
        "base_sha": manifest["integration_base_sha"],
        "node_defs": list(manifest["node_defs"]),
        "changed_files": list(manifest["files_allowlist"]),
        "tests": [
            {"command": command, "passed": 1, "failed": 0, "exit_code": 0}
            for command in manifest["focused_test_commands"]
        ],
    })
    completion.update(changes)
    return {
        "classification": "completion",
        "process_exit": 0,
        "completion": completion,
    }


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
            {"classification": "missing"},
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
        self.assertEqual([
            {key: worker[key] for key in ("id", "state", "batch_id")}
            for worker in result["workers"]
        ], [
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
        }, [
            {key: worker[key] for key in ("id", "state", "batch_id")}
            for worker in result["workers"] if worker["state"] == "active"
        ])
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
        manifest = make_manifest("finished-a")
        backend = FakeControllerBackend(harvests={
            ("blend05", "dispatch-finished"): {
                "classification": "nonzero_exit",
            },
        })
        result = run_controller_cycle(
            workers=[
                completed_worker(manifest),
                {"id": "blendit04", "state": "idle"},
            ],
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
            {"worker_id": "blend05", "classification": "invalid_completion"},
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

    def test_valid_completion_produces_sanitized_artifact_without_ledger_credit(self) -> None:
        manifest = make_manifest("next-a")
        backend = FakeControllerBackend(harvests={
            ("blend05", "dispatch-finished"): parsed_completion(manifest),
        })

        result = run_controller_cycle(
            workers=[completed_worker(manifest)],
            queued_batches=[],
            registered_families=REGISTERED_FAMILIES,
            backend=backend,
        )

        normalized = validate_batch_manifest(
            manifest, registered_families=REGISTERED_FAMILIES
        )
        self.assertEqual(result["artifacts"], [{
            "worker_id": "blend05",
            "batch_id": "next-a",
            "layer": "native_cycles",
            "assignment": normalized,
            "completion": result["artifacts"][0]["completion"],
        }])
        self.assertEqual(result["artifacts"][0]["completion"]["batch_id"], "next-a")
        self.assertIn({
            "worker_id": "blend05",
            "batch_id": "next-a",
            "event": "harvest_valid",
            "evidence": "completion_manifest_v2",
        }, result["journal"])
        self.assertNotIn("ledger", result)
        self.assertNotIn("prompt", repr(result).lower())

    def test_completion_mismatch_failures_receive_no_artifact_or_credit(self) -> None:
        manifest = make_manifest("next-a")
        invalid_completions = {
            "batch": {"batch_id": "wrong-batch"},
            "NodeDef": {"node_defs": ["ND_wrong"]},
            "allowlist": {"changed_files": ["outside/escape.cpp"]},
            "red_test": {
                "tests": [{
                    "command": manifest["focused_test_commands"][0],
                    "passed": 0,
                    "failed": 1,
                    "exit_code": 1,
                }],
            },
            "stale_base": {"base_sha": "c" * 40},
            "failed_review": {"review_verdict": "fail"},
        }
        for name, changes in invalid_completions.items():
            with self.subTest(name=name):
                backend = FakeControllerBackend(harvests={
                    ("blend05", "dispatch-finished"): parsed_completion(
                        manifest, **changes
                    ),
                })
                result = run_controller_cycle(
                    workers=[completed_worker(manifest)],
                    queued_batches=[],
                    registered_families=REGISTERED_FAMILIES,
                    backend=backend,
                )
                self.assertEqual(result["artifacts"], [])
                self.assertIn(
                    {"worker_id": "blend05", "classification": "invalid_completion"},
                    result["alerts"],
                )
                self.assertNotIn("private", repr(result))

    def test_exit_zero_without_completion_is_invalid_and_has_no_artifact(self) -> None:
        manifest = make_manifest("next-a")
        backend = FakeControllerBackend(harvests={
            ("blend05", "dispatch-finished"): {
                "classification": "invalid_completion",
            },
        })

        result = run_controller_cycle(
            workers=[completed_worker(manifest)],
            queued_batches=[],
            registered_families=REGISTERED_FAMILIES,
            backend=backend,
        )

        self.assertEqual(result["artifacts"], [])
        self.assertEqual(result["workers"], [{"id": "blend05", "state": "blocked"}])
        self.assertIn(
            {"worker_id": "blend05", "classification": "invalid_completion"},
            result["alerts"],
        )

    def test_one_invalid_worker_does_not_stop_four_valid_harvests(self) -> None:
        inputs = make_inputs()
        schedule = call_schedule(inputs)
        families = registered_families(inputs)
        manifests = list(schedule["assignments"].values())
        dispatch_id = _dispatch_id([manifest["batch_id"] for manifest in manifests])
        workers = [
            completed_worker(manifest, dispatch_id)
            for manifest in manifests
        ]
        harvests = {
            (worker["id"], dispatch_id): parsed_completion(worker["assignment"])
            for worker in workers
        }
        invalid_worker = workers[0]["id"]
        harvests[(invalid_worker, dispatch_id)] = {
            "classification": "invalid_completion"
        }
        backend = FakeControllerBackend(harvests=harvests)

        result = run_controller_cycle(
            workers=workers,
            queued_batches=[],
            registered_families=families,
            backend=backend,
        )

        self.assertEqual(len(backend.harvest_calls), 5)
        self.assertEqual(len(result["artifacts"]), 4)
        self.assertNotIn(
            workers[0]["assignment"]["batch_id"],
            {artifact["batch_id"] for artifact in result["artifacts"]},
        )
        self.assertIn(
            {"worker_id": invalid_worker, "classification": "invalid_completion"},
            result["alerts"],
        )

    def test_missing_or_ambiguous_active_assignment_is_rejected_before_harvest(self) -> None:
        manifest = make_manifest("next-a")
        cases = (
            {"id": "blend05", "state": "idle", "batch_id": "dispatch-finished"},
            {
                **completed_worker(manifest),
                "assignment": [manifest, second_manifest()],
            },
        )
        for worker in cases:
            with self.subTest(worker=worker["id"]):
                backend = FakeControllerBackend()
                result = run_controller_cycle(
                    workers=[worker],
                    queued_batches=[],
                    registered_families=REGISTERED_FAMILIES,
                    backend=backend,
                )
                self.assertEqual(backend.harvest_calls, [])
                self.assertEqual(result["artifacts"], [])
                self.assertIn(
                    {"worker_id": "blend05", "classification": "invalid_completion"},
                    result["alerts"],
                )

    def test_explicit_role_only_ownership_reenters_without_harvest_or_credit(self) -> None:
        backend = FakeControllerBackend()
        result = run_controller_cycle(
            workers=[{
                "id": "blendit04",
                "state": "idle",
                "batch_id": "dispatch-finished",
                "assignment": None,
            }],
            queued_batches=[],
            registered_families=REGISTERED_FAMILIES,
            backend=backend,
        )

        self.assertEqual(backend.harvest_calls, [])
        self.assertEqual(result["artifacts"], [])
        self.assertEqual(result["workers"], [{"id": "blendit04", "state": "idle"}])
        self.assertEqual(
            result["alerts"],
            [{"worker_id": "blendit04", "classification": "queue_empty"}],
        )

    def test_routes_only_valid_harvest_artifacts_to_integration_trains(self) -> None:
        manifest = make_manifest("next-a")
        worker = completed_worker(manifest)
        controller_backend = FakeControllerBackend(harvests={
            ("blend05", "dispatch-finished"): parsed_completion(manifest),
        })
        integration_backend = FakeIntegrationBackend()

        result = run_controller_cycle(
            workers=[worker],
            queued_batches=[],
            registered_families=REGISTERED_FAMILIES,
            backend=controller_backend,
            integration_backend=integration_backend,
        )

        self.assertEqual(len(result["artifacts"]), 1)
        self.assertEqual(result["integration_receipts"], [{
            "batch_id": "next-a",
            "layer": "native_cycles",
            "base_sha": "a" * 40,
            "head_sha": "b" * 40,
            "focused_commands": ["cycles_test --gtest_filter=MaterialXSemantic.Add"],
            "numeric_exits": [0],
            "final_state": "integrated",
        }])
        self.assertTrue(integration_backend.calls)

        invalid_integration = FakeIntegrationBackend()
        invalid = run_controller_cycle(
            workers=[worker],
            queued_batches=[],
            registered_families=REGISTERED_FAMILIES,
            backend=FakeControllerBackend(harvests={
                ("blend05", "dispatch-finished"): {"classification": "invalid_completion"},
            }),
            integration_backend=invalid_integration,
        )
        self.assertEqual(invalid["artifacts"], [])
        self.assertEqual(invalid["integration_receipts"], [])
        self.assertEqual(invalid_integration.calls, [])


if __name__ == "__main__":
    unittest.main()
