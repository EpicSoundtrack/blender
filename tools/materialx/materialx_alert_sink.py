#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Sanitized, dependency-injected MaterialX alert delivery."""

from __future__ import annotations

__all__ = (
    "ALLOWED_FAILURE_CLASSES",
    "AlertTransport",
    "SanitizedAlertSink",
)

from collections.abc import Mapping, Sequence
import math
import re
from typing import Any, Protocol

from materialx_velocity_manifest import EXPECTED_HORDE_WORKERS, LAYERS


ALLOWED_FAILURE_CLASSES = frozenset((
    "capacity_loss",
    "stale_source",
    "queue_empty",
    "auth_failure",
    "proxy_failure",
    "transport_failure",
    "invalid_completion",
    "integration_failure",
))
_DOCUMENTED_LANES = frozenset((
    *LAYERS,
    "horde",
    "local_cpu",
    "local_cuda",
    "windows_a40_cuda",
    "windows_local_build",
    "golden_review",
))
_STATIC_SUBJECTS = frozenset(("queue", "source", "runtime"))
_RECEIPT_ID = re.compile(r"^[A-Za-z0-9_.:-]{1,128}$")


class AlertTransport(Protocol):
    """Runtime-provided connector transport."""

    def send(self, message: Mapping[str, Any]) -> str:
        """Deliver one sanitized message and return an opaque receipt ID."""


def _timestamp(value: Any) -> float:
    if (
        isinstance(value, bool)
        or not isinstance(value, (int, float))
        or not math.isfinite(value)
    ):
        raise ValueError("alert timestamp must be finite")
    return float(value)


def _subject(value: Any) -> str:
    if not isinstance(value, str):
        raise ValueError("alert subject must be a documented identifier")
    if value in _STATIC_SUBJECTS:
        return value
    prefix, separator, identifier = value.partition(":")
    if (
        separator
        and (
            (prefix == "worker" and identifier in EXPECTED_HORDE_WORKERS)
            or (prefix == "lane" and identifier in _DOCUMENTED_LANES)
        )
    ):
        return value
    raise ValueError("alert subject must be a documented identifier")


def _normalize_failures(
    failures: Sequence[Mapping[str, Any]],
) -> list[dict[str, str]]:
    if isinstance(failures, (str, bytes)) or not isinstance(failures, Sequence):
        raise ValueError("alerts must be a sequence")
    normalized = []
    keys = set()
    for failure in failures:
        if not isinstance(failure, Mapping) or set(failure) != {
            "failure_class",
            "subject",
        }:
            raise ValueError("alerts must contain only failure_class and subject")
        failure_class = failure["failure_class"]
        if failure_class not in ALLOWED_FAILURE_CLASSES:
            raise ValueError("alert failure_class is not public")
        subject = _subject(failure["subject"])
        key = (failure_class, subject)
        if key in keys:
            raise ValueError("duplicate alert failure class and subject")
        keys.add(key)
        normalized.append({
            "failure_class": failure_class,
            "subject": subject,
        })
    return sorted(
        normalized,
        key=lambda item: (item["failure_class"], item["subject"]),
    )


def _sent_record(
    failure: Mapping[str, str],
    timestamp: float,
    receipt_id: Any,
) -> dict[str, Any] | None:
    if not isinstance(receipt_id, str) or not _RECEIPT_ID.fullmatch(receipt_id):
        return None
    return {
        "failure_class": failure["failure_class"],
        "subject": failure["subject"],
        "timestamp": timestamp,
        "delivery_state": "sent",
        "receipt_id": receipt_id,
    }


def _unsent_record(
    failure: Mapping[str, str],
    timestamp: float,
) -> dict[str, Any]:
    return {
        "failure_class": failure["failure_class"],
        "subject": failure["subject"],
        "timestamp": timestamp,
        "delivery_state": "unsent",
    }


class SanitizedAlertSink:
    """Deduplicate current failures and retain only bounded delivery evidence."""

    def __init__(
        self,
        transport: AlertTransport,
        *,
        max_delivery_attempts: int = 2,
    ):
        if (
            isinstance(max_delivery_attempts, bool)
            or not isinstance(max_delivery_attempts, int)
            or max_delivery_attempts < 1
            or max_delivery_attempts > 8
        ):
            raise ValueError("max_delivery_attempts must be between one and eight")
        if not callable(getattr(transport, "send", None)):
            raise ValueError("alert transport must define send(message)")
        self._transport = transport
        self._max_delivery_attempts = max_delivery_attempts
        self._records: dict[tuple[str, str], dict[str, Any]] = {}
        self._attempts: dict[tuple[str, str], int] = {}

    def process(
        self,
        failures: Sequence[Mapping[str, Any]],
        *,
        timestamp: float,
    ) -> list[dict[str, Any]]:
        """Deliver each newly current failure at most once per invocation."""
        normalized = _normalize_failures(failures)
        observed_at = _timestamp(timestamp)
        current = {
            (failure["failure_class"], failure["subject"]): failure
            for failure in normalized
        }
        recovered = set(self._records).difference(current)
        for key in recovered:
            self._records.pop(key, None)
            self._attempts.pop(key, None)

        for key, failure in current.items():
            previous = self._records.get(key)
            if previous is not None and previous["delivery_state"] == "sent":
                continue
            attempts = self._attempts.get(key, 0)
            first_observed = (
                previous["timestamp"] if previous is not None else observed_at
            )
            if attempts >= self._max_delivery_attempts:
                continue
            message = {
                "failure_class": failure["failure_class"],
                "subject": failure["subject"],
                "timestamp": first_observed,
            }
            self._attempts[key] = attempts + 1
            try:
                receipt_id = self._transport.send(message)
            except Exception:
                receipt = None
            else:
                receipt = _sent_record(failure, first_observed, receipt_id)
            self._records[key] = (
                receipt
                if receipt is not None
                else _unsent_record(failure, first_observed)
            )

        return [
            dict(self._records[key])
            for key in sorted(self._records)
            if key in current
        ]
