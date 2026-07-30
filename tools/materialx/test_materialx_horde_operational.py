"""Tests for the bounded operational Horde controller adapter."""

from __future__ import annotations

import base64
import copy
import inspect
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from materialx_alert_sink import SanitizedAlertSink
from materialx_horde_dispatch import CommandResult, HordeBackend, _dispatch_id
from materialx_integration_backend import GitIntegrationBackend
from materialx_horde_operational import (
    HordeOperationalAdapter,
    OperationalSupervisorController,
    load_runtime_config,
    main,
    run_operational_controller_cycle,
    run_operational_supervisor,
)
from materialx_completion_harvest import CLASSIFICATION_PREFIX
from test_materialx_batch_scheduler import call_schedule, make_inputs, registered_families
from test_materialx_completion_harvest import evidence, make_completion
from test_materialx_horde_dispatch_plan import REGISTERED_FAMILIES, make_manifest
from test_materialx_integration_train import FakeIntegrationBackend


PROMPT = "private MaterialX prompt"
CREDENTIAL = "private-credential"
RAW_LOG = "private remote log"
ENCODED_PROMPT = base64.b64encode(PROMPT.encode("utf-8")).decode("ascii")
COMMAND_LINE = f"123 python3 hermes_runner.py {ENCODED_PROMPT}"
ALL_WORKERS = ("blend05", "blendit04", "blendit", "blendit2", "blendit3")


class FakeAlertTransport:
    def send(self, message):
        return "receipt-1"


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
        {"id": worker_id, "state": "idle"}
        for worker_id in worker_ids
    ]


def completed_worker(manifest, dispatch_id="dispatch-finished"):
    return {
        "id": manifest["roles"]["implementation"],
        "state": "active",
        "batch_id": dispatch_id,
        "assignment": manifest,
    }


