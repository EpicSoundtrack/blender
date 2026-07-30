# MaterialX delivery-control design

## Goal

Make catalog completion measurable while keeping the exact five documented
Horde workers assigned to semantically safe, upstreamable MaterialX family
batches. Operational activity is evidence, never completion credit.

## Canonical authority

The checked 802-NodeDef ledger is the sole positive scheduling authority.
Normal work is emitted as homogeneous 8–16 NodeDef Batch Manifest v2 records
with a common integration/source SHA, registered semantic signature, disjoint
file allowlist, generated focused commands, and three distinct roles:
implementation, generated tests, and independent review. Prompt-only batches,
Phase-2 overlap, active-work overlap, unknown NodeDefs, or stale source fail
before remote launch.

A finished worker must emit bounded Completion Manifest v2 evidence. Exit zero
without that manifest, red/missing tests, a non-pass review, an incorrect
NodeDef set, stale base, or files outside the allowlist cannot enter an
integration train or ledger credit.

## Recurring execution

The production flow is:

1. synchronize and preflight all five worker sources;
2. schedule at least five canonical family batches;
3. combined-dispatch each derived worker once;
4. poll and bounded-harvest all finished workers;
5. validate Completion Manifest v2;
6. integrate independently through native Cycles, Hydra/OVRTX, and Blender
   authoring trains;
7. execute the exact current-generation focused/full cadence;
8. credit only correlated green NodeDefs;
9. refill every eligible worker in the same cycle;
10. deliver a sanitized sent/unsent alert receipt before locked canonical
    state persistence; and
11. derive the exact 802-row progress report.

Start `materialx_horde_operational.py` with `--once` until the deterministic
five-worker fixture is green, then use recurring mode. A blocked worker is
isolated while unaffected workers continue.

## Evidence separation

Local CPU, local CUDA, Windows A40 CUDA, and golden review are independent
state lanes and receipts. Horde observations cannot change them. Milestone
receipts must match the current generation; historical receipts are not
reused. Golden approval is a final human release gate.

The control plane is not a claim of full catalog, OVRTX, add-on, or release
parity. Those remain product-delivery blockers until their own evidence is
green.

## Persistence and alerts

Canonical schema-v2 state is written under an exclusive lock and atomic
replace. Its semantic journal is append-only. Noncanonical rewrites and
truncation are rejected without replacing the last good state.

New capacity, stale-source, queue, completion, integration, authentication,
proxy, and transport blockers are sanitized. A runtime
`SanitizedAlertSink` may deliver them to an injected connector; no connector
or a connector failure records `unsent` and remains nonblocking. Messages,
logs, prompts, credentials, and secrets are never persisted.

A healthy Horde cycle receipt exists only when all five documented workers
have coherent bounded process evidence and an attached or newly successful
dispatch ID after the cycle. The receipt is a deterministic digest of those
exact categorical facts. Partial, missing, or forged evidence produces no
healthy receipt. An optional process-local cycle observer receives one deep
copy of the exact controller output before supervisor sanitization and state
commit; its return value is ignored, mutation cannot affect persistence, and
an exception fails the cycle closed without exposing exception text.

## Verification

The final control-plane gate is:

```text
python -m unittest tools.materialx.test_materialx_velocity_pipeline tools.materialx.test_materialx_horde_operational -v
python -m unittest discover -s tools/materialx -p "test_*.py"
python -m compileall -q tools/materialx
git diff --check
```

The current native binary filter is
`materialx_graph.*:materialx_authority.*:materialx_usdshade_reader.*:materialx_authority_pipeline.*`.
Due local CPU/CUDA, Windows A40, and golden lanes produce separate receipts;
one lane never substitutes for another.
