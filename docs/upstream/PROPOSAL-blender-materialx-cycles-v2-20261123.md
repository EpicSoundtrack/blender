# Proposal v2 (draft): native MaterialX / UsdShade support in Cycles

STATUS: **internal review draft, 2026-11-23.** Not posted, not sent, not reviewed
by a human. It is the text proposed for the C0 design task in
`PATCH-SERIES-PLAN-2026-11-16.md` (add-on repo `docs/upstream/`). Posting it, and
who posts it, is the user's decision after internal review.

This version replaces `PROPOSAL-blender-materialx-cycles-DRAFT.md` (kept unchanged
next to this file). Every number in this file was measured on 2026-11-23 against
the native release line `release/native-materialx-line-20261122` @ `ee5b29316847`
(abbreviated `ee5b2931`), unless the row says otherwise. Each number carries the
command that produced it. Commands run from the root of the `blender-materialx-core`
checkout in Git Bash unless marked `[add-on]` (run from the root of
`materialx-finish-20260917`) or `[web]` (read-only HTTPS fetch of public upstream
files). **UNVERIFIED** marks a claim that I could not check with a command or an
opened file in this session.

Parts 1-10 are the design-task text. The Appendix holds the measurements (M1-M22)
and the internal notes that would not be posted.

---

## 1. Process research

Carried over from the draft. These pages were opened when the draft was written.
They were **not** re-opened for v2:
- Cycles module owner comments on MaterialX in Cycles:
  https://devtalk.blender.org/t/material-x-adopted-as-an-official-standard-by-asf-possibilities-for-cycles-eevee/19731
- Render & Cycles meeting notes (Aug 2022) on MaterialX/Hydra:
  https://devtalk.blender.org/t/2022-08-23-render-cycles-meeting/25591
- Existing shader-node to MaterialX export code: `source/blender/nodes/shader/materialx`.
- USD importer material code: `source/blender/io/usd/intern/usd_reader_material.cc`,
  built around UsdPreviewSurface (draft reading; truncated page, not conclusive).

Re-checked for v2:
- Upstream `main` is **5.4 alpha**: `BLENDER_VERSION 504`, `BLENDER_VERSION_CYCLE alpha`
  at `de111f7f13ef` (committed 2026-11-24T02:18Z) [M18]. New features therefore
  target 5.4 (main), not 5.3 as the draft said.
- Upstream pins MaterialX **1.39.4** and USD **26.08** in
  `build_files/build_environment/cmake/versions.cmake` at the same commit [M19].

