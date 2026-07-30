#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Fail-closed harvest-and-refill decisions for MaterialX Horde workers."""

from __future__ import annotations

__all__ = ("ControllerBackend", "build_refill_cycle", "run_controller_cycle")

from typing import Any, Mapping, Protocol, Sequence

from materialx_horde_dispatch import _dispatch_id
from materialx_velocity_manifest import validate_batch_manifest


class ControllerBackend(Protocol):
    """Injected remote operations for one bounded harvest-and-refill cycle."""

    def harvest_finished(self, worker_id: str, batch_id: str) -> Mapping[str, str]:
        """Return only categorical, sanitized harvest evidence."""

    def dispatch_batch(
        self, manifests: Sequence[Mapping[str, Any]]
    ) -> Mapping[str, Any]:
        """Dispatch one normalized manifest set exactly once."""


def _queue_entries(
    queued_batches: Sequence[Mapping[str, Any]],
    *,
    registered_families: Mapping[str, Any],
) -> list[dict[str, Any]]:
    if isinstance(queued_batches, (str, bytes)) or not isinstance(queued_batches, Sequence):
        raise ValueError("queued batches must be a sequence")
    entries = []
    batch_ids: set[str] = set()
    node_defs: set[str] = set()
    files: set[str] = set()
    workers: set[str] = set()
    for entry in queued_batches:
        if not isinstance(entry, Mapping) or set(entry) != {"worker_id", "manifest"}:
            raise ValueError("controller queue entries require exactly worker_id and manifest")
        worker_id = entry["worker_id"]
        if not isinstance(worker_id, str) or not worker_id:
            raise ValueError("controller queue worker_id must be a non-empty string")
        manifest = validate_batch_manifest(
            entry["manifest"],
            registered_families=registered_families,
        )
        if worker_id != manifest["roles"]["implementation"]:
            raise ValueError("queue worker_id must equal manifest roles implementation")
        batch_id = manifest["batch_id"]
        if batch_id in batch_ids:
            raise ValueError(f"duplicate batch ownership: {batch_id}")
        overlap = node_defs.intersection(manifest["node_defs"])
        if overlap:
            raise ValueError(f"duplicate NodeDef ownership: {sorted(overlap)}")
        overlap = files.intersection(manifest["files_allowlist"])
        if overlap:
            raise ValueError(f"duplicate file ownership: {sorted(overlap)}")
        if worker_id in workers:
            raise ValueError(f"duplicate worker ownership: {worker_id}")
        batch_ids.add(batch_id)
        node_defs.update(manifest["node_defs"])
        files.update(manifest["files_allowlist"])
        workers.add(worker_id)
        entries.append({"worker_id": worker_id, "manifest": manifest})
    return entries


def build_refill_cycle(
    *,
    workers: Sequence[Mapping[str, Any]],
    queued_batches: Sequence[Mapping[str, Any]],
    registered_families: Mapping[str, Any],
) -> dict[str, list[Any]]:
    """Assign explicit validated queue entries to successfully harvested workers."""
    entries = _queue_entries(queued_batches, registered_families=registered_families)
    completed_workers: list[str] = []
    blocked_workers: list[str] = []
    alerts: list[dict[str, str]] = []
    eligible_workers: set[str] = set()
    known_workers: set[str] = set()

    if isinstance(workers, (str, bytes)) or not isinstance(workers, Sequence):
        raise ValueError("workers must be a sequence of worker records")
    for worker in workers:
        if not isinstance(worker, Mapping):
            raise ValueError("worker records must be mappings")
        worker_id = worker.get("id")
        state = worker.get("state")
        harvest = worker.get("harvest")
        if (
            not isinstance(worker_id, str)
            or not worker_id
            or worker_id in known_workers
            or state not in {"active", "idle", "blocked"}
        ):
            raise ValueError("worker records require unique id and valid state")
        known_workers.add(worker_id)
        if state != "idle":
            continue
        if harvest != "success":
            blocked_workers.append(worker_id)
            classification = "harvest_failure" if harvest == "failure" else "harvest_missing"
            alerts.append({"worker_id": worker_id, "classification": classification})
            continue
        completed_workers.append(worker_id)
        eligible_workers.add(worker_id)

    refills = []
    selected_role_workers: set[str] = set()
    for entry in entries:
        manifest = entry["manifest"]
        role_workers = set(manifest["roles"].values())
        if not role_workers.issubset(known_workers):
            raise ValueError("manifest roles must identify controller workers")
        if not role_workers.issubset(eligible_workers):
            alerts.append({
                "worker_id": entry["worker_id"],
                "batch_id": manifest["batch_id"],
                "classification": "role_worker_unavailable",
            })
            continue
        refills.append(entry)
        selected_role_workers.update(role_workers)

    for worker_id in sorted(eligible_workers.difference(selected_role_workers)):
        blocked_workers.append(worker_id)
        alerts.append({"worker_id": worker_id, "classification": "queue_empty"})

    return {
        "completed_workers": completed_workers,
        "refills": refills,
        "blocked_workers": sorted(set(blocked_workers)),
        "alerts": sorted(alerts, key=lambda alert: (alert["worker_id"], alert["classification"])),
    }


def _worker_records(workers: Sequence[Mapping[str, Any]]) -> list[dict[str, str]]:
    if isinstance(workers, (str, bytes)) or not isinstance(workers, Sequence):
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


