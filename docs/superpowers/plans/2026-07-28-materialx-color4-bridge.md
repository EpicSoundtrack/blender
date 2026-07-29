# MaterialX Color4 Image Bridge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Preserve MaterialX image alpha for `ND_image_color4`, `ND_extract_color4`, and `ND_convert_color4_color3` through USDShade-to-Cycles lowering.

**Architecture:** Add a bounded `Color4` IR type and allow only `ND_image_color4` to produce it initially. Lower its RGB and alpha through the existing Cycles Image Texture `Color` and `Alpha` outputs; reject any other Color4 producer rather than fabricate alpha.

**Tech Stack:** Blender C++, Cycles MaterialX graph, USDShade reader, GoogleTest.

## Global Constraints

- Keep USDShade authoritative; do not create MaterialX document files.
- Do not add a general Cycles RGBA socket or arbitrary Color4 algebra.
- Reject unsupported Color4 producers, nonliteral extract indices, invalid indices, and unsupported image controls explicitly.
- Validate focused tests before any narrow commit.

---

### Task 1: Add bounded Color4 IR representation

**Files:**
- Modify: `intern/cycles/materialx/graph.h`
- Modify: `intern/cycles/materialx/graph.cpp`
- Test: `intern/cycles/test/materialx_graph_test.cpp`

**Interfaces:**
- Produces `Type::Color4` links and literal Color4 node inputs.
- Consumed by Color4 reader and lowerer tasks.

- [ ] **Step 1: Write failing graph type tests**

Create a graph fixture whose `ND_image_color4` output is `Type::Color4`; assert Color4-to-float and Color4-to-Color3 consumers validate only through their documented NodeDefs.

- [ ] **Step 2: Run the focused graph test**

Run: `cycles_test --gtest_filter=CyclesMaterialXGraph.*color4*`

Expected: FAIL because `Type::Color4` and Color4 literal storage do not exist.

- [ ] **Step 3: Add minimal IR support**

Add `Color4` to `materialx::Type`, a float4 literal map on `Node`, and exact type validation used only by the three Color4 node identifiers.

- [ ] **Step 4: Re-run the focused graph test**

Expected: PASS.

### Task 2: Read and validate Color4 image nodes

**Files:**
- Modify: `intern/cycles/materialx/usdshade_reader.cpp`
- Modify: `intern/cycles/test/materialx_usdshade_reader_test.cpp`

**Interfaces:**
- Consumes USD `Color4f` outputs and literals.
- Produces `ND_image_color4`, `ND_extract_color4`, and `ND_convert_color4_color3` graph nodes.

- [ ] **Step 1: Write failing USDShade reader tests**

Create a `Color4f` image output feeding `extract_color4(index=3)` and `convert_color4_color3`. Add invalid index tests for `-1`, `4`, and linked index.

- [ ] **Step 2: Run the focused reader test**

Run: `cycles_test --gtest_filter=CyclesMaterialXUSDShadeReader.*color4*`

Expected: FAIL because Color4 output parsing is unsupported.

- [ ] **Step 3: Implement bounded reader parsing**

Require `Color4f`, parse only literal index 0 through 3, and reject unsupported image controls and Color4 producers with node-specific diagnostics.

- [ ] **Step 4: Re-run focused reader tests**

Expected: PASS.

### Task 3: Lower RGB and alpha routing through one Image Texture node

**Files:**
- Modify: `intern/cycles/materialx/graph.cpp`
- Test: `intern/cycles/test/materialx_graph_test.cpp`

**Interfaces:**
- `image_color4` lowers to Image Texture.
- `extract_color4` routes RGB index through Separate Color and alpha index directly from Image Texture Alpha.
- `convert_color4_color3` routes Image Texture Color.

- [ ] **Step 1: Write failing structural lowering test**

Assert one Image Texture node serves both `extract_color4(index=3)` and `convert_color4_color3`; assert alpha is sourced from `Alpha` and RGB conversion from `Color`.

- [ ] **Step 2: Run focused graph test**

Expected: FAIL because no Color4 lowering exists.

- [ ] **Step 3: Implement bounded lowering**

Create one Image Texture node; wire UV to Vector; use Separate Color only for RGB extraction; route alpha directly for index 3; reject non-image Color4 links.

- [ ] **Step 4: Run focused graph and reader tests**

Expected: PASS.

### Task 4: Verify and checkpoint

**Files:**
- Test: `intern/cycles/test/materialx_graph_test.cpp`
- Test: `intern/cycles/test/materialx_usdshade_reader_test.cpp`

- [ ] **Step 1: Run focused Color4 suite**

Run graph and reader Color4 filters plus existing image ingress tests.

Expected: PASS with explicit failures only for unsupported arbitrary Color4 producers and unsupported image controls.

- [ ] **Step 2: Run diff validation**

Run: `git diff --check`

Expected: clean.

- [ ] **Step 3: Create a narrow commit**

Commit only the Color4 IR, reader, lowerer, and tests with message `Cycles: add bounded MaterialX Color4 image bridge`.

## Self-review

All approved scope is covered by Tasks 1–4. The plan deliberately excludes general Color4 algebra, default alpha fabrication, and unsupported image-control approximation. Interfaces are limited to the three approved NodeDefs and existing Cycles Image Texture outputs.