What prior discussion says (from the draft's reading of the devtalk pages):
- The Cycles module owner said the main benefit of MaterialX is interoperability
  and that it is mostly a subset of Cycles/EEVEE nodes. He suggested adding the most
  important MaterialX nodes and closures, with conversion in both directions. He was
  "not personally convinced" about closer integration into Cycles/EEVEE and opposed
  "two parallel shader node evaluation systems with different limitations".
- The 2022 meeting notes said direct MaterialX rendering in Cycles/EEVEE would need
  many low-level nodes or conversion to OSL/GLSL, and should not block Hydra work.
  Long term, low-level nodes could exist, with high-level nodes becoming node groups.
- These are 2021-2022 statements. Current module-owner positions: **UNVERIFIED**.

Still **UNVERIFIED** (projects.blender.org / developer.blender.org returned HTTP 403
to the draft's tooling and were not retried): the developer handbook's design-task
process, design task T100569, issue 112864, the AMD MaterialX add-on (T100894, PR
104594), current module ownership for Cycles and USD/IO, the test requirements and
the contribution licence (GPL-2.0-or-later is believed, not confirmed).

One upstream interaction exists: the standalone fix A1 (below) is open as
projects.blender.org PR #166886, created by the user on 2026-11-19 from
`fdca51fc8568` [M20]. Its review state today: **UNVERIFIED** (not opened).

## 2. Problem and user value

Artists receive USD assets whose materials are authored as UsdShade MaterialX
networks (including OpenPBR and Standard Surface). Blender's USD importer is
UsdPreviewSurface-centred, so these materials are lost or degraded, and Blender
writes MaterialX only for Hydra delegates. A native path would let Cycles render
MaterialX content as authored, and would report unsupported constructs visibly
instead of rendering something plausible but wrong.

## 3. Scope and non-goals

In scope, staged: MaterialX 1.39 standard-library NodeDefs (math, convert, logic,
channel, procedural, texture, geometric, adjustment, compositing, colour transform),
Standard Surface and OpenPBR lowered to Cycles closures, the UsdShade import path
(authored UsdShade networks and UsdMtlx references), and nodegraph interfaces.

Non-goals: a second user-facing shader evaluation system or node editor; EEVEE;
MDL; MaterialX writing (export exists); replacing Blender shader nodes; unrelated
kernel changes (each goes in its own PR with its own justification, see §6).

## 4. Architecture and alternatives

Pipeline in the fork: MaterialX/UsdShade source -> reader -> small intermediate
graph -> `validate()` (reject unsupported constructs) -> lowering into Cycles
`ShaderGraph` nodes -> normal Cycles compilation.

### 4.1 The alternatives

1. **Convert to Blender shader nodes / node groups in the importer.** Pros: matches
   the module owner's stated preference; the result is ordinary, editable Blender
   nodes; EEVEE benefits; there is no parallel system. Cons: some MaterialX
   operations have no Blender node, some only match in shape and not in value, and
   several types need large node groups.
2. **Native lowering into the Cycles `ShaderGraph`** (the fork). Pros: direct
   mapping, precise validation, no UI surface. Cons: Cycles-only, a very large code
   volume (§6), and it raises the "two systems" concern. Acceptable only as an
   internal translation layer.
3. **OSL via MaterialX ShaderGen.** Pros: reuses MaterialX's own generator and
   reference behaviour; far less hand-written code. Cons: OSL in Cycles is
   CPU-oriented (GPU status **UNVERIFIED**), it needs MaterialXGenOsl and library
   data shipped, and closure mapping still needs custom work. We have not evaluated
   it in depth.

### 4.2 What the stock-expressibility table says about alternative 1

We classified every NodeDef branch of the fork's lowering by whether stock Blender
shader nodes can build the same Cycles node configuration
(`stock-expressibility.csv`, next to this file). Of the **239** rows [M14]:

| Class | Rows | Meaning |
|---|---:|---|
| STOCK | 132 | one stock node with the needed sockets/properties |
| STOCK_EXPANDED | 94 | expressible only as a node-group expansion |
| NEEDS_UPSTREAM | 10 | needs a socket, property or node that stock Blender lacks |
| RUNTIME_ONLY | 3 | cannot be a static node tree (`ND_dot_surfaceshader`, `ND_time_float`, `ND_frame_float`) |

So **226 of 239 (94.6%)** analysed rows are structurally expressible with stock
nodes. That supports the owner's node-group preference for most of the analysed
library, and it is the strongest argument for alternative 1.

The table has limits, and the design discussion should hear them from us:

- **Shape, not value.** The table compares sockets and properties. It is not a
  pixel- or value-equivalence result. 133 rows are `inferred` and 97 are `measured`
  (a `create_node<>` call checked in the source). The other 9 have no usable value:
  7 rows are malformed (unquoted commas give 7, 9 or 10 fields instead of 8) and 2
  carry `RUNTIME_ONLY` in that field. The class column is intact in all of them [M14].
- **The procedural rows show the gap.** `ND_noise2d/3d_*`, `ND_cellnoise*` and
  `ND_worleynoise*` are classed STOCK via the White Noise texture (Worley marked
  "approx"). Yet the fork needed **11 MaterialX-specific Cycles nodes** (for example
  `MaterialXNoiseNode`, `MaterialXCellNoiseNode`, `MaterialXWorleyNode`,
  `MaterialXGlassBsdfNode`) to match MaterialX values [M10]. For these families,
  "STOCK" means a stock node with the right shape exists, not that it produces the
  MaterialX result.
- **STOCK_EXPANDED is often large.** It includes all 24 matrix rows and 22 of the 40
  colour rows (Color4 handled as a colour plus a separate alpha lane) [M14].
- **Coverage.** The table was measured at fork commit `9d4f0f97`, 192 commits before
  `ee5b2931`, and its 239 rows (238 unique NodeDef ids; `ND_switch_float` appears
  twice) are branch-level, not the whole library: `graph.cpp` at `ee5b2931` names 746
  distinct `ND_*` ids [M14, M15].
- **Row ids.** 17 of the 238 ids are not catalog NodeDef ids (for example
  `ND_maprange_float/remap_float`, `ND_multiply_bsdff`, `ND_generic_surface`), so they
  do not join to the ledger [M14].
- In the current project ledger, all 10 NEEDS_UPSTREAM rows and all 3 RUNTIME_ONLY
  rows have `full_node_parity` true through native lowering (most certificates are
  still pending application, §6.1). The CSV's own parity column is an older snapshot
  and marks only 3 of the 10 [M14]. Native lowering therefore covers things a stock
  node tree cannot express today.

**Our proposal for the discussion:** a hybrid. (a) The importer builds ordinary
Blender node trees wherever a stock node is value-equivalent, not merely
shape-equivalent, with that equivalence shown by tests. (b) A small, named set of
MaterialX-semantics nodes is introduced only where stock nodes cannot match, as
real nodes with a design review, not as hidden flags on stock nodes. (c) Native
`ShaderGraph` lowering is kept only if owners prefer an internal translation layer
behind the importer. This is a question to the owners (§10 Q1), not a decision we
can make.

## 5. Dependencies

MaterialX 1.39.4 is already a Blender dependency [M19]. The fork links MaterialXCore
and MaterialXFormat under `WITH_MATERIALX` and uses USD under `WITH_USD`. Whether
Blender's prebuilt libraries on every platform ship the components and standard
library data needed: **UNVERIFIED**.

## 6. State of the code today

Measured at `ee5b2931`:

- `intern/cycles/materialx/graph.cpp`: **35,034** lines [M1]. Its `lower_nodes()`
  is **18,298** lines and `validate()` is **8,009** lines [M3]; 4 functions exceed
  300 lines [M4].
- `intern/cycles/materialx/usdshade_reader.cpp`: **33,768** lines [M1], with **17**
  functions over 300 lines (the largest, `read_color_output()`, is 2,960) [M4].
- `intern/cycles/blender/shader.cpp`: **2,039** lines [M1] (the MaterialX bridge
  part is +52/−5 against the fork base [M6]).
- The whole fork diff against its upstream base `96294be75080` is **891** commits and
  **190** files, **+221,570 / −404** lines [M5]. By destination, it splits into
  [M6]: MaterialX core 11 files +69,721; Cycles tests 24 files +104,695; Hydra
  7 files +10,779/−226; Cycles kernel/scene/util/graph 40 files +3,613/−72; USD io
  15 files +1,481/−97; project tooling and docs 78 files +30,389 (never upstream).

This cannot be reviewed as it is. Before any code PR:

- **Split by family.** `graph.cpp` and `usdshade_reader.cpp` must become per-family
  files with a registration table, and the 18k-line `lower_nodes()` must be broken
  up. A file-split plan exists (`054413b1c90`), but only on a build agent. It is not
  in the local repository [M12].
- **Visible failures.** On the shipping line, a lowering failure is logged
  (`LOG_ERROR "MaterialX authority could not be lowered"`) and the function still
  returns `true`, so the material renders whatever graph state was left
  [M13]. A fix (`40d7522f0266`: magenta emission plus `RE_engine_report`) exists only
  on the local branch `fix/upstream-prep-20261116`. It has never been run. It no
  longer applies cleanly to `ee5b2931`: it conflicts in `intern/cycles/blender/shader.cpp`
  after `aa69ee67bdf6` [M11]. Upstream, this belongs in the C1 framework as a
  structured error channel.
- **Debug logging.** `G21_DIAG` is still on the shipping line: 4 hits in
  `intern/cycles/blender/session.cpp`, 4 in `source/blender/render/intern/engine.cc`
  and 1 in `source/blender/gpu/vulkan/vk_context.cc` [M8]. The removal `4c58443386e1`
  exists only on `fix/upstream-prep-20261116` (compiled, never linked or run). A
  cherry-pick onto `ee5b2931` is clean in a `git merge-tree` simulation and leaves no
  `G21_DIAG` [M11]. It has not been built on this line.
- **Generic Cycles changes (Track B).** 22 files that exist in stock Cycles
  (`intern/cycles/{kernel,scene,util,graph}`) still differ: **+1,653 / −72** [M7].
  The largest are `scene/shader_nodes.cpp` **+734/−34** and `scene/shader_nodes.h`
  **+270**. Of the `shader_nodes.cpp` additions, one 663-line block defines the 11
  MaterialX-specific nodes inside the stock file, and about 71 lines change stock
  nodes [M10]. The stock-node changes include hidden sockets that a user cannot set:
  `PrincipledBsdfNode` `base_weight`, `subsurface_color`,
  `use_independent_subsurface_color`; `AttributeNode` `use_fallback`,
  `use_materialx_exact`; `MathNode` `use_materialx_derivatives` [M10]. The
  11-17 ruling that Track B "moves into MaterialX-only code" is therefore only partly
  done. Progress since the 11-16 plan: 18 new MaterialX-named kernel/OSL files
  (+1,960) now hold code that used to sit in stock files [M7], and the changes to
  `kernel/device/cuda/compat.h` (B1), `util/math_base.h` (B2),
  `kernel/geom/triangle.h` (B4), `svm/noisetex.h`, `svm/voronoi.h`,
  `svm/white_noise.h` (B9) and `util/sha256.*` (B11) are no longer in the diff [M9].
  Still present: B3 (anisotropic glass in `bsdf_microfacet.h`), B5 (`graph/node.cpp`
  NaN compare, +15), B6/B7 (Burley/Principled/SVM closure changes), B8 (attribute
  fallback; upstream `3ff1f508e64f` landed a similar feature on main and is not in
  our line [M17]). Each needs a stock justification, a move into MaterialX-only code,
  or a drop.
- **Project apparatus in production code** (red-team Cz gate): a
  `fprintf(stderr, "MATERIALX_LOWER_NULL_SOCKET …")` at `graph.cpp:28580`, and the
  reader authors `/MaterialXManifestProbe` prims into the user stage
  (`usdshade_reader.cpp:33637`) [M13]. The material reaches Cycles through the
  `materialx_authoring.*` ID-property authority with a SHA-256 digest, which is
  project apparatus and must be replaced by reading the imported USD material (C2).
- **One stock bug fixed and proposed:** `MixNode::is_linear_operation()` looked up
  `"Factor"` instead of `"Fac"` (null dereference in volume optimisation with a
  legacy Mix RGB node). It is still present on upstream main [M16] and is open as
  PR #166886 (A1) [M20].
- **Tests.** 22 new Cycles test files, +104,525 lines, almost all in two files
  (`materialx_usdshade_reader_test.cpp` 53,283; `materialx_graph_test.cpp` 31,382),
  plus 5 fixture/goldens headers totalling 11,585 lines [M12]. They are gtests and
  Blender-side Python smokes, not yet `tests/render` cases. The test-data size needs
  a decision.

### 6.1 Validation status, stated plainly

The project ledger covers all **802** MaterialX 1.39 NodeDefs. Its parity classes
are computed by the project classifier from hash-verified certificates [M21]:

| Ledger class | Nodes | Of which final certificates | Of which pending application |
|---|---:|---:|---:|
| Both-engine parity (Cycles and NVIDIA OVRTX) | 709 | 150 | 559 |
| Cycles-only parity (OVRTX recorded, not gated) | 84 | 0 | 84 |
| Unclosed: documented boundaries | 9 | — | — |

"Pending application" means that the certificate is still a proposal, is marked
prepared-but-not-applied, or grants zero credit. The 9 boundaries are
`ND_UsdPrimvarReader_filename`, `ND_conical_edf`, `ND_geompropvalueuniform_filename`,
`ND_lama_dielectric`, `ND_lama_generalized_schlick`, `ND_lama_layer_bsdf`,
`ND_layer_bsdf`, `ND_layer_vdf`, `ND_measured_edf` [M21].

What this does and does not show:
- Parity is between Cycles and NVIDIA OVRTX. It is **not** conformance with the
  MaterialX reference implementation; that comparison has not been done.
- The ledger's `IMPLEMENTED` lowering status (649 nodes [M22]) means that a NodeDef id
  appears in a dispatch table. It is not render evidence.
- No human has reviewed the certificates.

## 7. Proposed patch sequence

1. **A1, standalone stock fix:** the `MixNode` `"Fac"` lookup, with two gtests. Built
   and tested on 5.4 main (`upstream/series-v2-main-20261117` `fdca51fc8568`;
   `cycles_test` 252/252 on Windows per the series plan §7, and on Linux per the
   CURRENT-GATES 11-18 summary; neither re-run here). Posted as PR #166886 [M20].
2. **Design task (no code):** this text, narrowed to the §10 questions.
3. **C1, framework:** CMake gate, small intermediate graph, NodeDef registry, thin
   `validate()`/lower dispatch with rollback, and a structured error channel
   (material, USD path, NodeDef, reason) that the Blender sync turns into a visible
   error shader and a UI report. Gtests only.
4. **C2, first vertical slice** (§10 Q4; proposed subset below).
5. **Family PRs**, one family per PR in its own files, each with tests and
   `tests/render` references, functions kept under about 300 lines.
6. **USD importer hook-up and UI**, after owners decide where the reader lives.
7. **Track B items** each as an independent PR with a stock justification, moved
   into MaterialX-only nodes, or dropped. B8 is rebased onto upstream `3ff1f508e64f`
   and most likely dropped.

**Proposed first subset for C2** (for the user and internal reviewer to confirm;
plan §6 item 6): the OpenPBR and Standard Surface surface terminals with literal
inputs, lowered to `PrincipledBsdfNode`, read from the imported USD material rather
than from project ID properties. Reasons: these are classed STOCK [M14], they are
what artists receive most, and the add-on already converts them to a stock
Principled BSDF (add-on `cycles_backend.py`, per `stock-expressibility.md`; not
re-measured here). This makes the slice a direct test of alternative 1 versus
alternative 2.

## 8. Testing plan

- Gtests for the reader, validation rejections and lowering structure.
- Cycles render regression cases in Blender's `tests/render` framework, with small
  reference images per family.
- Value comparison against MaterialX reference results where feasible (tooling not
  built).
- Negative tests proving that unsupported graphs fail visibly, not as a fallback.

## 9. Maintenance commitment

To be agreed by the contributing team before posting: named maintainers, response
times, and how MaterialX library upgrades are tracked. Not agreed yet.

## 10. Open questions for module owners

Kept from the draft, with Q1 and Q4 sharpened by §4.2:

1. Is native lowering acceptable as an internal translation layer, or must MaterialX
   map to Blender shader nodes/groups? If node groups: are new MaterialX-semantics
   nodes acceptable where stock nodes only match in shape (procedural noise, the
   11 nodes in §6)?
2. Is OSL via MaterialX ShaderGen preferred for the long tail of nodes?
3. Where should the UsdShade MaterialX reader live: `io/usd` or Cycles?
4. What is an acceptable first-release subset, and how should partial support be
   surfaced to users?
5. What test and reference-image expectations apply, and what maintenance is expected
   for this volume of new code?
6. Is there appetite for this, given earlier concerns about parallel node systems?

---

## Appendix A. Measurements (2026-11-23)

Identities: native line `ee5b29316847` (= `origin/release/native-materialx-line-20261122`);
fork base `96294be75080` ("Make format.", 2026-07-19), confirmed as
`git merge-base ee5b2931 upstream/main`. Draft value is shown where the draft or
the series plan had one.

| ID | Quantity | Value at `ee5b2931` | Earlier value (source) | Command |
|---|---|---|---|---|
| M1 | Lines: `graph.cpp` / `usdshade_reader.cpp` / `blender/shader.cpp` | 35,034 / 33,768 / 2,039 | 28,741 / 28,122 / 2,037 (draft §6) | `for f in intern/cycles/materialx/graph.cpp intern/cycles/materialx/usdshade_reader.cpp intern/cycles/blender/shader.cpp; do git show ee5b2931:$f \| wc -l; done` |
| M2 | Files in `intern/cycles/materialx/` | 11 (incl. `sha256.{h,cpp}`, `authority*.{h,cpp}`) | — | `git ls-tree -r --name-only ee5b2931 intern/cycles/materialx/` |
| M3 | Length of `lower_nodes()` / `validate()` | 18,298 (L16715) / 8,009 (L7151) | `lower()` 15,265 / `validate()` 6,891 (blnd01 split plan, older commit) | `git show ee5b2931:intern/cycles/materialx/graph.cpp \| awk '/^static bool lower_nodes\(/{s=NR} s && /^}/{print s, NR-s+1; exit}'` and the same with `/^bool validate\(const Graph &source,$/` |
| M4 | Functions over 300 lines: `graph.cpp` / `usdshade_reader.cpp` | 4 / 17 | reader: 13 (blnd01 split plan) | `git show ee5b2931:intern/cycles/materialx/graph.cpp \| awk '!s && /^[A-Za-z_]/ && /\(/ && !/;[ \t]*$/ && !/^(namespace\|using\|extern\|typedef)/ {s=NR; next} s && /^}[ \t]*;?[ \t]*(\/\/.*)?$/ && !/namespace/ {if (NR-s+1>300) n++; s=0} END{print n}'` (same for the reader). Heuristic: relies on clang-format column-0 layout. |
| M5 | Fork diff vs base | 891 commits; 190 files, +221,570 / −404 | 807 commits; 195 files, +205,478 / −501 (series plan §0, at `6c7388bb9d19`) | `git rev-list --count 96294be75080..ee5b2931; git diff --shortstat 96294be75080 ee5b2931` |
| M6 | Diff by destination | see §6 (MaterialX core 11 / +69,721; tests 24 / +104,695; Hydra 7 / +10,779 −226; kernel/scene/util/graph 40 / +3,613 −72; USD io 15 / +1,481 −97; Blender bridge 6 / +375 −5; add-on/Python 4 / +421 −1; engine/vk 2 / +88 −2; tooling/docs 78 / +30,389; other 3 / +8 −1) | series plan §1 table (different grouping: it put `session.cpp` under G) | `git diff --numstat 96294be75080 ee5b2931 \| awk '{p=$3; g="other"; if (p ~ /^(tools\/materialx\|docs\/\|doc\/materialx\|\.superpowers\|release\/\|\.gitignore)/) g="X"; else if (p ~ /^intern\/cycles\/materialx\//) g="M"; else if (p ~ /^intern\/cycles\/test\//) g="T"; else if (p ~ /^intern\/cycles\/hydra\//) g="H"; else if (p ~ /^intern\/cycles\/(kernel\|scene\|util\|graph)\//) g="SKU"; else if (p ~ /^intern\/cycles\/blender\/addon\// \|\| p ~ /python\.cpp$/) g="P"; else if (p ~ /^intern\/cycles\/blender\//) g="B"; else if (p ~ /^source\/blender\/io\/usd\// \|\| p ~ /bl_usd_export_test/) g="D"; else if (p ~ /(engine\.cc\|vk_context\.cc)$/) g="G"; n[g]++; a[g]+=$1; d[g]+=$2} END{for (g in n) print g, n[g], a[g], d[g]}'` |
| M7 | Track B: stock-named files modified under `intern/cycles/{kernel,scene,util,graph}` | 22 files, +1,653 / −72; `shader_nodes.cpp` +734 −34; `shader_nodes.h` +270 −0. New MaterialX-named files there: 18, +1,960 | 48 files +2,511 −166 (series plan §1 "S/K/U", different scope); "23 files" (CURRENT-GATES 11-18, scope not re-derived) | `git diff --numstat --diff-filter=M 96294be75080 ee5b2931 -- intern/cycles/kernel intern/cycles/scene intern/cycles/util intern/cycles/graph` (sum with `awk '{a+=$1;d+=$2;n++} END{print n, a, d}'`); new files: same with `--diff-filter=A` |
| M8 | `G21_DIAG` hits on the native line | `session.cpp` 4, `engine.cc` 4, `vk_context.cc` 1 | same (POST-PARITY-PLAN §0) | `git grep -c G21_DIAG ee5b2931` |
| M9 | Track B items no longer in the diff | 0 diff lines each for `kernel/device/cuda/compat.h`, `util/math_base.h`, `kernel/geom/triangle.h`, `kernel/svm/noisetex.h`, `kernel/svm/voronoi.h`, `kernel/svm/white_noise.h`, `util/sha256.h`, `test/kernel_camera_projection_test.cpp` | series plan B1, B2, B4, B9, B11 listed them | `for f in intern/cycles/kernel/device/cuda/compat.h intern/cycles/util/math_base.h intern/cycles/kernel/geom/triangle.h intern/cycles/kernel/svm/noisetex.h intern/cycles/kernel/svm/voronoi.h intern/cycles/kernel/svm/white_noise.h intern/cycles/util/sha256.h intern/cycles/test/kernel_camera_projection_test.cpp; do git diff --numstat 96294be75080 ee5b2931 -- $f \| wc -l; done` |
| M10 | MaterialX nodes in stock files; hidden sockets on stock nodes | 11 `class MaterialX*Node` in `shader_nodes.h`; 11 `NODE_DEFINE(MaterialX…)` in one 663-line added hunk at `shader_nodes.cpp:7891`; ~71 added lines elsewhere in stock nodes. Hidden sockets as listed in §6 | — | `git show ee5b2931:intern/cycles/scene/shader_nodes.h \| grep -oE 'class MaterialX[A-Za-z0-9]+Node' \| sort -u`; `git diff -U0 96294be75080 ee5b2931 -- intern/cycles/scene/shader_nodes.cpp \| grep '^@@'` (hunk `+7891,663`); `git show ee5b2931:intern/cycles/scene/shader_nodes.cpp \| sed -n '7891,8553p' \| grep -c '^NODE_DEFINE(MaterialX'`; `git show ee5b2931:intern/cycles/scene/shader_nodes.h \| awk '/^class /{c=$2} /use_materialx\|use_fallback\|base_weight\|subsurface_color\|use_independent/{print c": "$0}'`; `git show ee5b2931:intern/cycles/materialx/graph.cpp \| grep -oE 'create_node<MaterialX[A-Za-z0-9]+Node>' \| sort \| uniq -c` |
| M11 | Prep commits vs the native line | `4c58443386e1` and `40d7522f0266`: only on local `fix/upstream-prep-20261116`, not ancestors of `ee5b2931`. Simulated pick of `4c584433` onto `ee5b2931`: clean, 0 `G21_DIAG` left. `40d7522f` on top of that: **CONFLICT** in `intern/cycles/blender/shader.cpp` (touched since by `aa69ee67bdf6`) | series plan §3 (built objects only; runtime unverified) | `git branch -a --contains 4c58443386e1; git merge-base --is-ancestor 4c58443386e1 ee5b2931; echo $?`; `T=$(git merge-tree --write-tree --merge-base 4c58443386e1^ ee5b2931 4c58443386e1); git grep -c G21_DIAG $T` (no output = 0); `C=$(git commit-tree $T -p ee5b2931 -m sim); git merge-tree --write-tree --merge-base 40d7522f0266^ $C 40d7522f0266`; `git log --oneline 6c7388bb9d19..ee5b2931 -- intern/cycles/blender/shader.cpp` |
| M12 | Tests; split plan | 22 new files under `intern/cycles/test`, +104,525; 5 new headers `+11,585`; `054413b1c90` not a valid object locally | fixture headers "~11k" (series plan C3) | `git diff --numstat --diff-filter=A 96294be75080 ee5b2931 -- intern/cycles/test \| sort -rn`; `git diff --numstat --diff-filter=A 96294be75080 ee5b2931 -- 'intern/cycles/test/*fixture*' 'intern/cycles/test/*.h' \| awk '{a+=$1;n++} END{print n, a}'`; `git cat-file -t 054413b1c90` |
| M13 | Silent failure path; Cz items | `blender/shader.cpp:111` logs and the function returns `true`; `graph.cpp:28580` `MATERIALX_LOWER_NULL_SOCKET`; `usdshade_reader.cpp:33637` `/MaterialXManifestProbe` | — | `git show ee5b2931:intern/cycles/blender/shader.cpp \| sed -n 96,114p`; `git grep -n 'could not be lowered\|MATERIALX_LOWER_NULL_SOCKET\|MaterialXManifestProbe' ee5b2931 -- intern/cycles` |
| M14 | Stock-expressibility table | 239 rows, 238 unique ids (`ND_switch_float` twice); STOCK 132, STOCK_EXPANDED 94, NEEDS_UPSTREAM 10, RUNTIME_ONLY 3; CSV parity column true by class 129 / 55 / 3 / 0; `measured` 97, `inferred` 133, 9 unusable (7 malformed rows at CSV lines 29, 172, 181, 185, 197, 198, 199, plus 2 rows with `RUNTIME_ONLY` in that field); matrix group 24/24 STOCK_EXPANDED; colour group 22 STOCK_EXPANDED + 18 STOCK. Joined to the current ledger: 17 ids absent from the ledger; parity true for NEEDS_UPSTREAM 10/10, RUNTIME_ONLY 3/3, STOCK 122/132, STOCK_EXPANDED 87/94 | `stock-expressibility.md` prose says "~145 / ~60 / ~12 / ~5, ~222 handled" and lists `ND_generalized_schlick_edf` and `ND_lama_iridescence` as RUNTIME_ONLY; the CSV (authoritative per the md) says NEEDS_UPSTREAM | `python -I -c "import csv,collections,sys; r=list(csv.reader(open(sys.argv[1],encoding='utf-8')))[1:]; print(len(r), len({x[0] for x in r}), collections.Counter(x[3] for x in r), collections.Counter(x[3] for x in r if x[2].strip().lower()=='true'), collections.Counter(x[7] for x in r if len(x)==8), sum(len(x)!=8 for x in r))" docs/upstream/stock-expressibility.csv`; join (from the add-on root, with this branch's CSV path): `python -I -c "import csv,json,sys,collections; r=list(csv.reader(open(sys.argv[1],encoding='utf-8')))[1:]; L={n['node_id']:n for n in json.load(open(sys.argv[2],encoding='utf-8'))['nodes']}; print(sorted(collections.Counter((x[3], bool((L.get(x[0],{}).get('paired_input_contract_evidence') or {}).get('full_node_parity'))) for x in r).items()), [x[0] for x in r if x[0] not in L])" <csv> configs/materialx-full-catalog-node-ledger-v1.json` |
| M15 | NodeDef ids named in lowering code | `graph.cpp` 746 at `ee5b2931` (742 at `9d4f0f97`); `9d4f0f97` is 192 commits behind `ee5b2931` | — | `git show ee5b2931:intern/cycles/materialx/graph.cpp \| grep -oE '"ND_[A-Za-z0-9_]+"' \| sort -u \| wc -l`; `git rev-list --count 9d4f0f97..ee5b2931` |
| M16 | A1 bug on upstream main | present: `MixNode::is_linear_operation()` at `shader_nodes.cpp:5589` still reads `input("Factor")` (line 5600) at `de111f7f13ef` | present at `fa8c9b11` (series plan, 11-16) | [web] `curl -sS https://raw.githubusercontent.com/blender/blender/de111f7f13ef3949d0b1e9dd64445d1a3f608204/intern/cycles/scene/shader_nodes.cpp \| grep -n 'MixNode::is_linear_operation\|use_clamp == false && input("Factor")'` |
| M17 | Upstream attribute fallback `3ff1f508e64f` | in local `upstream/main` (`a24e6e330677`); not an ancestor of `ee5b2931` | series plan B8 | `git merge-base --is-ancestor 3ff1f508e64f upstream/main; echo $?; git merge-base --is-ancestor 3ff1f508e64f ee5b2931; echo $?` |
| M18 | Upstream target release | `BLENDER_VERSION 504`, cycle `alpha`, main `de111f7f13ef` (2026-11-24T02:18:02Z) | draft: 5.3; series plan: 5.4 | [web] `curl -sS https://raw.githubusercontent.com/blender/blender/main/source/blender/blenkernel/BKE_blender_version.h \| grep -E '#define BLENDER_VERSION(_CYCLE)? '`; `curl -sS https://api.github.com/repos/blender/blender/commits/main \| grep -m2 -E '"sha"\|"date"'` |
| M19 | Upstream dependency pins | `MATERIALX_VERSION 1.39.4`, `USD_VERSION 26.08` | same (draft §1) | [web] `curl -sS https://raw.githubusercontent.com/blender/blender/de111f7f13ef3949d0b1e9dd64445d1a3f608204/build_files/build_environment/cmake/versions.cmake \| grep -E '^set\((MATERIALX\|USD)_VERSION '` |
| M20 | A1 branch and PR | `fdca51fc8568` on `upstream/series-v2-main-20261117` (and `mine/fix-legacy-mixrgb-volume-crash`); PR #166886 created 2026-11-19 11:01 CST | — | `git branch -a --contains fdca51fc8568`; [add-on] `grep -n 166886 docs/REDTEAM-AUDIT-2026-11-10.md` (lines 501-502). PR state today: UNVERIFIED |
| M21 | Ledger parity classes | complete (both-engine) 709 = 150 strict + 559 pending; Cycles-only 84 (all pending); OVRTX-only 0; unclosed 9; invalid 0. Pending reasons (both-engine): proposed+not-applied 283, proposed+not-applied+zero-credit 148, proposed 97, not-applied 31 | draft: "587 paired, 150 final"; CURRENT-GATES line 1: 709/84/9; CURRENT-GATES 11-23 summary: 708/84/10 at ledger `056aa775` | [add-on] ledger `configs/materialx-full-catalog-node-ledger-v1.json` sha256 `7384b96b546a2f79f922c5515da2aa523873a35209d3b93a4883361ecdab40d7`, committed on `parity/ledger-and-records-20261123` (`863b32b6`, also on `origin`): `python -c "import json,sys; sys.path.insert(0,'scripts'); import materialx_live_dashboard as d; r=d.full_node_parity(json.load(open('configs/materialx-full-catalog-node-ledger-v1.json',encoding='utf-8'))['nodes']); print({k:v for k,v in r.items() if k.endswith('_count')})"` |
| M22 | Ledger lowering status | IMPLEMENTED 649, NOT_IMPLEMENTED 128, PARTIAL 24, PARTIAL_REQUIRED_CONTRACTS_FAIL 1 | ledger header block says 649 / 129 / 24 | [add-on] `python -c "import json,collections; L=json.load(open('configs/materialx-full-catalog-node-ledger-v1.json',encoding='utf-8')); print(collections.Counter(n['cycles_native_lowering_status'] for n in L['nodes']), L['cycles_native_lowering_counts'])"` |

## Appendix B. Series-plan §5 refreshes: where each was applied

| Series plan §5 item | Applied in v2 |
|---|---|
| §6 line counts: use measured numbers, then re-measure at the freeze | §6 and M1, M3, M4: re-measured at `ee5b2931` (the freeze line), replacing the blnd01 numbers |
| §6 "being fixed on a separate branch" | §6: named `fix/upstream-prep-20261116` `4c58443386e1` / `40d7522f0266`, not merged, runtime-unverified, and the new conflict (M11) |
| §7 step 1 points at the A1 commit | §7 step 1: now the 5.4-main rebase `fdca51fc8568` (supersedes `1e20b48fb170` on the 5.3 base) and PR #166886 |
| Add Track B items as separate PRs or dropped; B8 overlap with `3ff1f508e64f` | §6 Track B bullet and §7 step 7, with current per-item status (M7, M9, M10, M17) |
| Target release 5.4 (main), not 5.3 | §1 and M18: verified 5.4 alpha on upstream main |

## Appendix C. Internal notes (not for posting)

- Two records said the native line was not pushed. `docs/materialx-release-notes/native-line-20261122.md`:3
  is corrected in the same branch as this file. Add-on `docs/REDTEAM-AUDIT-2026-11-10.md`:1010
  (push denied) is in the other repository and is not edited here.
- The `stock-expressibility.md` prose totals disagree with its own CSV (M14). The CSV is
  stated to be authoritative, so v2 uses the CSV. The md is kept unchanged on purpose
  (it was committed byte-for-byte); correcting it is a separate edit.
- `docs/upstream/.gitattributes` keeps `stock-expressibility.csv` a plain git blob. The
  root `.gitattributes` would otherwise store it as an LFS pointer.
- Nothing in this branch changes C++.
