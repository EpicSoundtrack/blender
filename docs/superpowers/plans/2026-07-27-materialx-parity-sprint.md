# MaterialX Catalog Parity Sprint Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move every MaterialX catalog node toward proven Cycles render parity through generated family batches, not one-node patches.

**Architecture:** The catalog harness is the source of truth: 802 NodeDefs produce separate native and Hydra evidence rows. Direct-compatible nodes are implemented as mapping-table families; dimensional, texture, and closure nodes use dedicated endpoint/lowering templates. CPU tests run per mapping batch and the local RTX produces render evidence per completed family.

**Tech Stack:** Blender/Cycles C++, USD/Hydra, MaterialX stdlib, Python catalog tooling, Windows CUDA 12.8, RTX 5000 Ada.

## Global Constraints

- A node counts only with explicit CPU and GPU render-parity evidence.
- Keep native and Hydra results independent; never infer support across renderers.
- Use all authenticated Horde workers for non-overlapping implementation, test, and review roles.
- GPU/runtime issues receive one bounded diagnosis lane and never stop family implementation.
- Checkpoint each green family separately; no mixed commits.

---

## Time-boxed execution schedule

### Block 0 — 0:00–0:45: Stabilize and checkpoint

**Output:** four independently reviewable feature commits and a clean worktree boundary.

- [ ] Commit scalar math (`intern/cycles/hydra/material.cpp`, scalar test, CMake registration).
- [ ] Commit vector2/place2d (`material.h`, `node_util.cpp`, mapping test, endpoint hunks).
- [ ] Commit native coordinate/image ingress (`intern/cycles/materialx/graph.cpp`, `usdshade_reader.cpp`, reader test hunk).
- [ ] Commit mix/remap/range (`graph.cpp`, `usdshade_reader.cpp`, isolated reader-test hunk).
- [ ] Run focused CPU tests for each commit and `git diff --check` before each checkpoint.

**Stop rule:** do not re-run a passing suite; record its test ID/evidence in the harness instead.

### Block 1 — 0:45–3:00: Direct math/vector/channel factory

**Owner lanes:** native implementation, Hydra implementation, generated test matrix/review.

**Files:** `intern/cycles/hydra/material.cpp`, `intern/cycles/hydra/hydra_materialx_scalar_math_test.cpp`, `intern/cycles/materialx/{graph.cpp,usdshade_reader.cpp}`, `intern/cycles/test/materialx_usdshade_reader_test.cpp`.

- [ ] Add a table entry per direct-compatible scalar/vector/channel NodeDef.
- [ ] Generate one minimal graph fixture per table row from `tools/materialx/materialx_harness_plan.py`.
- [ ] Run all generated CPU fixtures; record only passing rows as explicit native/Hydra evidence.
- [ ] Render one GPU fixture per primitive class: math, vector math, combine/separate, convert.
- [ ] Commit native and Hydra tables separately.

**Coverage target:** 50 direct-compatible NodeDefs with CPU evidence; one RTX render per primitive class.

### Block 2 — 3:00–5:30: Texture-coordinate and image factory

**Owner lanes:** native coordinate/image implementation, Hydra coordinate/image implementation, GPU visual/reference lane.

**Files:** `intern/cycles/materialx/{graph.cpp,usdshade_reader.cpp}`, `intern/cycles/hydra/material.cpp`, reader/Hydra tests.

- [ ] Batch texture coordinates, geometry properties, image/tiled image, address/filter/colorspace aliases.
- [ ] Use explicit `(x,y,0)` adapters and reject unsupported split-wrap/object-space cases rather than silently approximating them.
- [ ] Run generated UV/image graphs on CPU and RTX; retain output image and pixel sample evidence.
- [ ] Commit native and Hydra coordinate/image batches separately.

**Coverage target:** 25 coordinate/image NodeDefs with CPU evidence; GPU evidence for UV/image and geomprop representative graphs.

### Block 3 — 5:30–8:00: Adjustment/range/procedural factory

**Owner lanes:** mix/range implementation, procedural/ramp implementation, range semantic review.

**Files:** native reader/lowerer plus `materialx_usdshade_reader_test.cpp`.

- [ ] Batch mix, remap, range, clamp, gamma, saturation, and color adjustment tables.
- [ ] Green `noise2d -> ramp` then expand compatible noise/checker/gradient/ramp variants.
- [ ] Require finite literals, non-zero range widths, and explicit clamp semantics in generated tests.
- [ ] Render representative adjustment and procedural graphs on RTX.
- [ ] Commit adjustment and procedural families separately.

**Coverage target:** 30 adjustment/procedural NodeDefs with CPU evidence; two RTX family renders.

### Block 4 — 8:00–10:00: Closure/PBR and composition sweep

**Owner lanes:** OpenPBR/closure mapping, composed-graph tests, classroom/junk-shop regression.

- [ ] Exercise mapped families through OpenPBR base color, roughness, normal, emission, and opacity inputs.
- [ ] Run classroom and junk-shop conversion with Ask-default UX; capture failures as named node-family gaps.
- [ ] Run GPU composed graphs combining coordinate, math, image, range, and closure nodes.
- [ ] Commit only composition-safe changes; leave unresolved semantic nodes in a separate queue.

**Coverage target:** all completed direct families exercised in one composed CPU graph and one composed RTX graph.

### Block 5 — 10:00–11:00: Evidence, release, and next queue

- [ ] Ingest every passing CPU/GPU result into the 1,604-row parity plan using explicit records.
- [ ] Generate report with proven parity count, failed nodes, and unstarted nodes grouped by lowering template.
- [ ] Produce a release candidate smoke checklist: addon conversion, native USD export, OVRTX path, CPU render, RTX render.
- [ ] Submit no release claim unless all recorded evidence is green for its stated scope.

## Capacity assignment

| Capacity | Continuous assignment |
| --- | --- |
| Local root | merge/checkpoint, family sequencing, GPU render evidence |
| Local agent 1 | current native family implementation |
| Local agent 2 | Hydra family implementation or GPU fixture |
| Local agent 3 | generated tests, review, evidence ingestion |
| Each healthy Horde worker | one implementation, one test matrix, one review task with non-overlapping files |
| RTX 5000 Ada | GPU render evidence only; no repeated runtime diagnosis after path is recorded |

## Risk controls

- A blocked semantic family moves to a named queue after its time box; the next direct family starts immediately.
- No manual node link mutation: use `ShaderGraph::connect` and assert required conversion nodes.
- No catalog row receives a parity status without a retained command result and output artifact.
- The standalone CUDA runtime requires `bin\\lib\\kernel_sm_89.cubin.zst`, `bin\\blender.shared`, and CUDA 12.8 `bin` on `PATH`; record this once and reuse it.
