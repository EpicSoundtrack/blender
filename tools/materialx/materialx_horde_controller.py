#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Fail-closed harvest-and-refill decisions for MaterialX Horde workers."""

from __future__ import annotations

__all__ = ("build_refill_cycle",)

from typing import Any, Mapping, Sequence


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
            alerts.append({"worker_id": worker_id, "classification": "harvest_missing"})
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
    for worker_id, batch in zip(eligible_workers, queued_batches):
        batch_id = batch.get("batch_id")
        prompt = batch.get("prompt")
        if not isinstance(batch_id, str) or not batch_id or not isinstance(prompt, str) or not prompt:
            raise ValueError("queued batches require non-empty batch_id and prompt")
        if batch_id in seen_batches:
            raise ValueError("queued batches must not overlap")
        seen_batches.add(batch_id)
        refills.append({"worker_id": worker_id, "batch_id": batch_id, "prompt": prompt})

    return {
        "completed_workers": completed_workers,
        "refills": refills,
        "blocked_workers": sorted(set(blocked_workers)),
        "alerts": alerts,
    }
