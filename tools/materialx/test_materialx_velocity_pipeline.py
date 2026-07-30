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
from materialx_batch_scheduler import (
    build_batch_schedule,
    build_template_candidates,
)
from materialx_completion_harvest import (
    COMPLETION_PREFIX,
    EXIT_PREFIX,
    parse_completion_evidence,
)
from materialx_horde_dispatch import CommandResult, HordeBackend
from materialx_horde_operational import (
    HordeOperationalAdapter,
    run_operational_supervisor,
)
from materialx_horde_supervisor import (
    AtomicJSONStateStore,
    SupervisorConfig,
)
from materialx_nodedef_ledger import build_ledger
from materialx_progress_report import build_progress_report
from materialx_project_state import (
    apply_lane_evidence,
    credit_integration,
    new_project_state,
    set_release_gate,
    update_horde_observation,
)
from materialx_test_cadence import (
    validate_cadence_execution_receipts,
)
from materialx_velocity_manifest import (
    EXPECTED_HORDE_WORKERS,
    validate_batch_manifest,
)


WORKERS = tuple(EXPECTED_HORDE_WORKERS)
SOURCE_SHA = "a" * 40
LAYERS = ("native_cycles", "hydra_ovrtx", "blender_authoring")


def _signature(index: int) -> dict:
    return {
        "operation": f"operation-{index}",
        "input_types": ["float"],
        "output_type": "float",
        "broadcast_policy": "none",
        "output_socket_class": "float",
    }


def _make_ledger(template_ids) -> dict:
    return build_ledger(
        [
            {
                "id": f"ND_test_{index:04d}",
                "category": "math",
                "types": ["float"],
                "source": "libraries/stdlib/test.mtlx",
            }
            for index in range(802)
        ],
        {
            "schema_version": 1,
            "rows": {
                node_id: {"next_action": "template"}
                for node_id in template_ids
            },
        },
    )


