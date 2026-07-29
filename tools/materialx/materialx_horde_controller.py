#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Fail-closed harvest-and-refill decisions for MaterialX Horde workers."""

from __future__ import annotations

__all__ = ("ControllerBackend", "build_refill_cycle", "run_controller_cycle")

from typing import Any, Mapping, Protocol, Sequence


class ControllerBackend(Protocol):
    """Injected remote operations for one bounded harvest-and-refill cycle."""

    def harvest_finished(self, worker_id: str, batch_id: str) -> Mapping[str, str]:
        """Return only categorical, sanitized harvest evidence."""

    def dispatch_batch(self, worker_id: str, batch: Mapping[str, Any]) -> Mapping[str, str]:
        """Dispatch one batch and return only its categorical outcome."""


def build_refill_cycle(
    *, workers: Sequence[Mapping[str, Any]], queued_batches: Sequence[Mapping[str, Any]]
) -> dict[str, list[Any]]:
    """Harvest completed workers and assign queued work to healthy idle slots.

    An idle worker without a successful harvest is blocked instead of being
    silently reused. Active workers retain their existing assignment.
    """
    completed_workers: list[str] = []
    blocked_workers: list[str] = []
    alerts: list[dict[str, str]] = []
    eligible_workers: list[str] = []

    for worker in workers:
        worker_id = worker.get("id")
        state = worker.get("state")
        harvest = worker.get("harvest")
        if not isinstance(worker_id, str) or not worker_id or state not in {"active", "idle", "blocked"}:
            raise ValueError("worker records require id and state")
        if state != "idle":
            continue
        if harvest != "success":
            blocked_workers.append(worker_id)
            classification = "harvest_failure" if harvest == "failure" else "harvest_missing"
            alerts.append({"worker_id": worker_id, "classification": classification})
            continue
        completed_workers.append(worker_id)
        eligible_workers.append(worker_id)

    if len(queued_batches) < len(eligible_workers):
        for worker_id in eligible_workers[len(queued_batches):]:
            blocked_workers.append(worker_id)
            alerts.append({"worker_id": worker_id, "classification": "queue_empty"})
        eligible_workers = eligible_workers[:len(queued_batches)]

    refills = []
    seen_batches = set()
    for batch in queued_batches:
        batch_id = batch.get("batch_id")
        prompt = batch.get("prompt")
        if not isinstance(batch_id, str) or not batch_id or not isinstance(prompt, str) or not prompt:
            raise ValueError("queued batches require non-empty batch_id and prompt")
        if batch_id in seen_batches:
            raise ValueError("queued batches must not overlap")
        seen_batches.add(batch_id)
    for worker_id, batch in zip(eligible_workers, queued_batches):
        batch_id = batch.get("batch_id")
        refills.append({"worker_id": worker_id, "batch_id": batch_id, "prompt": batch["prompt"]})

    return {
        "completed_workers": completed_workers,
        "refills": refills,
        "blocked_workers": sorted(set(blocked_workers)),
        "alerts": alerts,
    }


def _worker_records(workers: Sequence[Mapping[str, Any]]) -> list[dict[str, str]]:
    if isinstance(workers, (str, bytes)):
        raise ValueError("workers must be a sequence of worker records")
    result = []
    worker_ids = set()
    for worker in workers:
        if not isinstance(worker, Mapping):
            raise ValueError("worker records must be mappings")
        worker_id = worker.get("id")
        state = worker.get("state")
        batch_id = worker.get("batch_id")
        if not isinstance(worker_id, str) or not worker_id or worker_id in worker_ids:
            raise ValueError("worker records require unique non-empty ids")
        if state not in {"active", "idle", "blocked"}:
            raise ValueError("worker records require active, idle, or blocked state")
        if state == "idle" and (not isinstance(batch_id, str) or not batch_id):
            raise ValueError("idle worker records require a completed batch_id to harvest")
        if batch_id is not None and (not isinstance(batch_id, str) or not batch_id):
            raise ValueError("worker batch_id must be a non-empty string when present")
        result.append({"id": worker_id, "state": state, "batch_id": batch_id or ""})
        worker_ids.add(worker_id)
    return result


