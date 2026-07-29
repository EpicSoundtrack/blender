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

from materialx_horde_controller import run_controller_cycle
from materialx_horde_dispatch import (
    COMMAND_TIMEOUT_SECONDS,
    CommandResult,
    HordeBackend,
    Runner,
    _subprocess_runner,
    execute_dispatch,
    validate_batch_id,
)
from materialx_horde_dispatch_plan import build_dispatch_plan


DispatchOne = Callable[[str, Mapping[str, Any]], Mapping[str, Any]]


def _write_json(path: Path, document: Mapping[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="")


class HordeOperationalAdapter:
    """Provide exact categorical remote evidence to the pure controller."""

    def __init__(self, *, backend: HordeBackend, runner: Runner | None = None, dispatch_one: DispatchOne):
        self._backend = backend
        self._runner = runner or _subprocess_runner
        self._dispatch_one = dispatch_one

    @classmethod
    def with_dispatcher(
        cls,
        *,
        backend: HordeBackend,
        credential_file: str | Path,
        runner: Runner | None = None,
        capacity_state_path: str | Path,
        dispatch_journal_path: str | Path,
    ) -> "HordeOperationalAdapter":
        """Bind the existing one-shot dispatcher for one worker at a time."""
        active_runner = runner or _subprocess_runner

        def dispatch_one(worker_id: str, batch: Mapping[str, Any]) -> Mapping[str, str]:
            plan = build_dispatch_plan([worker_id], credential_file, batch)
            result = execute_dispatch(
                plan,
                backend=backend,
                runner=active_runner,
                capacity_state_path=None,
                journal_path=None,
            )
            return {"outcome": "success" if result.get("ok") is True else "failure"}

        return cls(backend=backend, runner=active_runner, dispatch_one=dispatch_one)

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

    def harvest_finished(self, worker_id: str, batch_id: str) -> Mapping[str, str]:
        """Map an exact remote exit sentinel to the pure controller contract."""
        result = self._run(self._backend.harvest_command(worker_id, batch_id))
        if result is None or result.returncode != 0 or result.stderr:
            return {}
        if result.stdout.strip() == "success":
            return {"outcome": "success", "evidence": "task_log"}
        if result.stdout.strip() == "failure":
            return {"outcome": "failure", "evidence": "task_log"}
        if result.stdout.strip() == "missing":
            return {"outcome": "missing", "evidence": "none"}
        return {}

    def dispatch_batch(self, worker_id: str, batch: Mapping[str, Any]) -> Mapping[str, str]:
        """Dispatch one exact batch and discard all non-categorical dispatcher output."""
        try:
            result = self._dispatch_one(worker_id, batch)
        except Exception:
            return {"outcome": "failure"}
        if isinstance(result, Mapping) and set(result) == {"outcome"} and result["outcome"] in {"success", "failure"}:
            return {"outcome": result["outcome"]}
        return {"outcome": "failure"}


def run_operational_controller_cycle(
    *,
    workers: Sequence[Mapping[str, Any]],
    queued_batches: Sequence[Mapping[str, Any]],
    adapter: HordeOperationalAdapter,
    state_path: str | Path | None = None,
    journal_path: str | Path | None = None,
) -> dict[str, Any]:
    """Run one bounded process-check, harvest, and one-worker refill cycle."""
    controller_workers: list[dict[str, str]] = []
    process_journal: list[dict[str, str]] = []
    process_alerts: list[dict[str, str]] = []
    seen_workers: set[str] = set()
    for worker in workers:
        if not isinstance(worker, Mapping) or not isinstance(worker.get("id"), str) or not worker["id"]:
            raise ValueError("worker records require non-empty ids")
        worker_id = worker["id"]
        if worker_id in seen_workers:
            raise ValueError("worker records require unique ids")
        seen_workers.add(worker_id)
        batch_id = worker.get("batch_id")
        if batch_id is not None:
            validate_batch_id(batch_id)
        state, evidence = adapter.process_evidence(worker_id)
        if state == "active":
            record = {"id": worker_id, "state": "active"}
            if batch_id:
                record["batch_id"] = batch_id
            controller_workers.append(record)
        elif state == "absent" and batch_id:
            controller_workers.append({"id": worker_id, "state": "idle", "batch_id": batch_id})
        else:
            record = {"id": worker_id, "state": "blocked"}
            if batch_id:
                record["batch_id"] = batch_id
            controller_workers.append(record)
            process_alerts.append({"worker_id": worker_id, "classification": "process_missing"})
        process_journal.append({"worker_id": worker_id, "event": f"process_{state}", "evidence": evidence})

    for batch in queued_batches:
        if not isinstance(batch, Mapping):
            raise ValueError("queued batches must be mappings")
        validate_batch_id(batch.get("batch_id"))

    result = run_controller_cycle(workers=controller_workers, queued_batches=queued_batches, backend=adapter)
    aggregate = {
        "workers": result["workers"],
        "assigned_batches": result["assigned_batches"],
        "alerts": sorted(process_alerts + result["alerts"], key=lambda alert: (alert["worker_id"], alert["classification"])),
        "journal": sorted(process_journal + result["journal"], key=lambda event: (event["worker_id"], event.get("batch_id", ""), event["event"])),
    }
    if state_path is not None:
        _write_json(Path(state_path), {"schema_version": 1, **{key: aggregate[key] for key in ("workers", "assigned_batches", "alerts")}})
    if journal_path is not None:
        _write_json(Path(journal_path), {"schema_version": 1, "journal": aggregate["journal"]})
    return aggregate
