# MaterialX maximum-velocity gate design

## Goal

Turn the existing MaterialX delivery rules into executable, fail-closed gates.
Healthy capacity must stay assigned to homogeneous semantic families, and no
worker result may enter the integration queue without source-version,
implementation, test, and review evidence.

This design strengthens the existing delivery-control design. It does not
change MaterialX semantics, weaken parity requirements, duplicate Phase-2
work, or make GPU/golden evidence optional.

## Existing enforced rules

The repository already enforces several important constraints:

- The authoritative ledger contains exactly 802 catalog NodeDefs.
- Template batches contain 8-16 NodeDefs.
- Batches are grouped by semantic template and evidence command.
- NodeDefs and batch IDs cannot overlap.
- A generated schedule cannot leave a declared healthy worker idle.
- Unsafe semantic forms have an exception budget of zero.
- Authentication and proxy failures fail closed and suppress refill.
- Harvest/refill decisions and capacity alerts use sanitized categorical
  evidence.
- Full parity remains distinct from implementation, focused semantic, CPU
  render, GPU render, and golden approval evidence.

These checks are individually useful but are not connected end to end.

## Enforcement gaps

The current dispatch and controller contracts accept arbitrary records that
contain only a batch ID and prompt. They do not require:

- the worker source SHA to equal the integration baseline;
- a semantic family, layer, or 8-16 exact NodeDef list;
- three independent worker roles for implementation, tests, and review;
- a file ownership allowlist or Phase-2 overlap check;
- machine-readable completion evidence;
- exact test counts, commit SHA, changed files, or review verdict;
- a recurring poll/harvest/refill loop;
- an alert sink that reaches Slack within one polling interval;
- five expected Horde workers rather than an arbitrary worker subset.

Consequently, a valid scheduler can be bypassed by manually constructed
prompts, and a remote exit-zero log can be harvested as success even when the
work used stale source or produced only an audit.

## Batch manifest v2

Every implementation dispatch must originate from the checked scheduler and
use a versioned manifest with these fields:

- `batch_id`: globally unique batch identifier.
- `batch_kind`: `family` for normal work or `complex_exception` for a
  separately approved non-template repair.
- `family_id`: registered semantic-template family.
- `layer`: one of `native_cycles`, `hydra_ovrtx`, or
  `blender_authoring`.
- `node_defs`: 8-16 exact catalog IDs for `family` work. A
  `complex_exception` contains 1-7 IDs, cites a red test and approval record,
  and cannot occupy more than one worker while family work remains queued.
- `template_signature`: registered operation, input/output types, broadcast
  policy, and output socket class.
- `integration_base_sha`: exact local integration baseline.
- `worker_source_sha`: worker SHA observed immediately before dispatch.
- `roles`: at least `implementation`, `generated_tests`, and
  `independent_review`.
- `files_allowlist`: non-overlapping ownership boundary for the batch.
- `focused_test_commands`: deterministic commands generated for the family.
- `generated_evidence_tier`: expected evidence tier.
- `exception_budget`: zero for family-template work.

Dispatch fails unless both SHAs match, every NodeDef is in the 802-row ledger,
the semantic registry contains the family, the batch size is valid, all three
roles exist, file ownership is non-overlapping, and no NodeDef is already
owned by Phase-2 or another active batch.

The canonical family size is 8-16. Older 3-8, 4+, or prompt-only guidance is
superseded and must be removed from operational documentation.

## Worker synchronization gate

Before assigning work, the controller performs a read-only worker preflight:

1. Confirm the expected repository and required current-architecture files.
2. Read the worker HEAD without trusting worker-supplied prompt text.
3. Synchronize through an approved patch/bundle mechanism when the worker is
   stale.
4. Re-read HEAD and reject dispatch unless it equals
   `integration_base_sha`.

A stale worker is blocked independently. The other workers remain assigned,
and the stale-worker failure is alerted immediately.

## Completion manifest v2

An exit-zero process is not completion evidence. Every finished batch must
produce a sanitized machine-readable manifest containing:

