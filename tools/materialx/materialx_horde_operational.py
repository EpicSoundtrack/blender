#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Bounded operational evidence adapter for the MaterialX Horde controller."""

from __future__ import annotations

__all__ = (
    "HordeOperationalAdapter",
    "OperationalSupervisorController",
    "main",
    "run_operational_controller_cycle",
    "run_operational_supervisor",
)

import argparse
from pathlib import Path
import time
from typing import Any, Callable, Mapping, Sequence

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


DispatchBatch = Callable[[Sequence[Mapping[str, Any]]], Mapping[str, Any]]
QueueSource = Callable[[], Sequence[Mapping[str, Any]]]


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
        capacity_state_path: str | Path,
        dispatch_journal_path: str | Path,
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
    )
    aggregate = {
        "workers": result["workers"],
        "assigned_batches": result["assigned_batches"],
        "artifacts": result["artifacts"],
        "integration_receipts": result["integration_receipts"],
        "alerts": sorted(process_alerts + result["alerts"], key=lambda alert: (alert["worker_id"], alert["classification"])),
        "journal": sorted(process_journal + result["journal"], key=lambda event: (event["worker_id"], event.get("batch_id", ""), event["event"])),
        "queue_depth": len(entries),
    }
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
    ):
        if isinstance(workers, (str, bytes)) or not isinstance(workers, Sequence):
            raise ValueError("workers must be a sequence")
        if not callable(queue_source):
            raise ValueError("queue_source must be callable")
        self._workers = [dict(worker) for worker in workers]
        self._queue_source = queue_source
        self._registered_families = registered_families
        self._adapter = adapter
        self._integration_backend = integration_backend

    def run_cycle(self) -> Mapping[str, Any]:
        queued_batches = self._queue_source()
        if (
            isinstance(queued_batches, (str, bytes))
            or not isinstance(queued_batches, Sequence)
        ):
            raise ValueError("queue_source must return a sequence")
        result = run_operational_controller_cycle(
            workers=self._workers,
            queued_batches=queued_batches,
            registered_families=self._registered_families,
            adapter=self._adapter,
            integration_backend=self._integration_backend,
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
    once: bool = False,
    max_cycles: int | None = None,
) -> int:
    """Wire the bounded operational controller into the canonical supervisor."""
    controller = OperationalSupervisorController(
        workers=workers,
        queue_source=queue_source,
        registered_families=registered_families,
        adapter=adapter,
        integration_backend=integration_backend,
    )
    return run_supervisor(
        config,
        controller=controller,
        state_store=state_store,
        clock=clock,
        sleeper=sleeper,
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


def _argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run the canonical MaterialX Horde supervisor"
    )
    parser.add_argument("--once", action="store_true")
    parser.add_argument("--poll-interval", type=float, default=30.0)
    parser.add_argument("--queue-watermark", type=int, default=5)
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
        parser.error("a configured operational runtime is required")
    runtime = runtime_factory(config)
    required = {
        "workers",
        "queue_source",
        "registered_families",
        "adapter",
        "integration_backend",
    }
    if not isinstance(runtime, Mapping) or set(runtime) != required:
        parser.error("operational runtime has invalid shape")
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
