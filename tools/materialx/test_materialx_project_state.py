#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import copy
import json
from pathlib import Path
import tempfile
import unittest

import materialx_project_state as project_state


WORKERS = ("blend05", "blendit04", "blendit", "blendit2", "blendit3")


class MaterialXProjectStateTest(unittest.TestCase):
    def test_new_state_is_strict_deterministic_and_has_independent_lanes(self):
        state = project_state.new_project_state(
            local_green_threshold=32,
            windows_green_threshold=128,
        )

        validated = project_state.validate_project_state(state)

        self.assertEqual(validated, state)
        self.assertEqual([item["id"] for item in state["workers"]], sorted(WORKERS))
        self.assertTrue(all(item["state"] == "unknown" for item in state["workers"]))
        self.assertEqual(
            set(state["lanes"]),
            {"horde", "local_cpu", "local_cuda", "windows_a40_cuda", "golden_review"},
        )
        self.assertEqual(
            project_state.serialize_project_state(state),
            json.dumps(state, allow_nan=False, indent=2, sort_keys=True) + "\n",
        )

    def test_rejects_missing_extra_duplicate_workers_and_competing_fields(self):
        state = project_state.new_project_state()
        cases = []
        missing = copy.deepcopy(state)
        missing["workers"].pop()
        cases.append(missing)
        extra = copy.deepcopy(state)
        extra["workers"].append({"id": "rogue", "state": "unknown", "last_evidence_id": ""})
        cases.append(extra)
        duplicate = copy.deepcopy(state)
        duplicate["workers"][-1]["id"] = duplicate["workers"][0]["id"]
        cases.append(duplicate)
        competing = copy.deepcopy(state)
        competing["healthy_workers"] = []
        cases.append(competing)

        for document in cases:
            with self.subTest(document=document):
                with self.assertRaises(ValueError):
                    project_state.validate_project_state(document)

    def test_rejects_contradictory_health_unknown_failures_and_free_form_receipts(self):
        state = project_state.new_project_state()
        state["healthy"] = True
        with self.assertRaises(ValueError):
            project_state.validate_project_state(state)

        state = project_state.new_project_state()
        state["failure_classifications"] = ["future_unbounded_failure"]
        with self.assertRaises(ValueError):
            project_state.validate_project_state(state)

        state = project_state.new_project_state()
        state["integration_receipts"] = [{"arbitrary": "payload"}]
        with self.assertRaises(ValueError):
            project_state.validate_project_state(state)

    def test_horde_observation_changes_only_horde_fields_and_journals_transitions(self):
        state = project_state.new_project_state()
        before_lanes = copy.deepcopy(state["lanes"])

        updated = project_state.update_horde_observation(
            state,
            worker_states={worker: "active" for worker in WORKERS},
            evidence_receipt="horde-cycle-0001",
        )

        self.assertEqual(updated["lanes"]["horde"], {
            "state": "active",
            "last_evidence_id": "horde-cycle-0001",
        })
        self.assertEqual(
            {key: updated["lanes"][key] for key in before_lanes if key != "horde"},
            {key: before_lanes[key] for key in before_lanes if key != "horde"},
        )
        self.assertTrue(all(
            worker["state"] == "active"
            and worker["last_evidence_id"] == "horde-cycle-0001"
            for worker in updated["workers"]
        ))
        self.assertEqual(
            [record["sequence"] for record in updated["semantic_journal"]],
            list(range(1, len(updated["semantic_journal"]) + 1)),
        )
        self.assertEqual(state["semantic_journal"], [])

    def test_milestones_make_matching_lanes_due_without_claiming_green(self):
        state = project_state.new_project_state(
            local_green_threshold=32,
            windows_green_threshold=64,
        )

        local_due = project_state.credit_integration(
            state,
            newly_integrated_nodedefs=32,
            render_path_edit=False,
            batch_id="batch-local",
        )
        windows_due = project_state.credit_integration(
            local_due,
            newly_integrated_nodedefs=32,
            render_path_edit=False,
            batch_id="batch-windows",
        )

        self.assertEqual(local_due["lanes"]["local_cpu"]["state"], "due")
        self.assertEqual(local_due["lanes"]["local_cuda"]["state"], "due")
        self.assertEqual(local_due["lanes"]["windows_a40_cuda"]["state"], "not_due")
        self.assertEqual(windows_due["lanes"]["windows_a40_cuda"]["state"], "due")
        self.assertEqual(windows_due["lanes"]["golden_review"]["state"], "not_due")
        self.assertEqual(windows_due["milestones"]["generation"], 2)

    def test_render_edit_makes_local_lanes_due_and_release_gate_alone_makes_golden_due(self):
        state = project_state.new_project_state()
        render_due = project_state.credit_integration(
            state,
            newly_integrated_nodedefs=0,
            render_path_edit=True,
            batch_id="render-path",
        )
        release_due = project_state.set_release_gate(render_due, due=True)

        self.assertEqual(render_due["lanes"]["local_cpu"]["state"], "due")
        self.assertEqual(render_due["lanes"]["local_cuda"]["state"], "due")
        self.assertEqual(render_due["lanes"]["golden_review"]["state"], "not_due")
        self.assertEqual(release_due["lanes"]["golden_review"]["state"], "due")

    def test_lane_green_requires_exact_current_generation_zero_exit_receipt(self):
        state = project_state.credit_integration(
            project_state.new_project_state(),
            newly_integrated_nodedefs=32,
            render_path_edit=False,
            batch_id="due",
        )
        receipt = {
            "schema_version": 1,
            "receipt_id": "cpu-smoke-0032",
            "lane": "local_cpu",
            "evidence_type": "local_cpu_render",
            "milestone_generation": state["milestones"]["generation"],
            "numeric_exits": [0, 0],
        }

        green = project_state.apply_lane_evidence(state, receipt)

        self.assertEqual(green["lanes"]["local_cpu"], {
            "state": "green",
            "last_evidence_id": "cpu-smoke-0032",
        })
        self.assertEqual(green["lanes"]["local_cuda"]["state"], "due")
        for mutation in (
            {"lane": "local_cuda"},
            {"evidence_type": "local_cuda_render"},
            {"milestone_generation": state["milestones"]["generation"] - 1},
            {"numeric_exits": [0, 1]},
        ):
            invalid = {**receipt, **mutation}
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                project_state.apply_lane_evidence(state, invalid)

    def test_lane_evidence_generation_rejects_boole_at_zero_and_one(self):
        generation_zero = project_state.new_project_state()
        generation_zero["lanes"]["local_cpu"] = {
            "state": "due",
            "last_evidence_id": "",
        }
        generation_one = project_state.credit_integration(
            project_state.new_project_state(),
            newly_integrated_nodedefs=32,
            render_path_edit=False,
            batch_id="generation-one",
        )
        for state, boolean_generation in (
            (generation_zero, False),
            (generation_one, True),
        ):
            with self.subTest(generation=state["milestones"]["generation"]):
                with self.assertRaises(ValueError):
                    project_state.apply_lane_evidence(state, {
                        "schema_version": 1,
                        "receipt_id": "typed-generation",
                        "lane": "local_cpu",
                        "evidence_type": "local_cpu_render",
                        "milestone_generation": boolean_generation,
                        "numeric_exits": [0],
                    })

    def test_lane_evidence_schema_version_requires_exact_integer(self):
        state = project_state.new_project_state()
        state["lanes"]["local_cpu"] = {
            "state": "due",
            "last_evidence_id": "",
        }
        for invalid_version in (True, 1.0):
            with self.subTest(schema_version=invalid_version), self.assertRaises(ValueError):
                project_state.apply_lane_evidence(state, {
                    "schema_version": invalid_version,
                    "receipt_id": "typed-schema-version",
                    "lane": "local_cpu",
                    "evidence_type": "local_cpu_render",
                    "milestone_generation": 0,
                    "numeric_exits": [0],
                })

    def test_semantic_journal_rejects_reordered_duplicate_rewritten_and_truncated_prefixes(self):
        state = project_state.update_horde_observation(
            project_state.new_project_state(),
            worker_states={worker: "active" for worker in WORKERS},
            evidence_receipt="cycle-1",
        )
        original = copy.deepcopy(state)

        for mutated in (
            {**state, "semantic_journal": list(reversed(state["semantic_journal"]))},
            {**state, "semantic_journal": state["semantic_journal"] + [state["semantic_journal"][-1]]},
        ):
            with self.subTest(mutated=mutated), self.assertRaises(ValueError):
                project_state.validate_project_state(mutated)

        rewritten = copy.deepcopy(state)
        rewritten["semantic_journal"][0]["reason"] = "probe_failure"
        with self.assertRaises(ValueError):
            project_state.assert_journal_extension(original, rewritten)
        with self.assertRaises(ValueError):
            project_state.assert_journal_extension(original, {
                **original,
                "semantic_journal": original["semantic_journal"][:-1],
            })

    def test_schema_v1_migration_is_one_way_preserves_records_and_never_invents_green(self):
        legacy = {
            "schema_version": 1,
            "healthy_workers": [
                {"id": "blend05", "state": "active"},
                {"id": "blendit", "state": "exited"},
            ],
            "completed_rows": ["ND_a"],
            "evidence_records": [{"row_id": "ND_a", "record": "evidence/a.json"}],
            "journal_records": [{"row_id": "ND_a", "record": "journal/a.md"}],
            "lanes": {"windows_local_build": {"state": "ready", "alerted": False}},
            "capacity_journal": [],
            "alerts": [],
        }

        migrated = project_state.migrate_project_state(legacy)

        self.assertEqual(migrated["schema_version"], project_state.SCHEMA_VERSION)
        self.assertEqual(migrated["completed_rows"], ["ND_a"])
        self.assertEqual(migrated["evidence_records"], legacy["evidence_records"])
        self.assertEqual(migrated["journal_records"], legacy["journal_records"])
        self.assertEqual(
            next(item for item in migrated["workers"] if item["id"] == "blend05")["state"],
            "unknown",
        )
        self.assertEqual(
            next(item for item in migrated["workers"] if item["id"] == "blendit")["state"],
            "exited",
        )
        self.assertEqual(migrated["lanes"]["windows_a40_cuda"]["state"], "due")
        self.assertNotIn("windows_local_build", migrated["lanes"])
        with self.assertRaises(ValueError):
            project_state.migrate_project_state(migrated)

    def test_schema_v1_migration_preserves_evidenced_categorical_probe_history(self):
        legacy = {
            "schema_version": 1,
            "healthy_workers": [{"id": "blend05", "state": "active"}],
            "completed_rows": [],
            "evidence_records": [],
            "journal_records": [],
            "lanes": {"windows_local_build": {"state": "blocked", "alerted": True}},
            "capacity_journal": [
                {
                    "kind": "windows_local_build",
                    "subject": "windows_local_build",
                    "state": "blocked",
                    "log_observed": True,
                },
                {
                    "kind": "worker_process",
                    "subject": "blend05",
                    "state": "active",
                    "log_observed": True,
                },
            ],
            "alerts": [],
        }

        migrated = project_state.migrate_project_state(legacy)

        worker = next(item for item in migrated["workers"] if item["id"] == "blend05")
        self.assertEqual(worker["state"], "active")
        self.assertTrue(worker["last_evidence_id"].startswith("legacy-"))
        self.assertEqual(migrated["lanes"]["windows_a40_cuda"]["state"], "failed")
        self.assertEqual(len(migrated["semantic_journal"]), 2)

    def test_v1_migration_tracks_repeated_worker_and_windows_transitions(self):
        legacy = {
            "schema_version": 1,
            "healthy_workers": [{"id": "blend05", "state": "exited"}],
            "completed_rows": [],
            "evidence_records": [],
            "journal_records": [],
            "lanes": {"windows_local_build": {"state": "blocked", "alerted": True}},
            "capacity_journal": [
                {
                    "kind": "worker_process",
                    "subject": "blend05",
                    "state": "active",
                    "log_observed": True,
                },
                {
                    "kind": "worker_process",
                    "subject": "blend05",
                    "state": "exited",
                    "log_observed": True,
                },
                {
                    "kind": "windows_local_build",
                    "subject": "windows_local_build",
                    "state": "ready",
                    "log_observed": True,
                },
                {
                    "kind": "windows_local_build",
                    "subject": "windows_local_build",
                    "state": "blocked",
                    "log_observed": True,
                },
            ],
            "alerts": [],
        }

        migrated = project_state.migrate_project_state(legacy)
        transitions = [
            (
                item["subject"],
                item["previous_state"],
                item["new_state"],
            )
            for item in migrated["semantic_journal"]
        ]

        self.assertEqual(transitions, [
            ("worker:blend05", "unknown", "active"),
            ("worker:blend05", "active", "exited"),
            ("lane:windows_a40_cuda", "unknown", "due"),
            ("lane:windows_a40_cuda", "due", "failed"),
        ])

    def test_validator_rejects_invented_golden_and_horde_cross_field_states(self):
        golden_due = project_state.new_project_state()
        golden_due["lanes"]["golden_review"] = {
            "state": "due",
            "last_evidence_id": "",
        }
        golden_green = copy.deepcopy(golden_due)
        golden_green["lanes"]["golden_review"] = {
            "state": "green",
            "last_evidence_id": "invented-golden",
        }

        horde_active = project_state.new_project_state()
        horde_active["lanes"]["horde"] = {
            "state": "active",
            "last_evidence_id": "cycle-1",
        }
        horde_active["workers"][0] = {
            "id": horde_active["workers"][0]["id"],
            "state": "idle",
            "last_evidence_id": "cycle-1",
        }

        incoherent_evidence = project_state.update_horde_observation(
            project_state.new_project_state(),
            worker_states={worker: "active" for worker in WORKERS},
            evidence_receipt="cycle-coherent",
        )
        incoherent_evidence["workers"][0]["last_evidence_id"] = "cycle-other"

        active_workers_unknown_lane = copy.deepcopy(incoherent_evidence)
        active_workers_unknown_lane["workers"][0]["last_evidence_id"] = "cycle-coherent"
        active_workers_unknown_lane["lanes"]["horde"] = {
            "state": "unknown",
            "last_evidence_id": "",
        }
        active_workers_unknown_lane["healthy"] = False

        for state in (
            golden_due,
            golden_green,
            horde_active,
            incoherent_evidence,
            active_workers_unknown_lane,
        ):
            with self.subTest(state=state), self.assertRaises(ValueError):
                project_state.validate_project_state(state)

    def test_write_project_state_rejects_rewritten_or_truncated_existing_journal(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "state.json"
            original = project_state.update_horde_observation(
                project_state.new_project_state(),
                worker_states={worker: "active" for worker in WORKERS},
                evidence_receipt="cycle-original",
            )
            project_state.write_project_state(path, original)
            original_bytes = path.read_bytes()

            rewritten = copy.deepcopy(original)
            rewritten["semantic_journal"][0]["reason"] = "probe_failure"
            truncated = {
                **copy.deepcopy(original),
                "semantic_journal": original["semantic_journal"][:-1],
            }
            for candidate in (rewritten, truncated):
                with self.subTest(candidate=candidate):
                    with self.assertRaises(ValueError):
                        project_state.write_project_state(path, candidate)
                    self.assertEqual(path.read_bytes(), original_bytes)

    def test_write_project_state_rejects_invalid_existing_document_without_replacement(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "state.json"
            path.write_text('{"invalid":true}\n', encoding="utf-8")
            original_bytes = path.read_bytes()

            with self.assertRaises(ValueError):
                project_state.write_project_state(
                    path,
                    project_state.new_project_state(),
                )

            self.assertEqual(path.read_bytes(), original_bytes)

    def test_write_project_state_migrates_valid_v1_before_extension_check(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "state.json"
            legacy = {
                "schema_version": 1,
                "healthy_workers": [],
                "completed_rows": [],
                "evidence_records": [],
                "journal_records": [],
                "lanes": {
                    "windows_local_build": {
                        "state": "unknown",
                        "alerted": False,
                    },
                },
                "capacity_journal": [],
                "alerts": [],
            }
            path.write_text(json.dumps(legacy), encoding="utf-8")

            project_state.write_project_state(
                path,
                project_state.migrate_project_state(legacy),
            )

            self.assertEqual(
                project_state.load_project_state(path),
                project_state.migrate_project_state(legacy),
            )

    def test_serializer_rejects_secret_like_payload_values(self):
        state = project_state.new_project_state()
        state["evidence_records"] = [{
            "row_id": "ND_a",
            "record": "NVIDIA_API_KEY=must-not-persist",
        }]
        with self.assertRaises(ValueError):
            project_state.serialize_project_state(state)

    def test_v1_migration_rejects_unknown_worker_identity(self):
        legacy = {
            "schema_version": 1,
            "healthy_workers": [{"id": "rogue", "state": "active"}],
            "completed_rows": [],
            "evidence_records": [],
            "journal_records": [],
            "lanes": {"windows_local_build": {"state": "ready", "alerted": False}},
            "capacity_journal": [],
            "alerts": [],
        }
        with self.assertRaises(ValueError):
            project_state.migrate_project_state(legacy)

    def test_atomic_persist_validates_and_round_trips(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "state.json"
            state = project_state.new_project_state()
            project_state.write_project_state(path, state)
            self.assertEqual(project_state.load_project_state(path), state)


if __name__ == "__main__":
    unittest.main()
