# MaterialX Maximum-Velocity Gates Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the approved MaterialX delivery rules into fail-closed code so five Horde workers remain synchronized and assigned to 8-16-node semantic families, completions carry verifiable evidence, and CPU/GPU milestones remain distinct.

**Architecture:** A shared versioned manifest module becomes the contract between the ledger, scheduler, dispatcher, controller, supervisor, capacity monitor, and integration trains. The scheduler is the only source of implementation batches. A recurring supervisor polls all five workers, validates structured completion manifests, refills eligible workers in the same cycle, emits sanitized alerts, and persists one canonical capacity state plus an append-only journal. GPU and golden evidence are separate milestone lanes and never derive from Horde dispatch state.

**Tech Stack:** Python 3 standard library, `unittest`, existing MaterialX catalog/ledger tools, injected subprocess/clock/alert adapters, Blender Cycles CPU/CUDA smoke scripts.

## Global Constraints

- Preserve exact MaterialX semantics and all 802 catalog NodeDefs; never replace unsupported behavior with an approximation.
- Do not duplicate Phase-2 ownership or credit a NodeDef from a prompt, process ID, audit, source reference, or historical test.
- The canonical normal family size is 8-16 NodeDefs. A 1-7-node `complex_exception` requires a red test and an approval record and may occupy at most one worker while family work remains.
- Every implementation batch carries independent `implementation`, `generated_tests`, and `independent_review` roles.
- Every dispatch uses the exact integration base SHA and an independently probed worker SHA.
- Never serialize credentials, raw remote logs, host secrets, or command output that can contain secrets.
- Do not conflate Horde health with local GPU, Windows A40 GPU, or golden-image readiness.
- Keep runtime state out of source commits unless a task explicitly introduces a deterministic fixture.
- Run focused tests after every task; run the complete MaterialX tooling suite after Tasks 4, 8, and 12.

### Task 1: Define the canonical batch and completion manifests

**Files:**

- Create: `tools/materialx/materialx_velocity_manifest.py`
- Create: `tools/materialx/test_materialx_velocity_manifest.py`

- [ ] Write a failing test for a valid 8-node family manifest and exact required fields.

```python
def test_accepts_complete_family_manifest(self):
    manifest = make_batch_manifest(node_defs=[f"ND_add_float_{i}" for i in range(8)])
    self.assertEqual(
        velocity_manifest.validate_batch_manifest(manifest)["batch_id"],
        "native-add-float-001",
    )
```

- [ ] Write table-driven rejection tests for prompt-only input, SHA mismatch, unknown layer, missing role, duplicate NodeDef, 7-node family, nonzero family exception budget, and an unregistered family.

```python
for field, value, message in (
    ("worker_source_sha", "b" * 40, "worker_source_sha must equal integration_base_sha"),
    ("layer", "cycles", "unsupported layer"),
    ("roles", {"implementation": "blend05"}, "missing required roles"),
):
    with self.subTest(field=field), self.assertRaisesRegex(ValueError, message):
        velocity_manifest.validate_batch_manifest({**valid, field: value})
```

- [ ] Implement schema constants, exact five-worker IDs, layer names, role names, SHA validation, family/exception size validation, and deterministic normalized output.

```python
SCHEMA_VERSION = 2
EXPECTED_HORDE_WORKERS = ("blend05", "blendit04", "blendit", "blendit2", "blendit3")
LAYERS = frozenset(("native_cycles", "hydra_ovrtx", "blender_authoring"))
REQUIRED_ROLES = frozenset(("implementation", "generated_tests", "independent_review"))

def validate_batch_manifest(
    manifest: Mapping[str, Any],
    *,
    registered_families: Collection[str],
) -> dict[str, Any]:
    missing = BATCH_FIELDS.difference(manifest)
    if missing:
        raise ValueError(f"batch manifest is missing fields: {sorted(missing)}")
    if manifest["integration_base_sha"] != manifest["worker_source_sha"]:
        raise ValueError("worker_source_sha must equal integration_base_sha")
    if manifest["family_id"] not in registered_families:
        raise ValueError("unregistered family_id")
    return {field: manifest[field] for field in sorted(BATCH_FIELDS)}
```

