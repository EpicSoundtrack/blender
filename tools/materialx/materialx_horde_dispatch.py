#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Safely dispatch MaterialX work to documented Horde workers over SSH.

This command is a dry-run unless ``--execute`` is supplied. The execution path
uses only configured SSH targets: bounded no-write probes, one stdin-based
credential persistence operation, a remote ``hermes_runner.py`` launch, and a
remote ``pgrep`` process check.
"""

from __future__ import annotations

__all__ = ("CommandResult", "HordeBackend", "credential_values", "dry_run", "execute_dispatch", "main", "validate_batch_id")

import argparse
import base64
import json
import os
from dataclasses import dataclass
from pathlib import Path
import re
import shlex
import subprocess
import sys
from typing import Any, Callable, Mapping, Sequence

from materialx_horde_dispatch_plan import REQUIRED_CREDENTIAL_KEY, build_dispatch_plan, validate_credential_file
from materialx_worker_preflight import REQUIRED_ARCHITECTURE_FILES, WorkerProbe, WorkerSynchronizer, parse_probe_document, preflight_workers


PROBE_TIMEOUT_SECONDS = 15
COMMAND_TIMEOUT_SECONDS = 60
Runner = Callable[..., "CommandResult"]
REMOTE_ENV_PATH = "/home/horde/.hermes/.env"
DEFAULT_RUNNER_PATH = "/home/horde/matx_tasks/hermes_runner.py"
DEFAULT_SOURCE_ROOT = "/home/horde/matx_tasks"
KNOWN_HORDE_WORKERS = {
    "blend05": {"host": "canderson-blend05.ov-agent-farm.svc.cluster.local"},
    "blendit04": {"host": "canderson-blendit04.ov-agent-farm.svc.cluster.local"},
    "blendit": {"host": "canderson-blendit.ov-agent-farm.svc.cluster.local"},
    "blendit2": {"host": "canderson-canderson-blendit2-bot.ov-agent-farm.svc.cluster.local"},
    "blendit3": {"host": "canderson-canderson-blendit3-bot.ov-agent-farm.svc.cluster.local"},
}
BATCH_ID_PATTERN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")
AUTH_FAILURE_PATTERN = (
    r"(?:\bHTTP(?:Error)?\b|\bRequest failed with status(?:[_ ]code)?\b)[^0-9\r\n]*401\b|"
    r"401.*(?:invalid credentials|unauthorized|authentication)|"
    r"(?:authentication|error code|status[_ ]?code).*401|invalid credentials"
)
PROXY_FAILURE_PATTERN = (
    r"ProxyError|407[^a-z0-9]*Proxy|"
    r"Proxy (?:authentication|authorization|connection|request|tunnel).*(?:failed|required|refused|error)"
)


def _harvest_classifier_script() -> str:
    """Return the secret-free classifier executed by the remote harvest command."""
    return "\n".join((
        "import re",
        "import sys",
        "from pathlib import Path",
        "path = Path(sys.argv[1])",
        "if not path.is_file():",
        '    print("missing", end="")',
        "else:",
        '    text = path.read_text(encoding="utf-8", errors="replace")',
        f"    auth_failure = re.search({json.dumps(AUTH_FAILURE_PATTERN)}, text, re.IGNORECASE)",
        f"    proxy_failure = re.search({json.dumps(PROXY_FAILURE_PATTERN)}, text, re.IGNORECASE)",
        "    last = text.splitlines()[-1] if text.splitlines() else \"\"",
        "    if auth_failure:",
        '        category = "auth_failure"',
        "    elif proxy_failure:",
        '        category = "proxy_failure"',
        '    elif last == "MATERIALX_HORDE_EXIT:0":',
        '        category = "success"',
        '    elif re.fullmatch(r"MATERIALX_HORDE_EXIT:[1-9][0-9]*", last):',
        '        category = "failure"',
        "    else:",
        '        category = "invalid"',
        '    print(category, end="")',
    ))


def validate_batch_id(batch_id: str) -> str:
    """Return a batch ID safe for use in a fixed remote log directory."""
    if not isinstance(batch_id, str) or not BATCH_ID_PATTERN.fullmatch(batch_id):
        raise ValueError("batch_id must contain only letters, digits, dot, underscore, or hyphen")
    return batch_id


@dataclass(frozen=True)
class CommandResult:
    """A command result deliberately limited to data safe for a journal."""

    returncode: int
    stdout: str
    stderr: str


@dataclass(frozen=True)
class HordeWorker:
    """Validated SSH details for one documented Horde worker."""

    host: str
    user: str
    runner_path: str
    environment_path: str

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> "HordeWorker":
        if not isinstance(value, Mapping):
            raise ValueError("Each worker configuration must be an object")
        fields = {
            "host": value.get("host"),
            "user": value.get("user", "horde"),
            "runner_path": value.get("runner_path", DEFAULT_RUNNER_PATH),
            "environment_path": value.get("environment_path", REMOTE_ENV_PATH),
        }
        if not all(isinstance(item, str) and item and item.strip() == item for item in fields.values()):
            raise ValueError("Worker configuration requires non-empty host, user, runner_path, and environment_path strings")
        if any(any(character.isspace() for character in item) for item in fields.values()):
            raise ValueError("Worker host, user, and runner_path must not contain whitespace")
        if not fields["runner_path"].endswith("hermes_runner.py"):
            raise ValueError("Worker runner_path must name hermes_runner.py")
        if not fields["environment_path"].endswith("/.env"):
            raise ValueError("Worker environment_path must name a .env file")
        return cls(**fields)


class HordeBackend:
    """Construct safe SSH command vectors from a named Horde-worker mapping."""

    def __init__(self, workers: Mapping[str, Mapping[str, Any]]):
        if not isinstance(workers, Mapping) or not workers:
            raise ValueError("Horde backend requires a non-empty worker mapping")
        self._workers = {
            worker_id: HordeWorker.from_mapping(worker)
            for worker_id, worker in workers.items()
            if isinstance(worker_id, str) and worker_id and worker_id.strip() == worker_id
        }
        if len(self._workers) != len(workers):
            raise ValueError("Horde worker IDs must be non-empty strings without surrounding whitespace")

    @classmethod
    def from_json_file(cls, path: str | Path) -> "HordeBackend":
        try:
            document = json.loads(Path(path).read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as ex:
            raise ValueError("Horde worker configuration is unavailable or invalid JSON") from ex
        workers = document.get("workers") if isinstance(document, Mapping) else None
        return cls(workers)

    @classmethod
    def documented_defaults(cls) -> "HordeBackend":
        """Return the currently documented MaterialX Horde worker mapping."""
        return cls(KNOWN_HORDE_WORKERS)

    def _worker(self, worker_id: str) -> HordeWorker:
        try:
            return self._workers[worker_id]
        except KeyError as ex:
            raise ValueError(f"No documented Horde host is configured for worker {worker_id!r}") from ex

    @staticmethod
    def _ssh(worker: HordeWorker, remote_command: str) -> tuple[str, ...]:
        return (
            "ssh",
            "-o",
            "BatchMode=yes",
            "-o",
            "ConnectTimeout=12",
            "-o",
            "StrictHostKeyChecking=no",
            "-J",
            "horde@bastion.horde-gke.nvidia.com:2222",
            f"{worker.user}@{worker.host}",
            remote_command,
        )

    def probe_command(self, worker_id: str, batch_id: str) -> tuple[str, ...]:
        worker = self._worker(worker_id)
        command = f"test -f {shlex.quote(worker.runner_path)} && command -v hermes >/dev/null && printf ready"
        return self._ssh(worker, command)

    def source_preflight_command(self, worker_id: str) -> tuple[str, ...]:
        """Return a read-only probe that emits only fixed source-check JSON."""
        self._worker(worker_id)
        script = "\n".join((
            "import json, re, subprocess",
            "from pathlib import Path",
            f"root = Path({DEFAULT_SOURCE_ROOT!r})",
            f"required = {REQUIRED_ARCHITECTURE_FILES!r}",
            "zero = '0' * 40",
            "repository_present = False",
            "head = zero",
            "try:",
            "    top = Path(subprocess.check_output(('git', '-C', str(root), 'rev-parse', '--show-toplevel'), stderr=subprocess.DEVNULL, text=True).strip()).resolve()",
            "    if top == root.resolve():",
            "        value = subprocess.check_output(('git', '-C', str(root), 'rev-parse', 'HEAD'), stderr=subprocess.DEVNULL, text=True).strip().lower()",
            "        if re.fullmatch(r'[0-9a-f]{40}', value):",
            "            repository_present = True",
            "            head = value",
            "except (OSError, subprocess.SubprocessError):",
            "    pass",
            "files = {path: bool(repository_present and (root / path).is_file()) for path in required}",
            "print(json.dumps({'repository_present': repository_present, 'files': files, 'head': head}, separators=(',', ':'), sort_keys=True), end='')",
        ))
        return self._ssh(self._worker(worker_id), " ".join(("python3", "-c", shlex.quote(script))))

    def persist_command(self, worker_id: str) -> tuple[str, ...]:
        worker = self._worker(worker_id)
        script = (
            "from pathlib import Path; import re, sys; "
            f"path = Path({worker.environment_path!r}); value = sys.stdin.read().strip(); "
            "assert value; existing = [line.rstrip('\\r') for line in path.read_text().splitlines()] if path.exists() else []; "
            f"kept = [line for line in existing if re.match(r'(?:export )?[A-Za-z_][A-Za-z0-9_]*=', line) and not (line.startswith({REQUIRED_CREDENTIAL_KEY!r} + '=') or line.startswith('export ' + {REQUIRED_CREDENTIAL_KEY!r} + '='))]; "
            f"path.parent.mkdir(parents=True, exist_ok=True); path.write_text('\\n'.join(kept + ['export ' + {REQUIRED_CREDENTIAL_KEY!r} + '=' + value]) + '\\n')"
        )
        command = " ".join(("python3", "-c", shlex.quote(script)))
        return self._ssh(worker, command)

    def launch_command(self, worker_id: str, batch_id: str, prompt: str | None = None) -> tuple[str, ...]:
        worker = self._worker(worker_id)
        batch_id = validate_batch_id(batch_id)
        task_prompt = prompt or f"MaterialX batch {batch_id}: inspect the assigned work and report exact evidence."
        runner_command = self._runner_command(worker, task_prompt)
        shell_command = (
            f"set -a; . {shlex.quote(worker.environment_path)}; set +a; {runner_command}; "
            "status=$?; printf '\\nMATERIALX_HORDE_EXIT:%s\\n' \"$status\"; exit \"$status\""
        )
        log_path = f"/home/horde/matx_tasks/{batch_id}.log"
        command = f"nohup sh -lc {shlex.quote(shell_command)} >{shlex.quote(log_path)} 2>&1 < /dev/null & echo $!"
        return self._ssh(worker, command)

    def process_command(self, worker_id: str) -> tuple[str, ...]:
        command = (
            "pids=$(pgrep -f '[h]ermes_runner.py' 2>/dev/null || true); set -- $pids; "
            "if [ \"$#\" -eq 0 ]; then printf absent; "
            "elif [ \"$#\" -eq 1 ]; then printf 'active:%s' \"$1\"; else printf ambiguous; fi"
        )
        return self._ssh(self._worker(worker_id), command)

    def harvest_command(self, worker_id: str, batch_id: str) -> tuple[str, ...]:
        """Return only categorical evidence for the exact final task-log sentinel."""
        worker = self._worker(worker_id)
        batch_id = validate_batch_id(batch_id)
        log_path = shlex.quote(f"/home/horde/matx_tasks/{batch_id}.log")
        command = " ".join(("python3", "-c", shlex.quote(_harvest_classifier_script()), log_path))
        return self._ssh(worker, command)

    @staticmethod
    def _runner_command(worker: HordeWorker, prompt: str) -> str:
        encoded_prompt = base64.b64encode(prompt.encode("utf-8")).decode("ascii")
        return " ".join(("python3", shlex.quote(worker.runner_path), encoded_prompt))


def credential_values(credential_file: str | Path) -> list[str]:
    """Return validated single-line credential values without emitting them."""
    structure = validate_credential_file(credential_file)
    content = Path(credential_file).read_text(encoding="utf-8")
    if structure["format"] == "raw_three_token":
        return content.split()
    for line in content.splitlines():
        stripped = line.strip()
        if stripped and not stripped.startswith("#"):
            key, value = stripped.split("=", 1)
            if key == REQUIRED_CREDENTIAL_KEY:
                return [value]
    raise ValueError("Credential file must contain exactly one NVIDIA_API_KEY entry")


def _subprocess_runner(
    command: tuple[str, ...],
    *,
    env: Mapping[str, str] | None = None,
    input_text: str | None = None,
    timeout: int,
) -> CommandResult:
    """Run a bounded SSH command; remote secrets are supplied only through stdin."""
    command_environment = os.environ.copy()
    if env:
        command_environment.update(env)
    try:
        completed = subprocess.run(
            command,
            capture_output=True,
            check=False,
            env=command_environment,
            input=input_text,
            text=True,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired as ex:
        stdout = ex.stdout if isinstance(ex.stdout, str) else ""
        stderr = ex.stderr if isinstance(ex.stderr, str) else "command timed out"
        return CommandResult(returncode=124, stdout=stdout, stderr=stderr)
    except OSError as ex:
        return CommandResult(returncode=127, stdout="", stderr=str(ex))
    return CommandResult(completed.returncode, completed.stdout, completed.stderr)


def _sanitized_log(result: CommandResult, secret: str | Sequence[str] | None) -> str:
    combined = "\n".join(part for part in (result.stdout, result.stderr) if part).strip()
    secrets = (secret,) if isinstance(secret, str) else (secret or ())
    for value in secrets:
        combined = combined.replace(value, "[REDACTED]")
    combined = re.sub(r"(?i)(NVIDIA_API_KEY\s*[:=]\s*)\S+", r"\1[REDACTED]", combined)
    return combined[:1000]


def _run_step(
    runner: Runner,
    command: tuple[str, ...],
    *,
    secret: str | Sequence[str] | None = None,
    input_text: str | None = None,
    timeout: int,
) -> tuple[CommandResult, str]:
    result = runner(command, env={}, input_text=input_text, timeout=timeout)
    return result, _sanitized_log(result, secret)


def _active_process_evidence(result: CommandResult) -> bool:
    """Accept only the exact categorical post-launch active-PID evidence."""
    if result.returncode != 0 or result.stderr:
        return False
    prefix, separator, pid = result.stdout.strip().partition(":")
    return prefix == "active" and separator == ":" and pid.isdecimal() and int(pid) > 0


def _write_json(path: Path, document: Mapping[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="")


def _capacity_state(
    workers: Sequence[str], batch_id: str, outcome: str, detail: Mapping[str, Any], worker_states: Mapping[str, str] | None = None
) -> dict[str, Any]:
    worker_state = "active" if outcome == "success" else "failure"
    capacity_workers = [{"id": worker, "state": (worker_states or {}).get(worker, worker_state)} for worker in workers]
    return {
        "schema_version": 1,
        "healthy_workers": capacity_workers,
        "completed_rows": [],
        "evidence_records": [{"row_id": batch_id, "record": {"kind": "horde_dispatch", "outcome": outcome}}],
        "journal_records": [{"row_id": batch_id, "record": dict(detail)}],
        "lanes": {"windows_local_build": {"state": "unknown", "alerted": False}},
    }


def dry_run(plan: Mapping[str, Any]) -> dict[str, Any]:
    """Return the safe default result without opening credentials or using SSH."""
    return {"mode": "dry_run", "ok": True, "batch_id": plan["batch_manifest"]["batch_id"], "workers": list(plan["workers"]), "steps": [step["id"] for step in plan["required_steps"]]}


def _worker_prompt(batch_manifest: Mapping[str, Any], worker: str) -> str:
    prompts = batch_manifest.get("worker_prompts", {})
    if isinstance(prompts, Mapping):
        prompt = prompts.get(worker)
        if isinstance(prompt, str) and prompt.strip():
            return prompt
    goal = batch_manifest.get("goal", "implement the assigned MaterialX batch with focused evidence")
    return f"MaterialX batch {batch_manifest['batch_id']} for worker {worker}: {goal}"


def _expected_source_sha(batch_manifest: Mapping[str, Any]) -> str | None:
    """Return the v2 source contract SHA, leaving legacy plans untouched."""
    fields = ("integration_base_sha", "worker_source_sha", "roles")
    present = [field in batch_manifest for field in fields]
    if not any(present):
        return None
    if not all(present):
        raise ValueError("batch manifest has an incomplete source-preflight contract")
    integration_base = batch_manifest["integration_base_sha"]
    worker_source = batch_manifest["worker_source_sha"]
    roles = batch_manifest["roles"]
    if not isinstance(integration_base, str) or not re.fullmatch(r"[0-9a-f]{40}", integration_base):
        raise ValueError("integration_base_sha must be a lowercase 40-hex SHA")
    if worker_source != integration_base:
        raise ValueError("worker_source_sha must equal integration_base_sha")
    if not isinstance(roles, Mapping) or not isinstance(roles.get("implementation"), str) or not roles["implementation"]:
        raise ValueError("batch manifest roles must name an implementation worker")
    return integration_base


class _DispatchWorkerProbe:
    """Adapt bounded SSH source-probe JSON to the injected preflight protocol."""

    def __init__(self, backend: HordeBackend, runner: Runner):
        self._backend = backend
        self._runner = runner

    def probe(self, worker: str) -> Mapping[str, Any]:
        result = self._runner(self._backend.source_preflight_command(worker), env={}, input_text=None, timeout=PROBE_TIMEOUT_SECONDS)
        if result.returncode != 0 or result.stderr:
            raise ValueError("source probe failed")
        return parse_probe_document(result.stdout)


def execute_dispatch(
    plan: Mapping[str, Any],
    *,
    backend: HordeBackend,
    runner: Runner | None = None,
    capacity_state_path: str | Path | None,
    journal_path: str | Path | None,
    worker_probe: WorkerProbe | None = None,
    worker_synchronizer: WorkerSynchronizer | None = None,
) -> dict[str, Any]:
    """Execute one approved plan over configured SSH hosts and journal safe facts."""
    active_runner = runner or _subprocess_runner
    workers = list(plan["workers"])
    batch_id = plan["batch_manifest"]["batch_id"]
    events: list[dict[str, Any]] = []
    secrets: list[str] = []

    def persist_result(worker_states: Mapping[str, str], *, failure_classification: str | None = None) -> dict[str, Any]:
        active_workers = [worker for worker in workers if worker_states[worker] == "active"]
        outcome = "success" if len(active_workers) == len(workers) else "partial" if active_workers else "failure"
        result: dict[str, Any] = {
            "mode": "execute", "ok": outcome != "failure", "outcome": outcome, "batch_id": batch_id,
            "workers": workers, "worker_states": dict(worker_states), "events": events,
        }
        if failure_classification is not None:
            result["alert"] = {
                "kind": "capacity_alert", "timing": "immediate", "batch_id": batch_id,
                "workers": workers, "classification": failure_classification, "log": failure_classification,
            }
        if capacity_state_path is not None:
            _write_json(Path(capacity_state_path), _capacity_state(workers, batch_id, outcome, result, worker_states))
        if journal_path is not None:
            journal = {"schema_version": 1, "batch_id": batch_id, "outcome": outcome, "worker_states": dict(worker_states), "events": events}
            if "alert" in result:
                journal["alert"] = result["alert"]
            _write_json(Path(journal_path), journal)
        return result

    try:
        expected_source_sha = _expected_source_sha(plan["batch_manifest"])
    except (KeyError, ValueError, TypeError):
        return persist_result({worker: "source_preflight_failure" for worker in workers}, failure_classification="source_preflight_failure")

    worker_states = {worker: "ready" for worker in workers}
    if expected_source_sha is not None:
        implementation_worker = plan["batch_manifest"]["roles"]["implementation"]
        if implementation_worker not in workers:
            return persist_result({worker: "source_preflight_failure" for worker in workers}, failure_classification="source_preflight_failure")
        active_probe = worker_probe or _DispatchWorkerProbe(backend, active_runner)
        statuses = preflight_workers(workers, expected_source_sha, probe=active_probe, synchronizer=worker_synchronizer)
        worker_states = {worker: str(statuses[worker]["state"]) for worker in workers}
        events.extend({"step": "source_preflight", "worker": worker, "state": worker_states[worker]} for worker in workers)

    try:
        secrets = credential_values(plan["credential_file"])
    except (OSError, ValueError):
        for worker in workers:
            if worker_states[worker] == "ready":
                worker_states[worker] = "configuration_or_credential_failure"
        return persist_result(worker_states, failure_classification="configuration_or_credential_failure")

    for worker in workers:
        if worker_states[worker] != "ready":
            continue
        try:
            result, log = _run_step(active_runner, backend.probe_command(worker, batch_id), secret=secrets, timeout=PROBE_TIMEOUT_SECONDS)
        except Exception:
            worker_states[worker] = "probe_failure"
            events.append({"step": "no_write_probe", "worker": worker, "state": "probe_failure"})
            continue
        events.append({"step": "no_write_probe", "worker": worker, "returncode": result.returncode, "log": log})
        if result.returncode != 0:
            worker_states[worker] = "probe_failure"

    # Each remote host receives one normalized variable assignment, never on a command line.
    for worker_index, worker in enumerate(workers):
        if worker_states[worker] != "ready":
            continue
        worker_secret = secrets[worker_index % len(secrets)]
        try:
            result, log = _run_step(active_runner, backend.persist_command(worker), secret=secrets, input_text=worker_secret + "\n", timeout=COMMAND_TIMEOUT_SECONDS)
        except Exception:
            worker_states[worker] = "credential_persistence_failure"
            events.append({"step": "persist_nvidia_api_key", "worker": worker, "state": "credential_persistence_failure"})
            continue
        events.append({"step": "persist_nvidia_api_key", "worker": worker, "returncode": result.returncode, "log": log})
        if result.returncode != 0:
            worker_states[worker] = "credential_persistence_failure"

    for worker in workers:
        if worker_states[worker] != "ready":
            continue
        try:
            result, log = _run_step(
                active_runner,
                backend.launch_command(worker, batch_id, _worker_prompt(plan["batch_manifest"], worker)),
                secret=secrets,
                timeout=COMMAND_TIMEOUT_SECONDS,
            )
        except Exception:
            worker_states[worker] = "launch_failure"
            events.append({"step": "hermes_launch", "worker": worker, "state": "launch_failure"})
            continue
        events.append({"step": "hermes_launch", "worker": worker, "returncode": result.returncode, "log": log})
        if result.returncode != 0:
            worker_states[worker] = "launch_failure"
            continue
        try:
            result, log = _run_step(active_runner, backend.process_command(worker), secret=secrets, timeout=COMMAND_TIMEOUT_SECONDS)
        except Exception:
            worker_states[worker] = "process_missing"
            events.append({"step": "hermes_process_check", "worker": worker, "state": "process_missing"})
            continue
        events.append({"step": "hermes_process_check", "worker": worker, "returncode": result.returncode, "log": "active" if _active_process_evidence(result) else "invalid"})
        if not _active_process_evidence(result):
            worker_states[worker] = "process_missing"
        else:
            worker_states[worker] = "active"

    first_failure = next((worker_states[worker] for worker in workers if worker_states[worker] != "active"), None)
    return persist_result(worker_states, failure_classification=first_failure if not any(state == "active" for state in worker_states.values()) else None)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worker", action="append", required=True, help="Worker identifier; repeat per worker")
    parser.add_argument("--credential-file", type=Path, required=True)
    parser.add_argument("--batch-manifest", type=Path, required=True)
    parser.add_argument("--worker-config", type=Path, help="Documented Horde SSH worker mapping JSON")
    parser.add_argument("--execute", action="store_true", help="Run bounded SSH dispatch; default is non-mutating")
    parser.add_argument("--capacity-state", type=Path, default=Path(__file__).with_name("materialx_project_capacity_state.json"))
    parser.add_argument("--journal", type=Path, default=Path(__file__).with_name("materialx_horde_dispatch_journal.json"))
    args = parser.parse_args(argv)
    try:
        manifest = json.loads(args.batch_manifest.read_text(encoding="utf-8"))
        plan = build_dispatch_plan(args.worker, args.credential_file, manifest)
        backend = HordeBackend.from_json_file(args.worker_config) if args.worker_config else HordeBackend.documented_defaults()
        result = execute_dispatch(plan, backend=backend, capacity_state_path=args.capacity_state, journal_path=args.journal) if args.execute else dry_run(plan)
    except (OSError, ValueError, json.JSONDecodeError) as ex:
        print(f"materialx_horde_dispatch.py: error: {ex}", file=sys.stderr)
        return 1
    sys.stdout.write(json.dumps(result, indent=2, sort_keys=True) + "\n")
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
