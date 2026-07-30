"""Tests for the safe operational MaterialX Horde dispatcher."""

from __future__ import annotations

import base64
import copy
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

import materialx_horde_dispatch as horde_dispatch
import materialx_project_state
from materialx_horde_dispatch import CommandResult, HordeBackend, credential_values, dry_run, execute_dispatch
from materialx_horde_dispatch_plan import build_dispatch_plan
from test_materialx_horde_dispatch_plan import REGISTERED_FAMILIES, make_manifest


WORKERS = ["blend05", "blendit", "blendit04"]


class FakeRunner:
    def __init__(self, results: dict[tuple[str, ...], CommandResult] | None = None):
        self.results = results or {}
        self.calls: list[tuple[tuple[str, ...], dict[str, str]]] = []
        self.inputs: list[str | None] = []

    def __call__(
        self,
        command: tuple[str, ...],
        *,
        env: dict[str, str] | None = None,
        input_text: str | None = None,
        timeout: int,
    ) -> CommandResult:
        self.calls.append((command, dict(env or {})))
        self.inputs.append(input_text)
        if command in self.results:
            return self.results[command]
        if "--show-toplevel" in command[-1]:
            return CommandResult(
                0,
                json.dumps({
                    "repository_present": True,
                    "files": {
                        "intern/cycles/scene/materialx.cpp": True,
                        "tools/materialx/materialx_velocity_manifest.py": True,
                        "tools/materialx/materialx_batch_scheduler.py": True,
                    },
                    "head": "a" * 40,
                }, separators=(",", ":")),
                "",
            )
        if command[-1].startswith("pids=$(pgrep -f"):
            return CommandResult(0, "active:123", "")
        return CommandResult(0, "ok", "")