- [ ] Write completion-manifest tests that reject exit zero without a manifest, base/head SHA errors, mismatched/rejected NodeDefs, files outside the allowlist, missing numeric test exits/counts, failed tests, and a non-pass review.

- [ ] Implement `validate_completion_manifest(assignment, completion)` so returned data is sanitized, normalized, and contains only assigned NodeDefs and allowed files.

```python
def validate_completion_manifest(
    assignment: Mapping[str, Any],
    completion: Mapping[str, Any],
) -> dict[str, Any]:
    if completion["base_sha"] != assignment["integration_base_sha"]:
        raise ValueError("completion base_sha does not match assignment")
    if set(completion["node_defs"]) != set(assignment["node_defs"]):
        raise ValueError("completion NodeDefs do not match assignment")
    escaped = set(completion["changed_files"]).difference(assignment["files_allowlist"])
    if escaped:
        raise ValueError(f"completion changed files outside allowlist: {sorted(escaped)}")
    if completion["review_verdict"] != "pass":
        raise ValueError("completion review_verdict is not pass")
    if any(test["exit_code"] != 0 for test in completion["tests"]):
        raise ValueError("completion contains failed tests")
    return dict(completion)
```

- [ ] Run the focused test.

```powershell
python -m unittest tools.materialx.test_materialx_velocity_manifest -v
```

Expected: all manifest acceptance and rejection tests pass.

### Task 2: Make the ledger the only schedulable-node authority

**Files:**

- Modify: `tools/materialx/materialx_nodedef_ledger.py`
- Modify: `tools/materialx/test_materialx_nodedef_ledger.py`
- Modify: `tools/materialx/materialx_batch_scheduler.py`
- Modify: `tools/materialx/test_materialx_batch_scheduler.py`

- [ ] Add failing ledger tests for `remaining_node_ids()`: completed rows, Phase-2-owned rows, and rows already in active assignments are excluded.

```python
self.assertEqual(
    remaining_node_ids(ledger, phase2_ids={"ND_phase2"}, active_ids={"ND_active"}),
    ["ND_remaining"],
)
```

- [ ] Implement `remaining_node_ids()` after `validate_ledger()` and require every excluded ID to exist in the 802-row ledger.

- [ ] Replace the scheduler's external three-field backlog authority with a ledger-derived candidate builder. Preserve classification metadata only after the NodeDef has passed the ledger ownership filter.

```python
candidates = build_template_candidates(
    ledger,
    semantic_registry,
    phase2_ids=phase2_ids,
    active_manifests=active_manifests,
)
```

- [ ] Add rejection tests for a completed ID, Phase-2 ID, active ID, unknown catalog ID, and duplicated ownership across layers.

- [ ] Run the focused tests.

```powershell
python -m unittest tools.materialx.test_materialx_nodedef_ledger tools.materialx.test_materialx_batch_scheduler -v
```

Expected: exactly 802 ledger rows validate and only unowned remaining rows become candidates.

### Task 3: Emit Batch Manifest v2 from homogeneous family schedules

**Files:**

- Modify: `tools/materialx/materialx_batch_scheduler.py`
- Modify: `tools/materialx/test_materialx_batch_scheduler.py`
- Modify: `tools/materialx/materialx_semantic_registry.py`
- Modify: `tools/materialx/test_materialx_semantic_registry.py`
- Modify: `tools/materialx/materialx_semantic_registry.json`
- Modify: `tools/materialx/materialx_velocity_manifest.py`
- Modify: `tools/materialx/test_materialx_velocity_manifest.py`

- [ ] Add a failing registry test that every schedulable family exposes a deterministic template signature: operation, input types, output type, broadcast policy, and output socket class.

- [ ] Add failing scheduler tests requiring the exact five workers, one assignment per worker, 8-16 homogeneous NodeDefs per family, non-overlapping file allowlists, all three roles, and matching source SHAs.

