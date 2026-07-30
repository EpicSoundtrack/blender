# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Canonical, fail-closed manifests for MaterialX velocity work."""

from __future__ import annotations

from collections.abc import Collection, Mapping, Sequence
from typing import Any
import re


SCHEMA_VERSION = 2
EXPECTED_HORDE_WORKERS = ("blend05", "blendit04", "blendit", "blendit2", "blendit3")
LAYERS = frozenset(("native_cycles", "hydra_ovrtx", "blender_authoring"))
REQUIRED_ROLES = frozenset(("implementation", "generated_tests", "independent_review"))

BATCH_FIELDS = frozenset(
    (
        "schema_version",
        "batch_id",
        "batch_kind",
        "family_id",
        "template_signature",
        "layer",
        "node_defs",
        "integration_base_sha",
        "worker_source_sha",
        "roles",
        "files_allowlist",
        "focused_test_commands",
        "generated_evidence_tier",
        "exception_budget",
        "red_test",
        "approval_record",
    )
)
TEMPLATE_SIGNATURE_FIELDS = frozenset(("operation", "input_types", "output_type", "broadcast_policy", "output_socket_class"))
REGISTERED_FAMILY_FIELDS = frozenset(
    ("template_signature", "node_defs", "generated_evidence_tier", "focused_test_commands")
)
COMPLETION_FIELDS = frozenset(
    (
        "schema_version",
        "batch_id",
        "base_sha",
        "head_sha",
        "node_defs",
        "rejected_node_defs",
        "changed_files",
        "tests",
        "review_verdict",
        "role_evidence",
    )
)
TEST_FIELDS = frozenset(("command", "passed", "failed", "exit_code"))

_SHA_PATTERN = re.compile(r"^[0-9a-f]{40}$")
_RECEIPT_PATTERN = re.compile(r"^[A-Z][A-Z_]{0,31}-[0-9a-f]{8,40}$")
_ROLE_RECEIPT_PREFIXES = {
    "implementation": "IMPLEMENTATION",
    "generated_tests": "GENERATED_TESTS",
    "independent_review": "INDEPENDENT_REVIEW",
}


def _require_mapping(value: Any, name: str) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise ValueError(f"{name} must be a mapping")
    return value


def _require_sha(value: Any, name: str) -> str:
    if not isinstance(value, str) or not _SHA_PATTERN.fullmatch(value):
        raise ValueError(f"{name} must be a 40-hex SHA")
    return value


def _require_string(value: Any, name: str) -> str:
    if not isinstance(value, str) or not value:
        raise ValueError(f"{name} must be a non-empty string")
    return value


def _require_receipt(value: Any, name: str, *, prefix: str) -> str:
    if (
        not isinstance(value, str)
        or not value.startswith(prefix + "-")
        or not _RECEIPT_PATTERN.fullmatch(value)
    ):
        raise ValueError(f"{name} must be a safe receipt identifier")
    return value


def _normalized_string_list(value: Any, name: str, *, allow_empty: bool = False) -> list[str]:
    if isinstance(value, str) or not isinstance(value, Sequence):
        raise ValueError(f"{name} must be a list of strings")
    if not allow_empty and not value:
        raise ValueError(f"{name} must not be empty")
    if any(not isinstance(item, str) or not item for item in value):
        raise ValueError(f"{name} must be a list of strings")
    if len(set(value)) != len(value):
        raise ValueError(f"duplicate {name[:-1] if name.endswith('s') else name}")
    return sorted(value)


def _ordered_string_list(value: Any, name: str) -> list[str]:
    if isinstance(value, str) or not isinstance(value, Sequence) or not value:
        raise ValueError(f"{name} must be a non-empty list of strings")
    if any(not isinstance(item, str) or not item for item in value):
        raise ValueError(f"{name} must be a non-empty list of strings")
    if len(set(value)) != len(value):
        raise ValueError(f"duplicate {name[:-1] if name.endswith('s') else name}")
    return list(value)


def _missing_fields(manifest: Mapping[str, Any], fields: Collection[str], kind: str) -> None:
    missing = set(fields).difference(manifest)
    if missing:
        raise ValueError(f"{kind} is missing fields: {sorted(missing)}")