class MaterialXHordeDispatchTest(unittest.TestCase):
    @staticmethod
    def source_document(head="a" * 40, *, repository_present=True, files=None):
        return json.dumps({
            "repository_present": repository_present,
            "files": files or {
                "intern/cycles/scene/materialx.cpp": True,
                "tools/materialx/materialx_velocity_manifest.py": True,
                "tools/materialx/materialx_batch_scheduler.py": True,
            },
            "head": head,
        }, separators=(",", ":"))

    @staticmethod
    def backend(workers=WORKERS):
        return HordeBackend({
            worker: {
                "host": f"horde-{worker}",
                "user": "horde",
                "runner_path": "/home/horde/ovrtx/hermes_runner.py",
            }
            for worker in workers
        })

    @staticmethod
    def prompt_from_command(command: tuple[str, ...]) -> str:
        for encoded in re.findall(r"[A-Za-z0-9+/]{16,}={0,2}", command[-1]):
            try:
                prompt = base64.b64decode(encoded).decode("utf-8")
            except (UnicodeDecodeError, ValueError):
                continue
            if "MaterialX" in prompt:
                return prompt
        raise AssertionError("runner prompt is missing")

    def make_plan(
        self,
        root: Path,
        manifests=None,
        *,
        credential_text: str = "NVIDIA_API_KEY=not-a-real-nvidia-key\n",
    ):
        credential_file = root / "credentials.env"
        credential_file.write_text(credential_text, encoding="utf-8")
        return build_dispatch_plan(
            manifests or [make_manifest("materialx-smoke")],
            credential_file,
            registered_families=REGISTERED_FAMILIES,
        )

    def test_dry_run_does_not_open_credentials_or_write_records(self) -> None:
        plan = build_dispatch_plan(
            [make_manifest("materialx-smoke")],
            "does-not-exist.env",
            registered_families=REGISTERED_FAMILIES,
        )

        result = dry_run(plan)

        self.assertTrue(result["ok"])
        self.assertEqual(result["mode"], "dry_run")
        self.assertEqual(result["batch_ids"], ["materialx-smoke"])
        self.assertIn("no_write_probe", result["steps"])

    def test_worker_instruction_is_deterministic_combines_roles_and_copies_no_prose(self) -> None:
        second = make_manifest(
            "materialx-second",
            node_start=8,
            roles={
                "implementation": "blendit2",
                "generated_tests": "blendit3",
                "independent_review": "blend05",
            },
        )
        with tempfile.TemporaryDirectory() as directory:
            plan = self.make_plan(Path(directory), [second, make_manifest("materialx-smoke")])

        instruction = horde_dispatch._worker_prompt(
            plan["assignments"], plan["worker_tasks"], "blend05"
        )
        reverse_instruction = horde_dispatch._worker_prompt(
            list(reversed(plan["assignments"])), plan["worker_tasks"], "blend05"
        )

        self.assertEqual(instruction, reverse_instruction)
        self.assertEqual(instruction.count("Batch ID:"), 2)
        self.assertIn("Worker ID: blend05", instruction)
        self.assertIn("Role: implementation", instruction)
        self.assertIn("Role: independent_review", instruction)
        self.assertIn("Layer: native_cycles", instruction)
        self.assertIn("Family: add", instruction)
        self.assertIn("Base SHA: " + "a" * 40, instruction)
        self.assertIn("Exact NodeDefs:", instruction)
        self.assertIn("Exact files:", instruction)
        self.assertIn("Exact test commands:", instruction)
        self.assertIn("exact-completion", instruction.lower())
        self.assertNotIn("prompt", instruction.lower())

    def test_execute_launches_each_derived_worker_once_with_safe_dispatch_identity(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan = self.make_plan(root)
            backend = self.backend()
            runner = FakeRunner()

            result = execute_dispatch(
                plan, backend=backend, runner=runner,
                capacity_state_path=root / "capacity.json", journal_path=root / "journal.json",
            )

            launches = [call[0] for call in runner.calls if call[0][-1].startswith("nohup ")]
            self.assertTrue(result["ok"])
            self.assertEqual(len(launches), len(WORKERS))
            self.assertEqual({command[-2] for command in launches}, {f"horde@horde-{worker}" for worker in WORKERS})
            self.assertTrue(all(plan["dispatch_id"] in command[-1] for command in launches))
            self.assertEqual(result["batch_ids"], ["materialx-smoke"])
            journal = json.loads((root / "journal.json").read_text(encoding="utf-8"))
            self.assertEqual(journal["dispatch_id"], plan["dispatch_id"])
            self.assertEqual(journal["batch_ids"], ["materialx-smoke"])
            self.assertNotIn("instruction", json.dumps(journal).lower())

    def test_dispatch_updates_only_horde_fields_in_existing_canonical_state(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "capacity.json"
            state = materialx_project_state.new_project_state()
            state["lanes"]["local_cpu"] = {
                "state": "green",
                "last_evidence_id": "cpu-green-before-dispatch",
            }
            state["lanes"]["local_cuda"] = {
                "state": "green",
                "last_evidence_id": "cuda-green-before-dispatch",
            }
            state["lanes"]["windows_a40_cuda"] = {
                "state": "due",
                "last_evidence_id": "",
            }
            materialx_project_state.write_project_state(path, state)
            plan = self.make_plan(root)

            result = execute_dispatch(
                plan,
                backend=self.backend(),
                runner=FakeRunner(),
                capacity_state_path=path,
                journal_path=root / "journal.json",
            )

            persisted = materialx_project_state.load_project_state(path)
            self.assertTrue(result["ok"])
            self.assertEqual(persisted["lanes"]["local_cpu"], state["lanes"]["local_cpu"])
            self.assertEqual(persisted["lanes"]["local_cuda"], state["lanes"]["local_cuda"])
            self.assertEqual(
                persisted["lanes"]["windows_a40_cuda"],
                state["lanes"]["windows_a40_cuda"],
            )
            self.assertEqual(persisted["lanes"]["horde"]["state"], "degraded")
            worker_states = {worker["id"]: worker["state"] for worker in persisted["workers"]}
            self.assertTrue(all(worker_states[worker] == "active" for worker in WORKERS))
            self.assertTrue(all(
                worker_states[worker] == "unknown"
                for worker in set(worker_states) - set(WORKERS)
            ))
            self.assertTrue(any(
                record["event_kind"] == "dispatch"
                for record in persisted["semantic_journal"]
            ))

    def test_dispatch_explicitly_migrates_v1_and_adds_missing_workers_unknown(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "capacity.json"
            path.write_text(json.dumps({
                "schema_version": 1,
                "healthy_workers": [],
                "completed_rows": [],
                "evidence_records": [],
                "journal_records": [],
                "lanes": {
                    "windows_local_build": {"state": "ready", "alerted": False},
                },
                "capacity_journal": [],
                "alerts": [],
            }), encoding="utf-8")

            result = execute_dispatch(
                self.make_plan(root),
                backend=self.backend(),
                runner=FakeRunner(),
                capacity_state_path=path,
                journal_path=root / "journal.json",
            )

            persisted = materialx_project_state.load_project_state(path)
            self.assertTrue(result["ok"])
            self.assertEqual(
                persisted["lanes"]["windows_a40_cuda"],
                {"state": "due", "last_evidence_id": ""},
            )
            self.assertEqual(
                [worker["id"] for worker in persisted["workers"]],
                sorted(("blend05", "blendit04", "blendit", "blendit2", "blendit3")),
            )

    def test_safe_dispatch_identity_keeps_valid_manifest_batch_ids_out_of_remote_paths(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            batch_id = "batch/identity retained exactly"
            plan = self.make_plan(root, [make_manifest(batch_id)])
            runner = FakeRunner()

            result = execute_dispatch(
                plan,
                backend=self.backend(),
                runner=runner,
                capacity_state_path=root / "capacity.json",
                journal_path=root / "journal.json",
            )

            self.assertTrue(result["ok"])
            self.assertEqual(result["batch_ids"], [batch_id])
            remote_commands = [command[-1] for command, _ in runner.calls]
            self.assertTrue(any(plan["dispatch_id"] in command for command in remote_commands))
            self.assertFalse(any(batch_id in command for command in remote_commands))

    def test_source_preflight_isolates_one_stale_worker_and_persists_partial_state(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan = self.make_plan(root)
            backend = self.backend()
            stale = "blendit"
            runner = FakeRunner({
                backend.source_preflight_command(stale): CommandResult(0, self.source_document("b" * 40), ""),
            })

            result = execute_dispatch(
                plan, backend=backend, runner=runner,
                capacity_state_path=root / "capacity.json", journal_path=root / "journal.json",
            )

            self.assertTrue(result["ok"])
            self.assertEqual(result["outcome"], "partial")
            self.assertEqual(result["worker_states"][stale], "stale_source")
            source_targets = {command[-2] for command, _ in runner.calls if "--show-toplevel" in command[-1]}
            launch_targets = {command[-2] for command, _ in runner.calls if command[-1].startswith("nohup ")}
            self.assertEqual(source_targets, {f"horde@horde-{worker}" for worker in WORKERS})
            self.assertEqual(launch_targets, {f"horde@horde-{worker}" for worker in WORKERS if worker != stale})

    def test_source_preflight_failure_categories_isolate_other_workers(self) -> None:
        cases = (
            (self.source_document(repository_present=False), "missing_repository"),
            (self.source_document(files={
                "intern/cycles/scene/materialx.cpp": False,
                "tools/materialx/materialx_velocity_manifest.py": True,
                "tools/materialx/materialx_batch_scheduler.py": True,
            }), "missing_required_file"),
            ("not-json", "invalid_probe"),
        )
        for source_result, expected in cases:
            with self.subTest(expected=expected), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                plan = self.make_plan(root)
                backend = self.backend()
                runner = FakeRunner({
                    backend.source_preflight_command("blend05"): CommandResult(0, source_result, ""),
                })

                result = execute_dispatch(
                    plan, backend=backend, runner=runner,
                    capacity_state_path=root / "capacity.json", journal_path=root / "journal.json",
                )

                self.assertEqual(result["outcome"], "partial")
                self.assertEqual(result["worker_states"]["blend05"], expected)
                self.assertEqual(
                    {command[-2] for command, _ in runner.calls if "NVIDIA_API_KEY" in command[-1]},
                    {f"horde@horde-{worker}" for worker in WORKERS if worker != "blend05"},
                )

    def test_probe_persistence_launch_and_process_failures_are_isolated(self) -> None:
        for failed_step, expected_state in (
            ("probe", "probe_failure"),
            ("persistence", "credential_persistence_failure"),
            ("launch", "launch_failure"),
            ("process", "process_missing"),
        ):
            with self.subTest(failed_step=failed_step), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                plan = self.make_plan(root)
                backend = self.backend()
                instruction = horde_dispatch._worker_prompt(
                    plan["assignments"], plan["worker_tasks"], "blend05"
                )
                command = {
                    "probe": backend.probe_command("blend05", plan["dispatch_id"]),
                    "persistence": backend.persist_command("blend05"),
                    "launch": backend.launch_command("blend05", plan["dispatch_id"], instruction),
                    "process": backend.process_command("blend05"),
                }[failed_step]
                runner = FakeRunner({command: CommandResult(1, "", f"{failed_step} unavailable")})

                result = execute_dispatch(
                    plan, backend=backend, runner=runner,
                    capacity_state_path=root / "capacity.json", journal_path=root / "journal.json",
                )

                self.assertEqual(result["outcome"], "partial")
                self.assertEqual(result["worker_states"]["blend05"], expected_state)
                self.assertTrue(all(
                    result["worker_states"][worker] == "active"
                    for worker in WORKERS if worker != "blend05"
                ))

    def test_rejects_mismatched_plan_source_contract_before_remote_calls(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan = self.make_plan(root)
            plan["assignments"][0]["worker_source_sha"] = "b" * 40
            runner = FakeRunner()

            result = execute_dispatch(
                plan, backend=self.backend(), runner=runner,
                capacity_state_path=root / "capacity.json", journal_path=root / "journal.json",
            )

            self.assertFalse(result["ok"])
            self.assertEqual(result["alert"]["classification"], "source_preflight_failure")
            self.assertEqual(runner.calls, [])

    def test_execute_success_persists_one_key_per_worker_and_sanitizes_records(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            secret = "not-a-real-nvidia-key"
            plan = self.make_plan(root, credential_text=f"NVIDIA_API_KEY={secret}\n")
            runner = FakeRunner()

            result = execute_dispatch(
                plan, backend=self.backend(), runner=runner,
                capacity_state_path=root / "capacity.json", journal_path=root / "journal.json",
            )

            persistence_calls = [call for call in runner.calls if "NVIDIA_API_KEY" in call[0][-1]]
            self.assertTrue(result["ok"])
            self.assertEqual(len(persistence_calls), len(WORKERS))
            self.assertTrue(all(runner.inputs[runner.calls.index(call)] == secret + "\n" for call in persistence_calls))
            self.assertNotIn(secret, repr(result))
            self.assertNotIn(secret, (root / "capacity.json").read_text(encoding="utf-8"))
            self.assertNotIn(secret, (root / "journal.json").read_text(encoding="utf-8"))

    def test_alert_log_is_sanitized_without_stopping_other_workers(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            secret = "not-a-real-nvidia-key"
            plan = self.make_plan(root, credential_text=f"NVIDIA_API_KEY={secret}\n")
            backend = self.backend()
            runner = FakeRunner({
                backend.probe_command("blend05", plan["dispatch_id"]):
                    CommandResult(1, "", f"NVIDIA_API_KEY={secret} probe rejected"),
            })

            result = execute_dispatch(
                plan, backend=backend, runner=runner,
                capacity_state_path=root / "capacity.json", journal_path=root / "journal.json",
            )

            rendered = json.dumps(result, sort_keys=True)
            self.assertNotIn(secret, rendered)
            self.assertIn("NVIDIA_API_KEY=[REDACTED]", rendered)
            self.assertEqual(result["outcome"], "partial")

    def test_raw_three_token_credential_is_supported_without_emitting_values(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            secret = "part-one part-two part-three"
            plan = self.make_plan(root, credential_text=secret + "\n")
            runner = FakeRunner()

            self.assertEqual(credential_values(plan["credential_file"]), ["part-one", "part-two", "part-three"])
            result = execute_dispatch(
                plan, backend=self.backend(), runner=runner,
                capacity_state_path=root / "capacity.json", journal_path=root / "journal.json",
            )

            self.assertTrue(result["ok"])
            self.assertNotIn(secret, json.dumps(result, sort_keys=True))

    def test_documented_defaults_map_all_recorded_horde_hosts(self) -> None:
        backend = HordeBackend.documented_defaults()
        expected_hosts = {
            "blend05": "canderson-blend05.ov-agent-farm.svc.cluster.local",
            "blendit04": "canderson-blendit04.ov-agent-farm.svc.cluster.local",
            "blendit": "canderson-blendit.ov-agent-farm.svc.cluster.local",
            "blendit2": "canderson-canderson-blendit2-bot.ov-agent-farm.svc.cluster.local",
            "blendit3": "canderson-canderson-blendit3-bot.ov-agent-farm.svc.cluster.local",
        }
        for worker_id, host in expected_hosts.items():
            command = backend.process_command(worker_id)
            self.assertIn(f"horde@{host}", command)
            self.assertIn("horde@bastion.horde-gke.nvidia.com:2222", command)

    def test_backend_commands_are_bounded_and_emit_only_categorical_evidence(self) -> None:
        backend = self.backend()
        persistence = backend.persist_command("blend05")[-1]
        launch = backend.launch_command(
            "blend05", "dispatch-safe", "deterministic manifest instruction"
        )[-1]
        process = backend.process_command("blend05")[-1]
        harvest = backend.harvest_command("blend05", "dispatch-safe")[-1]
        source = backend.source_preflight_command("blend05")[-1]

        self.assertIn("export ", persistence)
        self.assertIn("nohup", launch)
        self.assertNotIn("pgrep -af", process)
        self.assertIn("active:", process)
        self.assertIn("MATERIALX_COMPLETION_V2", harvest)
        self.assertIn("MATERIALX_HORDE_EXIT", harvest)
        self.assertIn("MATERIALX_HARVEST_V2", harvest)
        self.assertIn("MAX_LOG_WINDOW_BYTES", harvest)
        self.assertNotIn("read_text", harvest)
        self.assertIn("auth_failure", harvest)
        self.assertIn("proxy_failure", harvest)
        self.assertIn("repository_present", source)
        self.assertIn("tools/materialx/materialx_velocity_manifest.py", source)
        self.assertIn("--show-toplevel", source)
        self.assertNotIn("NVIDIA_API_KEY", source)
        with self.assertRaises(ValueError):
            backend.launch_command(
                "blend05", "../unsafe", "deterministic manifest instruction"
            )

    def test_launch_requires_an_explicit_non_empty_instruction(self) -> None:
        backend = self.backend()

        with self.assertRaises(TypeError):
            backend.launch_command("blend05", "dispatch-safe")
        for instruction in ("", " \t\r\n"):
            with self.subTest(instruction=repr(instruction)):
                with self.assertRaises(ValueError):
                    backend.launch_command("blend05", "dispatch-safe", instruction)

    def test_harvest_executes_authentication_classifier_before_zero_exit_sentinel(self) -> None:
        classifier_script = horde_dispatch._harvest_classifier_script
        representative_failures = (
            "Error code: 401 - invalid credentials",
            "AuthenticationError: status_code=401",
            "401 Client Error: Unauthorized",
            "HTTP 401",
            "HTTPError: 401 Client Error",
            "Request failed with status 401",
        )
        with tempfile.TemporaryDirectory() as directory:
            log_path = Path(directory, "representative.log").resolve()
            for failure in representative_failures:
                with self.subTest(failure=failure):
                    log_path.write_text(f"{failure}\nMATERIALX_HORDE_EXIT:0\n", encoding="utf-8")
                    result = subprocess.run(
                        (sys.executable, "-c", classifier_script(), str(log_path)),
                        capture_output=True, text=True, check=False, timeout=5,
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(
                        result.stdout, "MATERIALX_HARVEST_V2:auth_failure"
                    )


if __name__ == "__main__":
    unittest.main()
