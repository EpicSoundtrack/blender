#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Fail-closed, secret-free source checks for MaterialX Horde workers."""

from __future__ import annotations

__all__ = (
    "REQUIRED_ARCHITECTURE_FILES",
    "WorkerProbe",
    "WorkerSynchronizer",
    "parse_probe_document",
    "preflight_workers",
)

import json
import re
from typing import Any, Mapping, Protocol, Sequence


REQUIRED_ARCHITECTURE_FILES = (
    "intern/cycles/scene/materialx.cpp",
    "tools/materialx/materialx_velocity_manifest.py",
    "tools/materialx/materialx_batch_scheduler.py",
)
SHA_PATTERN = re.compile(r"^[0-9a-f]{40}$")


class WorkerProbe(Protocol):
    """Read only the bounded source facts for one named worker."""

    def probe(self, worker: str) -> Mapping[str, Any]: ...


class WorkerSynchronizer(Protocol):
    """Apply an approved source bundle without returning its contents."""

    def synchronize(self, worker: str, expected_sha: str) -> bool: ...


def _require_sha(value: Any) -> str:
    if not isinstance(value, str) or not SHA_PATTERN.fullmatch(value):
        raise ValueError("expected_sha must be a lowercase 40-hex SHA")
    return value


def _safe_snapshot(document: Mapping[str, Any]) -> dict[str, Any]:
    if not isinstance(document, Mapping) or set(document) != {"repository_present", "files", "head"}:
        raise ValueError("source probe must contain only repository_present, files, and head")
    repository_present = document["repository_present"]
    files = document["files"]
    head = document["head"]
    if not isinstance(repository_present, bool) or not isinstance(files, Mapping) or set(files) != set(REQUIRED_ARCHITECTURE_FILES):
        raise ValueError("source probe has invalid categorical file checks")
    if not all(isinstance(files[path], bool) for path in REQUIRED_ARCHITECTURE_FILES):
        raise ValueError("source probe has invalid categorical file checks")
    if not isinstance(head, str) or not SHA_PATTERN.fullmatch(head):
        raise ValueError("source probe has invalid HEAD")
    return {
        "repository_present": repository_present,
        "files": {path: files[path] for path in REQUIRED_ARCHITECTURE_FILES},
        "head": head,
    }


def parse_probe_document(output: str) -> dict[str, Any]:
    """Parse the production sentinel, rejecting all unbounded probe output."""
    if not isinstance(output, str) or len(output) > 1000:
        raise ValueError("source probe output is invalid")
    try:
        document = json.loads(output)
    except (TypeError, json.JSONDecodeError) as ex:
        raise ValueError("source probe output is invalid") from ex
    return _safe_snapshot(document)


def _blocked(state: str) -> dict[str, Any]:
    return {"state": state}


def _evaluate(document: Mapping[str, Any], expected_sha: str) -> dict[str, Any]:
    snapshot = _safe_snapshot(document)
    if not snapshot["repository_present"]:
        return _blocked("missing_repository")
    if not all(snapshot["files"].values()):
        return _blocked("missing_required_file")
    if snapshot["head"] != expected_sha:
        return _blocked("stale_source")
    return {"state": "ready"}


def _probe_once(probe: WorkerProbe, worker: str, expected_sha: str) -> dict[str, Any]:
    try:
        return _evaluate(probe.probe(worker), expected_sha)
    except Exception:
        return _blocked("invalid_probe")


def preflight_workers(
    workers: Sequence[str],
    expected_sha: str,
    *,
    probe: WorkerProbe,
    synchronizer: WorkerSynchronizer | None = None,
) -> dict[str, dict[str, Any]]:
    """Return per-worker categorical readiness; stale workers are re-probed after sync."""
    expected_sha = _require_sha(expected_sha)
    if isinstance(workers, (str, bytes)) or not isinstance(workers, Sequence):
        raise ValueError("workers must be a sequence of worker IDs")
    result: dict[str, dict[str, Any]] = {}
    for worker in workers:
        if not isinstance(worker, str) or not worker or worker.strip() != worker:
            raise ValueError("worker IDs must be non-empty strings without surrounding whitespace")
        status = _probe_once(probe, worker, expected_sha)
        if status["state"] == "stale_source" and synchronizer is not None:
            try:
                synchronizer.synchronize(worker, expected_sha)
            except Exception:
                pass
            status = _probe_once(probe, worker, expected_sha)
        result[worker] = status
    return result