- batch, family, layer, base SHA, and resulting commit SHA;
- exact NodeDefs implemented and exact NodeDefs explicitly rejected;
- changed tracked files;
- focused test commands, pass/fail counts, and numeric exits;
- full-suite evidence when required by the milestone policy;
- independent review verdict;
- evidence for all three required roles;
- categorical failure classification when unsuccessful.

The controller rejects completion if the base SHA differs, the NodeDef set
does not match the assignment, files escape the allowlist, tests are absent or
red, review is not a pass, or the result contains unregistered/overlapping
NodeDefs. Rejected completion is recorded but never credited to the ledger.

## Continuous controller

The production controller is a recurring state machine, not a manually called
one-shot function:

1. Poll all five expected Horde workers on a bounded interval.
2. Classify active, finished, blocked, authentication, proxy, and transport
   states without copying raw logs.
3. Harvest every finished completion manifest.
4. Validate and enqueue successful artifacts for the correct integration
   train.
5. Refill every eligible worker in the same cycle.
6. Alert on capacity loss, stale source, invalid completion, or empty queue
   within that polling interval.
7. Persist sanitized state and append-only success/failure journal records.

The controller may block one worker but must not stop unaffected workers.
Queue exhaustion while healthy workers exist is a project-gate failure.
The supervisor records its last successful poll and is unhealthy when that
timestamp exceeds two polling intervals.

## Integration trains

Root integration uses three independent trains:

- native Cycles reader/lowering;
- Hydra/OVRTX;
- Blender MaterialX authoring.

Each artifact is applied against its declared base in an isolated worktree.
The train runs generated focused tests before merging. Conflicts, stale
architecture, approximations, static-only tests, or cross-layer edits reject
the artifact without blocking the other trains.

This prevents remote stale patches from becoming root's serial integration
bottleneck while preserving clean, upstreamable Blender changes.

## Test cadence

- Every family: generated semantic and graph tests.
- Every 3-5 integrated families: complete affected native, Hydra, or authoring
  suite.
- Every 32-64 newly credited NodeDefs, or any render-path change: local CPU
  and local CUDA milestone smoke.
- Every larger green milestone: independent Windows A40 CUDA smoke.
- Final release only: golden-image comparison and human approval.

Simple family members do not receive bespoke GPU renders. A failing batch is
bisected within its family without reducing unrelated throughput.

## Alerts and reporting

Capacity, source-sync, queue-empty, authentication, proxy, invalid-completion,
and integration failures produce a sanitized immediate alert. The Slack sink
must deliver new capacity-blocking alerts within one controller interval and
deduplicate unchanged state. It records a delivery receipt; an unsent alert is
itself visible controller state.

Progress reports are generated from:

- exact ledger deltas;
- active five-worker assignments;
- integration-train state;
- focused/full/GPU evidence counts;
- explicit blockers and their recovery action.

No report may substitute launched PIDs, historical tests, audits, or raw source
references for verified completion.

## Acceptance criteria

1. Arbitrary prompt-only batches are rejected by dispatch and controller APIs.
2. A worker with a stale SHA cannot receive an implementation batch.
3. A valid schedule requires all five configured healthy Horde workers and
   assigns each exactly once.
4. Every normal batch contains 8-16 homogeneous NodeDefs and three required
   roles; a smaller complex exception requires a red test and approval record.
5. Overlap across NodeDefs, layers, files, Phase-2 ownership, or active work is
   rejected.
6. Exit zero without a valid completion manifest is a failure.
7. Invalid tests, review, NodeDef sets, base SHA, or file ownership prevent
   ledger credit and refill with dependent work.
8. A successful harvest refills the worker in the same controller cycle.
9. A blocked worker alerts immediately while the other workers remain active.
10. Focused, full-suite, local GPU, A40 GPU, and golden evidence remain
    separate, auditable tiers.
11. Tests cover every rejection path and a complete five-worker
    harvest/refill/integration cycle.
12. The checked runtime capacity state validates against one canonical schema,
    contains all five expected workers, and cannot conflate Horde dispatch
    health with local or Windows GPU readiness.