def _make_inputs(offset: int = 0) -> dict:
    node_ids = [
        f"ND_test_{index:04d}"
        for index in range(offset, offset + 40)
    ]
    roles = {
        "blend05": {
            "implementation": "blend05",
            "generated_tests": "blendit04",
            "independent_review": "blendit",
        },
        "blendit04": {
            "implementation": "blendit04",
            "generated_tests": "blendit",
            "independent_review": "blendit2",
        },
        "blendit": {
            "implementation": "blendit",
            "generated_tests": "blendit2",
            "independent_review": "blendit3",
        },
        "blendit2": {
            "implementation": "blendit2",
            "generated_tests": "blendit3",
            "independent_review": "blend05",
        },
        "blendit3": {
            "implementation": "blendit3",
            "generated_tests": "blend05",
            "independent_review": "blendit04",
        },
    }
    return {
        "ledger": _make_ledger(node_ids),
        "registry": [
            {
                "id": node_id,
                "template": "unary_componentwise",
                "types": ["float"],
                "input_order": ["in"],
                "broadcast": False,
                "family_id": f"family-{(index + offset) // 8}",
                "template_signature": _signature((index + offset) // 8),
            }
            for index, node_id in enumerate(node_ids)
        ],
        "metadata": [
            {
                "id": node_id,
                "classification": "direct_template",
                "next_action": "template",
            }
            for node_id in node_ids
        ],
        "capacity": {
            "healthy_workers": [
                {"id": worker, "state": "active"}
                for worker in WORKERS
            ],
        },
        "integration_base_sha": SOURCE_SHA,
        "probed_worker_shas": {
            worker: SOURCE_SHA for worker in WORKERS
        },
        "layer": "native_cycles",
        "role_allocations": roles,
        "files_allowlists": {
            worker: [f"intern/cycles/{worker}_{offset}.cpp"]
            for worker in WORKERS
        },
        "completed_ids": [],
        "phase2_ids": [],
        "active_manifests": [],
    }


def _schedule(inputs: dict, **overrides) -> dict:
    arguments = {
        "integration_base_sha": inputs["integration_base_sha"],
        "probed_worker_shas": inputs["probed_worker_shas"],
        "layer": inputs["layer"],
        "role_allocations": inputs["role_allocations"],
        "files_allowlists": inputs["files_allowlists"],
        "completed_ids": inputs["completed_ids"],
        "phase2_ids": inputs["phase2_ids"],
        "active_manifests": inputs["active_manifests"],
    }
    arguments.update(overrides)
    return build_batch_schedule(
        inputs["ledger"],
        inputs["registry"],
        inputs["metadata"],
        inputs["capacity"],
        **arguments,
    )


def _registered_families(inputs: dict) -> dict:
    families = {}
    for row in inputs["registry"]:
        contracts = families.setdefault(row["family_id"], [])
        contract = next(
            (
                value
                for value in contracts
                if value["template_signature"]
                == row["template_signature"]
            ),
            None,
        )
        if contract is None:
            contract = {
                "template_signature": row["template_signature"],
                "node_defs": [],
                "generated_evidence_tier":
                    "generated_semantic_template",
                "focused_test_commands": [
                    "cycles_test --gtest_filter="
                    f"MaterialXSemantic.{row['template']}"
                ],
            }
            contracts.append(contract)
        contract["node_defs"].append(row["id"])
    return families


def _cadence_config() -> dict:
    return {
        "schema_version": 1,
        "full_suite_interval": 3,
        "full_suite_commands": {
            layer: [["python", "-m", "unittest", f"full_{layer}"]]
            for layer in LAYERS
        },
        "lane_commands": {
            "local_cpu": [
                ["blender", "--background", "--device", "CPU"],
            ],
            "local_cuda": [
                ["blender", "--background", "--device", "CUDA"],
            ],
            "windows_a40_cuda": [
                ["blender", "--background", "--device", "A40"],
            ],
            "golden_review": [
                ["materialx-golden-review", "--release-gate"],
            ],
        },
    }


def _shifted_inputs(offset: int) -> dict:
    return _make_inputs(offset)


def _family_union(*inputs_documents: dict) -> dict:
    result = {}
    for inputs in inputs_documents:
        for family_id, records in _registered_families(inputs).items():
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


class _CommandRunner:
    """Fake only the external process boundary, retaining exact responses."""

    def __init__(self, backend, *, results=None):
        self.backend = backend
        self.results = results or {}
        self.calls = []
        self.inputs = []
        self.source_probe_documents = []

    @staticmethod
    def source_document(head=SOURCE_SHA):
        return json.dumps({
            "repository_present": True,
            "files": {
                "intern/cycles/scene/materialx.cpp": True,
                "tools/materialx/materialx_velocity_manifest.py": True,
                "tools/materialx/materialx_batch_scheduler.py": True,
            },
            "head": head,
        }, sort_keys=True, separators=(",", ":"))

    def __call__(
        self,
        command,
        *,
        env,
        input_text,
        timeout,
    ):
        command = tuple(command)
        self.calls.append(command)
        self.inputs.append(input_text)
        configured = self.results.get(command)
        if isinstance(configured, list):
            if not configured:
                raise AssertionError(f"unexpected repeated command: {command}")
            return configured.pop(0)
        if configured is not None:
            return configured
        if "--show-toplevel" in command[-1]:
            document = self.source_document()
            self.source_probe_documents.append(json.loads(document))
            return CommandResult(0, document, "")
        if command[-1].startswith("pids=$(pgrep -f"):
            return CommandResult(0, "active:999", "")
        return CommandResult(0, "ok", "")


class _IntegrationBackend:
    """Fake only isolated Git/process effects while retaining exact calls."""

    def __init__(self):
        self.calls = []

    def prepare_worktree(self, layer, batch_id, base_sha):
        self.calls.append(("prepare", batch_id, base_sha))
        return {
            "worktree": f"worktree:{layer}:{batch_id}",
            "base_sha": base_sha,
        }

    def apply_artifact(self, worktree, head_sha, changed_files):
        batch_id = worktree.rsplit(":", 1)[-1]
        self.calls.append(("apply", batch_id, head_sha))
        return {
            "status": "applied",
            "head_sha": head_sha,
            "changed_files": list(changed_files),
        }

    def run_commands(self, worktree, focused_commands):
        batch_id = worktree.rsplit(":", 1)[-1]
        self.calls.append(("commands", batch_id, tuple(focused_commands)))
        return {
            "commands": [
                {"command": command, "exit_code": 0}
                for command in focused_commands
            ],
        }

    def merge_commit(self, worktree, layer, batch_id, head_sha):
        self.calls.append(("merge", batch_id, head_sha))
        return {"status": "merged", "head_sha": head_sha}


class _OrderingAtomicStateStore(AtomicJSONStateStore):
    def __init__(self, path, events):
        super().__init__(path)
        self.events = events

    def commit(self, state):
        self.events.append(("commit", copy.deepcopy(state)))
        super().commit(state)


class _Transport:
    def __init__(self, *, fail: bool = False, events=None):
        self.fail = fail
        self.messages = []
        self.events = events

    def send(self, message):
        self.messages.append(copy.deepcopy(message))
        if self.events is not None:
            self.events.append(("alert", copy.deepcopy(message)))
        if self.fail:
            raise RuntimeError("transport unavailable")
        return f"receipt-{len(self.messages)}"


class MaterialXVelocityPipelineTest(unittest.TestCase):
    def _five_batch_fixture(self):
        inputs = _make_inputs()
        schedule = _schedule(inputs)
        families = _registered_families(inputs)
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

    def _production_end_to_end(
        self,
        *,
        stale_worker=None,
        queue_empty=False,
        transport_fail=False,
    ):
        completed_inputs = _make_inputs()
        refill_inputs = _shifted_inputs(40)
        completed_schedule = _schedule(completed_inputs)
        refill_schedule = _schedule(refill_inputs)
        families = _family_union(completed_inputs, refill_inputs)
        completed = [] if stale_worker or queue_empty else [
            copy.deepcopy(completed_schedule["assignments"][worker])
            for worker in WORKERS[:2]
        ]
        refills = [] if queue_empty else [
            copy.deepcopy(refill_schedule["assignments"][worker])
            for worker in WORKERS
        ]
        for manifest in refills:
            manifest["batch_id"] = "refill-" + manifest["batch_id"]
        for manifest, layer in zip(
            completed,
            ("native_cycles", "hydra_ovrtx"),
        ):
            manifest["layer"] = layer
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
            self.assertEqual(
                validate_batch_manifest(
                    manifest,
                    registered_families=families,
                ),
                manifest,
            )

        backend = HordeBackend({
            worker: {
                "host": worker,
                "user": "horde",
                "runner_path": "/home/horde/hermes_runner.py",
            }
            for worker in WORKERS
        })
        prior_dispatch_id = "dispatch-prior-completed"
        results = {}
        for index, worker in enumerate(WORKERS):
            process_command = backend.process_command(worker)
            results[process_command] = (
                CommandResult(0, f"active:{700 + index}", "")
                if queue_empty
                else [
                    CommandResult(0, "absent", ""),
                    CommandResult(
                        0, f"active:{900 + index}", ""
                    ),
                ]
            )
        for index, assignment in enumerate(completed):
            completion_text = (
                f"{COMPLETION_PREFIX}"
                + json.dumps(
                    _completion(
                        assignment,
                        chr(ord("b") + index),
                    ),
                    sort_keys=True,
                    separators=(",", ":"),
                )
                + f"\n{EXIT_PREFIX}0"
            )
            results[
                backend.harvest_command(
                    assignment["roles"]["implementation"],
                    prior_dispatch_id,
                )
            ] = CommandResult(0, completion_text, "")
        if stale_worker:
            stale_document = _CommandRunner.source_document("f" * 40)
            results[backend.source_preflight_command(stale_worker)] = [
                CommandResult(0, stale_document, ""),
                CommandResult(0, stale_document, ""),
            ]
        runner = _CommandRunner(backend, results=results)
        integration_backend = _IntegrationBackend()
        cadence_runs = []

        def cadence_runner(argv, *, timeout_seconds):
            cadence_runs.append((tuple(argv), timeout_seconds))
            return {"exit_code": 0}

        completed_by_worker = {
            assignment["roles"]["implementation"]: assignment
            for assignment in completed
        }
        workers = []
        for worker in WORKERS:
            assignment = completed_by_worker.get(worker)
            if queue_empty:
                workers.append({
                    "id": worker,
                    "state": "active",
                    "batch_id": "dispatch-prior-active",
                })
            elif assignment is None:
                workers.append({"id": worker, "state": "idle"})
            else:
                workers.append({
                    "id": worker,
                    "state": "active",
                    "batch_id": prior_dispatch_id,
                    "assignment": assignment,
                })
        queue_entries = [
            {
                "worker_id":
                    manifest["roles"]["implementation"],
                "manifest": manifest,
            }
            for manifest in refills
        ]
        observed_cycles = []
        events = []
        transport = _Transport(
            fail=transport_fail,
            events=events,
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            credential_file = root / "horde.env"
            credential_file.write_text(
                "NVIDIA_API_KEY=private-credential\n",
                encoding="utf-8",
            )
            state_path = root / "state.json"
            adapter = HordeOperationalAdapter.with_dispatcher(
                backend=backend,
                credential_file=credential_file,
                registered_families=families,
                runner=runner,
            )

            def observe(cycle):
                events.append(("observer", copy.deepcopy(cycle)))
                observed_cycles.append(cycle)

            exit_code = run_operational_supervisor(
                SupervisorConfig(1.0, queue_watermark=5),
                workers=workers,
                queue_source=lambda: queue_entries,
                registered_families=families,
                adapter=adapter,
                integration_backend=integration_backend,
                state_store=_OrderingAtomicStateStore(
                    state_path, events
                ),
                clock=_Clock(),
                sleeper=_Sleeper(),
                alert_sink=SanitizedAlertSink(transport),
                project_state=new_project_state(),
                cadence_config=_cadence_config(),
                cadence_runner=cadence_runner,
                cycle_observer=observe,
                once=True,
            )
            persisted = json.loads(
                state_path.read_text(encoding="utf-8")
            )

        self.assertEqual(len(observed_cycles), 1)
        cycle = observed_cycles[0]
        result = {
            "cycle": cycle,
            "families": families,
            "refills": refills,
            "persisted": persisted,
            "runner": runner,
            "integration_backend": integration_backend,
            "cadence_runs": cadence_runs,
            "transport": transport,
            "events": events,
            "exit_code": exit_code,
        }
        if stale_worker or queue_empty:
            return result
        credits = [
            _credit(artifact, receipt)
            for artifact, receipt in zip(
                cycle["artifacts"],
                cycle["integration_receipts"],
            )
        ]
        result["report"] = build_progress_report(
            ledger=_canonical_ledger(),
            phase2_ids=[
                f"ND_test_{index:04d}" for index in range(792, 802)
            ],
            credit_records=credits,
            cadence_config=_cadence_config(),
            cadence_decision=cycle["cadence_decision"],
            cadence_receipts=cycle[
                "cadence_execution_receipts"
            ],
            project_state=persisted,
            integration_train_states={
                "native_cycles": "integrated",
                "hydra_ovrtx": "integrated",
                "blender_authoring": "idle",
            },
            registered_families=families,
        )
        return result

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
        inputs = _make_inputs()
        stale_shas = dict(inputs["probed_worker_shas"])
        stale_shas[WORKERS[0]] = "b" * 40
        with self.assertRaisesRegex(ValueError, "integration_base_sha"):
            _schedule(inputs, probed_worker_shas=stale_shas)

        node_id = inputs["metadata"][0]["id"]
        with self.assertRaisesRegex(ValueError, "Phase-2"):
            build_template_candidates(
                inputs["ledger"],
                inputs["registry"],
                inputs["metadata"],
                phase2_ids=[node_id],
            )

        stale_worker = WORKERS[-1]
        fixture = self._production_end_to_end(
            stale_worker=stale_worker,
        )
        launches = [
            command for command in fixture["runner"].calls
            if command[-1].startswith("nohup ")
        ]
        launch_workers = {
            command[-2].rsplit("@", 1)[-1]
            for command in launches
        }
        self.assertEqual(len(launches), 4)
        self.assertEqual(len(launch_workers), 4)
        self.assertNotIn(stale_worker, launch_workers)
        self.assertEqual(
            next(
                worker["state"]
                for worker in fixture["cycle"]["workers"]
                if worker["id"] == stale_worker
            ),
            "blocked",
        )
        self.assertEqual(
            len(fixture["cycle"]["assigned_batches"]),
            2,
        )
        self.assertNotIn(
            "horde_evidence_receipt",
            fixture["cycle"],
        )
        self.assertEqual(fixture["exit_code"], 1)
        self.assertTrue(all(
            alert["delivery_state"] == "sent"
            for alert in fixture["persisted"]["alerts"]
        ))
        event_kinds = [event[0] for event in fixture["events"]]
        self.assertLess(
            event_kinds.index("observer"),
            event_kinds.index("alert"),
        )
        self.assertLess(
            event_kinds.index("alert"),
            event_kinds.index("commit"),
        )

    def test_acceptance_03_combined_dispatch_launches_each_worker_once(self):
        fixture = self._production_end_to_end()
        cycle = fixture["cycle"]
        launches = [
            command for command in fixture["runner"].calls
            if command[-1].startswith("nohup ")
        ]
        source_probes = [
            command for command in fixture["runner"].calls
            if "--show-toplevel" in command[-1]
        ]
        launch_workers = {
            command[-2].rsplit("@", 1)[-1]
            for command in launches
        }

        self.assertEqual(len(launches), 5)
        self.assertEqual(len(launch_workers), 5)
        self.assertEqual(len(source_probes), 5)
        self.assertEqual(
            {
                document["head"]
                for document in fixture[
                    "runner"
                ].source_probe_documents
            },
            {SOURCE_SHA},
        )
        self.assertEqual(
            launch_workers,
            set(WORKERS),
        )
        self.assertEqual(len(cycle["assigned_batches"]), 5)
        self.assertTrue(all(worker["state"] == "active" for worker in cycle["workers"]))
        self.assertEqual(fixture["exit_code"], 0)
        self.assertRegex(
            cycle["horde_evidence_receipt"],
            r"^horde-cycle-[0-9a-f]{24}$",
        )

    def test_acceptance_04_two_completions_integrate_and_refill_same_cycle(self):
        fixture = self._production_end_to_end()
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
        self.assertEqual(
            len(fixture["persisted"]["integration_receipts"]),
            2,
        )
        self.assertEqual(
            len(fixture["persisted"]["assigned_batches"]),
            5,
        )
        self.assertEqual(
            sum(
                call[0] == "merge"
                for call in fixture["integration_backend"].calls
            ),
            2,
        )

    def test_acceptance_05_invalid_completion_never_reaches_credit(self):
        self.assertEqual(
            parse_completion_evidence(f"{EXIT_PREFIX}0"),
            {"classification": "invalid_completion"},
        )
        _, _, manifests = self._five_batch_fixture()
        manifest = manifests[0]
        with self.assertRaises(ValueError):
            validate_batch_manifest(
                {"batch_id": manifest["batch_id"], "prompt": "implement nodes"},
                registered_families={"add": []},
            )

    def test_acceptance_06_cadence_is_generation_bound_before_credit(self):
        fixture = self._production_end_to_end()
        decision = fixture["cycle"]["cadence_decision"]
        receipts = fixture["cycle"][
            "cadence_execution_receipts"
        ]

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
        fixture = self._production_end_to_end()
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
        fixture = self._production_end_to_end(
            stale_worker=WORKERS[-1],
            transport_fail=True,
        )
        persisted = fixture["persisted"]

        self.assertEqual(fixture["exit_code"], 1)
        self.assertTrue(fixture["transport"].messages)
        self.assertTrue(any(
            record["delivery_state"] == "unsent"
            for record in persisted["alerts"]
        ))
        self.assertIn("transport_failure", persisted["failure_classifications"])
        self.assertEqual(
            persisted["lanes"]["horde"]["state"],
            "unknown",
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
        fixture = self._production_end_to_end(queue_empty=True)
        persisted = fixture["persisted"]

        self.assertEqual(fixture["exit_code"], 1)
        self.assertIn("queue_empty", persisted["failure_classifications"])
        self.assertEqual(persisted["queue_depth"], 0)


if __name__ == "__main__":
    unittest.main()