```python
self.assertEqual(set(schedule["assignments"]), set(EXPECTED_HORDE_WORKERS))
for assignment in schedule["assignments"].values():
    self.assertGreaterEqual(len(assignment["node_defs"]), 8)
    self.assertLessEqual(len(assignment["node_defs"]), 16)
    self.assertEqual(
        set(assignment["roles"]),
        {"implementation", "generated_tests", "independent_review"},
    )
```

- [ ] Extend `build_schedule()` to accept `integration_base_sha`, per-worker probed SHAs, active manifests, Phase-2 IDs, and an explicit role allocation. Emit only manifests accepted by `validate_batch_manifest()`.

- [ ] Align the strict Batch Manifest v2 schema with the approved design: replace the temporary `complex_exception` boolean with `batch_kind` (`family` or `complex_exception`) and require `template_signature` plus `generated_evidence_tier`. Preserve conditional `red_test` and `approval_record` receipt fields for complex exceptions. Update Task 1 manifest tests in the same commit so scheduler output and the trust-boundary validator use one contract.

- [ ] Add `complex_exception` tests: 1-7 IDs require `red_test`, `approval_record`, and an exception budget; reject two simultaneous complex exceptions when normal families remain queued.

- [ ] Run the scheduler and registry tests.

```powershell
python -m unittest tools.materialx.test_materialx_semantic_registry tools.materialx.test_materialx_batch_scheduler -v
```

Expected: a healthy five-worker schedule contains five valid manifests or fails with an explicit queue/capacity reason.

### Task 4: Enforce exact worker-source synchronization before dispatch

**Files:**

- Create: `tools/materialx/materialx_worker_preflight.py`
- Create: `tools/materialx/test_materialx_worker_preflight.py`
- Modify: `tools/materialx/materialx_horde_dispatch.py`
- Modify: `tools/materialx/test_materialx_horde_dispatch.py`

- [ ] Write failing tests for missing repository, missing required architecture file, stale SHA, successful synchronization followed by a matching re-probe, and one stale worker not blocking four healthy workers.

```python
probe = FakeProbe(
    heads={"blend05": "a" * 40, "blendit04": "b" * 40},
    files={"intern/cycles/scene/materialx.cpp": True},
)
result = preflight_workers(EXPECTED_HORDE_WORKERS, "a" * 40, probe=probe, synchronizer=sync)
self.assertEqual(result["blend05"]["state"], "ready")
self.assertEqual(result["blendit04"]["state"], "stale_source")
```

- [ ] Define injected `WorkerProbe` and `WorkerSynchronizer` protocols. Production probing remains read-only; synchronization accepts an approved patch/bundle adapter and always re-probes HEAD.

- [ ] Add `HordeBackend.source_preflight_command()` that prints only categorical file checks and a 40-hex HEAD, never raw environment or credentials.

- [ ] Make dispatch reject manifests whose observed worker SHA differs from the integration base. Preserve dispatch to unaffected workers.

- [ ] Run focused and full tooling tests.

```powershell
python -m unittest tools.materialx.test_materialx_worker_preflight tools.materialx.test_materialx_horde_dispatch -v
python -m unittest discover -s tools/materialx -p "test_materialx_*.py"
```

Expected: focused tests and the complete MaterialX tooling suite pass.

### Task 5: Remove prompt-only dispatch and controller bypasses

**Files:**

- Modify: `tools/materialx/materialx_horde_dispatch_plan.py`
- Modify: `tools/materialx/test_materialx_horde_dispatch_plan.py`
- Modify: `tools/materialx/materialx_horde_dispatch.py`
- Modify: `tools/materialx/test_materialx_horde_dispatch.py`
- Modify: `tools/materialx/materialx_horde_controller.py`
- Modify: `tools/materialx/test_materialx_horde_controller.py`

- [ ] Replace tests that construct only `batch_id` and `prompt` fields with Batch Manifest v2 fixtures, then add explicit tests proving old prompt-only records fail.

- [ ] Change `build_dispatch_plan()` to accept validated manifests, derive workers from role allocation, and copy no arbitrary prompt text into the plan.