def _dispatch_result(
    result: Mapping[str, Any],
    expected_workers: set[str],
    expected_dispatch_id: str,
) -> tuple[dict[str, str], str]:
    if (
        not isinstance(result, Mapping)
        or set(result) != {"outcome", "worker_states", "dispatch_id"}
        or result["dispatch_id"] != expected_dispatch_id
    ):
        return {worker: "failure" for worker in expected_workers}, "invalid"
    states = result["worker_states"]
    if (
        not isinstance(states, Mapping)
        or set(states) != expected_workers
        or any(state not in {"active", "failure"} for state in states.values())
    ):
        return {worker: "failure" for worker in expected_workers}, "invalid"
    active_count = sum(state == "active" for state in states.values())
    expected_outcome = (
        "success"
        if active_count == len(expected_workers)
        else "partial"
        if active_count
        else "failure"
    )
    if result["outcome"] != expected_outcome:
        return {worker: "failure" for worker in expected_workers}, "invalid"
    return dict(states), "command_result"


def run_controller_cycle(
    *,
    workers: Sequence[Mapping[str, Any]],
    queued_batches: Sequence[Mapping[str, Any]],
    registered_families: Mapping[str, Any],
    backend: ControllerBackend,
) -> dict[str, list[Mapping[str, str]]]:
    """Validate all queue authority, then harvest and refill one bounded cycle."""
    current_workers = _worker_records(workers)
    entries = _queue_entries(queued_batches, registered_families=registered_families)
    attached_batch_ids = {worker["batch_id"] for worker in current_workers if worker["batch_id"]}
    for entry in entries:
        if entry["manifest"]["batch_id"] in attached_batch_ids:
            raise ValueError("queued batch_id is already attached to a worker")

    decision_workers = []
    journal: list[dict[str, str]] = []
    worker_states = {worker["id"]: worker["state"] for worker in current_workers}
    worker_batch_ids = {
        worker["id"]: worker["batch_id"] for worker in current_workers
    }
    for worker in current_workers:
        if worker["state"] != "idle":
            decision_workers.append({
                "id": worker["id"],
                "state": worker["state"],
                "harvest": "pending",
            })
            continue
        try:
            outcome, evidence = _harvest_result(
                backend.harvest_finished(worker["id"], worker["batch_id"])
            )
        except Exception:
            outcome, evidence = "missing", "invalid"
        decision_workers.append({"id": worker["id"], "state": "idle", "harvest": outcome})
        worker_batch_ids[worker["id"]] = ""
        journal.append({
            "worker_id": worker["id"],
            "batch_id": worker["batch_id"],
            "event": f"harvest_{outcome}",
            "evidence": evidence,
        })

    decisions = build_refill_cycle(
        workers=decision_workers,
        queued_batches=entries,
        registered_families=registered_families,
    )
    alerts = list(decisions["alerts"])
    queue_empty_workers = {
        alert["worker_id"] for alert in alerts if alert["classification"] == "queue_empty"
    }
    for worker_id in decisions["blocked_workers"]:
        if worker_id not in queue_empty_workers:
            worker_states[worker_id] = "blocked"

    assigned_batches = []
    selected_manifests = [refill["manifest"] for refill in decisions["refills"]]
    selected_workers = {
        role_worker
        for manifest in selected_manifests
        for role_worker in manifest["roles"].values()
    }
    if selected_manifests:
        dispatch_id = _dispatch_id(
            [manifest["batch_id"] for manifest in selected_manifests]
        )
        try:
            dispatched_worker_states, evidence = _dispatch_result(
                backend.dispatch_batch(selected_manifests),
                selected_workers,
                dispatch_id,
            )
        except Exception:
            dispatched_worker_states = {
                worker: "failure" for worker in selected_workers
            }
            evidence = "missing"
        for worker_id, state in dispatched_worker_states.items():
            worker_states[worker_id] = "active" if state == "active" else "blocked"
            worker_batch_ids[worker_id] = dispatch_id if state == "active" else ""
            if state != "active":
                alerts.append({
                    "worker_id": worker_id,
                    "classification": "dispatch_failure",
                })

    for refill in decisions["refills"]:
        worker_id = refill["worker_id"]
        manifest = refill["manifest"]
        batch_id = manifest["batch_id"]
        succeeded = all(
            worker_states[role_worker] == "active"
            for role_worker in manifest["roles"].values()
        )
        journal.append({
            "worker_id": worker_id,
            "batch_id": batch_id,
            "event": "dispatch_success" if succeeded else "dispatch_failure",
            "evidence": evidence,
        })
        if succeeded:
            assigned_batches.append({"worker_id": worker_id, "batch_id": batch_id})

    alerts.sort(key=lambda alert: (alert["worker_id"], alert["classification"]))
    journal.sort(key=lambda event: (event["worker_id"], event["batch_id"], event["event"]))
    return {
        "workers": [
            (
                {
                    "id": worker_id,
                    "state": worker_states[worker_id],
                    "batch_id": worker_batch_ids[worker_id],
                }
                if (
                    worker_states[worker_id] == "active"
                    and worker_batch_ids[worker_id]
                )
                else {"id": worker_id, "state": worker_states[worker_id]}
            )
            for worker_id in sorted(worker_states)
        ],
        "assigned_batches": assigned_batches,
        "journal": journal,
        "alerts": alerts,
    }
