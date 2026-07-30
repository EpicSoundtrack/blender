"""End-to-end proof for the canonical MaterialX delivery control plane.

The fixture deliberately uses only public production APIs.  Its fake backends
stand at process, Git-integration, cadence-runner, alert-transport, clock, and
state-store boundaries; no validator or scheduling decision is bypassed.
"""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from materialx_alert_sink import SanitizedAlertSink
from materialx_batch_scheduler import build_template_candidates
from materialx_completion_harvest import EXIT_PREFIX, parse_completion_evidence
from materialx_horde_dispatch import CommandResult, HordeBackend
from materialx_horde_operational import (
    HordeOperationalAdapter,
    run_operational_controller_cycle,
)
from materialx_horde_supervisor import (
    AtomicJSONStateStore,
    SupervisorConfig,
    run_supervisor,
)
from materialx_nodedef_ledger import build_ledger
from materialx_progress_report import build_progress_report
from materialx_project_state import (
    apply_lane_evidence,
    credit_integration,
    new_project_state,
    set_release_gate,
    update_horde_observation,
    validate_project_state,
)
from materialx_test_cadence import (
    build_cadence_decision,
    execute_cadence,
    validate_cadence_execution_receipts,
)
from materialx_velocity_manifest import (
    EXPECTED_HORDE_WORKERS,
    validate_batch_manifest,
)
from test_materialx_batch_scheduler import (
    call_schedule,
    make_inputs,
    make_ledger,
    registered_families,
)
from test_materialx_completion_harvest import evidence
from test_materialx_horde_dispatch_plan import make_manifest
from test_materialx_horde_operational import FakeDispatcher, FakeRunner
from test_materialx_integration_train import FakeIntegrationBackend
from test_materialx_test_cadence import config as cadence_config


WORKERS = tuple(EXPECTED_HORDE_WORKERS)
SOURCE_SHA = "a" * 40
LAYERS = ("native_cycles", "hydra_ovrtx", "blender_authoring")


def _shifted_inputs(offset: int) -> dict:
    inputs = make_inputs()
    old_ids = [f"ND_test_{index:04d}" for index in range(40)]
    new_ids = [f"ND_test_{index + offset:04d}" for index in range(40)]
    renamed = dict(zip(old_ids, new_ids))
    inputs["metadata"] = [
        {**row, "id": renamed[row["id"]]}
        for row in inputs["metadata"]
    ]
    inputs["registry"] = [
        {
            **row,
            "id": renamed[row["id"]],
            "family_id": (
                f"family-{int(row['family_id'].split('-')[-1]) + offset // 8}"
            ),
            "template_signature": {
                **row["template_signature"],
                "operation": (
                    f"operation-{int(row['family_id'].split('-')[-1]) + offset // 8}"
                ),
            },
        }
        for row in inputs["registry"]
    ]
    inputs["ledger"] = make_ledger(new_ids)
    inputs["files_allowlists"] = {
        worker: [f"intern/cycles/{worker}_{offset}.cpp"]
        for worker in WORKERS
    }
    return inputs


def _family_union(*inputs_documents: dict) -> dict:
    result = {}
    for inputs in inputs_documents:
        for family_id, records in registered_families(inputs).items():
            result[family_id] = copy.deepcopy(records)
    return result


def _completion(assignment: dict, head_character: str) -> dict:
    return {
        "schema_version": 2,
        "batch_id": assignment["batch_id"],
        "base_sha": assignment["integration_base_sha"],
        "head_sha": head_character * 40,
        "node_defs": list(assignment["node_defs"]),
        "rejected_node_defs": [],
        "changed_files": list(assignment["files_allowlist"]),
        "tests": [
            {
                "command": command,
                "passed": 1,
                "failed": 0,
                "exit_code": 0,
            }
            for command in assignment["focused_test_commands"]
        ],
        "review_verdict": "pass",
        "role_evidence": {
            "implementation": "IMPLEMENTATION-12345678",
            "generated_tests": "GENERATED_TESTS-12345678",
            "independent_review": "INDEPENDENT_REVIEW-12345678",
        },
    }


def _credit(artifact: dict, receipt: dict) -> dict:
    return {
        "ledger_delta": {
            "receipt_id": f"ledger-delta-{artifact['batch_id']}",
            "node_defs": list(artifact["assignment"]["node_defs"]),
        },
        "assignment": artifact["assignment"],
        "completion": artifact["completion"],
        "integration_receipt": receipt,
    }


