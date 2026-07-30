#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Fail-closed harvest-and-refill decisions for MaterialX Horde workers."""

from __future__ import annotations

__all__ = ("ControllerBackend", "build_refill_cycle", "run_controller_cycle")

from typing import Any, Mapping, Protocol, Sequence

from materialx_horde_dispatch import _dispatch_id
from materialx_velocity_manifest import (
    validate_batch_manifest,
    validate_completion_result,
)


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
        if harvest not in {"success", "not_required"}:
            blocked_workers.append(worker_id)
            classification = (
                harvest
                if harvest in {
                    "invalid_completion",
                    "auth_failure",
                    "proxy_failure",
                    "missing",
                }
                else "harvest_failure"
                if harvest == "failure"
                else "harvest_missing"
            )
            alerts.append({"worker_id": worker_id, "classification": classification})
            continue
        if harvest == "success":
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


def _worker_records(
    workers: Sequence[Mapping[str, Any]],
    *,
    registered_families: Mapping[str, Any],
) -> list[dict[str, Any]]:
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
        unexpected = set(worker).difference({"id", "state", "batch_id", "assignment"})
        if unexpected:
            raise ValueError("worker records contain unsupported fields")
        if not isinstance(worker_id, str) or not worker_id or worker_id in worker_ids:
            raise ValueError("worker records require unique non-empty ids")
        if state not in {"active", "idle", "blocked"}:
            raise ValueError("worker records require active, idle, or blocked state")
        if batch_id is not None and (not isinstance(batch_id, str) or not batch_id):
            raise ValueError("worker batch_id must be a non-empty string when present")
        record: dict[str, Any] = {
            "id": worker_id,
            "state": state,
            "batch_id": batch_id or "",
            "assignment": None,
            "ownership_valid": "assignment" not in worker,
            "ownership_explicit": False,
        }
        if "assignment" in worker:
            if worker["assignment"] is None and batch_id:
                record["ownership_valid"] = True
                record["ownership_explicit"] = True
            else:
                try:
                    assignment = validate_batch_manifest(
                        worker["assignment"],
                        registered_families=registered_families,
                    )
                    if assignment["roles"]["implementation"] != worker_id or not batch_id:
                        raise ValueError("active assignment ownership mismatch")
                except (TypeError, ValueError):
                    record["ownership_valid"] = False
                else:
                    record["assignment"] = assignment
                    record["ownership_valid"] = True
                    record["ownership_explicit"] = True
        elif batch_id:
            record["ownership_valid"] = False
        result.append(record)
        worker_ids.add(worker_id)
    return result


def _harvest_result(
    result: Mapping[str, Any],
    assignment: Mapping[str, Any],
) -> tuple[str, str, dict[str, Any] | None]:
    """Validate bounded parser output against exactly one active assignment."""
    if not isinstance(result, Mapping):
        return "invalid_completion", "invalid_completion", None
    classification = result.get("classification")
    if classification == "completion" and set(result) == {
        "classification",
        "process_exit",
        "completion",
    }:
        try:
            completion = validate_completion_result(
                assignment,
                result["process_exit"],
                result["completion"],
            )
        except (TypeError, ValueError):
            return "invalid_completion", "invalid_completion", None
        return "success", "completion_manifest_v2", completion
    if set(result) == {"classification"} and classification in {
        "missing",
        "auth_failure",
        "proxy_failure",
        "invalid_completion",
        "invalid_exit",
        "invalid_json",
        "nonzero_exit",
        "oversized_line",
        "oversized_payload",
        "oversized_log_window",
        "secret_like_key",
        "unsupported_schema",
    }:
        public = (
            classification
            if classification in {"missing", "auth_failure", "proxy_failure"}
            else "invalid_completion"
        )
        return public, classification, None
    return "invalid_completion", "invalid_completion", None


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
    current_workers = _worker_records(
        workers,
        registered_families=registered_families,
    )
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
    worker_assignments = {
        worker["id"]: worker["assignment"] for worker in current_workers
    }
    worker_ownership_explicit = {
        worker["id"]: worker["ownership_explicit"] for worker in current_workers
    }
    artifacts: list[dict[str, Any]] = []
    for worker in current_workers:
        if worker["state"] != "idle":
            decision_workers.append({
                "id": worker["id"],
                "state": worker["state"],
                "harvest": "pending",
            })
            continue
        if not worker["batch_id"]:
            decision_workers.append({
                "id": worker["id"],
                "state": "idle",
                "harvest": "not_required",
            })
            continue
        if worker["ownership_valid"] and worker["assignment"] is None:
            decision_workers.append({
                "id": worker["id"],
                "state": "idle",
                "harvest": "not_required",
            })
            worker_batch_ids[worker["id"]] = ""
            worker_ownership_explicit[worker["id"]] = False
            journal.append({
                "worker_id": worker["id"],
                "batch_id": worker["batch_id"],
                "event": "harvest_not_required",
                "evidence": "explicit_role_only",
            })
            continue
        if not worker["ownership_valid"]:
            outcome, evidence, completion = (
                "invalid_completion",
                "invalid_completion",
                None,
            )
        else:
            try:
                outcome, evidence, completion = _harvest_result(
                    backend.harvest_finished(worker["id"], worker["batch_id"]),
                    worker["assignment"],
                )
            except Exception:
                outcome, evidence, completion = (
                    "invalid_completion",
                    "invalid_completion",
                    None,
                )
        decision_workers.append({"id": worker["id"], "state": "idle", "harvest": outcome})
        worker_batch_ids[worker["id"]] = ""
        worker_assignments[worker["id"]] = None
        if completion is not None:
            assignment = worker["assignment"]
            artifacts.append({
                "worker_id": worker["id"],
                "batch_id": assignment["batch_id"],
                "layer": assignment["layer"],
                "assignment": assignment,
                "completion": completion,
            })
        journal.append({
            "worker_id": worker["id"],
            "batch_id": (
                worker["assignment"]["batch_id"]
                if worker["assignment"] is not None
                else worker["batch_id"]
            ),
            "event": "harvest_valid" if completion is not None else "harvest_invalid",
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
            worker_assignments[worker_id] = next(
                (
                    manifest
                    for manifest in selected_manifests
                    if manifest["roles"]["implementation"] == worker_id
                ),
                None,
            ) if state == "active" else None
            worker_ownership_explicit[worker_id] = state == "active"
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
                    **({
                        "assignment": worker_assignments[worker_id]
                    } if worker_ownership_explicit[worker_id] else {}),
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
        "artifacts": sorted(artifacts, key=lambda artifact: artifact["batch_id"]),
    }