```python
def build_dispatch_plan(
    manifests: Sequence[Mapping[str, Any]],
    credential_file: Path,
    *,
    registered_families: Collection[str],
) -> dict[str, Any]:
    assignments = [validate_batch_manifest(item, registered_families=registered_families)
                   for item in manifests]
    return {
        "schema_version": 2,
        "assignments": assignments,
        "workers": sorted({role_worker
                           for item in assignments
                           for role_worker in item["roles"].values()}),
        "credential_file": str(credential_file),
    }
```

- [ ] Change `_worker_prompt()` to render a deterministic instruction from normalized manifest fields and forbid `worker_prompts`.

- [ ] Change controller queue entries from `{worker_id, batch_id, prompt}` to `{worker_id, manifest}` and validate them before any backend call.

- [ ] Add a cross-API test showing scheduler output is accepted unchanged by dispatch and controller while a handwritten minimal manifest is rejected by both.

- [ ] Run focused tests.

```powershell
python -m unittest tools.materialx.test_materialx_horde_dispatch_plan tools.materialx.test_materialx_horde_dispatch tools.materialx.test_materialx_horde_controller -v
```

Expected: no implementation path accepts a prompt-only batch.

### Task 6: Harvest and validate Completion Manifest v2

**Files:**

- Create: `tools/materialx/materialx_completion_harvest.py`
- Create: `tools/materialx/test_materialx_completion_harvest.py`
- Modify: `tools/materialx/materialx_horde_dispatch.py`
- Modify: `tools/materialx/materialx_horde_controller.py`
- Modify: `tools/materialx/test_materialx_horde_controller.py`

- [ ] Write failing parser tests for one bounded sentinel line:

```text
MATERIALX_COMPLETION_V2:{"schema_version":2,"batch_id":"native-add-float-001","base_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","head_sha":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","node_defs":["ND_add_float_0"],"rejected_node_defs":[],"changed_files":["intern/cycles/scene/materialx.cpp"],"tests":[{"command":"cycles_test --gtest_filter=MaterialXSemantic.add_float","passed":1,"failed":0,"exit_code":0}],"review_verdict":"pass","role_evidence":{"implementation":"commit","generated_tests":"test_receipt","independent_review":"review_receipt"}}
```

Reject missing sentinel, duplicate sentinel, invalid JSON, oversized payload, secret-like keys, and `MATERIALX_HORDE_EXIT:0` without a completion manifest.

- [ ] Implement a size-limited, categorical parser that returns either a normalized completion or a failure classification. Do not persist the surrounding log.

- [ ] Change `HordeBackend.harvest_command()` to read only the completion sentinel and exit classifier from the bounded task log.

- [ ] Validate completion against the active assignment before enqueuing an artifact or crediting any ledger row.

- [ ] Add controller tests for NodeDef mismatch, changed-file escape, red test, stale base, failed review, valid completion, and an invalid completion on one worker while the other four continue.

- [ ] Run focused tests.

```powershell
python -m unittest tools.materialx.test_materialx_completion_harvest tools.materialx.test_materialx_horde_dispatch tools.materialx.test_materialx_horde_controller -v
```

Expected: exit zero alone is a failure and only a valid completion reaches an integration train.

### Task 7: Add three isolated integration trains

**Files:**

- Create: `tools/materialx/materialx_integration_train.py`
- Create: `tools/materialx/test_materialx_integration_train.py`
- Modify: `tools/materialx/materialx_horde_controller.py`
- Modify: `tools/materialx/test_materialx_horde_controller.py`

- [ ] Write failing routing tests for `native_cycles`, `hydra_ovrtx`, and `blender_authoring`.

- [ ] Define an injected `IntegrationBackend` with `prepare_worktree`, `apply_artifact`, `run_commands`, and `merge_commit`. Require the declared base SHA at worktree creation.

- [ ] Add tests rejecting conflicts, stale base, edits outside the allowlist, approximation/review rejection, and failed focused tests without blocking another train.

- [ ] Implement deterministic per-layer queues and artifact states: `queued`, `validating`, `integrated`, `rejected`.

