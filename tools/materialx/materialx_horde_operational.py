#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Bounded operational evidence adapter for the MaterialX Horde controller."""

from __future__ import annotations

__all__ = (
    "HordeOperationalAdapter",
    "OperationalSupervisorController",
    "load_runtime_config",
    "main",
    "run_operational_controller_cycle",
    "run_operational_supervisor",
)

import argparse
import copy
import hashlib
import json
from pathlib import Path
import time
from typing import Any, Callable, Mapping, Sequence

from materialx_alert_sink import SanitizedAlertSink
from materialx_horde_controller import _queue_entries, run_controller_cycle
from materialx_horde_dispatch import (
    COMMAND_TIMEOUT_SECONDS,
    CommandResult,
    HordeBackend,
    Runner,
    _dispatch_id,
    _subprocess_runner,
    execute_dispatch,
    validate_batch_id,
)
from materialx_horde_dispatch_plan import build_dispatch_plan
from materialx_horde_dispatch_plan import validate_credential_file
from materialx_completion_harvest import parse_completion_evidence
from materialx_horde_supervisor import (
    AtomicJSONStateStore,
    Clock,
    Sleeper,
    StateStore,
    SupervisorConfig,
    run_supervisor,
)
from materialx_integration_train import IntegrationBackend
from materialx_integration_backend import GitIntegrationBackend
from materialx_velocity_manifest import (
    EXPECTED_HORDE_WORKERS,
    validate_batch_manifest,
)


DispatchBatch = Callable[[Sequence[Mapping[str, Any]]], Mapping[str, Any]]
QueueSource = Callable[[], Sequence[Mapping[str, Any]]]
CycleObserver = Callable[[Mapping[str, Any]], None]


