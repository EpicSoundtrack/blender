# MaterialX delivery-control design

## Goal

Make catalog completion measurable and keep available implementation capacity
assigned to semantically safe, testable MaterialX batches.

## Decision

Adopt a checked-in JSON ledger generated from the 802-NodeDef MaterialX
catalog. A row records `id`, `cycles_reader`, `cycles_lowering`, `hydra`,
`disposition`, `evidence`, `owner`, and `next_action`. Tooling validates the
schema, refuses unknown or duplicate IDs, and emits deterministic summary
counts. It never infers support from source references.

`disposition` is `unclassified`, `supported`, or `explicitly_rejected`.
Evidence tiers are `implementation`, `focused_semantic`, `cpu_render`,
`gpu_render`, and `golden_approved`. Full parity requires GPU plus golden
evidence; all lower tiers remain explicitly lower-tier evidence.

## Execution rules

- Batches contain 8-16 semantically related NodeDefs with an owner and focused test.
- One build owner runs shared local builds; other workers implement/review/test in parallel.
- An unsafe form is explicitly rejected with its semantic reason and replaced immediately.
- Updates are generated from ledger totals, active allocations, and red tests.
- Remote provisioning is sidecar-only after its bounded attempt.

## Acceptance criteria

1. Tooling deterministically creates and validates 802 rows.
2. Summary reports counts by disposition and evidence tier.
3. Tests cover invalid schema, duplicates, unknown IDs, and totals.
4. Current verified evidence is entered; uncertain forms remain unclassified.
