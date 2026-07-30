#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

import copy
import unittest

from materialx_nodedef_ledger import build_ledger
from materialx_progress_report import (
    build_progress_report,
    progress_report_json,
    progress_report_text,
)
from materialx_project_state import new_project_state, update_horde_observation
from materialx_test_cadence import build_cadence_decision, execute_cadence
from materialx_velocity_manifest import (
    validate_batch_manifest,
    validate_completion_manifest,
)
from test_materialx_completion_harvest import make_completion
from test_materialx_horde_dispatch_plan import REGISTERED_FAMILIES, make_manifest
from test_materialx_test_cadence import config, integration_record


def ledger():
    return build_ledger([
        {
            "id": f"ND_{index:04d}",
            "category": "test",
            "types": ["float"],
            "source": "stdlib",
        }
        for index in range(802)
    ])


def make_credit(registered, batch_id, node_start):
    assignment = make_manifest(batch_id, node_start=node_start)
    assignment["node_defs"] = [
        f"ND_{index:04d}" for index in range(node_start, node_start + 8)
    ]
    assignment["files_allowlist"] = [f"intern/cycles/add_{node_start}.cpp"]
    assignment = validate_batch_manifest(
        assignment, registered_families=registered
    )
    completion = make_completion(batch_id)
    completion.update({
        "base_sha": assignment["integration_base_sha"],
        "node_defs": list(assignment["node_defs"]),
        "changed_files": list(assignment["files_allowlist"]),
        "tests": [{
            "command": assignment["focused_test_commands"][0],
            "passed": 1,
            "failed": 0,
            "exit_code": 0,
        }],
    })
    completion = validate_completion_manifest(assignment, completion)
    integration = {
        "batch_id": batch_id,
        "layer": "native_cycles",
        "base_sha": completion["base_sha"],
        "head_sha": completion["head_sha"],
        "focused_commands": list(assignment["focused_test_commands"]),
        "numeric_exits": [0],
        "final_state": "integrated",
    }
    return {
        "ledger_delta": {
            "receipt_id": f"ledger-delta-{batch_id}",
            "node_defs": list(assignment["node_defs"]),
        },
        "assignment": assignment,
        "completion": completion,
        "integration_receipt": integration,
    }


def registered_and_credit():
    registered = copy.deepcopy(REGISTERED_FAMILIES)
    registered["add"][0]["node_defs"] = [
        f"ND_{index:04d}" for index in range(16)
    ]
    credit = make_credit(registered, "credit-a", 0)
    decision = build_cadence_decision(
        integrations=[{
            "assignment": credit["assignment"],
            "receipt": credit["integration_receipt"],
        }],
        project_state=new_project_state(),
        cadence_config=config(),
        registered_families=registered,
    )
    cadence = execute_cadence(
        decision,
        runner=lambda argv, *, timeout_seconds: {"exit_code": 0},
    )
    return registered, credit, decision, cadence


def three_credit_boundary():
    registered, _, _, _ = registered_and_credit()
    registered["add"][0]["node_defs"] = [
        f"ND_{index:04d}" for index in range(24)
    ]
    credits = [
        make_credit(registered, f"credit-{suffix}", node_start)
        for suffix, node_start in (("a", 0), ("b", 8), ("c", 16))
    ]
    state = new_project_state()
    state["integration_receipts"] = [
        {
            key: credit["integration_receipt"][key]
            for key in (
                "batch_id",
                "layer",
                "base_sha",
                "head_sha",
                "final_state",
            )
        }
        for credit in credits[:2]
    ]
    replay = copy.deepcopy(state)
    replay["integration_receipts"] = []
    decision = build_cadence_decision(
        integrations=[
            {
                "assignment": credit["assignment"],
                "receipt": credit["integration_receipt"],
            }
            for credit in credits
        ],
        project_state=replay,
        cadence_config=config(),
        registered_families=registered,
    )
    return registered, credits, state, decision


def train_states():
    return {
        "native_cycles": "integrated",
        "hydra_ovrtx": "idle",
        "blender_authoring": "blocked",
    }