def _coherent_horde_evidence_receipt(
    workers: Sequence[Mapping[str, Any]],
    process_journal: Sequence[Mapping[str, str]],
) -> str | None:
    """Bind one receipt to exact-five process evidence and active dispatch IDs."""
    if (
        len(workers) != len(EXPECTED_HORDE_WORKERS)
        or {worker.get("id") for worker in workers}
        != set(EXPECTED_HORDE_WORKERS)
        or any(
            worker.get("state") != "active"
            or not isinstance(worker.get("batch_id"), str)
            or not worker["batch_id"]
            for worker in workers
        )
    ):
        return None
    process_by_worker: dict[str, dict[str, str]] = {}
    for event in process_journal:
        worker_id = event.get("worker_id")
        classification = event.get("event")
        evidence = event.get("evidence")
        if (
            worker_id not in EXPECTED_HORDE_WORKERS
            or worker_id in process_by_worker
            or (classification, evidence)
            not in {("process_active", "pid"), ("process_absent", "none")}
        ):
            return None
        process_by_worker[worker_id] = {
            "classification": classification,
            "evidence": evidence,
        }
    if set(process_by_worker) != set(EXPECTED_HORDE_WORKERS):
        return None
    evidence_document = [
        {
            "worker_id": worker_id,
            **process_by_worker[worker_id],
            "dispatch_id": next(
                worker["batch_id"]
                for worker in workers
                if worker["id"] == worker_id
            ),
        }
        for worker_id in sorted(EXPECTED_HORDE_WORKERS)
    ]
    encoded = json.dumps(
        evidence_document,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    return f"horde-cycle-{hashlib.sha256(encoded).hexdigest()[:24]}"


class HordeOperationalAdapter:
    """Provide exact categorical remote evidence to the pure controller."""

    def __init__(
        self,
        *,
        backend: HordeBackend,
        runner: Runner | None = None,
        dispatch_batch: DispatchBatch,
    ):
        self._backend = backend
        self._runner = runner or _subprocess_runner
        self._dispatch_batch = dispatch_batch

    @classmethod
    def with_dispatcher(
        cls,
        *,
        backend: HordeBackend,
        credential_file: str | Path,
        registered_families: Mapping[str, Any],
        runner: Runner | None = None,
    ) -> "HordeOperationalAdapter":
        """Bind the schema-v2 dispatcher for one validated manifest at a time."""
        active_runner = runner or _subprocess_runner

        def dispatch_batch(
            manifests: Sequence[Mapping[str, Any]],
        ) -> Mapping[str, Any]:
            plan = build_dispatch_plan(
                manifests,
                credential_file,
                registered_families=registered_families,
            )
            result = execute_dispatch(
                plan,
                backend=backend,
                runner=active_runner,
                capacity_state_path=None,
                journal_path=None,
            )
            plan_workers = plan["workers"]
            raw_states = result.get("worker_states")
            if not isinstance(raw_states, Mapping) or set(raw_states) != set(plan_workers):
                states = {worker: "failure" for worker in plan_workers}
            else:
                states = {
                    worker: "active"
                    if raw_states[worker] == "active"
                    else "failure"
                    for worker in plan_workers
                }
            active_count = sum(state == "active" for state in states.values())
            outcome = (
                "success"
                if active_count == len(states)
                else "partial"
                if active_count
                else "failure"
            )
            return {
                "outcome": outcome,
                "worker_states": states,
                "dispatch_id": result.get("dispatch_id"),
            }

        return cls(
            backend=backend,
            runner=active_runner,
            dispatch_batch=dispatch_batch,
        )

    def _run(self, command: tuple[str, ...]) -> CommandResult | None:
        try:
            return self._runner(command, env={}, input_text=None, timeout=COMMAND_TIMEOUT_SECONDS)
        except Exception:
            return None

    def process_evidence(self, worker_id: str) -> tuple[str, str]:
        """Return ``active/pid`` or fail-closed categorical process evidence."""
        result = self._run(self._backend.process_command(worker_id))
        if result is None or result.returncode != 0 or result.stderr:
            return "missing", "invalid"
        output = result.stdout.strip()
        if output == "absent":
            return "absent", "none"
        prefix, separator, pid = output.partition(":")
        if prefix == "active" and separator and pid.isdecimal() and int(pid) > 0:
            return "active", "pid"
        return "missing", "invalid"

    def harvest_finished(self, worker_id: str, batch_id: str) -> Mapping[str, Any]:
        """Parse only the remote bounded completion protocol."""
        result = self._run(self._backend.harvest_command(worker_id, batch_id))
        if result is None or result.returncode != 0 or result.stderr:
            return {"classification": "invalid_completion"}
        return parse_completion_evidence(result.stdout)

    def dispatch_batch(
        self, manifests: Sequence[Mapping[str, Any]]
    ) -> Mapping[str, Any]:
        """Dispatch one normalized manifest set and return categorical states."""
        try:
            return self._dispatch_batch(manifests)
        except Exception:
            workers = {
                worker
                for manifest in manifests
                for worker in manifest["roles"].values()
            }
            return {
                "outcome": "failure",
                "worker_states": {worker: "failure" for worker in workers},
                "dispatch_id": _dispatch_id(
                    [manifest["batch_id"] for manifest in manifests]
                ),
            }


def run_operational_controller_cycle(
    *,
    workers: Sequence[Mapping[str, Any]],
    queued_batches: Sequence[Mapping[str, Any]],
    registered_families: Mapping[str, Any],
    adapter: HordeOperationalAdapter,
    integration_backend: IntegrationBackend | None = None,
    project_state: Mapping[str, Any] | None = None,
    cadence_config: Mapping[str, Any] | None = None,
    cadence_runner: Any = None,
) -> dict[str, Any]:
    """Run one bounded process-check, harvest, integration, and refill cycle."""
    entries = _queue_entries(
        queued_batches,
        registered_families=registered_families,
    )
    if isinstance(workers, (str, bytes)) or not isinstance(workers, Sequence):
        raise ValueError("workers must be a sequence")
    validated_workers: list[dict[str, str]] = []
    raw_worker_ids: set[str] = set()
    attached_batch_ids: set[str] = set()
    for worker in workers:
        if not isinstance(worker, Mapping):
            raise ValueError("worker records require non-empty ids")
        if set(worker).difference({"id", "state", "batch_id", "assignment"}):
            raise ValueError(
                "worker records require exactly id, state, optional batch_id, and optional assignment"
            )
        worker_id = worker.get("id")
        worker_state = worker.get("state")
        batch_id = worker.get("batch_id")
        assignment = worker.get("assignment")
        if not isinstance(worker_id, str) or not worker_id or worker_id in raw_worker_ids:
            raise ValueError("worker records require unique non-empty ids")
        if worker_state not in {"active", "idle", "blocked"}:
            raise ValueError("worker records require active, idle, or blocked state")
        if batch_id is not None:
            validate_batch_id(batch_id)
            attached_batch_ids.add(batch_id)
        raw_worker_ids.add(worker_id)
        record = {"id": worker_id, "state": worker_state}
        if batch_id:
            record["batch_id"] = batch_id
        if "assignment" in worker:
            record["assignment"] = assignment
        validated_workers.append(record)
    if any(entry["worker_id"] not in raw_worker_ids for entry in entries):
        raise ValueError("queued worker must identify an operational worker")
    role_workers = {
        worker
        for entry in entries
        for worker in entry["manifest"]["roles"].values()
    }
    if any(entry["manifest"]["batch_id"] in attached_batch_ids for entry in entries):
        raise ValueError("queued batch_id is already attached to a worker")
    if not role_workers.issubset(raw_worker_ids):
        raise ValueError("manifest roles must identify operational workers")

    controller_workers: list[dict[str, str]] = []
    process_journal: list[dict[str, str]] = []
    process_alerts: list[dict[str, str]] = []
    for worker in validated_workers:
        worker_id = worker["id"]
        persisted_state = worker["state"]
        batch_id = worker.get("batch_id")
        assignment = worker.get("assignment")
        state, evidence = adapter.process_evidence(worker_id)
        if state == "active":
            record = {"id": worker_id, "state": "active"}
            if batch_id:
                record["batch_id"] = batch_id
            if "assignment" in worker:
                record["assignment"] = assignment
            controller_workers.append(record)
        elif state == "absent" and batch_id:
            record = {"id": worker_id, "state": "idle", "batch_id": batch_id}
            if "assignment" in worker:
                record["assignment"] = assignment
            controller_workers.append(record)
        elif state == "absent" and persisted_state == "idle":
            controller_workers.append({"id": worker_id, "state": "idle"})
        else:
            record = {"id": worker_id, "state": "blocked"}
            if batch_id:
                record["batch_id"] = batch_id
            controller_workers.append(record)
            process_alerts.append({"worker_id": worker_id, "classification": "process_missing"})
        process_journal.append({"worker_id": worker_id, "event": f"process_{state}", "evidence": evidence})

    result = run_controller_cycle(
        workers=controller_workers,
        queued_batches=entries,
        registered_families=registered_families,
        backend=adapter,
        integration_backend=integration_backend,
        project_state=project_state,
        cadence_config=cadence_config,
        cadence_runner=cadence_runner,
    )
    aggregate = {
        "workers": result["workers"],
        "assigned_batches": result["assigned_batches"],
        "artifacts": result["artifacts"],
        "integration_receipts": result["integration_receipts"],
        "alerts": sorted(process_alerts + result["alerts"], key=lambda alert: (alert["worker_id"], alert["classification"])),
        "journal": sorted(process_journal + result["journal"], key=lambda event: (event["worker_id"], event.get("batch_id", ""), event["event"])),
        "queue_depth": len(entries),
        "cadence_decision": result["cadence_decision"],
        "cadence_execution_receipts": result[
            "cadence_execution_receipts"
        ],
    }
    horde_evidence_receipt = _coherent_horde_evidence_receipt(
        aggregate["workers"],
        process_journal,
    )
    if horde_evidence_receipt is not None:
        aggregate["horde_evidence_receipt"] = horde_evidence_receipt
    return aggregate


class OperationalSupervisorController:
    """Keep worker state while delegating each cycle to the bounded adapter."""

    def __init__(
        self,
        *,
        workers: Sequence[Mapping[str, Any]],
        queue_source: QueueSource,
        registered_families: Mapping[str, Any],
        adapter: HordeOperationalAdapter,
        integration_backend: IntegrationBackend,
        project_state: Mapping[str, Any] | None = None,
        cadence_config: Mapping[str, Any] | None = None,
        cadence_runner: Any = None,
        cycle_observer: CycleObserver | None = None,
    ):
        validated_workers = _validate_operational_workers(
            workers,
            registered_families=registered_families,
        )
        if not callable(queue_source):
            raise ValueError("queue_source must be callable")
        self._workers = validated_workers
        self._queue_source = queue_source
        self._registered_families = registered_families
        self._adapter = adapter
        self._integration_backend = integration_backend
        if cycle_observer is not None and not callable(cycle_observer):
            raise ValueError("cycle_observer must be callable")
        self._project_state = (
            copy.deepcopy(project_state) if project_state is not None else None
        )
        self._cadence_config = (
            copy.deepcopy(cadence_config)
            if cadence_config is not None
            else None
        )
        self._cadence_runner = cadence_runner
        self._cycle_observer = cycle_observer
        self._retired_batch_ids: set[str] = set()

    def run_cycle(self) -> Mapping[str, Any]:
        queued_batches = self._queue_source()
        if (
            isinstance(queued_batches, (str, bytes))
            or not isinstance(queued_batches, Sequence)
        ):
            raise ValueError("queue_source must return a sequence")
        entries = _queue_entries(
            queued_batches,
            registered_families=self._registered_families,
        )
        entries = [
            entry
            for entry in entries
            if entry["manifest"]["batch_id"] not in self._retired_batch_ids
        ]
        result = run_operational_controller_cycle(
            workers=self._workers,
            queued_batches=entries,
            registered_families=self._registered_families,
            adapter=self._adapter,
            integration_backend=self._integration_backend,
            project_state=self._project_state,
            cadence_config=self._cadence_config,
            cadence_runner=self._cadence_runner,
        )
        if self._cycle_observer is not None:
            self._cycle_observer(copy.deepcopy(result))
        self._retired_batch_ids.update(
            assignment["batch_id"]
            for assignment in result["assigned_batches"]
        )
        self._workers = [dict(worker) for worker in result["workers"]]
        return result


def run_operational_supervisor(
    config: SupervisorConfig,
    *,
    workers: Sequence[Mapping[str, Any]],
    queue_source: QueueSource,
    registered_families: Mapping[str, Any],
    adapter: HordeOperationalAdapter,
    integration_backend: IntegrationBackend,
    state_store: StateStore,
    clock: Clock,
    sleeper: Sleeper,
    alert_sink: SanitizedAlertSink | None = None,
    project_state: Mapping[str, Any] | None = None,
    cadence_config: Mapping[str, Any] | None = None,
    cadence_runner: Any = None,
    cycle_observer: CycleObserver | None = None,
    once: bool = False,
    max_cycles: int | None = None,
) -> int:
    """Wire the bounded operational controller into the canonical supervisor."""
    if alert_sink is not None and not isinstance(alert_sink, SanitizedAlertSink):
        raise ValueError("alert_sink must be a SanitizedAlertSink")
    controller = OperationalSupervisorController(
        workers=workers,
        queue_source=queue_source,
        registered_families=registered_families,
        adapter=adapter,
        integration_backend=integration_backend,
        project_state=project_state,
        cadence_config=cadence_config,
        cadence_runner=cadence_runner,
        cycle_observer=cycle_observer,
    )
    return run_supervisor(
        config,
        controller=controller,
        state_store=state_store,
        clock=clock,
        sleeper=sleeper,
        alert_sink=alert_sink,
        once=once,
        max_cycles=max_cycles,
    )


class _SystemClock:
    def now(self) -> float:
        return time.time()


class _SystemSleeper:
    def sleep(self, seconds: float) -> None:
        time.sleep(seconds)


RuntimeFactory = Callable[[SupervisorConfig], Mapping[str, Any]]

_RUNTIME_CONFIG_FIELDS = {
    "schema_version",
    "workers",
    "queued_batches",
    "registered_families",
    "horde_workers",
    "repository_root",
    "integration_worktree_root",
}


def _validate_operational_workers(
    workers: Sequence[Mapping[str, Any]],
    *,
    registered_families: Mapping[str, Any],
) -> list[dict[str, Any]]:
    if isinstance(workers, (str, bytes)) or not isinstance(workers, Sequence):
        raise ValueError("operational runtime requires the exact five Horde workers")
    normalized = []
    worker_ids = []
    for worker in workers:
        if not isinstance(worker, Mapping):
            raise ValueError("operational runtime requires the exact five Horde workers")
        if set(worker).difference({"id", "state", "batch_id", "assignment"}):
            raise ValueError("worker record contains unsupported fields")
        worker_id = worker.get("id")
        state = worker.get("state")
        if not isinstance(worker_id, str) or state not in {"active", "idle", "blocked"}:
            raise ValueError("worker record is invalid")
        record = {"id": worker_id, "state": state}
        batch_id = worker.get("batch_id")
        if batch_id is not None:
            validate_batch_id(batch_id)
            record["batch_id"] = batch_id
        if "assignment" in worker:
            assignment = validate_batch_manifest(
                worker["assignment"],
                registered_families=registered_families,
            )
            if assignment != worker["assignment"]:
                raise ValueError("worker assignment must be canonical")
            record["assignment"] = assignment
        worker_ids.append(worker_id)
        normalized.append(record)
    if (
        len(worker_ids) != len(EXPECTED_HORDE_WORKERS)
        or len(set(worker_ids)) != len(worker_ids)
        or set(worker_ids) != set(EXPECTED_HORDE_WORKERS)
    ):
        raise ValueError("operational runtime requires the exact five Horde workers")
    return sorted(normalized, key=lambda worker: worker["id"])


def load_runtime_config(
    config_path: str | Path,
    credential_file: str | Path,
) -> Mapping[str, Any]:
    """Build a trusted runtime exclusively from canonical schema-v2 JSON."""
    try:
        document = json.loads(Path(config_path).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError("runtime config is unavailable or invalid JSON") from error
    if not isinstance(document, Mapping) or set(document) != _RUNTIME_CONFIG_FIELDS:
        raise ValueError("runtime config has invalid fields")
    if document["schema_version"] != 2 or isinstance(document["schema_version"], bool):
        raise ValueError("runtime config requires schema_version 2")
    registered_families = document["registered_families"]
    if not isinstance(registered_families, Mapping):
        raise ValueError("registered_families must be a mapping")
    workers = _validate_operational_workers(
        document["workers"],
        registered_families=registered_families,
    )
    queued_batches = _queue_entries(
        document["queued_batches"],
        registered_families=registered_families,
    )
    horde_workers = document["horde_workers"]
    if (
        not isinstance(horde_workers, Mapping)
        or set(horde_workers) != set(EXPECTED_HORDE_WORKERS)
    ):
        raise ValueError("horde_workers must configure the exact five workers")
    credential_structure = validate_credential_file(credential_file)
    credential_path = credential_structure["path"]
    repository_root = document["repository_root"]
    worktree_root = document["integration_worktree_root"]
    if not isinstance(repository_root, str) or not isinstance(worktree_root, str):
        raise ValueError("runtime roots must be strings")
    backend = HordeBackend(horde_workers)
    adapter = HordeOperationalAdapter.with_dispatcher(
        backend=backend,
        credential_file=credential_path,
        registered_families=registered_families,
    )
    integration_backend = GitIntegrationBackend(repository_root, worktree_root)
    canonical_queue = copy.deepcopy(queued_batches)
    return {
        "workers": workers,
        "queue_source": lambda: copy.deepcopy(canonical_queue),
        "registered_families": copy.deepcopy(registered_families),
        "adapter": adapter,
        "integration_backend": integration_backend,
    }


def _argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run the canonical MaterialX Horde supervisor"
    )
    parser.add_argument("--once", action="store_true")
    parser.add_argument("--poll-interval", type=float, default=30.0)
    parser.add_argument("--queue-watermark", type=int, default=5)
    parser.add_argument("--config", type=Path)
    parser.add_argument("--credentials", type=Path)
    parser.add_argument("--state", type=Path, required=True)
    return parser


def main(
    argv: Sequence[str] | None = None,
    *,
    runtime_factory: RuntimeFactory | None = None,
) -> int:
    """Parse strict supervisor options; runtime construction stays injected."""
    parser = _argument_parser()
    arguments = parser.parse_args(argv)
    try:
        config = SupervisorConfig(
            arguments.poll_interval,
            queue_watermark=arguments.queue_watermark,
        )
    except ValueError as error:
        parser.error(str(error))
    if runtime_factory is None:
        if arguments.config is None or arguments.credentials is None:
            parser.error("--config and --credentials are required")
        try:
            runtime = load_runtime_config(
                arguments.config,
                arguments.credentials,
            )
        except ValueError as error:
            parser.error(str(error))
    else:
        runtime = runtime_factory(config)
    required = {
        "workers",
        "queue_source",
        "registered_families",
        "adapter",
        "integration_backend",
    }
    if (
        not isinstance(runtime, Mapping)
        or set(runtime) not in (required, required | {"alert_sink"})
    ):
        parser.error("operational runtime has invalid shape")
    alert_sink = runtime.get("alert_sink")
    if alert_sink is not None and not isinstance(alert_sink, SanitizedAlertSink):
        raise ValueError("alert_sink must be a SanitizedAlertSink")
    return run_operational_supervisor(
        config,
        **runtime,
        state_store=AtomicJSONStateStore(arguments.state),
        clock=_SystemClock(),
        sleeper=_SystemSleeper(),
        once=arguments.once,
    )


if __name__ == "__main__":
    raise SystemExit(main())
