#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Tests for sanitized MaterialX alert delivery and receipts."""

from __future__ import annotations

import json
import math
import unittest

import materialx_alert_sink


ALLOWED_FAILURES = (
    "capacity_loss",
    "stale_source",
    "queue_empty",
    "auth_failure",
    "proxy_failure",
    "transport_failure",
    "invalid_completion",
    "integration_failure",
)


class RecordingTransport:
    def __init__(self, *, fail_count=0):
        self.fail_count = fail_count
        self.messages = []

    def send(self, message):
        self.messages.append(message)
        if len(self.messages) <= self.fail_count:
            raise RuntimeError("secret transport exception")
        return f"receipt-{len(self.messages)}"


class MaterialXAlertSinkTest(unittest.TestCase):
    def test_all_and_only_public_failure_classes_are_accepted(self):
        transport = RecordingTransport()
        sink = materialx_alert_sink.SanitizedAlertSink(transport)

        records = sink.process(
            [
                {"failure_class": failure, "subject": "runtime"}
                for failure in ALLOWED_FAILURES
            ],
            timestamp=10.5,
        )

        self.assertEqual(
            [record["failure_class"] for record in records],
            sorted(ALLOWED_FAILURES),
        )
        self.assertTrue(all(record["delivery_state"] == "sent" for record in records))
        with self.assertRaises(ValueError):
            sink.process(
                [{"failure_class": "worker_exit", "subject": "runtime"}],
                timestamp=11,
            )
        self.assertEqual(len(transport.messages), len(ALLOWED_FAILURES))

    def test_validates_exact_subjects_timestamp_and_input_fields_before_transport(self):
        transport = RecordingTransport()
        sink = materialx_alert_sink.SanitizedAlertSink(transport)
        valid_subjects = (
            "worker:blend05",
            "worker:blendit04",
            "worker:blendit",
            "worker:blendit2",
            "worker:blendit3",
            "lane:native_cycles",
            "lane:hydra_ovrtx",
            "lane:blender_authoring",
            "lane:windows_local_build",
            "queue",
            "source",
            "runtime",
        )

        for index, subject in enumerate(valid_subjects):
            sink.process(
                [{"failure_class": "capacity_loss", "subject": subject}],
                timestamp=float(index),
            )
            sink.process([], timestamp=float(index) + 0.5)

        invalid = (
            {"failure_class": "capacity_loss", "subject": "worker:unknown"},
            {"failure_class": "capacity_loss", "subject": "lane:unknown"},
            {"failure_class": "capacity_loss", "subject": "runtime", "message": "raw"},
        )
        for alert in invalid:
            with self.subTest(alert=alert), self.assertRaises(ValueError):
                sink.process([alert], timestamp=20)
        for timestamp in (math.inf, math.nan, True, "1"):
            with self.subTest(timestamp=timestamp), self.assertRaises(ValueError):
                sink.process([], timestamp=timestamp)

    def test_message_and_receipt_store_only_exact_sanitized_fields(self):
        transport = RecordingTransport()
        sink = materialx_alert_sink.SanitizedAlertSink(transport)

        records = sink.process(
            [{"failure_class": "proxy_failure", "subject": "worker:blend05"}],
            timestamp=42,
        )

        self.assertEqual(
            transport.messages,
            [{
                "failure_class": "proxy_failure",
                "subject": "worker:blend05",
                "timestamp": 42.0,
            }],
        )
        self.assertEqual(
            records,
            [{
                "failure_class": "proxy_failure",
                "subject": "worker:blend05",
                "timestamp": 42.0,
                "delivery_state": "sent",
                "receipt_id": "receipt-1",
            }],
        )
        serialized = json.dumps(records)
        for forbidden in ("stdout", "stderr", "prompt", "instruction", "credential", "patch"):
            self.assertNotIn(forbidden, serialized.casefold())

    def test_deduplicates_exact_pair_and_recovery_allows_recurrence(self):
        transport = RecordingTransport()
        sink = materialx_alert_sink.SanitizedAlertSink(transport)
        first = {"failure_class": "capacity_loss", "subject": "worker:blend05"}
        second = {"failure_class": "capacity_loss", "subject": "worker:blendit04"}

        first_records = sink.process([first], timestamp=1)
        unchanged = sink.process([first], timestamp=2)
        changed = sink.process([first, second], timestamp=3)
        recovered = sink.process([], timestamp=4)
        recurred = sink.process([first], timestamp=5)

        self.assertEqual(len(transport.messages), 3)
        self.assertEqual(unchanged, first_records)
        self.assertEqual(len(changed), 2)
        self.assertEqual(recovered, [])
        self.assertEqual(recurred[0]["timestamp"], 5.0)
        self.assertEqual(recurred[0]["receipt_id"], "receipt-3")

    def test_transport_failure_is_visible_sanitized_and_retries_once_per_poll(self):
        transport = RecordingTransport(fail_count=2)
        sink = materialx_alert_sink.SanitizedAlertSink(
            transport,
            max_delivery_attempts=2,
        )
        failure = {"failure_class": "auth_failure", "subject": "worker:blendit"}

        first = sink.process([failure], timestamp=10)
        second = sink.process([failure], timestamp=11)
        exhausted = sink.process([failure], timestamp=12)

        expected = [{
            "failure_class": "auth_failure",
            "subject": "worker:blendit",
            "timestamp": 10.0,
            "delivery_state": "unsent",
        }]
        self.assertEqual(first, expected)
        self.assertEqual(second, expected)
        self.assertEqual(exhausted, expected)
        self.assertEqual(len(transport.messages), 2)
        self.assertNotIn("secret", json.dumps(exhausted))

    def test_invalid_or_oversized_receipt_fails_closed_as_unsent(self):
        for receipt in ("contains/slash", "x" * 129, "", None):
            class BadReceiptTransport:
                def send(self, message):
                    return receipt

            with self.subTest(receipt=receipt):
                records = materialx_alert_sink.SanitizedAlertSink(
                    BadReceiptTransport()
                ).process(
                    [{"failure_class": "queue_empty", "subject": "queue"}],
                    timestamp=1,
                )
                self.assertEqual(records[0]["delivery_state"], "unsent")
                self.assertNotIn("receipt_id", records[0])


if __name__ == "__main__":
    unittest.main()