- [ ] Return integration receipts containing batch ID, layer, base SHA, head SHA, focused commands, numeric exits, and final state.

- [ ] Run focused tests.

```powershell
python -m unittest tools.materialx.test_materialx_integration_train tools.materialx.test_materialx_horde_controller -v
```

Expected: each layer progresses independently and rejected artifacts receive no ledger credit.

### Task 8: Build the recurring five-worker supervisor

**Files:**

- Create: `tools/materialx/materialx_horde_supervisor.py`
- Create: `tools/materialx/test_materialx_horde_supervisor.py`
- Modify: `tools/materialx/materialx_horde_operational.py`
- Modify: `tools/materialx/test_materialx_horde_operational.py`

- [ ] Write a failing same-cycle test: poll five workers, harvest two valid completions, integrate them, and refill both workers before the cycle returns.

- [ ] Write failing tests for a queue watermark below five, empty queue with healthy workers, stale last-successful-poll after two intervals, one blocked worker with four continuing, and bounded retry without a busy loop.

- [ ] Implement `SupervisorConfig(poll_interval_seconds, stale_intervals=2, queue_watermark=5)` and injected clock/sleeper/backend dependencies.

```python
def run_supervisor(
    config: SupervisorConfig,
    *,
    controller: Controller,
    state_store: StateStore,
    clock: Clock,
    sleeper: Sleeper,
    once: bool = False,
) -> int:
    while True:
        cycle = controller.run_cycle()
        state_store.commit(cycle)
        if once:
            return 0 if cycle["healthy"] else 1
        sleeper(config.poll_interval_seconds)
```

- [ ] Make `materialx_horde_operational.py` a thin CLI adapter with `--once`, `--poll-interval`, and `--queue-watermark`; remove its separate one-shot state schema.

- [ ] Add an atomic state-write test using temp file plus `os.replace()`.

- [ ] Run focused and full tooling tests.

```powershell
python -m unittest tools.materialx.test_materialx_horde_supervisor tools.materialx.test_materialx_horde_operational -v
python -m unittest discover -s tools/materialx -p "test_materialx_*.py"
```

Expected: the complete suite passes and a worker is refilled in the same cycle as valid harvest.

### Task 9: Add a sanitized alert sink with delivery receipts

**Files:**

- Create: `tools/materialx/materialx_alert_sink.py`
- Create: `tools/materialx/test_materialx_alert_sink.py`
- Modify: `tools/materialx/materialx_capacity_monitor.py`
- Modify: `tools/materialx/test_materialx_capacity_monitor.py`
- Modify: `tools/materialx/materialx_horde_supervisor.py`
- Modify: `tools/materialx/test_materialx_horde_supervisor.py`

- [ ] Write tests for allowed failure classes: `capacity_loss`, `stale_source`, `queue_empty`, `auth_failure`, `proxy_failure`, `transport_failure`, `invalid_completion`, and `integration_failure`.

- [ ] Define an injected `AlertTransport.send(message) -> receipt_id` protocol. The repository implementation creates sanitized messages and receipts; the runtime connector supplies Slack delivery.

- [ ] Add deduplication tests: unchanged failure state sends once, a changed failure sends again, recovery clears the dedupe key, and transport failure leaves `delivery_state="unsent"` visible.

- [ ] Preserve worker identity in alerts without collapsing all workers into `worker_exit`.

- [ ] Wire supervisor alerts so a new blocking state is sent before the cycle completes. Store only failure class, subject, timestamp, delivery state, and opaque receipt ID.

- [ ] Run focused tests.

```powershell
python -m unittest tools.materialx.test_materialx_alert_sink tools.materialx.test_materialx_capacity_monitor tools.materialx.test_materialx_horde_supervisor -v
```

Expected: every new capacity blocker has a delivery receipt or explicit unsent state within one supervisor interval.

### Task 10: Canonicalize capacity, GPU milestones, and the append-only journal

**Files:**

