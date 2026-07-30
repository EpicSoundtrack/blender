"""Tests for the bounded operational Horde controller adapter."""

from __future__ import annotations

import base64
import copy
import json
from pathlib import Path
import tempfile
import unittest

from materialx_horde_dispatch import CommandResult, HordeBackend, _dispatch_id
from materialx_horde_operational import HordeOperationalAdapter, run_operational_controller_cycle
from test_materialx_batch_scheduler import call_schedule, make_inputs, registered_families
from test_materialx_horde_dispatch_plan import REGISTERED_FAMILIES, make_manifest


PROMPT = "private MaterialX prompt"
CREDENTIAL = "private-credential"
RAW_LOG = "private remote log"
ENCODED_PROMPT = base64.b64encode(PROMPT.encode("utf-8")).decode("ascii")
COMMAND_LINE = f"123 python3 hermes_runner.py {ENCODED_PROMPT}"
ALL_WORKERS = ("blend05", "blendit04", "blendit", "blendit2", "blendit3")


def entry(manifest=None):
    manifest = manifest or make_manifest("next")
    return {"worker_id": manifest["roles"]["implementation"], "manifest": manifest}


def second_manifest(batch_id="next-second"):
    return make_manifest(
        batch_id,
        node_start=8,
        roles={
            "implementation": "blendit2",
            "generated_tests": "blendit3",
            "independent_review": "blendit",
        },
    )


def finished_workers(worker_ids):
    return [
        {"id": worker_id, "batch_id": f"finished-{index}"}
        for index, worker_id in enumerate(worker_ids)
    ]


class FakeRunner:
    def __init__(self, results=None):
        self.results = results or {}
        self.calls = []

    def __call__(self, command, *, env, input_text, timeout):
        self.calls.append(command)
        result = self.results.get(command)
        if result is None and "--show-toplevel" in command[-1]:
            return CommandResult(0, json.dumps({
                "repository_present": True,
                "files": {
                    "intern/cycles/scene/materialx.cpp": True,
                    "tools/materialx/materialx_velocity_manifest.py": True,
                    "tools/materialx/materialx_batch_scheduler.py": True,
                },
                "head": "a" * 40,
            }, separators=(",", ":")), "")
        if result is None and "MATERIALX_HORDE_EXIT" in command[-1]:
            return CommandResult(0, "success", "")
        if result is None:
            return CommandResult(0, "absent", "")
        return result.pop(0) if isinstance(result, list) else result


class FakeDispatcher:
    def __init__(self, result=None):
        self.result = result
        self.calls = []

    def __call__(self, manifests):
        normalized = copy.deepcopy(list(manifests))
        self.calls.append(normalized)
        if isinstance(self.result, Exception):
            raise self.result
        if self.result is not None:
            return self.result
        workers = sorted({
            worker
            for manifest in normalized
            for worker in manifest["roles"].values()
        })
        return {
            "outcome": "success",
            "worker_states": {worker: "active" for worker in workers},
            "dispatch_id": _dispatch_id([
                manifest["batch_id"] for manifest in normalized
            ]),
        }


