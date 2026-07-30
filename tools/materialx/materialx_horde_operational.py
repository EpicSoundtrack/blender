#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Bounded operational evidence adapter for the MaterialX Horde controller."""

from __future__ import annotations

__all__ = ("HordeOperationalAdapter", "run_operational_controller_cycle")

import json
from pathlib import Path
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


DispatchBatch = Callable[
    [Sequence[Mapping[str, Any]]],
    Mapping[str, Any],
]


def _write_json(path: Path, document: Mapping[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="")


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
    state_path: str | Path | None = None,
    journal_path: str | Path | None = None,
) -> dict[str, Any]:
    """Run one bounded process-check, harvest, and one-worker refill cycle."""
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
    )
    aggregate = {
        "workers": result["workers"],
        "assigned_batches": result["assigned_batches"],
        "artifacts": result["artifacts"],
        "alerts": sorted(process_alerts + result["alerts"], key=lambda alert: (alert["worker_id"], alert["classification"])),
        "journal": sorted(process_journal + result["journal"], key=lambda event: (event["worker_id"], event.get("batch_id", ""), event["event"])),
    }
    if state_path is not None:
        _write_json(Path(state_path), {"schema_version": 1, **{key: aggregate[key] for key in ("workers", "assigned_batches", "artifacts", "alerts")}})
    if journal_path is not None:
        _write_json(Path(journal_path), {"schema_version": 1, "journal": aggregate["journal"]})
    return aggregate
