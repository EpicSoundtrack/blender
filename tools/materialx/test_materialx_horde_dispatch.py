"""Tests for the safe operational MaterialX Horde dispatcher."""

from __future__ import annotations

import json
import base64
import re
from pathlib import Path
import tempfile
import unittest

from materialx_horde_dispatch import CommandResult, HordeBackend, credential_values, dry_run, execute_dispatch
from materialx_horde_dispatch_plan import build_dispatch_plan


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
        if command[-1].startswith("pids=$(pgrep -f"):
            return self.results.get(command, CommandResult(returncode=0, stdout="active:123", stderr=""))
        return self.results.get(command, CommandResult(returncode=0, stdout="ok", stderr=""))


class MaterialXHordeDispatchTest(unittest.TestCase):
    @staticmethod
    def prompt_from_command(command: tuple[str, ...]) -> str:
        for encoded in re.findall(r"[A-Za-z0-9+/]{16,}={0,2}", command[-1]):
            try:
                prompt = base64.b64decode(encoded).decode("utf-8")
            except UnicodeDecodeError:
                continue
            if "MaterialX" in prompt:
                return prompt
        raise AssertionError("runner prompt is missing")

    @staticmethod
    def make_backend() -> HordeBackend:
        return HordeBackend(
            {
                "gpu-a": {
                    "host": "horde-gpu-a",
                    "user": "horde",
                    "runner_path": "/home/horde/ovrtx/hermes_runner.py",
                }
            }
        )

    def make_plan(self, root: Path) -> tuple[dict[str, object], str]:
        secret = "not-a-real-nvidia-key"
        credential_file = root / "credentials.env"
        credential_file.write_text(f"NVIDIA_API_KEY={secret}\n", encoding="utf-8")
        return (
            build_dispatch_plan(
                workers=["gpu-a"],
                credential_file=credential_file,
                batch_manifest={"batch_id": "materialx-smoke"},
            ),
            secret,
        )

    def test_dry_run_does_not_open_credentials_or_write_records(self) -> None:
        plan = build_dispatch_plan(
            workers=["gpu-a"],
            credential_file="does-not-exist.env",
            batch_manifest={"batch_id": "materialx-smoke"},
        )

        result = dry_run(plan)

        self.assertTrue(result["ok"])
        self.assertEqual(result["mode"], "dry_run")
        self.assertIn("no_write_probe", result["steps"])

    def test_execute_success_persists_one_key_and_writes_sanitized_records(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            plan, secret = self.make_plan(root)
            capacity_path = root / "capacity.json"
            journal_path = root / "journal.json"
            runner = FakeRunner()

            result = execute_dispatch(
                plan,
                backend=self.make_backend(),
                runner=runner,
                capacity_state_path=capacity_path,
                journal_path=journal_path,
            )

            self.assertTrue(result["ok"])
            self.assertEqual(result["mode"], "execute")
            persistence_calls = [call for call in runner.calls if "NVIDIA_API_KEY" in call[0][-1]]
            self.assertEqual(len(persistence_calls), 1)
            self.assertEqual(persistence_calls[0][1], {})
            self.assertEqual(runner.inputs[runner.calls.index(persistence_calls[0])], secret + "\n")
            self.assertNotIn(secret, repr(result))
            self.assertNotIn(secret, capacity_path.read_text(encoding="utf-8"))
            self.assertNotIn(secret, journal_path.read_text(encoding="utf-8"))

            capacity = json.loads(capacity_path.read_text(encoding="utf-8"))
            journal = json.loads(journal_path.read_text(encoding="utf-8"))
            self.assertEqual(capacity["healthy_workers"], [{"id": "gpu-a", "state": "active"}])
            self.assertFalse(capacity["lanes"]["windows_local_build"]["alerted"])
            self.assertEqual(journal["outcome"], "success")

    def test_probe_failure_stops_before_launch_and_writes_immediate_alert(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            plan, _ = self.make_plan(root)
            probe_command = self.make_backend().probe_command("gpu-a", "materialx-smoke")
            runner = FakeRunner(
                {probe_command: CommandResult(returncode=1, stdout="", stderr="probe unavailable")}
            )
            capacity_path = root / "capacity.json"
            journal_path = root / "journal.json"

            result = execute_dispatch(
                plan,
                backend=self.make_backend(),
                runner=runner,
                capacity_state_path=capacity_path,
                journal_path=journal_path,
            )

            self.assertFalse(result["ok"])
            self.assertEqual(result["alert"]["timing"], "immediate")
            self.assertEqual(result["alert"]["classification"], "probe_failure")
            self.assertFalse(any(call[0][1] == "launch" for call in runner.calls))
            capacity = json.loads(capacity_path.read_text(encoding="utf-8"))
            self.assertEqual(capacity["healthy_workers"], [{"id": "gpu-a", "state": "blocked"}])
            self.assertTrue(capacity["lanes"]["windows_local_build"]["alerted"])

    def test_process_missing_fails_after_launch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            plan, _ = self.make_plan(root)
            process_command = self.make_backend().process_command("gpu-a")
            runner = FakeRunner(
                {process_command: CommandResult(returncode=1, stdout="", stderr="Hermes absent")}
            )

            result = execute_dispatch(
                plan,
                backend=self.make_backend(),
                runner=runner,
                capacity_state_path=root / "capacity.json",
                journal_path=root / "journal.json",
            )

            self.assertFalse(result["ok"])
            self.assertEqual(result["alert"]["classification"], "process_missing")
            self.assertTrue(
                any(
                    call[0][-1].startswith("nohup ")
                    and "MaterialX" in self.prompt_from_command(call[0])
                    for call in runner.calls
                )
            )

    def test_absent_or_ambiguous_post_launch_process_evidence_fails(self) -> None:
        for evidence in ("absent", "ambiguous"):
            with self.subTest(evidence=evidence), tempfile.TemporaryDirectory() as temporary_directory:
                root = Path(temporary_directory)
                plan, _ = self.make_plan(root)
                process_command = self.make_backend().process_command("gpu-a")
                runner = FakeRunner({process_command: CommandResult(returncode=0, stdout=evidence, stderr="")})

                result = execute_dispatch(
                    plan, backend=self.make_backend(), runner=runner,
                    capacity_state_path=root / "capacity.json", journal_path=root / "journal.json",
                )

                self.assertFalse(result["ok"])
                self.assertEqual(result["alert"]["classification"], "process_missing")

    def test_alert_log_is_sanitized(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            plan, secret = self.make_plan(root)
            probe_command = self.make_backend().probe_command("gpu-a", "materialx-smoke")
            runner = FakeRunner(
                {
                    probe_command: CommandResult(
                        returncode=1,
                        stdout="",
                        stderr=f"NVIDIA_API_KEY={secret} probe rejected",
                    )
                }
            )

            result = execute_dispatch(
                plan,
                backend=self.make_backend(),
                runner=runner,
                capacity_state_path=root / "capacity.json",
                journal_path=root / "journal.json",
            )

            rendered = json.dumps(result, sort_keys=True)
            self.assertNotIn(secret, rendered)
            self.assertIn("NVIDIA_API_KEY=[REDACTED]", rendered)

    def test_ssh_backend_runs_remote_probe_persistence_launch_and_pgrep(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            plan, secret = self.make_plan(root)
            backend = HordeBackend(
                {
                    "gpu-a": {
                        "host": "horde-gpu-a",
                        "user": "horde",
                        "runner_path": "/home/horde/ovrtx/hermes_runner.py",
                    }
                }
            )
            runner = FakeRunner()

            result = execute_dispatch(
                plan,
                backend=backend,
                runner=runner,
                capacity_state_path=root / "capacity.json",
                journal_path=root / "journal.json",
            )

            self.assertTrue(result["ok"])
            commands = [call[0] for call in runner.calls]
            ssh_prefix = (
                "ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=12", "-o",
                "StrictHostKeyChecking=no", "-J", "horde@bastion.horde-gke.nvidia.com:2222",
                "horde@horde-gpu-a",
            )
            self.assertNotIn("python3 /home/horde/ovrtx/hermes_runner.py", backend.probe_command("gpu-a", "materialx-smoke")[-1])
            prompt_commands = [command for command in commands if command[-1].startswith("nohup ")]
            prompts = [self.prompt_from_command(command) for command in prompt_commands]
            self.assertTrue(any("materialx-smoke" in prompt for prompt in prompts))
            self.assertIn(ssh_prefix + (backend.process_command("gpu-a")[-1],), commands)
            persistence = [call for call in runner.calls if "NVIDIA_API_KEY" in call[0][-1]]
            self.assertEqual(len(persistence), 1)
            self.assertEqual(persistence[0][1], {})
            self.assertIn("/home/horde/.hermes/.env", persistence[0][0][-1])
            self.assertNotIn(secret, " ".join(persistence[0][0]))

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

    def test_persistence_replaces_export_form_and_launches_runner_in_background(self) -> None:
        backend = self.make_backend()

        persistence = backend.persist_command("gpu-a")[-1]
        launch = backend.launch_command("gpu-a", "materialx-smoke")[-1]

        self.assertIn("export ", persistence)
        self.assertIn("rstrip", persistence)
        self.assertIn("nohup", launch)
        self.assertIn("hermes_runner.py", launch)

    def test_process_and_harvest_commands_emit_only_categorical_evidence(self) -> None:
        backend = self.make_backend()

        process = backend.process_command("gpu-a")[-1]
        harvest = backend.harvest_command("gpu-a", "materialx-smoke")[-1]

        self.assertNotIn("pgrep -af", process)
        self.assertIn("active:", process)
        self.assertIn("MATERIALX_HORDE_EXIT", harvest)
        self.assertIn("auth_failure", harvest)
        self.assertIn("proxy_failure", harvest)
        self.assertIn("401", harvest)
        self.assertLess(harvest.index("auth_failure"), harvest.index("MATERIALX_HORDE_EXIT:0"))
        self.assertLess(harvest.index("proxy_failure"), harvest.index("MATERIALX_HORDE_EXIT:0"))
        with self.assertRaises(ValueError):
            backend.launch_command("gpu-a", "../unsafe")

    def test_worker_specific_prompt_is_sent_to_hermes_but_never_used_for_probe(self) -> None:
        backend = self.make_backend()

        probe = backend.probe_command("gpu-a", "materialx-smoke")[-1]
        launch = backend.launch_command("gpu-a", "materialx-smoke", "MaterialX exact test task")

        self.assertIn("test -f", probe)
        self.assertNotIn("python3 /home/horde/ovrtx/hermes_runner.py", probe)
        self.assertIn("MaterialX exact test task", self.prompt_from_command(launch))

    def test_raw_three_token_credential_is_supported_without_emitting_value(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            credential_file = root / "credentials.token"
            secret = "part-one part-two part-three"
            credential_file.write_text(secret + "\n", encoding="utf-8")
            self.assertEqual(credential_values(credential_file), ["part-one", "part-two", "part-three"])
            plan = build_dispatch_plan(["gpu-a"], credential_file, {"batch_id": "materialx-smoke"})
            backend = HordeBackend(
                {
                    "gpu-a": {
                        "host": "horde-gpu-a",
                        "user": "horde",
                        "runner_path": "/home/horde/ovrtx/hermes_runner.py",
                    }
                }
            )
            runner = FakeRunner()

            result = execute_dispatch(
                plan,
                backend=backend,
                runner=runner,
                capacity_state_path=root / "capacity.json",
                journal_path=root / "journal.json",
            )

            self.assertTrue(result["ok"])
            rendered = json.dumps(result, sort_keys=True)
            self.assertNotIn(secret, rendered)


if __name__ == "__main__":
    unittest.main()