def _template_signature(value: Any) -> dict[str, Any]:
    signature = _require_mapping(value, "template_signature")
    if set(signature) != TEMPLATE_SIGNATURE_FIELDS:
        raise ValueError("template_signature has unsupported fields")
    normalized = {field: _require_string(signature[field], f"template_signature[{field}]")
                  for field in ("operation", "output_type", "broadcast_policy", "output_socket_class")}
    input_types = signature["input_types"]
    if isinstance(input_types, str) or not isinstance(input_types, Sequence) or not input_types or any(
        not isinstance(value, str) or not value for value in input_types
    ):
        raise ValueError("template_signature input_types must be a non-empty list of strings")
    normalized["input_types"] = list(input_types)
    return {field: normalized[field] for field in sorted(TEMPLATE_SIGNATURE_FIELDS)}


def _registered_family(
    family_id: str,
    template_signature: Mapping[str, Any],
    registered_families: Mapping[str, Any],
) -> list[dict[str, Any]]:
    families = _require_mapping(registered_families, "registered_families")
    if family_id not in families:
        raise ValueError("unregistered family_id")
    records = families[family_id]
    if isinstance(records, (str, bytes)) or not isinstance(records, Sequence) or not records:
        raise ValueError("registered family contracts must be a non-empty sequence")
    normalized_signature = _template_signature(template_signature)
    matches = []
    for raw_record in records:
        record = _require_mapping(raw_record, f"registered family {family_id}")
        if set(record) != REGISTERED_FAMILY_FIELDS:
            raise ValueError("registered family has unsupported fields")
        normalized = {
            "template_signature": _template_signature(record["template_signature"]),
            "node_defs": _normalized_string_list(record["node_defs"], "registered family node_defs"),
            "generated_evidence_tier": _require_string(record["generated_evidence_tier"], "registered family generated_evidence_tier"),
            "focused_test_commands": _ordered_string_list(record["focused_test_commands"], "registered family focused_test_commands"),
        }
        if normalized["template_signature"] == normalized_signature:
            matches.append(normalized)
    if not matches:
        raise ValueError("template_signature has no registered family contract")
    return matches


