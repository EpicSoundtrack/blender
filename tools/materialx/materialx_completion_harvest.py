# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Bounded, categorical parsing for MaterialX worker completion evidence."""

from __future__ import annotations

from collections.abc import Mapping, Sequence
from typing import Any
import json
import re


COMPLETION_PREFIX = "MATERIALX_COMPLETION_V2:"
EXIT_PREFIX = "MATERIALX_HORDE_EXIT:"
CLASSIFICATION_PREFIX = "MATERIALX_HARVEST_V2:"
MAX_PAYLOAD_BYTES = 16_384
MAX_LINE_BYTES = len(COMPLETION_PREFIX) + MAX_PAYLOAD_BYTES + 1_024
MAX_LOG_WINDOW_BYTES = 65_536

_REMOTE_FAILURES = frozenset(
    ("missing", "auth_failure", "proxy_failure", "oversized_log_window")
)
_SECRET_KEY = re.compile(
    r"(?:^|_)(?:api_?key|authorization|credential|password|secret|token)(?:$|_)",
    re.IGNORECASE,
)


def _contains_secret_key(value: Any) -> bool:
    if isinstance(value, Mapping):
        return any(
            not isinstance(key, str)
            or bool(_SECRET_KEY.search(key))
            or _contains_secret_key(item)
            for key, item in value.items()
        )
    if isinstance(value, Sequence) and not isinstance(value, (str, bytes)):
        return any(_contains_secret_key(item) for item in value)
    return False


def _failure(classification: str) -> dict[str, str]:
    return {"classification": classification}


def parse_completion_evidence(value: str) -> dict[str, Any]:
    """Parse only the bounded completion/exit protocol, never surrounding logs."""
    if not isinstance(value, str):
        return _failure("invalid_completion")
    if len(value.encode("utf-8")) > MAX_LOG_WINDOW_BYTES:
        return _failure("oversized_log_window")
    lines = value.splitlines()
    if len(lines) == 1 and lines[0].startswith(CLASSIFICATION_PREFIX):
        classification = lines[0][len(CLASSIFICATION_PREFIX):]
        return _failure(
            classification if classification in _REMOTE_FAILURES else "invalid_completion"
        )

    completion_lines = [
        line for line in lines if line.startswith(COMPLETION_PREFIX)
    ]
    exit_lines = [line for line in lines if line.startswith(EXIT_PREFIX)]
    if len(completion_lines) != 1:
        return _failure("invalid_completion")
    if len(exit_lines) != 1:
        return _failure("invalid_exit")
    if lines != [completion_lines[0], exit_lines[0]]:
        return _failure("invalid_completion")
    if any(len(line.encode("utf-8")) > MAX_LINE_BYTES for line in lines):
        return _failure("oversized_line")

    payload = completion_lines[0][len(COMPLETION_PREFIX):]
    if len(payload.encode("utf-8")) > MAX_PAYLOAD_BYTES:
        return _failure("oversized_payload")
    exit_text = exit_lines[0][len(EXIT_PREFIX):]
    if not re.fullmatch(r"(?:0|[1-9][0-9]*)", exit_text):
        return _failure("invalid_exit")
    process_exit = int(exit_text)
    try:
        completion = json.loads(payload)
    except (json.JSONDecodeError, RecursionError, ValueError):
        return _failure("invalid_json")
    if not isinstance(completion, Mapping):
        return _failure("invalid_completion")
    try:
        contains_secret = _contains_secret_key(completion)
    except RecursionError:
        return _failure("invalid_json")
    if contains_secret:
        return _failure("secret_like_key")
    if completion.get("schema_version") != 2:
        return _failure("unsupported_schema")
    if process_exit != 0:
        return _failure("nonzero_exit")
    return {
        "classification": "completion",
        "process_exit": process_exit,
        "completion": dict(completion),
    }