class MaterialXHordeOperationalTest(unittest.TestCase):
    @staticmethod
    def backend():
        return HordeBackend({worker: {"host": worker} for worker in ALL_WORKERS})

    def test_active_worker_is_retained_without_harvest_or_dispatch(self):
        backend = self.backend()
        runner = FakeRunner({
            backend.process_command("blendit"): CommandResult(0, "active:321", ""),
        })
        dispatcher = FakeDispatcher()
        adapter = HordeOperationalAdapter(
            backend=backend, runner=runner, dispatch_batch=dispatcher
        )

        result = run_operational_controller_cycle(
            workers=[{"id": "blendit", "batch_id": "running"}],
            queued_batches=[],
            registered_families=REGISTERED_FAMILIES,
            adapter=adapter,
        )

        self.assertEqual(result["workers"], [
            {"id": "blendit", "state": "active", "batch_id": "running"}
        ])
        self.assertEqual(dispatcher.calls, [])
        self.assertEqual(result["journal"], [
            {"worker_id": "blendit", "event": "process_active", "evidence": "pid"},
        ])

    def test_absent_role_workers_harvest_then_dispatch_one_manifest_set(self):
        backend = self.backend()
        dispatcher = FakeDispatcher()
        adapter = HordeOperationalAdapter(
            backend=backend, runner=FakeRunner(), dispatch_batch=dispatcher
        )
        manifest = make_manifest("next")

        result = run_operational_controller_cycle(
            workers=finished_workers(("blend05", "blendit04", "blendit")),
            queued_batches=[entry(manifest)],
            registered_families=REGISTERED_FAMILIES,
            adapter=adapter,
        )

        self.assertEqual(len(dispatcher.calls), 1)
        self.assertEqual(dispatcher.calls[0], [manifest])
        self.assertEqual(result["workers"], [
            {"id": "blend05", "state": "active", "batch_id": _dispatch_id(["next"])},
            {"id": "blendit", "state": "active", "batch_id": _dispatch_id(["next"])},
            {"id": "blendit04", "state": "active", "batch_id": _dispatch_id(["next"])},
        ])

    def test_auth_or_proxy_failure_blocks_dependent_manifest_without_dispatch(self):
        for category in ("auth_failure", "proxy_failure"):
            with self.subTest(category=category):
                backend = self.backend()
                runner = FakeRunner({
                    backend.harvest_command("blend05", "finished-0"):
                        CommandResult(0, category, ""),
                })
                dispatcher = FakeDispatcher()
                adapter = HordeOperationalAdapter(
                    backend=backend, runner=runner, dispatch_batch=dispatcher
                )

                result = run_operational_controller_cycle(
                    workers=finished_workers(("blend05", "blendit04", "blendit")),
                    queued_batches=[entry()],
                    registered_families=REGISTERED_FAMILIES,
                    adapter=adapter,
                )

                self.assertEqual(dispatcher.calls, [])
                self.assertIn(
                    {"worker_id": "blend05", "classification": "harvest_failure"},
                    result["alerts"],
                )
                self.assertIn({
                    "worker_id": "blend05",
                    "batch_id": "next",
                    "classification": "role_worker_unavailable",
                }, result["alerts"])

    def test_malformed_process_and_harvest_evidence_stays_sanitized(self):
        backend = self.backend()
        runner = FakeRunner({
            backend.process_command("blend05"): CommandResult(0, "absent", ""),
            backend.harvest_command("blend05", "finished-first"):
                CommandResult(0, "invalid", RAW_LOG),
            backend.process_command("blendit2"): CommandResult(0, COMMAND_LINE, ""),
        })
        adapter = HordeOperationalAdapter(
            backend=backend, runner=runner, dispatch_batch=FakeDispatcher()
        )

        result = run_operational_controller_cycle(
            workers=[
                {"id": "blend05", "batch_id": "finished-first"},
                {"id": "blendit2", "batch_id": "finished-second"},
            ],
            queued_batches=[],
            registered_families=REGISTERED_FAMILIES,
            adapter=adapter,
        )

        self.assertEqual(result["workers"], [
            {"id": "blend05", "state": "blocked"},
            {"id": "blendit2", "state": "blocked"},
        ])
        rendered = json.dumps(result, sort_keys=True)
        self.assertNotIn(COMMAND_LINE, rendered)
        self.assertNotIn(RAW_LOG, rendered)

    def test_two_manifests_use_one_dispatch_and_partial_state_is_truthful(self):
        backend = self.backend()
        first = make_manifest("next-first")
        second = second_manifest()
        states = {worker: "active" for worker in ALL_WORKERS}
        states["blend05"] = "failure"
        dispatcher = FakeDispatcher({
            "outcome": "partial",
            "worker_states": states,
            "dispatch_id": _dispatch_id(["next-first", "next-second"]),
        })
        adapter = HordeOperationalAdapter(
            backend=backend, runner=FakeRunner(), dispatch_batch=dispatcher
        )

        result = run_operational_controller_cycle(
            workers=finished_workers(ALL_WORKERS),
            queued_batches=[entry(first), entry(second)],
            registered_families=REGISTERED_FAMILIES,
            adapter=adapter,
        )

        self.assertEqual(len(dispatcher.calls), 1)
        self.assertEqual(
            [manifest["batch_id"] for manifest in dispatcher.calls[0]],
            ["next-first", "next-second"],
        )
        self.assertEqual(result["assigned_batches"], [
            {"worker_id": "blendit2", "batch_id": "next-second"},
        ])
        self.assertIn({"id": "blend05", "state": "blocked"}, result["workers"])

    def test_prompt_and_ownership_failures_happen_before_process_calls(self):
        cases = (
            (
                [{"id": "blend05", "batch_id": "finished"}],
                [{"batch_id": "legacy", "prompt": PROMPT}],
                "queue",
            ),
            (
                [{"id": "blend05", "batch_id": "next"}],
                [entry(make_manifest("next"))],
                "already attached",
            ),
            (
                [
                    {"id": "blend05", "batch_id": "finished-a"},
                    {"id": "blend05", "batch_id": "finished-b"},
                ],
                [],
                "unique",
            ),
            (
                [{"id": "blend05", "batch_id": "finished"}],
                [entry()],
                "roles",
            ),
        )
        for workers, queue, message in cases:
            with self.subTest(message=message):
                runner = FakeRunner()
                adapter = HordeOperationalAdapter(
                    backend=self.backend(),
                    runner=runner,
                    dispatch_batch=FakeDispatcher(),
                )
                with self.assertRaisesRegex(ValueError, message):
                    run_operational_controller_cycle(
                        workers=workers,
                        queued_batches=queue,
                        registered_families=REGISTERED_FAMILIES,
                        adapter=adapter,
                    )
                self.assertEqual(runner.calls, [])

    def test_persisted_aggregate_is_sanitized(self):
        backend = self.backend()
        adapter = HordeOperationalAdapter(
            backend=backend, runner=FakeRunner(), dispatch_batch=FakeDispatcher()
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result = run_operational_controller_cycle(
                workers=[{"id": "blend05", "batch_id": "finished"}],
                queued_batches=[],
                registered_families=REGISTERED_FAMILIES,
                adapter=adapter,
                state_path=root / "state.json",
                journal_path=root / "journal.json",
            )
            rendered = (
                (root / "state.json").read_text()
                + (root / "journal.json").read_text()
                + json.dumps(result)
            )
            for private_value in (PROMPT, CREDENTIAL, RAW_LOG, COMMAND_LINE, ENCODED_PROMPT):
                self.assertNotIn(private_value, rendered)

    def test_unassigned_absent_worker_is_safely_blocked(self):
        backend = self.backend()
        adapter = HordeOperationalAdapter(
            backend=backend, runner=FakeRunner(), dispatch_batch=FakeDispatcher()
        )
        result = run_operational_controller_cycle(
            workers=[{"id": "blend05"}],
            queued_batches=[],
            registered_families=REGISTERED_FAMILIES,
            adapter=adapter,
        )
        self.assertEqual(result["workers"], [{"id": "blend05", "state": "blocked"}])
        self.assertEqual(
            result["alerts"],
            [{"worker_id": "blend05", "classification": "process_missing"}],
        )

    def test_with_dispatcher_runs_one_combined_plan_and_launches_each_worker_once(self):
        inputs = make_inputs()
        schedule = call_schedule(inputs)
        families = registered_families(inputs)
        backend = self.backend()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            credential_file = root / "credentials.env"
            credential_file.write_text(f"NVIDIA_API_KEY={CREDENTIAL}\n", encoding="utf-8")
            runner = FakeRunner({
                backend.process_command(worker): [
                    CommandResult(0, "absent", ""),
                    CommandResult(0, f"active:{700 + index}", ""),
                    CommandResult(0, "absent", ""),
                ]
                for index, worker in enumerate(ALL_WORKERS)
            })
            adapter = HordeOperationalAdapter.with_dispatcher(
                backend=backend,
                credential_file=credential_file,
                registered_families=families,
                runner=runner,
                capacity_state_path=root / "dispatcher-capacity.json",
                dispatch_journal_path=root / "dispatcher-journal.json",
            )

            result = run_operational_controller_cycle(
                workers=finished_workers(ALL_WORKERS),
                queued_batches=[
                    {"worker_id": worker, "manifest": manifest}
                    for worker, manifest in schedule["assignments"].items()
                ],
                registered_families=families,
                adapter=adapter,
            )

            launches = [command for command in runner.calls if command[-1].startswith("nohup ")]
            self.assertEqual(len(launches), 5)
            self.assertEqual(len({command[-2] for command in launches}), 5)
            self.assertEqual(len(result["assigned_batches"]), 5)
            dispatch_id = _dispatch_id([
                manifest["batch_id"] for manifest in schedule["assignments"].values()
            ])
            self.assertTrue(all(
                worker["batch_id"] == dispatch_id for worker in result["workers"]
            ))

            second_cycle = run_operational_controller_cycle(
                workers=result["workers"],
                queued_batches=[],
                registered_families=families,
                adapter=adapter,
            )
            harvest_commands = {
                backend.harvest_command(worker, dispatch_id)
                for worker in ALL_WORKERS
            }
            self.assertTrue(harvest_commands.issubset(set(runner.calls)))
            self.assertFalse(any(
                alert["classification"] == "harvest_missing"
                for alert in second_cycle["alerts"]
            ))
            self.assertFalse((root / "dispatcher-capacity.json").exists())
            self.assertFalse((root / "dispatcher-journal.json").exists())

    def test_with_dispatcher_partial_role_failure_fails_only_dependent_batches(self):
        backend = self.backend()
        first = make_manifest("next-first")
        second = second_manifest()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            credential_file = root / "credentials.env"
            credential_file.write_text(f"NVIDIA_API_KEY={CREDENTIAL}\n", encoding="utf-8")
            runner = FakeRunner({
                backend.process_command(worker): [
                    CommandResult(0, "absent", ""),
                    CommandResult(
                        0,
                        "absent" if worker == "blend05" else f"active:{700 + index}",
                        "",
                    ),
                ]
                for index, worker in enumerate(ALL_WORKERS)
            })
            adapter = HordeOperationalAdapter.with_dispatcher(
                backend=backend,
                credential_file=credential_file,
                registered_families=REGISTERED_FAMILIES,
                runner=runner,
                capacity_state_path=root / "dispatcher-capacity.json",
                dispatch_journal_path=root / "dispatcher-journal.json",
            )

            result = run_operational_controller_cycle(
                workers=finished_workers(ALL_WORKERS),
                queued_batches=[entry(first), entry(second)],
                registered_families=REGISTERED_FAMILIES,
                adapter=adapter,
            )

            self.assertEqual(result["assigned_batches"], [
                {"worker_id": "blendit2", "batch_id": "next-second"},
            ])
            self.assertIn({"id": "blend05", "state": "blocked"}, result["workers"])
            launches = [command for command in runner.calls if command[-1].startswith("nohup ")]
            self.assertEqual(len(launches), 5)


if __name__ == "__main__":
    unittest.main()