- Create: `tools/materialx/materialx_project_state.py`
- Create: `tools/materialx/test_materialx_project_state.py`
- Modify: `tools/materialx/materialx_project_preflight.py`
- Modify: `tools/materialx/test_materialx_project_preflight.py`
- Modify: `tools/materialx/materialx_capacity_monitor.py`
- Modify: `tools/materialx/test_materialx_capacity_monitor.py`
- Modify: `tools/materialx/materialx_horde_dispatch.py`
- Modify: `tools/materialx/test_materialx_horde_dispatch.py`

- [ ] Write the canonical schema test requiring all five Horde workers and independent lanes:

```python
"lanes": {
    "horde": {"state": "active", "last_evidence_id": "cycle-0007"},
    "local_cpu": {"state": "green", "last_evidence_id": "cpu-smoke-0032"},
    "local_cuda": {"state": "green", "last_evidence_id": "cuda-smoke-0032"},
    "windows_a40_cuda": {"state": "due", "last_evidence_id": ""},
    "golden_review": {"state": "not_due", "last_evidence_id": ""},
}
```

- [ ] Add milestone counters and tests: local CPU plus CUDA becomes due after 32-64 newly credited NodeDefs or any render-path edit; Windows A40 becomes due only at a larger configured green milestone; golden review remains release-only.

- [ ] Implement one validator/serializer in `materialx_project_state.py`. Import it from preflight, monitor, dispatch, and supervisor instead of constructing incompatible dictionaries.

- [ ] Remove `_capacity_state()` behavior that sets `windows_local_build` from Horde dispatch success or failure.

- [ ] Define append-only semantic journal records with sequence, event kind, subject, previous state, new state, categorical reason, batch ID, and evidence receipt. Reject out-of-order or rewritten records.

- [ ] Add migration tests for the current schema-v1 fixtures; migration must preserve categorical history and add missing workers as `unknown`, never `active`.

- [ ] Run focused tests.

```powershell
python -m unittest tools.materialx.test_materialx_project_state tools.materialx.test_materialx_project_preflight tools.materialx.test_materialx_capacity_monitor tools.materialx.test_materialx_horde_dispatch -v
```

Expected: one state document validates everywhere and Horde events cannot change GPU readiness.

### Task 11: Enforce bulk test cadence and auditable progress reports

**Files:**

- Create: `tools/materialx/materialx_test_cadence.py`
- Create: `tools/materialx/test_materialx_test_cadence.py`
- Create: `tools/materialx/materialx_progress_report.py`
- Create: `tools/materialx/test_materialx_progress_report.py`
- Modify: `tools/materialx/materialx_horde_controller.py`

- [ ] Write cadence tests: every family requires generated semantic/graph tests; every third through fifth integrated family requires the affected full suite; 32-64 credited nodes or a render-path edit requires local CPU and CUDA; larger configured milestones require Windows A40; golden review is final-only.

- [ ] Implement a deterministic cadence decision from integration receipts and project state. Store why each tier is due and the exact commands to run.

- [ ] Make the controller execute due focused/full commands through an injected runner and record numeric exits. A command string that was merely generated is not evidence.

- [ ] Write report tests that derive counts only from ledger deltas, valid completion receipts, integration receipts, and GPU/golden evidence. Reject launched PIDs, audits, historical tests, and raw source references as completion.

- [ ] Implement JSON and concise text reports containing exact remaining/credited counts, five worker assignments, three train states, evidence-tier counts, blockers, and recovery action.

- [ ] Run focused tests.

```powershell
python -m unittest tools.materialx.test_materialx_test_cadence tools.materialx.test_materialx_progress_report tools.materialx.test_materialx_horde_controller -v
```

Expected: due tests are executed, evidence tiers stay distinct, and report totals reconcile to the 802-row ledger.

### Task 12: Prove the complete five-worker flow and reconcile project rules

**Files:**

- Create: `tools/materialx/test_materialx_velocity_pipeline.py`
- Modify: `doc/materialx_project_operations.md`
- Modify: `doc/materialx_next_session_handoff.md`
- Modify: `docs/superpowers/specs/2026-07-29-materialx-delivery-control-design.md`
- Modify: `docs/superpowers/specs/2026-07-29-materialx-maximum-velocity-gates-design.md`