def _canonical_ledger() -> dict:
    return build_ledger([
        {
            "id": f"ND_test_{index:04d}",
            "category": "math",
            "types": ["float"],
            "source": "libraries/stdlib/test.mtlx",
        }
        for index in range(802)
    ])


class _Clock:
    def __init__(self, value: float = 100.0):
        self.value = value

    def now(self) -> float:
        return self.value


class _Sleeper:
    def __init__(self):
        self.calls = []

    def sleep(self, seconds: float) -> None:
        self.calls.append(seconds)


class _Controller:
    def __init__(self, result: dict):
        self.result = result

    def run_cycle(self) -> dict:
        return copy.deepcopy(self.result)


class _Transport:
    def __init__(self, *, fail: bool = False):
        self.fail = fail
        self.messages = []

    def send(self, message):
        self.messages.append(copy.deepcopy(message))
        if self.fail:
            raise RuntimeError("transport unavailable")
        return f"receipt-{len(self.messages)}"


class MaterialXVelocityPipelineTest(unittest.TestCase):
    def _five_batch_fixture(self):
        inputs = make_inputs()
        schedule = call_schedule(inputs)
        families = registered_families(inputs)
        manifests = [
            copy.deepcopy(schedule["assignments"][worker])
            for worker in WORKERS
        ]
        for manifest, layer in zip(
            manifests,
            (
                "native_cycles",
                "hydra_ovrtx",
                "blender_authoring",
                "native_cycles",
                "hydra_ovrtx",
            ),
        ):
            manifest["layer"] = layer
            validate_batch_manifest(manifest, registered_families=families)
        return inputs, families, manifests

    def _healthy_end_to_end(self):
        completed_inputs = make_inputs()
        refill_inputs = _shifted_inputs(40)
        completed_schedule = call_schedule(completed_inputs)
        refill_schedule = call_schedule(refill_inputs)
        families = _family_union(completed_inputs, refill_inputs)
        completed = [
            copy.deepcopy(completed_schedule["assignments"][worker])
            for worker in WORKERS[:2]
        ]
        completed[0]["layer"] = "native_cycles"
        completed[1]["layer"] = "hydra_ovrtx"
        refills = [
            copy.deepcopy(refill_schedule["assignments"][worker])
            for worker in WORKERS
        ]
        for manifest in refills:
            manifest["batch_id"] = "refill-" + manifest["batch_id"]
        for manifest, layer in zip(
            refills,
            (
                "native_cycles",
                "hydra_ovrtx",
                "blender_authoring",
                "native_cycles",
                "hydra_ovrtx",
            ),
        ):
            manifest["layer"] = layer
        for manifest in (*completed, *refills):
            validate_batch_manifest(manifest, registered_families=families)

        backend = HordeBackend({
            worker: {"host": worker}
            for worker in WORKERS
        })
        harvest_dispatch_id = "dispatch-completed"
        runner = FakeRunner({
            backend.harvest_command(
                assignment["roles"]["implementation"],
                harvest_dispatch_id,
            ): CommandResult(
                0,
                evidence(_completion(assignment, chr(ord("b") + index))),
                "",
            )
            for index, assignment in enumerate(completed)
        })
        dispatcher = FakeDispatcher()
        adapter = HordeOperationalAdapter(
            backend=backend,
            runner=runner,
            dispatch_batch=dispatcher,
        )
        integration_backend = FakeIntegrationBackend()
        completed_workers = {
            assignment["roles"]["implementation"]: assignment
            for assignment in completed
        }
        workers = []
        for worker in WORKERS:
            assignment = completed_workers.get(worker)
            if assignment is None:
                workers.append({"id": worker, "state": "idle"})
            else:
                workers.append({
                    "id": worker,
                    "state": "active",
                    "batch_id": harvest_dispatch_id,
                    "assignment": assignment,
                })

        cycle = run_operational_controller_cycle(
            workers=workers,
            queued_batches=[
                {
                    "worker_id": manifest["roles"]["implementation"],
                    "manifest": manifest,
                }
                for manifest in refills
            ],
            registered_families=families,
            adapter=adapter,
            integration_backend=integration_backend,
        )
        state = update_horde_observation(
            new_project_state(),
            worker_states={worker: "active" for worker in WORKERS},
            evidence_receipt="horde-cycle-e2e",
        )
        state["assigned_batches"] = copy.deepcopy(cycle["assigned_batches"])
        state["integration_receipts"] = [
            {
                key: receipt[key]
                for key in (
                    "batch_id",
                    "layer",
                    "base_sha",
                    "head_sha",
                    "final_state",
                )
            }
            for receipt in cycle["integration_receipts"]
        ]
        state = validate_project_state(state)
        integrations = [
            {"assignment": artifact["assignment"], "receipt": receipt}
            for artifact, receipt in zip(
                cycle["artifacts"],
                cycle["integration_receipts"],
            )
        ]
        replay_state = copy.deepcopy(state)
        replay_state["integration_receipts"] = []
        replay_state = validate_project_state(replay_state)
        decision = build_cadence_decision(
            integrations=integrations,
            project_state=replay_state,
            cadence_config=cadence_config(),
            registered_families=families,
        )
        cadence_receipts = execute_cadence(
            decision,
            runner=lambda argv, *, timeout_seconds: {"exit_code": 0},
        )
        credits = [
            _credit(artifact, receipt)
            for artifact, receipt in zip(
                cycle["artifacts"],
                cycle["integration_receipts"],
            )
        ]
        report = build_progress_report(
            ledger=_canonical_ledger(),
            phase2_ids=[
                f"ND_test_{index:04d}" for index in range(792, 802)
            ],
            credit_records=credits,
            cadence_config=cadence_config(),
            cadence_decision=decision,
            cadence_receipts=cadence_receipts,
            project_state=state,
            integration_train_states={
                "native_cycles": "integrated",
                "hydra_ovrtx": "integrated",
                "blender_authoring": "idle",
            },
            registered_families=families,
        )
        return {
            "cycle": cycle,
            "dispatcher": dispatcher,
            "families": families,
            "refills": refills,
            "decision": decision,
            "cadence_receipts": cadence_receipts,
            "report": report,
            "state": state,
        }

    def test_acceptance_01_exact_five_workers_and_canonical_family_batches(self):
        _, families, manifests = self._five_batch_fixture()

        self.assertEqual(len(manifests), 5)
        self.assertEqual(
            {manifest["roles"]["implementation"] for manifest in manifests},
            set(WORKERS),
        )
        self.assertTrue(all(len(manifest["node_defs"]) == 8 for manifest in manifests))
        self.assertTrue(all(
            manifest["worker_source_sha"] == SOURCE_SHA
            and manifest["integration_base_sha"] == SOURCE_SHA
            for manifest in manifests
        ))
        self.assertEqual(
            {manifest["layer"] for manifest in manifests},
            set(LAYERS),
        )
        for manifest in manifests:
            self.assertEqual(
                validate_batch_manifest(
                    manifest, registered_families=families
                ),
                manifest,
            )

    def test_acceptance_02_scheduler_rejects_stale_source_and_phase2_overlap(self):
        inputs = make_inputs()
        stale_shas = dict(inputs["probed_worker_shas"])
        stale_shas[WORKERS[0]] = "b" * 40
        with self.assertRaisesRegex(ValueError, "integration_base_sha"):
            call_schedule(inputs, probed_worker_shas=stale_shas)

        node_id = inputs["metadata"][0]["id"]
        with self.assertRaisesRegex(ValueError, "Phase-2"):
            build_template_candidates(
                inputs["ledger"],
                inputs["registry"],
                inputs["metadata"],
                phase2_ids=[node_id],
            )

        _, families, manifests = self._five_batch_fixture()
        backend = HordeBackend({
            worker: {"host": worker}
            for worker in WORKERS
        })
        stale_worker = WORKERS[-1]
        stale_probe = CommandResult(
            0,
            json.dumps({
                "repository_present": True,
                "files": {
                    "intern/cycles/scene/materialx.cpp": True,
                    "tools/materialx/materialx_velocity_manifest.py": True,
                    "tools/materialx/materialx_batch_scheduler.py": True,
                },
                "head": "b" * 40,
            }, separators=(",", ":")),
            "",
        )
        results = {
            backend.source_preflight_command(stale_worker): [
                stale_probe,
                stale_probe,
            ],
            **{
                backend.process_command(worker): [
                    CommandResult(0, "absent", ""),
                    CommandResult(0, f"active:{900 + index}", ""),
                ]
                for index, worker in enumerate(WORKERS)
                if worker != stale_worker
            },
        }
        with tempfile.TemporaryDirectory() as directory:
            credentials = Path(directory) / "credentials.env"
            credentials.write_text(
                "NVIDIA_API_KEY=private-credential\n",
                encoding="utf-8",
            )
            runner = FakeRunner(results)
            adapter = HordeOperationalAdapter.with_dispatcher(
                backend=backend,
                credential_file=credentials,
                registered_families=families,
                runner=runner,
            )
            result = run_operational_controller_cycle(
                workers=[
                    {"id": worker, "state": "idle"}
                    for worker in WORKERS
                ],
                queued_batches=[
                    {
                        "worker_id": manifest["roles"]["implementation"],
                        "manifest": manifest,
                    }
                    for manifest in manifests
                ],
                registered_families=families,
                adapter=adapter,
            )

        launches = [
            command for command in runner.calls
            if command[-1].startswith("nohup ")
        ]
        self.assertEqual(len(launches), 4)
        self.assertNotIn(stale_worker, {command[-2] for command in launches})
        self.assertEqual(
            next(
                worker["state"]
                for worker in result["workers"]
                if worker["id"] == stale_worker
            ),
            "blocked",
        )
        self.assertEqual(len(result["assigned_batches"]), 2)

    def test_acceptance_03_combined_dispatch_launches_each_worker_once(self):
        fixture = self._healthy_end_to_end()
        cycle = fixture["cycle"]

        self.assertEqual(len(fixture["dispatcher"].calls), 1)
        self.assertEqual(len(fixture["dispatcher"].calls[0]), 5)
        self.assertEqual(
            {
                role_worker
                for manifest in fixture["dispatcher"].calls[0]
                for role_worker in manifest["roles"].values()
            },
            set(WORKERS),
        )
        self.assertEqual(len(cycle["assigned_batches"]), 5)
        self.assertTrue(all(worker["state"] == "active" for worker in cycle["workers"]))

    def test_acceptance_04_two_completions_integrate_and_refill_same_cycle(self):
        fixture = self._healthy_end_to_end()
        cycle = fixture["cycle"]

        self.assertEqual(cycle["queue_depth"], 5)
        self.assertEqual(len(cycle["artifacts"]), 2)
        self.assertEqual(len(cycle["integration_receipts"]), 2)
        self.assertTrue(all(
            receipt["final_state"] == "integrated"
            for receipt in cycle["integration_receipts"]
        ))
        self.assertEqual(
            {receipt["layer"] for receipt in cycle["integration_receipts"]},
            {"native_cycles", "hydra_ovrtx"},
        )
        self.assertEqual(len(cycle["assigned_batches"]), 5)

    def test_acceptance_05_invalid_completion_never_reaches_credit(self):
        self.assertEqual(
            parse_completion_evidence(f"{EXIT_PREFIX}0"),
            {"classification": "invalid_completion"},
        )
        manifest = make_manifest("minimal-rejected")
        with self.assertRaises(ValueError):
            validate_batch_manifest(
                {"batch_id": manifest["batch_id"], "prompt": "implement nodes"},
                registered_families={"add": []},
            )

    def test_acceptance_06_cadence_is_generation_bound_before_credit(self):
        fixture = self._healthy_end_to_end()
        decision = fixture["decision"]
        receipts = fixture["cadence_receipts"]

        self.assertEqual(
            {receipt["milestone_generation"] for receipt in receipts},
            {decision["milestone_generation"]},
        )
        self.assertEqual(
            validate_cadence_execution_receipts(decision, receipts),
            receipts,
        )
        forged = copy.deepcopy(receipts)
        forged[0]["milestone_generation"] += 1
        with self.assertRaises(ValueError):
            validate_cadence_execution_receipts(decision, forged)

    def test_acceptance_07_progress_credits_only_correlated_green_nodes(self):
        fixture = self._healthy_end_to_end()
        report = fixture["report"]

        self.assertEqual(report["total"], 802)
        self.assertEqual(report["credited"], 16)
        self.assertEqual(report["remaining"], 776)
        self.assertEqual(report["phase2"], 10)
        self.assertEqual(report["cadence"]["state"], "green")
        self.assertEqual(
            report["evidence_tier_counts"]["completion_manifest_v2"],
            16,
        )

    def test_acceptance_08_gpu_and_golden_lanes_remain_independent_of_horde(self):
        state = credit_integration(
            new_project_state(),
            newly_integrated_nodedefs=128,
            render_path_edit=False,
            batch_id="milestone-128",
        )
        state = set_release_gate(state, due=True)
        before = copy.deepcopy(state["lanes"])
        observed = update_horde_observation(
            state,
            worker_states={
                **{worker: "active" for worker in WORKERS},
                WORKERS[-1]: "blocked",
            },
            evidence_receipt="horde-stale-worker",
            reason="probe_failure",
        )

        for lane in (
            "local_cpu",
            "local_cuda",
            "windows_a40_cuda",
            "golden_review",
        ):
            self.assertEqual(observed["lanes"][lane], before[lane])
        self.assertEqual(observed["lanes"]["horde"]["state"], "degraded")

    def test_acceptance_09_lane_evidence_is_current_generation_and_distinct(self):
        state = credit_integration(
            new_project_state(),
            newly_integrated_nodedefs=128,
            render_path_edit=False,
            batch_id="milestone-128",
        )
        state = set_release_gate(state, due=True)
        generation = state["milestones"]["generation"]
        evidence_types = {
            "local_cpu": "local_cpu_render",
            "local_cuda": "local_cuda_render",
            "windows_a40_cuda": "windows_a40_cuda_render",
            "golden_review": "golden_review",
        }
        for lane, evidence_type in evidence_types.items():
            state = apply_lane_evidence(state, {
                "schema_version": 1,
                "receipt_id": f"{lane}-generation-{generation}",
                "lane": lane,
                "evidence_type": evidence_type,
                "milestone_generation": generation,
                "numeric_exits": [0],
            })
        self.assertTrue(all(
            state["lanes"][lane]["state"] == "green"
            for lane in evidence_types
        ))
        self.assertEqual(
            len({
                state["lanes"][lane]["last_evidence_id"]
                for lane in evidence_types
            }),
            4,
        )

    def test_acceptance_10_alert_delivery_failure_is_visible_and_nonblocking(self):
        controller_result = {
            "workers": [
                {
                    "id": worker,
                    "state": "blocked" if worker == WORKERS[-1] else "active",
                }
                for worker in WORKERS
            ],
            "assigned_batches": [],
            "integration_receipts": [],
            "alerts": [{
                "worker_id": WORKERS[-1],
                "classification": "source_preflight_failure",
            }],
            "queue_depth": 5,
            "horde_evidence_receipt": "horde-e2e-stale",
        }
        transport = _Transport(fail=True)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "state.json"
            exit_code = run_supervisor(
                SupervisorConfig(1.0, queue_watermark=5),
                controller=_Controller(controller_result),
                state_store=AtomicJSONStateStore(path),
                clock=_Clock(),
                sleeper=_Sleeper(),
                alert_sink=SanitizedAlertSink(transport),
                once=True,
            )
            persisted = json.loads(path.read_text(encoding="utf-8"))

        self.assertEqual(exit_code, 1)
        self.assertTrue(transport.messages)
        self.assertTrue(any(
            record["delivery_state"] == "unsent"
            for record in persisted["alerts"]
        ))
        self.assertIn("transport_failure", persisted["failure_classifications"])
        self.assertEqual(
            persisted["lanes"]["horde"]["state"],
            "degraded",
        )

    def test_acceptance_11_state_lock_rejects_noncanonical_journal_rewrite(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "state.json"
            store = AtomicJSONStateStore(path)
            original = update_horde_observation(
                new_project_state(),
                worker_states={worker: "active" for worker in WORKERS},
                evidence_receipt="canonical-original",
            )
            store.commit(original)
            original_bytes = path.read_bytes()
            forged = copy.deepcopy(original)
            forged["semantic_journal"][0]["reason"] = "probe_failure"

            with self.assertRaises(ValueError):
                store.commit(forged)
            self.assertEqual(path.read_bytes(), original_bytes)

    def test_acceptance_12_queue_exhaustion_is_a_persisted_blocker(self):
        controller_result = {
            "workers": [
                {"id": worker, "state": "active"}
                for worker in WORKERS
            ],
            "assigned_batches": [],
            "integration_receipts": [],
            "alerts": [],
            "queue_depth": 0,
            "horde_evidence_receipt": "horde-e2e-queue-empty",
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "state.json"
            exit_code = run_supervisor(
                SupervisorConfig(1.0, queue_watermark=5),
                controller=_Controller(controller_result),
                state_store=AtomicJSONStateStore(path),
                clock=_Clock(),
                sleeper=_Sleeper(),
                once=True,
            )
            persisted = json.loads(path.read_text(encoding="utf-8"))

        self.assertEqual(exit_code, 1)
        self.assertIn("queue_empty", persisted["failure_classifications"])
        self.assertEqual(persisted["queue_depth"], 0)


if __name__ == "__main__":
    unittest.main()