class MaterialXProgressReportTest(unittest.TestCase):
    def test_reconciles_exactly_802_rows_from_correlated_green_evidence(self):
        registered, credit, decision, cadence = registered_and_credit()
        state = update_horde_observation(
            new_project_state(),
            worker_states={
                "blend05": "active",
                "blendit04": "active",
                "blendit": "active",
                "blendit2": "active",
                "blendit3": "active",
            },
            evidence_receipt="horde-cycle-0001",
        )
        state["assigned_batches"] = [
            {"worker_id": "blend05", "batch_id": "next-a"},
            {"worker_id": "blendit", "batch_id": "next-b"},
            {"worker_id": "blendit04", "batch_id": "next-c"},
            {"worker_id": "blendit2", "batch_id": "next-d"},
            {"worker_id": "blendit3", "batch_id": "next-e"},
        ]

        report = build_progress_report(
            ledger=ledger(),
            phase2_ids=[f"ND_{index:04d}" for index in range(792, 802)],
            credit_records=[credit],
            cadence_config=config(),
            cadence_decision=decision,
            cadence_receipts=cadence,
            project_state=state,
            integration_train_states=train_states(),
            registered_families=registered,
        )

        self.assertEqual(
            {key: report[key] for key in ("total", "credited", "remaining", "phase2")},
            {"total": 802, "credited": 8, "remaining": 784, "phase2": 10},
        )
        self.assertEqual(len(report["workers"]), 5)
        self.assertEqual(set(report["integration_trains"]), {
            "native_cycles", "hydra_ovrtx", "blender_authoring"
        })
        self.assertEqual(report["evidence_tier_counts"], {
            "completion_manifest_v2": 8,
            "generated_semantic_template": 8,
            "integrated": 8,
            "focused_green": 8,
        })
        self.assertEqual(report["cadence"]["due"], len(decision["commands"]))
        self.assertEqual(report["cadence"]["executed_green"], len(cadence))
        self.assertEqual(report["blockers"], [{
            "classification": "integration_train_blocked",
            "subject": "blender_authoring",
            "recovery_action": "repair_integration_train",
        }])

    def test_untrusted_or_incomplete_evidence_never_credits_nodes(self):
        registered, credit, decision, cadence = registered_and_credit()
        cases = []
        pid = copy.deepcopy(credit)
        pid["launched_pid"] = 123
        cases.append(pid)
        rejected = copy.deepcopy(credit)
        rejected["integration_receipt"]["final_state"] = "rejected"
        rejected["integration_receipt"]["failure_classification"] = "merge_failure"
        cases.append(rejected)
        stale = copy.deepcopy(credit)
        stale["integration_receipt"]["head_sha"] = "c" * 40
        cases.append(stale)
        failed_cadence = execute_cadence(
            decision,
            runner=lambda argv, *, timeout_seconds: {"exit_code": 1},
        )

        for record in cases:
            with self.subTest(keys=set(record)), self.assertRaises(ValueError):
                build_progress_report(
                    ledger=ledger(),
                    phase2_ids=[],
                    credit_records=[record],
                    cadence_config=config(),
                    cadence_decision=decision,
                    cadence_receipts=cadence,
                    project_state=new_project_state(),
                    integration_train_states=train_states(),
                    registered_families=registered,
                )

        report = build_progress_report(
            ledger=ledger(),
            phase2_ids=[],
            credit_records=[credit],
            cadence_config=config(),
            cadence_decision=decision,
            cadence_receipts=failed_cadence,
            project_state=new_project_state(),
            integration_train_states=train_states(),
            registered_families=registered,
        )
        self.assertEqual(report["credited"], 0)
        self.assertEqual(report["remaining"], 802)

    def test_failed_layer_full_suite_withholds_affected_family_credit(self):
        registered, credits, state, decision = three_credit_boundary()
        receipts = execute_cadence(
            decision,
            runner=lambda argv, *, timeout_seconds: {
                "exit_code": 5 if argv[-1] == "full_native_cycles" else 0
            },
        )

        report = build_progress_report(
            ledger=ledger(),
            phase2_ids=[],
            credit_records=credits,
            cadence_config=config(),
            cadence_decision=decision,
            cadence_receipts=receipts,
            project_state=state,
            integration_train_states=train_states(),
            registered_families=registered,
        )
        self.assertEqual(report["credited"], 0)
        self.assertEqual(report["remaining"], 802)
        self.assertIn("cadence_failure", {
            item["classification"] for item in report["blockers"]
        })

    def test_overlap_unknown_duplicate_and_non_802_ledger_fail_closed(self):
        registered, credit, decision, cadence = registered_and_credit()
        kwargs = {
            "ledger": ledger(),
            "phase2_ids": ["ND_0000"],
            "credit_records": [credit],
            "cadence_config": config(),
            "cadence_decision": decision,
            "cadence_receipts": cadence,
            "project_state": new_project_state(),
            "integration_train_states": train_states(),
            "registered_families": registered,
        }
        with self.assertRaises(ValueError):
            build_progress_report(**kwargs)
        with self.assertRaises(ValueError):
            build_progress_report(**{**kwargs, "phase2_ids": ["ND_unknown"]})
        with self.assertRaises(ValueError):
            build_progress_report(**{
                **kwargs,
                "ledger": build_ledger([{
                    "id": "only",
                    "category": "test",
                    "types": ["float"],
                    "source": "stdlib",
                }]),
                "phase2_ids": [],
            })

    def test_text_report_is_concise_and_contains_no_raw_evidence(self):
        registered, credit, decision, cadence = registered_and_credit()
        report = build_progress_report(
            ledger=ledger(),
            phase2_ids=[],
            credit_records=[credit],
            cadence_config=config(),
            cadence_decision=decision,
            cadence_receipts=cadence,
            project_state=new_project_state(),
            integration_train_states={
                "native_cycles": "integrated",
                "hydra_ovrtx": "idle",
                "blender_authoring": "idle",
            },
            registered_families=registered,
        )
        text = progress_report_text(report)
        self.assertIn("802 total", text)
        self.assertIn("8 credited", text)
        self.assertLess(len(text.splitlines()), 12)
        self.assertNotIn("role_evidence", text)
        self.assertNotIn("head_sha", text)
        with self.assertRaises(ValueError):
            progress_report_json({**report, "stdout": "private"})

    def test_rejects_forged_or_incomplete_cadence_authority_for_eight_nodes(self):
        registered, credits, state, decision = three_credit_boundary()
        canonical_config = config()
        receipts = execute_cadence(
            decision,
            runner=lambda argv, *, timeout_seconds: {"exit_code": 0},
        )
        base = {
            "ledger": ledger(),
            "phase2_ids": [],
            "credit_records": credits,
            "cadence_config": canonical_config,
            "cadence_decision": decision,
            "cadence_receipts": receipts,
            "project_state": state,
            "integration_train_states": train_states(),
            "registered_families": registered,
        }
        forged_id = copy.deepcopy(decision)
        forged_id["commands"][0]["command_id"] = "cadence-" + "f" * 24
        forged_argv = copy.deepcopy(decision)
        forged_argv["commands"][0]["argv"] = ["forged", "command"]
        forged_reason = copy.deepcopy(decision)
        forged_reason["reason"] = ["new_integrated_family", "caller_claim"]
        forged_generation = copy.deepcopy(decision)
        forged_generation["milestone_generation"] += 1
        omitted_full = copy.deepcopy(decision)
        omitted_full["commands"] = [
            command for command in omitted_full["commands"]
            if command["tier"] != "full"
        ]
        forged_receipt = copy.deepcopy(receipts)
        forged_receipt[0]["receipt_id"] = "cadence-receipt-" + "e" * 24
        missing_receipt = receipts[:-1]
        wrong_config = copy.deepcopy(canonical_config)
        wrong_config["full_suite_interval"] = 4

        cases = (
            {"cadence_decision": forged_id},
            {"cadence_decision": forged_argv},
            {"cadence_decision": forged_reason},
            {"cadence_decision": forged_generation},
            {"cadence_decision": omitted_full},
            {"cadence_receipts": forged_receipt},
            {"cadence_receipts": missing_receipt},
            {"cadence_config": wrong_config},
        )
        for mutation in cases:
            with self.subTest(mutation=tuple(mutation)), self.assertRaises(ValueError):
                build_progress_report(**{**base, **mutation})

    def test_worker_assignment_ownership_must_be_unique_active_and_batch_consistent(self):
        registered, credit, decision, receipts = registered_and_credit()
        active = update_horde_observation(
            new_project_state(),
            worker_states={
                "blend05": "active",
                "blendit04": "active",
                "blendit": "active",
                "blendit2": "active",
                "blendit3": "active",
            },
            evidence_receipt="horde-cycle-0001",
        )
        duplicate_worker = copy.deepcopy(active)
        duplicate_worker["assigned_batches"] = [
            {"worker_id": "blend05", "batch_id": "a"},
            {"worker_id": "blend05", "batch_id": "b"},
        ]
        duplicate_batch = copy.deepcopy(active)
        duplicate_batch["assigned_batches"] = [
            {"worker_id": "blend05", "batch_id": "a"},
            {"worker_id": "blendit04", "batch_id": "a"},
        ]
        inactive_owner = new_project_state()
        inactive_owner["assigned_batches"] = [
            {"worker_id": "blend05", "batch_id": "a"},
        ]
        completed_batch_owner = copy.deepcopy(active)
        completed_batch_owner["assigned_batches"] = [
            {"worker_id": "blend05", "batch_id": "credit-a"},
        ]
        active_without_assignment = copy.deepcopy(active)
        active_without_assignment["assigned_batches"] = [
            {"worker_id": worker, "batch_id": f"batch-{index}"}
            for index, worker in enumerate(
                ("blend05", "blendit04", "blendit", "blendit2")
            )
        ]
        for state in (
            duplicate_worker,
            duplicate_batch,
            inactive_owner,
            completed_batch_owner,
            active_without_assignment,
        ):
            with self.subTest(assignments=state["assigned_batches"]):
                with self.assertRaises(ValueError):
                    build_progress_report(
                        ledger=ledger(),
                        phase2_ids=[],
                        credit_records=[credit],
                        cadence_config=config(),
                        cadence_decision=decision,
                        cadence_receipts=receipts,
                        project_state=state,
                        integration_train_states=train_states(),
                        registered_families=registered,
                    )

    def test_cumulative_historical_audit_replays_persisted_and_new_credits(self):
        registered, credit_a, _, _ = registered_and_credit()
        credit_b = make_credit(registered, "credit-b", 8)
        state = new_project_state()
        state["integration_receipts"] = [{
            key: credit_a["integration_receipt"][key]
            for key in (
                "batch_id",
                "layer",
                "base_sha",
                "head_sha",
                "final_state",
            )
        }]
        replay = copy.deepcopy(state)
        replay["integration_receipts"] = []
        decision = build_cadence_decision(
            integrations=[
                {
                    "assignment": credit["assignment"],
                    "receipt": credit["integration_receipt"],
                }
                for credit in (credit_a, credit_b)
            ],
            project_state=replay,
            cadence_config=config(),
            registered_families=registered,
        )
        receipts = execute_cadence(
            decision,
            runner=lambda argv, *, timeout_seconds: {"exit_code": 0},
        )

        both = build_progress_report(
            ledger=ledger(),
            phase2_ids=[],
            credit_records=[credit_a, credit_b],
            cadence_config=config(),
            cadence_decision=decision,
            cadence_receipts=receipts,
            project_state=state,
            integration_train_states=train_states(),
            registered_families=registered,
        )
        self.assertEqual(both["credited"], 16)
        self.assertEqual(both["remaining"], 786)

        historical_decision = build_cadence_decision(
            integrations=[{
                "assignment": credit_a["assignment"],
                "receipt": credit_a["integration_receipt"],
            }],
            project_state=replay,
            cadence_config=config(),
            registered_families=registered,
        )
        historical_receipts = execute_cadence(
            historical_decision,
            runner=lambda argv, *, timeout_seconds: {"exit_code": 0},
        )
        historical = build_progress_report(
            ledger=ledger(),
            phase2_ids=[],
            credit_records=[credit_a],
            cadence_config=config(),
            cadence_decision=historical_decision,
            cadence_receipts=historical_receipts,
            project_state=state,
            integration_train_states=train_states(),
            registered_families=registered,
        )
        self.assertEqual(historical["credited"], 8)
        self.assertEqual(historical["remaining"], 794)

        with self.assertRaises(ValueError):
            build_progress_report(
                ledger=ledger(),
                phase2_ids=[],
                credit_records=[credit_b],
                cadence_config=config(),
                cadence_decision=build_cadence_decision(
                    integrations=[{
                        "assignment": credit_b["assignment"],
                        "receipt": credit_b["integration_receipt"],
                    }],
                    project_state=state,
                    cadence_config=config(),
                    registered_families=registered,
                ),
                cadence_receipts=execute_cadence(
                    build_cadence_decision(
                        integrations=[{
                            "assignment": credit_b["assignment"],
                            "receipt": credit_b["integration_receipt"],
                        }],
                        project_state=state,
                        cadence_config=config(),
                        registered_families=registered,
                    ),
                    runner=lambda argv, *, timeout_seconds: {"exit_code": 0},
                ),
                project_state=state,
                integration_train_states=train_states(),
                registered_families=registered,
            )

        mismatched_state = copy.deepcopy(state)
        mismatched_state["integration_receipts"][0]["head_sha"] = "d" * 40
        rejected_state = copy.deepcopy(state)
        rejected_state["integration_receipts"][0] = {
            **rejected_state["integration_receipts"][0],
            "final_state": "rejected",
            "failure_classification": "merge_failure",
        }
        for invalid_state in (mismatched_state, rejected_state):
            with self.subTest(receipt=invalid_state["integration_receipts"][0]):
                with self.assertRaises(ValueError):
                    build_progress_report(
                        ledger=ledger(),
                        phase2_ids=[],
                        credit_records=[credit_a],
                        cadence_config=config(),
                        cadence_decision=historical_decision,
                        cadence_receipts=historical_receipts,
                        project_state=invalid_state,
                        integration_train_states=train_states(),
                        registered_families=registered,
                    )


if __name__ == "__main__":
    unittest.main()