def validate_batch_manifest(
    manifest: Mapping[str, Any], *, registered_families: Mapping[str, Any]
) -> dict[str, Any]:
    """Validate a batch assignment at the scheduler/dispatch trust boundary."""
    manifest = _require_mapping(manifest, "batch manifest")
    _missing_fields(manifest, BATCH_FIELDS, "batch manifest")
    unexpected = set(manifest).difference(BATCH_FIELDS)
    if unexpected:
        raise ValueError(f"batch manifest has unsupported fields: {sorted(unexpected)}")

    if manifest["schema_version"] != SCHEMA_VERSION:
        raise ValueError("unsupported batch schema_version")
    _require_string(manifest["batch_id"], "batch_id")
    template_signature = _template_signature(manifest["template_signature"])
    family_id = _require_string(manifest["family_id"], "family_id")
    registered_contracts = _registered_family(family_id, template_signature, registered_families)
    batch_kind = manifest["batch_kind"]
    if batch_kind not in {"family", "complex_exception"}:
        raise ValueError("batch_kind must be family or complex_exception")
    if not isinstance(manifest["layer"], str) or manifest["layer"] not in LAYERS:
        raise ValueError("unsupported layer")

    integration_base_sha = _require_sha(manifest["integration_base_sha"], "integration_base_sha")
    worker_source_sha = _require_sha(manifest["worker_source_sha"], "worker_source_sha")
    if integration_base_sha != worker_source_sha:
        raise ValueError("worker_source_sha must equal integration_base_sha")

    node_defs = _normalized_string_list(manifest["node_defs"], "NodeDefs")
    roles = _require_mapping(manifest["roles"], "roles")
    missing_roles = REQUIRED_ROLES.difference(roles)
    if missing_roles:
        raise ValueError(f"missing required roles: {sorted(missing_roles)}")
    if set(roles).difference(REQUIRED_ROLES):
        raise ValueError("unsupported roles")
    role_workers = {role: roles[role] for role in REQUIRED_ROLES}
    if any(not isinstance(worker, str) or worker not in EXPECTED_HORDE_WORKERS for worker in role_workers.values()):
        raise ValueError("unsupported Horde worker")
    if len(set(role_workers.values())) != len(role_workers):
        raise ValueError("roles must use independent Horde workers")

    files_allowlist = _normalized_string_list(manifest["files_allowlist"], "files_allowlist")
    focused_test_commands = _ordered_string_list(
        manifest["focused_test_commands"], "focused_test_commands"
    )
    generated_evidence_tier = _require_string(manifest["generated_evidence_tier"], "generated_evidence_tier")
    exception_budget = manifest["exception_budget"]
    if not isinstance(exception_budget, int) or isinstance(exception_budget, bool) or exception_budget < 0:
        raise ValueError("exception_budget must be a non-negative integer")

    red_test = manifest["red_test"]
    approval_record = manifest["approval_record"]
    node_contracts = [
        contract
        for contract in registered_contracts
        if set(node_defs).issubset(contract["node_defs"])
    ]
    if not node_contracts:
        raise ValueError("node_defs do not match exactly one registered family contract")
    tier_contracts = [
        contract
        for contract in node_contracts
        if generated_evidence_tier == contract["generated_evidence_tier"]
    ]
    if not tier_contracts:
        raise ValueError("generated_evidence_tier does not match registered family")
    matching_contracts = [
        contract
        for contract in tier_contracts
        if focused_test_commands == contract["focused_test_commands"]
    ]
    if not matching_contracts:
        raise ValueError("focused_test_commands do not match registered family")
    if len(matching_contracts) != 1:
        raise ValueError("node_defs match ambiguous registered family contracts")
    if batch_kind == "complex_exception":
        if not 1 <= len(node_defs) <= 7:
            raise ValueError("complex exception must contain 1-7 NodeDefs")
        if exception_budget != 1:
            raise ValueError("complex exception_budget must be one")
        _require_receipt(red_test, "red_test", prefix="RED_TEST")
        _require_receipt(approval_record, "approval_record", prefix="APPROVAL")
    else:
        if not isinstance(red_test, str) or not isinstance(approval_record, str):
            raise ValueError("exception evidence must be strings")
        if not 8 <= len(node_defs) <= 16:
            raise ValueError("family must contain 8-16 NodeDefs")
        if exception_budget != 0:
            raise ValueError("exception_budget must be zero for a normal family")
        if red_test or approval_record:
            raise ValueError("normal family cannot carry exception evidence")

    normalized = {
        "schema_version": SCHEMA_VERSION,
        "batch_id": manifest["batch_id"],
        "batch_kind": batch_kind,
        "family_id": family_id,
        "template_signature": template_signature,
        "layer": manifest["layer"],
        "node_defs": node_defs,
        "integration_base_sha": integration_base_sha,
        "worker_source_sha": worker_source_sha,
        "roles": {role: role_workers[role] for role in sorted(role_workers)},
        "files_allowlist": files_allowlist,
        "focused_test_commands": focused_test_commands,
        "generated_evidence_tier": generated_evidence_tier,
        "exception_budget": exception_budget,
        "red_test": red_test,
        "approval_record": approval_record,
    }
    return {field: normalized[field] for field in sorted(BATCH_FIELDS)}