- [ ] Build a deterministic integration fixture with five workers, five valid 8-node families, two completions, one stale worker, three active integration trains, a queue watermark of five, and separate local/Windows GPU state.

- [ ] Test the full sequence: synchronize, schedule, dispatch, poll, harvest, validate, integrate, credit, refill, alert, persist, and report.

- [ ] Add negative end-to-end tests for stale SHA, Phase-2 overlap, prompt-only batch, invalid completion, queue exhaustion, Slack delivery failure, and GPU/Horde conflation.

- [ ] Update operations and handoff documents to identify the supervisor command, manifest paths, state/journal paths, failure recovery, and alert behavior. Remove all operational guidance that says 3-8, 4+, prompt-only, or manual one-shot refill.

- [ ] Run the complete tooling suite and syntax checks.

```powershell
python -m unittest discover -s tools/materialx -p "test_materialx_*.py"
python -m compileall -q tools/materialx
git diff --check
```

Expected: all MaterialX tooling tests pass, compileall exits zero, and `git diff --check` produces no output.

- [ ] Run the existing native MaterialX focused suite against the integrated branch.

```powershell
C:\tmp\blender-materialx-core-build-local\bin\tests\cycles_test.exe --gtest_filter="MaterialX*"
```

Expected: all selected MaterialX tests pass. A worker using another configured build directory must record its absolute executable path in the completion receipt.

- [ ] Run the existing local CPU and CUDA composed smoke harness when the milestone counter is due.

```powershell
$env:CYCLES_TEST_DEVICE='CPU'
& C:\tmp\blender-materialx-core-current-gpu-runtime\blender.exe --background --factory-startup --python-exit-code 1 --env-system-scripts C:\tmp\blender-materialx-core-work\scripts --python C:\tmp\blender-materialx-core-work\intern\cycles\test\materialx_usdshade_blender_composed_cuda_smoke.py
$env:CYCLES_TEST_DEVICE='CUDA'
& C:\tmp\blender-materialx-core-current-gpu-runtime\blender.exe --background --factory-startup --python-exit-code 1 --env-system-scripts C:\tmp\blender-materialx-core-work\scripts --python C:\tmp\blender-materialx-core-work\intern\cycles\test\materialx_usdshade_blender_composed_cuda_smoke.py
```

Expected: both commands exit zero and record separate CPU/CUDA evidence. Do not claim Windows A40 or golden approval from these results.

- [ ] Commit implementation in reviewable task-level commits, then run superpowers:requesting-code-review and superpowers:verification-before-completion before any completion claim.

## Rollout Order

1. Land Tasks 1-6 as the P0 trust boundary. Do not run implementation dispatch through the old prompt-only path afterward.
2. Land Tasks 7-10 as the continuous control plane. Start the supervisor in `--once` mode until the five-worker integration fixture is green, then enable recurring mode.
3. Land Tasks 11-12 as cadence/reporting and operational migration.
4. Preserve the existing node-implementation pipeline during control-plane work, but credit no new remote result that lacks Batch/Completion Manifest v2 evidence.
5. Keep local and Windows render validation available in parallel; trigger them only at the approved bulk milestones or on render-path changes.

## Definition of Done

- All twelve acceptance criteria in `docs/superpowers/specs/2026-07-29-materialx-maximum-velocity-gates-design.md` have named green tests.
- All five configured Horde workers are represented and independently classified in canonical state.
- Healthy workers cannot remain idle when at least five valid family batches exist.
- No implementation dispatch can bypass the scheduler or source-SHA gate.
- No completion can bypass evidence, review, allowlist, NodeDef ownership, or integration-train checks.
- Local CPU, local CUDA, Windows A40 CUDA, and golden approval remain separate evidence tiers.
- Runtime blockers reach the Slack transport within one polling interval or remain visibly unsent.
- The full MaterialX tooling suite, native focused suite, and due GPU milestone tests pass with recorded numeric exits.