def completion_for(manifest):
    completion = make_completion(manifest["batch_id"])
    completion["node_defs"] = list(manifest["node_defs"])
    completion["changed_files"] = list(manifest["files_allowlist"])
    completion["tests"] = [
        {"command": command, "passed": 1, "failed": 0, "exit_code": 0}
        for command in manifest["focused_test_commands"]
    ]
    return completion


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
            return CommandResult(0, "MATERIALX_HORDE_EXIT:0", "")
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
            workers=[{"id": "blendit", "state": "active", "batch_id": "running"}],
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
        self.assertEqual([
            {key: worker[key] for key in ("id", "state", "batch_id")}
            for worker in result["workers"]
        ], [
            {"id": "blend05", "state": "active", "batch_id": _dispatch_id(["next"])},
            {"id": "blendit", "state": "active", "batch_id": _dispatch_id(["next"])},
            {"id": "blendit04", "state": "active", "batch_id": _dispatch_id(["next"])},
        ])

    def test_auth_or_proxy_failure_blocks_dependent_manifest_without_dispatch(self):
        for category in ("auth_failure", "proxy_failure"):
            with self.subTest(category=category):
                backend = self.backend()
                runner = FakeRunner({
                    backend.harvest_command("blend05", "dispatch-finished"):
                        CommandResult(0, CLASSIFICATION_PREFIX + category, ""),
                })
                dispatcher = FakeDispatcher()
                adapter = HordeOperationalAdapter(
                    backend=backend, runner=runner, dispatch_batch=dispatcher
                )

                result = run_operational_controller_cycle(
                    workers=[
                        completed_worker(make_manifest("finished-a")),
                        {"id": "blendit04", "state": "idle"},
                        {"id": "blendit", "state": "idle"},
                    ],
                    queued_batches=[entry()],
                    registered_families=REGISTERED_FAMILIES,
                    adapter=adapter,
                )

                self.assertEqual(dispatcher.calls, [])
                self.assertIn(
                    {"worker_id": "blend05", "classification": category},
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
            backend.harvest_command("blend05", "dispatch-finished"):
                CommandResult(0, "invalid", RAW_LOG),
            backend.process_command("blendit2"): CommandResult(0, COMMAND_LINE, ""),
        })
        adapter = HordeOperationalAdapter(
            backend=backend, runner=runner, dispatch_batch=FakeDispatcher()
        )

        result = run_operational_controller_cycle(
            workers=[
                completed_worker(make_manifest("finished-first")),
                {"id": "blendit2", "state": "active", "batch_id": "finished-second"},
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

    def test_adapter_parses_only_bounded_completion_protocol(self):
        backend = self.backend()
        completion = make_completion("next")
        command = backend.harvest_command("blend05", "dispatch-finished")
        adapter = HordeOperationalAdapter(
            backend=backend,
            runner=FakeRunner({
                command: CommandResult(0, evidence(completion), ""),
            }),
            dispatch_batch=FakeDispatcher(),
        )

        result = adapter.harvest_finished("blend05", "dispatch-finished")

        self.assertEqual(result["classification"], "completion")
        self.assertEqual(result["completion"], completion)
        self.assertNotIn("log", repr(result).lower())

    def test_adapter_rejects_exit_zero_without_completion(self):
        backend = self.backend()
        command = backend.harvest_command("blend05", "dispatch-finished")
        adapter = HordeOperationalAdapter(
            backend=backend,
            runner=FakeRunner({
                command: CommandResult(0, "MATERIALX_HORDE_EXIT:0", ""),
            }),
            dispatch_batch=FakeDispatcher(),
        )

        self.assertEqual(
            adapter.harvest_finished("blend05", "dispatch-finished"),
            {"classification": "invalid_completion"},
        )

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
                [{"id": "blend05", "state": "active", "batch_id": "finished"}],
                [{"batch_id": "legacy", "prompt": PROMPT}],
                "queue",
            ),
            (
                [{"id": "blend05", "state": "active", "batch_id": "next"}],
                [entry(make_manifest("next"))],
                "already attached",
            ),
            (
                [
                    {"id": "blend05", "state": "active", "batch_id": "finished-a"},
                    {"id": "blend05", "state": "active", "batch_id": "finished-b"},
                ],
                [],
                "unique",
            ),
            (
                [{"id": "blend05", "state": "active", "batch_id": "finished"}],
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

    def test_operational_cycle_has_no_legacy_direct_state_write_path(self):
        parameters = inspect.signature(run_operational_controller_cycle).parameters

        self.assertNotIn("state_path", parameters)
        self.assertNotIn("journal_path", parameters)

    def test_two_completions_integrate_and_refill_in_the_same_operational_cycle(self):
        backend = self.backend()
        first_finished = make_manifest("finished-a")
        second_finished = second_manifest("finished-b")
        second_finished["node_defs"].sort()
        first_next = make_manifest("next-a")
        second_next = second_manifest("next-b")
        runner = FakeRunner({
            backend.harvest_command("blend05", "dispatch-finished"):
                CommandResult(0, evidence(completion_for(first_finished)), ""),
            backend.harvest_command("blendit2", "dispatch-finished"):
                CommandResult(0, evidence(completion_for(second_finished)), ""),
        })
        dispatcher = FakeDispatcher()
        adapter = HordeOperationalAdapter(
            backend=backend, runner=runner, dispatch_batch=dispatcher
        )
        integration_backend = FakeIntegrationBackend()

        result = run_operational_controller_cycle(
            workers=[
                completed_worker(first_finished),
                completed_worker(second_finished),
                {"id": "blendit04", "state": "idle"},
                {"id": "blendit", "state": "idle"},
                {"id": "blendit3", "state": "idle"},
            ],
            queued_batches=[entry(first_next), entry(second_next)],
            registered_families=REGISTERED_FAMILIES,
            adapter=adapter,
            integration_backend=integration_backend,
        )

        self.assertEqual(result["queue_depth"], 2)
        self.assertEqual(
            [receipt["batch_id"] for receipt in result["integration_receipts"]],
            ["finished-a", "finished-b"],
        )
        self.assertTrue(all(
            receipt["final_state"] == "integrated"
            for receipt in result["integration_receipts"]
        ))
        self.assertEqual(
            [batch["batch_id"] for batch in result["assigned_batches"]],
            ["next-a", "next-b"],
        )
        self.assertEqual(len(dispatcher.calls), 1)
        self.assertTrue(integration_backend.calls)

    def test_supervisor_controller_reuses_bounded_cycle_and_updates_workers(self):
        manifest = make_manifest("next")
        dispatcher = FakeDispatcher()
        controller = OperationalSupervisorController(
            workers=finished_workers(ALL_WORKERS),
            queue_source=lambda: [entry(manifest)],
            registered_families=REGISTERED_FAMILIES,
            adapter=HordeOperationalAdapter(
                backend=self.backend(),
                runner=FakeRunner(),
                dispatch_batch=dispatcher,
            ),
            integration_backend=FakeIntegrationBackend(),
        )

        first = controller.run_cycle()
        second = controller.run_cycle()

        self.assertEqual(first["queue_depth"], 1)
        self.assertEqual(len(first["assigned_batches"]), 1)
        self.assertEqual(len(dispatcher.calls), 1)
        self.assertEqual(second["queue_depth"], 0)
        self.assertEqual(second["assigned_batches"], [])

    def test_supervisor_controller_rejects_nonexact_worker_set_before_adapter(self):
        cases = (
            finished_workers(ALL_WORKERS[:-1]),
            finished_workers((*ALL_WORKERS, "extra")),
            finished_workers((*ALL_WORKERS[:-1], ALL_WORKERS[0])),
            finished_workers((*ALL_WORKERS[:-1], "wrong")),
        )
        for workers in cases:
            with self.subTest(worker_ids=[worker["id"] for worker in workers]):
                runner = FakeRunner()
                dispatcher = FakeDispatcher()
                adapter = HordeOperationalAdapter(
                    backend=self.backend(),
                    runner=runner,
                    dispatch_batch=dispatcher,
                )
                with self.assertRaisesRegex(ValueError, "exact five"):
                    OperationalSupervisorController(
                        workers=workers,
                        queue_source=lambda: [],
                        registered_families=REGISTERED_FAMILIES,
                        adapter=adapter,
                        integration_backend=FakeIntegrationBackend(),
                    )
                self.assertEqual(runner.calls, [])
                self.assertEqual(dispatcher.calls, [])

    def test_cli_delegates_only_to_canonical_supervisor_state_path(self):
        runtime = {
            "workers": object(),
            "queue_source": object(),
            "registered_families": object(),
            "adapter": object(),
            "integration_backend": object(),
        }
        with tempfile.TemporaryDirectory() as directory, mock.patch(
            "materialx_horde_operational.run_operational_supervisor",
            return_value=0,
        ) as run:
            state_path = Path(directory) / "state.json"

            exit_code = main(
                [
                    "--once",
                    "--poll-interval",
                    "2.5",
                    "--queue-watermark",
                    "7",
                    "--state",
                    str(state_path),
                ],
                runtime_factory=lambda config: runtime,
            )

        self.assertEqual(exit_code, 0)
        config = run.call_args.args[0]
        self.assertEqual(config.poll_interval_seconds, 2.5)
        self.assertEqual(config.queue_watermark, 7)
        self.assertTrue(run.call_args.kwargs["once"])
        self.assertEqual(run.call_args.kwargs["state_store"].path, state_path)
        self.assertNotIn("state_path", run.call_args.kwargs)
        self.assertNotIn("journal_path", run.call_args.kwargs)

    def test_cli_validation_fails_before_runtime_construction(self):
        for arguments in (
            ["--once", "--poll-interval", "0", "--state", "state.json"],
            ["--once", "--queue-watermark", "4", "--state", "state.json"],
        ):
            with self.subTest(arguments=arguments):
                factory = mock.Mock()
                with self.assertRaises(SystemExit) as raised:
                    main(arguments, runtime_factory=factory)
                self.assertEqual(raised.exception.code, 2)
                factory.assert_not_called()

    def test_no_injection_cli_loads_canonical_v2_runtime_and_reaches_supervisor(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repository = root / "repository"
            repository.mkdir()
            (repository / ".git").mkdir()
            credentials = root / "credentials.env"
            credentials.write_text(
                f"NVIDIA_API_KEY={CREDENTIAL}\n",
                encoding="utf-8",
            )
            manifest = make_manifest("next")
            runtime_config = root / "runtime.json"
            runtime_config.write_text(json.dumps({
                "schema_version": 2,
                "workers": finished_workers(ALL_WORKERS),
                "queued_batches": [entry(manifest)],
                "registered_families": REGISTERED_FAMILIES,
                "horde_workers": {
                    worker: {"host": worker} for worker in ALL_WORKERS
                },
                "repository_root": str(repository),
                "integration_worktree_root": str(root / "worktrees"),
            }), encoding="utf-8")
            state = root / "state.json"

            with mock.patch(
                "materialx_horde_operational.run_operational_supervisor",
                return_value=0,
            ) as run:
                exit_code = main([
                    "--once",
                    "--config",
                    str(runtime_config),
                    "--credentials",
                    str(credentials),
                    "--state",
                    str(state),
                ])

            self.assertEqual(exit_code, 0)
            runtime = run.call_args.kwargs
            self.assertEqual(
                [worker["id"] for worker in runtime["workers"]],
                sorted(ALL_WORKERS),
            )
            self.assertIsInstance(runtime["integration_backend"], GitIntegrationBackend)
            self.assertNotIn(CREDENTIAL, repr(run.call_args))

    def test_runtime_config_missing_or_noncanonical_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            credentials = root / "credentials.env"
            credentials.write_text(
                f"NVIDIA_API_KEY={CREDENTIAL}\n",
                encoding="utf-8",
            )
            for document in (
                None,
                {"schema_version": 1},
                {"schema_version": 2, "prompt": PROMPT},
            ):
                config = root / (
                    "missing.json" if document is None else f"bad-{document['schema_version']}.json"
                )
                if document is not None:
                    config.write_text(json.dumps(document), encoding="utf-8")
                with self.subTest(document=document):
                    with self.assertRaises(ValueError):
                        load_runtime_config(config, credentials)

    def test_unassigned_absent_worker_is_safely_blocked(self):
        backend = self.backend()
        adapter = HordeOperationalAdapter(
            backend=backend, runner=FakeRunner(), dispatch_batch=FakeDispatcher()
        )
        result = run_operational_controller_cycle(
            workers=[{"id": "blend05", "state": "blocked"}],
            queued_batches=[],
            registered_families=REGISTERED_FAMILIES,
            adapter=adapter,
        )
        self.assertEqual(result["workers"], [{"id": "blend05", "state": "blocked"}])
        self.assertEqual(
            result["alerts"],
            [{"worker_id": "blend05", "classification": "process_missing"}],
        )

    def test_fresh_absent_idle_role_workers_dispatch_without_harvest(self):
        backend = self.backend()
        dispatcher = FakeDispatcher()
        runner = FakeRunner()
        adapter = HordeOperationalAdapter(
            backend=backend, runner=runner, dispatch_batch=dispatcher
        )
        manifest = make_manifest("fresh")

        result = run_operational_controller_cycle(
            workers=[
                {"id": worker, "state": "idle"}
                for worker in manifest["roles"].values()
            ],
            queued_batches=[entry(manifest)],
            registered_families=REGISTERED_FAMILIES,
            adapter=adapter,
        )

        self.assertEqual(dispatcher.calls, [[manifest]])
        self.assertEqual(result["assigned_batches"], [
            {"worker_id": "blend05", "batch_id": "fresh"}
        ])
        self.assertFalse(any(
            "MATERIALX_HORDE_EXIT" in command[-1] for command in runner.calls
        ))

    def test_queue_empty_and_deferred_operational_outputs_reenter(self):
        backend = self.backend()
        queue_empty_runner = FakeRunner()
        queue_empty_adapter = HordeOperationalAdapter(
            backend=backend,
            runner=queue_empty_runner,
            dispatch_batch=FakeDispatcher(),
        )
        queue_empty = run_operational_controller_cycle(
            workers=[{"id": "blend05", "state": "idle"}],
            queued_batches=[],
            registered_families=REGISTERED_FAMILIES,
            adapter=queue_empty_adapter,
        )
        first_harvest_count = sum(
            "MATERIALX_HORDE_EXIT" in command[-1]
            for command in queue_empty_runner.calls
        )
        queue_empty_next = run_operational_controller_cycle(
            workers=queue_empty["workers"],
            queued_batches=[],
            registered_families=REGISTERED_FAMILIES,
            adapter=queue_empty_adapter,
        )
        self.assertEqual(queue_empty_next["workers"], [
            {"id": "blend05", "state": "idle"}
        ])
        self.assertEqual(sum(
            "MATERIALX_HORDE_EXIT" in command[-1]
            for command in queue_empty_runner.calls
        ), first_harvest_count)

        deferred_runner = FakeRunner({
            backend.harvest_command("blend05", "dispatch-finished"):
                CommandResult(0, "MATERIALX_HORDE_EXIT:1", ""),
        })
        deferred_dispatcher = FakeDispatcher()
        deferred_adapter = HordeOperationalAdapter(
            backend=backend,
            runner=deferred_runner,
            dispatch_batch=deferred_dispatcher,
        )
        deferred = run_operational_controller_cycle(
            workers=[
                completed_worker(make_manifest("finished-a")),
                {"id": "blendit04", "state": "idle"},
                {"id": "blendit", "state": "idle"},
            ],
            queued_batches=[entry()],
            registered_families=REGISTERED_FAMILIES,
            adapter=deferred_adapter,
        )
        first_harvest_count = sum(
            "MATERIALX_HORDE_EXIT" in command[-1]
            for command in deferred_runner.calls
        )
        deferred_next = run_operational_controller_cycle(
            workers=deferred["workers"],
            queued_batches=[entry()],
            registered_families=REGISTERED_FAMILIES,
            adapter=deferred_adapter,
        )
        self.assertEqual(len(deferred_dispatcher.calls), 0)
        self.assertEqual(sum(
            "MATERIALX_HORDE_EXIT" in command[-1]
            for command in deferred_runner.calls
        ), first_harvest_count)
        self.assertTrue(any(
            alert["classification"] == "role_worker_unavailable"
            for alert in deferred_next["alerts"]
        ))

    def test_missing_persisted_worker_state_fails_before_process_calls(self):
        runner = FakeRunner()
        adapter = HordeOperationalAdapter(
            backend=self.backend(), runner=runner, dispatch_batch=FakeDispatcher()
        )

        with self.assertRaisesRegex(ValueError, "state"):
            run_operational_controller_cycle(
                workers=[{"id": "blend05"}],
                queued_batches=[],
                registered_families=REGISTERED_FAMILIES,
                adapter=adapter,
            )
        self.assertEqual(runner.calls, [])

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

    def test_operational_supervisor_forwards_strict_alert_sink(self):
        alert_sink = SanitizedAlertSink(FakeAlertTransport())
        with mock.patch(
            "materialx_horde_operational.run_supervisor",
            return_value=0,
        ) as run:
            result = run_operational_supervisor(
                mock.Mock(),
                workers=finished_workers(ALL_WORKERS),
                queue_source=lambda: [],
                registered_families=REGISTERED_FAMILIES,
                adapter=mock.Mock(),
                integration_backend=mock.Mock(),
                state_store=mock.Mock(),
                clock=mock.Mock(),
                sleeper=mock.Mock(),
                alert_sink=alert_sink,
                once=True,
            )

        self.assertEqual(result, 0)
        self.assertIs(run.call_args.kwargs["alert_sink"], alert_sink)

    def test_runtime_factory_may_supply_alert_sink_to_canonical_supervisor(self):
        alert_sink = SanitizedAlertSink(FakeAlertTransport())
        runtime = {
            "workers": finished_workers(ALL_WORKERS),
            "queue_source": lambda: [],
            "registered_families": REGISTERED_FAMILIES,
            "adapter": mock.Mock(),
            "integration_backend": mock.Mock(),
            "alert_sink": alert_sink,
        }
        with tempfile.TemporaryDirectory() as directory, mock.patch(
            "materialx_horde_operational.run_operational_supervisor",
            return_value=0,
        ) as run:
            result = main(
                ["--once", "--state", str(Path(directory) / "state.json")],
                runtime_factory=lambda config: runtime,
            )

        self.assertEqual(result, 0)
        self.assertIs(run.call_args.kwargs["alert_sink"], alert_sink)

    def test_runtime_factory_rejects_non_sanitized_alert_sink(self):
        runtime = {
            "workers": finished_workers(ALL_WORKERS),
            "queue_source": lambda: [],
            "registered_families": REGISTERED_FAMILIES,
            "adapter": mock.Mock(),
            "integration_backend": mock.Mock(),
            "alert_sink": object(),
        }
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "SanitizedAlertSink"):
                main(
                    ["--once", "--state", str(Path(directory) / "state.json")],
                    runtime_factory=lambda config: runtime,
                )


if __name__ == "__main__":
    unittest.main()