def validate_completion_manifest(
    assignment: Mapping[str, Any], completion: Mapping[str, Any]
) -> dict[str, Any]:
    """Validate and sanitize a worker completion against its assignment."""
    assignment = _require_mapping(assignment, "assignment")
    completion = _require_mapping(completion, "completion manifest")
    _missing_fields(completion, COMPLETION_FIELDS, "completion manifest")

    if completion["schema_version"] != SCHEMA_VERSION:
        raise ValueError("unsupported completion schema_version")
    batch_id = _require_string(completion["batch_id"], "completion batch_id")
    if batch_id != assignment.get("batch_id"):
        raise ValueError("completion batch_id does not match assignment")

    base_sha = _require_sha(completion["base_sha"], "completion base_sha")
    head_sha = _require_sha(completion["head_sha"], "completion head_sha")
    if base_sha != assignment.get("integration_base_sha"):
        raise ValueError("completion base_sha does not match assignment")
    if head_sha == base_sha:
        raise ValueError("completion head_sha must differ from completion base_sha")

    node_defs = _normalized_string_list(completion["node_defs"], "NodeDefs")
    assigned_node_defs = _normalized_string_list(assignment.get("node_defs"), "assigned NodeDefs")
    if set(node_defs) != set(assigned_node_defs):
        raise ValueError("completion NodeDefs do not match assignment")
    rejected_node_defs = _normalized_string_list(
        completion["rejected_node_defs"], "rejected NodeDefs", allow_empty=True
    )
    if rejected_node_defs:
        raise ValueError("completion contains rejected NodeDefs")

    changed_files = _normalized_string_list(completion["changed_files"], "changed_files")
    allowed_files = _normalized_string_list(assignment.get("files_allowlist"), "assignment files_allowlist")
    escaped = set(changed_files).difference(allowed_files)
    if escaped:
        raise ValueError(f"completion changed files outside allowlist: {sorted(escaped)}")

    tests = completion["tests"]
    if isinstance(tests, str) or not isinstance(tests, Sequence) or not tests:
        raise ValueError("completion tests must not be empty")
    normalized_tests: list[dict[str, Any]] = []
    for test in tests:
        test = _require_mapping(test, "completion test")
        missing = TEST_FIELDS.difference(test)
        if missing or any(
            not isinstance(test[field], int) or isinstance(test[field], bool)
            for field in ("passed", "failed", "exit_code")
        ):
            raise ValueError("missing numeric test fields")
        command = _require_string(test["command"], "test command")
        if test["passed"] < 0 or test["failed"] < 0:
            raise ValueError("test counts must be non-negative")
        normalized_tests.append({field: test[field] for field in sorted(TEST_FIELDS) if field != "command"} | {"command": command})
    assigned_test_commands = _ordered_string_list(
        assignment.get("focused_test_commands"), "assignment focused_test_commands"
    )
    if [test["command"] for test in normalized_tests] != assigned_test_commands:
        raise ValueError("completion test commands do not match assignment")
    if any(test["exit_code"] != 0 or test["failed"] != 0 for test in normalized_tests):
        raise ValueError("completion contains failed tests")

    if completion["review_verdict"] != "pass":
        raise ValueError("completion review_verdict is not pass")
    role_evidence = _require_mapping(completion["role_evidence"], "role_evidence")
    missing_evidence = REQUIRED_ROLES.difference(role_evidence)
    if missing_evidence:
        raise ValueError(f"missing required role evidence: {sorted(missing_evidence)}")
    if set(role_evidence).difference(REQUIRED_ROLES):
        raise ValueError("unsupported role evidence")
    normalized_evidence = {
        role: _require_receipt(
            role_evidence[role],
            f"role_evidence[{role}]",
            prefix=_ROLE_RECEIPT_PREFIXES[role],
        )
        for role in sorted(REQUIRED_ROLES)
    }

    normalized = {
        "schema_version": SCHEMA_VERSION,
        "batch_id": batch_id,
        "base_sha": base_sha,
        "head_sha": head_sha,
        "node_defs": node_defs,
        "rejected_node_defs": rejected_node_defs,
        "changed_files": changed_files,
        "tests": normalized_tests,
        "review_verdict": "pass",
        "role_evidence": normalized_evidence,
    }
    return {field: normalized[field] for field in sorted(COMPLETION_FIELDS)}


def validate_completion_result(
    assignment: Mapping[str, Any], process_exit: int, completion: Mapping[str, Any] | None
) -> dict[str, Any]:
    """Accept a completion only when a successful process supplied its manifest."""
    if not isinstance(process_exit, int) or isinstance(process_exit, bool):
        raise ValueError("process_exit must be an integer")
    if process_exit == 0 and completion is None:
        raise ValueError("process exited zero without a completion manifest")
    if process_exit != 0:
        raise ValueError("completion process_exit is not zero")
    return validate_completion_manifest(assignment, completion)
