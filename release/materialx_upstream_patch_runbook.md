# MaterialX Native Cycles Upstream Patch Packaging Runbook

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Rewrite the audited `e3adc6da` production union plus Switch10 into a reviewable, evidence-bound Blender/Cycles patch series.

**Architecture:** `materialx_upstream_patch_series.v1.json` is the machine authority for commits, path scope, order, and gates. Build the series in a new clean worktree from the current Blender upstream commit. Historical commits are source pools, not cherry-pick authority; each logical patch must build and carry its own tests.

**Tech Stack:** Blender/Cycles C++, Hydra/USDShade, Blender Python, CMake, Git, SHA-256.

## Constraints

- Never mutate dirty `C:\src\blender-materialx-core`.
- Exclude `tools/materialx/**`, delivery docs, Horde/controller/ledger state, and add-on RQ/dashboard projections.
- Static NodeDef sets are route-specific accepted-code inventories, not qualification.
- Resolve a current immutable Blender upstream base; `96294be...` is audit context only.
- Apply Switch10 semantics from `613be0fb...` then `67511832...` after reconstructing the `e3adc6da...` union.
- Do not publish or push until every series-wide evidence field is populated.

## Construct

- [ ] Parse and hash the JSON manifest; record the exact execution copy.
- [ ] Resolve the full upstream base commit in an authorized fetch and create a clean worktree from it.
- [ ] Rebuild patches 1-9 from listed commits and paths. Split mixed `ad007ddd...`; rewrite merge-shaped `b937ed5e...` logically.
- [ ] After every patch, require changed paths to be allowlisted, run its gates, and record patch SHA-256 and result tree.
- [ ] Port `613be0fb...` then `67511832...`; reject any Switch10 change outside its four exact paths.
- [ ] Preserve `e3adc6da...` rotation/ramp behavior while resolving Switch10 and rerun both regressions.
- [ ] Add only the two bounded integration smoke files in patch 11.

## Seal authority and evidence

- [ ] Generate independent native and Hydra sorted NodeDef manifests with route, exact ID, types/defaults, semantic digest, implementation path, source patch, and targeted test.
- [ ] Compare route-set hashes to the audited JSON values. Explain and review every delta caused by rebase or Switch10.
- [ ] Run format/license checks and configure Cycles with tests, USD, and Hydra enabled.
- [ ] Hash the CMake cache, `cycles_test`, `cycles_hydra_test`, and Blender executables; capture machine-readable results.
- [ ] Run CPU, CUDA, Hydra, MaterialX-off, invalid-authority, and clean-profile Blender smoke gates.
- [ ] Measure performance/memory against the same upstream base.
- [ ] Generate the series twice from clean clones and require byte-identical patch files.
- [ ] Obtain Blender/Cycles architecture, ownership/licensing, fallback, and performance review.

## Open blockers

- No current Blender upstream base/rebase is sealed.
- Switch10 remains divergent and must be ported.
- No per-NodeDef route/semantic evidence manifest exists.
- Historical exact-e3 native binaries do not prove green: bounded runs timed out, and the Hydra binary predates final `e3adc6da`.
- Stale `patches/0001-Add-MaterialX-parity-tooling-and-handoff.patch` mixes production and internal tooling and is not submit-ready.