def _harvest_result(result: Mapping[str, Any]) -> tuple[str, str]:
    """Validate the deliberately small, secret-free harvest contract."""
    if not isinstance(result, Mapping) or set(result) != {"outcome", "evidence"}:
        return "missing", "invalid"
    outcome = result["outcome"]
    evidence = result["evidence"]
    if outcome == "success" and evidence == "task_log":
        return "success", "task_log"
    if outcome == "failure" and evidence == "task_log":
        return "failure", "task_log"
    if outcome == "missing" and evidence == "none":
        return "missing", "none"
    return "missing", "invalid"


def _dispatch_result(result: Mapping[str, Any]) -> str:
    if isinstance(result, Mapping) and set(result) == {"outcome"} and result["outcome"] in {"success", "failure"}:
        return result["outcome"]
    return "failure"


def run_controller_cycle(
    *,
    workers: Sequence[Mapping[str, Any]],
    queued_batches: Sequence[Mapping[str, Any]],
    backend: ControllerBackend,
) -> dict[str, list[Mapping[str, str]]]:
    """Harvest idle workers and refill them without emitting logs, prompts, or secrets.

    ``workers`` is current process/activity evidence: active workers are not
    touched, while each idle worker must identify the batch whose finished log
    is to be harvested. The injected backend makes this single cycle bounded
    and testable without contacting a live worker.
    """
    current_workers = _worker_records(workers)
    attached_batch_ids = {worker["batch_id"] for worker in current_workers if worker["batch_id"]}
    queued_batch_ids = set()
    for batch in queued_batches:
        if not isinstance(batch, Mapping):
            raise ValueError("queued batches must be mappings")
        batch_id = batch.get("batch_id")
        prompt = batch.get("prompt")
        if not isinstance(batch_id, str) or not batch_id or not isinstance(prompt, str) or not prompt:
            raise ValueError("queued batches require non-empty batch_id and prompt")
        if batch_id in queued_batch_ids:
            raise ValueError("queued batches must not overlap")
        if batch_id in attached_batch_ids:
            raise ValueError("queued batch_id is already attached to a worker")
        queued_batch_ids.add(batch_id)
    harvested_workers = []
    journal: list[dict[str, str]] = []
    worker_states = {worker["id"]: worker["state"] for worker in current_workers}

    for worker in current_workers:
        if worker["state"] != "idle":
            continue
        try:
            outcome, evidence = _harvest_result(backend.harvest_finished(worker["id"], worker["batch_id"]))
        except Exception:
            outcome, evidence = "missing", "invalid"
        harvested_workers.append({"id": worker["id"], "state": "idle", "harvest": outcome})
        journal.append({
            "worker_id": worker["id"],
            "batch_id": worker["batch_id"],
            "event": f"harvest_{outcome}",
            "evidence": evidence,
        })

    decisions = build_refill_cycle(workers=harvested_workers, queued_batches=queued_batches)
    alerts = list(decisions["alerts"])
    for worker_id in decisions["blocked_workers"]:
        worker_states[worker_id] = "blocked"

    assigned_batches = []
    for refill in decisions["refills"]:
        worker_id = refill["worker_id"]
        batch_id = refill["batch_id"]
        try:
            outcome = _dispatch_result(backend.dispatch_batch(worker_id, refill))
        except Exception:
            outcome = "failure"
        journal.append({"worker_id": worker_id, "batch_id": batch_id, "event": f"dispatch_{outcome}", "evidence": "command_result"})
        if outcome == "success":
            worker_states[worker_id] = "active"
            assigned_batches.append({"worker_id": worker_id, "batch_id": batch_id})
        else:
            worker_states[worker_id] = "blocked"
            alerts.append({"worker_id": worker_id, "classification": "dispatch_failure"})

    alerts.sort(key=lambda alert: (alert["worker_id"], alert["classification"]))
    journal.sort(key=lambda event: (event["worker_id"], event["batch_id"], event["event"]))
    return {
        "workers": [{"id": worker_id, "state": worker_states[worker_id]} for worker_id in sorted(worker_states)],
        "assigned_batches": assigned_batches,
        "journal": journal,
        "alerts": alerts,
    }
