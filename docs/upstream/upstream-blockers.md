# Upstream blockers for the current MaterialX/Cycles work

This is an adversarial audit of the current branch for changes that are likely
to block review if submitted as-is.  I did not find an in-tree
`CONTRIBUTING.md`; process claims below are therefore either backed by files in
this repository or marked **UNVERIFIED**.

## Ranking

| Rank | Blocker | Severity | Cost | Why it blocks |
| --- | --- | --- | --- | --- |
| 1 | Two enormous translation units and two enormous functions | High | High | `intern/cycles/materialx/graph.cpp` is 26,899 lines and `lower()` alone is 14,510 lines; `usdshade_reader.cpp` is 22,501 lines and has five reader functions over 1,400 lines. This is not just style: `graph.cpp` still carries an explicit comment saying branches were hoisted to stay under MSVC C1061. |
| 2 | Project-private authority/manifest/digest apparatus is in production Cycles code | High | High | `materialx_authoring.*`, `authority_pipeline.*`, digest checks, fixture digest authentication, and manifest-bound admission are validation infrastructure for this project, not general Blender material import/export behavior. |
| 3 | Fail-closed behavior reaches the user only as a log plus an empty graph | High | Medium | Rejections are correct boundaries, but a selected MaterialX authority material can render as no authored material if lowering fails. The user-facing communication is not visible enough for an upstream feature. |
| 4 | Reader contains a private add-on/OVRTX fallback convention | High | Medium | `resolve_controlled_scoped_output()` explicitly admits a private `materialx_authoring` controlled-USD sibling graph layout, with direct references to `extensions/materialx_authoring` and `exact91_ovrtx`. |
| 5 | Comment density and tone are not upstream-ready | Medium | Medium | The code repeatedly says `Task N`, `Phase N`, `exact91`, `value oracle`, `OVRTX`, and explains project incidents rather than durable Blender/Cycles design. |
| 6 | Runtime MaterialX library loading in the reader would be a blocker if reintroduced | Medium | Medium | I could not find the referenced `552c0e65485` commit in this checkout, and the current Cycles MaterialX reader does not link `MaterialXCore`/`MaterialXFormat`. But the proposed shape -- load MaterialX standard libraries from disk during shader translation to discover one NodeDef default -- should be treated as not upstreamable unless a Blender reviewer explicitly accepts it. |

## Evidence and details

### 1. File/function size and nesting

Measured on this checkout:

| File | Lines | Approx. functions | Longest function | Worst measured brace/control nesting |
| --- | ---: | ---: | --- | --- |
| `intern/cycles/materialx/graph.cpp` | 26,899 | 299 | `lower()` at lines 12386-26895, 14,510 lines | `lower()` max brace depth 7; approximate control nesting 10 |
| `intern/cycles/materialx/usdshade_reader.cpp` | 22,501 | 231 | `read_color_output()` at lines 8895-11389, 2,495 lines | `read_color4_output()` max brace/control depth 7 |

Other large functions:

- `graph.cpp:4773-11181` `validate()` is 6,409 lines.
- `usdshade_reader.cpp:6847-8893` `read_color4_output()` is 2,047 lines.
- `usdshade_reader.cpp:13469-15385` `read_float_output()` is 1,917 lines.
- `usdshade_reader.cpp:15513-17226` `read_vector3_output()` is 1,714 lines.
- `usdshade_reader.cpp:12048-13467` `read_vector2_output()` is 1,420 lines.
- `usdshade_reader.cpp:4231-5645` `read_vector4_output()` is 1,415 lines.
- `usdshade_reader.cpp:21111-22213` `read_usdshade_graph()` is 1,103 lines.

The C1061 risk is still real enough to be self-documented in the code:
`graph.cpp:12417-12423` says Vector4/Color4 adapter branches were hoisted out of
the main `else-if` chain "purely to keep the chain's nesting depth under MSVC's
internal block-nesting limit (C1061)".  Even if the current measured brace depth
is lower than the historical failure, keeping a 14.5k-line dispatch function with
an MSVC-limit workaround is a review blocker.

Actionable fix: split the reader/lowerer by semantic families and replace the
single `lower()`/typed-reader mega-dispatches with a registry/table of small
handlers.  Keep validation and lowering adjacent per family so unsupported
boundaries remain explicit, but make each family independently reviewable and
testable.

### 2. Runtime MaterialX dependency: current state and safer alternative

I could not find commit `552c0e65485` in this checkout (`git rev-list --all` has
no matching SHA), so I could not inspect that exact change.  The current
`intern/cycles/materialx/CMakeLists.txt` does **not** link MaterialXCore or
MaterialXFormat into `cycles_materialx`: it builds `authority.cpp` and
`graph.cpp`, and when `WITH_USD` is enabled it adds `authority_pipeline.cpp`,
`usdshade_reader.cpp`, USD, and Python only (`intern/cycles/materialx/CMakeLists.txt:12-38`).

Repository evidence for existing MaterialX library use elsewhere:

- Shader-node MaterialX export links `MaterialXCore` and `MaterialXFormat`
  (`source/blender/nodes/shader/CMakeLists.txt:160-164`) and includes
  `<MaterialXFormat/XmlIo.h>` (`source/blender/nodes/shader/materialx/material.cc:5`).
- USD code links `MaterialXCore` when `WITH_MATERIALX` is enabled
  (`source/blender/io/usd/CMakeLists.txt:219-222`) and calls `pxr::UsdMtlxRead`
  during export/Hydra paths (`source/blender/io/usd/intern/usd_writer_material.cc:1581`,
  `source/blender/io/usd/hydra/material.cc:149-151`).
- The current Cycles reader uses USD and project authority, not direct MaterialX
  XML loading, in its material path (`intern/cycles/materialx/CMakeLists.txt:26-38`).

**UNVERIFIED reviewer claim:** I expect a Blender reviewer to reject shader
translation that loads MaterialX standard-library XML from disk through USD's
`PlugRegistry` just to read a NodeDef default, because it adds runtime file I/O,
plugin/discovery sensitivity, and a second source of truth to material sync.  I
cannot cite a repository rule that forbids it.  Confirmation would require a
Cycles/USD module owner review.

Actionable alternative: keep correctness without hand-copying by generating a
small checked-in default table from the authoritative MaterialX/UsdMtlx library
as a build-maintenance artifact, plus a test that regenerates/compares it in
`WITH_MATERIALX` test builds.  The runtime reader then consumes only the checked
constant/table.  If the value is already authored into USD by `UsdMtlxRead`, use
that authored value directly and test the USD fixture; do not load the MaterialX
library during Cycles shader translation.

### 3. Project-specific validation apparatus in source code

These are not generic Blender mechanisms; they are specific to this validation
pipeline and should be removed, moved to tests/tools, or explicitly redesigned as
a real product feature before upstream submission.

| Location | Evidence | Recommendation |
| --- | --- | --- |
| `intern/cycles/blender/shader.cpp:11` | Includes `materialx/authority_pipeline.h` under `WITH_USD`. | Remove from production shader sync unless this becomes an accepted Blender feature with UI/docs. |
| `intern/cycles/blender/shader.cpp:94-111` | `build_materialx_graph_from_id_authority()` selects an ID-property authority contract, logs errors, and returns `true` after failures. | Replace with a normal Blender material/import path or move to tests/tools. If kept, it needs visible user errors. |
| `intern/cycles/blender/materialx_authority.cpp:66-104` | Reads `materialx_authoring.*` ID properties and a referenced Text datablock. | Project/add-on bridge; remove or move out of core Cycles. |
| `intern/cycles/blender/materialx_authority.h:16-24` | `BlenderMaterialXAuthority` wraps project authority state. | Same as above. |
| `intern/cycles/materialx/CMakeLists.txt:28,32` | Builds `authority_pipeline.cpp/.h` into `cycles_materialx`. | Do not ship validation pipeline in production Cycles target. |
| `intern/cycles/materialx/authority.h:15-21` | Hard-coded ID properties `materialx_authoring.cycles_native_authority`, `.document_uuid`, `.document_digest`, `.document_usda_text_name`, `.document_material_path`. | Remove or move to an experimental add-on/module; these are not general USD/MaterialX schema fields. |
| `intern/cycles/materialx/authority.h:23-48` | `Authority` stores digest, USDA text, selected outputs, fixture digests. | Keep only in test harness/tools unless an upstream design accepts signed in-memory USD as a feature. |
| `intern/cycles/materialx/authority.cpp:56-78` | `usda_sha256_digest()` and `is_valid()` bind material to exact USDA Text bytes. | Validation harness concern; not normal material translation. |
| `intern/cycles/materialx/authority_pipeline.cpp:32-76` | Parses in-memory USDA authority and lowers it through the reader. | Move to tests/tools or replace with normal USD material import path. |
| `intern/cycles/materialx/authority_pipeline.cpp:79-141` | `resolve_usdshade_authority_outputs()` authenticates selected outputs and manifest. | Move to test harness. |
| `intern/cycles/materialx/authority_pipeline.cpp:144-179` | `authenticate_resolved_fixture_bytes()` reads texture files and compares `sha256:` fixture digests. | Remove from production runtime; if needed for tests, keep test-only. |
| `intern/cycles/materialx/authority_pipeline.h:23-55` | Documents manifest-bound admission and fixture-bound authentication. | Move to tests/tools. |
| `intern/cycles/materialx/graph.h:72-88` | `SelectedOutput` is a manifest-bound output descriptor. | If generic multi-output lowering is needed, rename/reframe without manifest authentication language. |
| `intern/cycles/materialx/usdshade_reader.cpp:2111-2120` | Traversal comment says it is used by the Phase 1 manifest-bound resolver. | Reword/remove with the manifest resolver. |
| `intern/cycles/materialx/usdshade_reader.cpp:22217-22497` | `manifest_output_usd_type()`, `dispatch_typed_output()`, and `resolve_manifest_outputs()` implement manifest-bound admission. | Move to tests/tools or redesign as normal public API without project manifest semantics. |
| `intern/cycles/materialx/usdshade_reader.cpp:845-846` | Mentions `docs/findings/materialx/place2d-cycles-ovrtx-disagreement.md` and OVRTX. | Remove project/OVRTX reference; keep only the technical rule. |
| `intern/cycles/materialx/usdshade_reader.cpp:6693` | Mentions the same OVRTX finding from production code. | Remove/reword. |
| `intern/cycles/materialx/usdshade_reader.cpp:17714-17720` | Names `materialx_authoring`, `extensions/materialx_authoring`, `scripts/blender/exact91_ovrtx`, and calls the behavior private/add-on-specific. | Remove from production reader or isolate behind test-only compatibility. |

Notes on false positives:

- Generic Blender `manifest` uses in Windows packaging, source archives,
  Cryptomatte, and Python add-on metadata are not blockers.
- Existing NVIDIA copyright lines in `source/blender/io/usd/intern/*` are
  already present repository content and are not by themselves a MaterialX
  blocker.  New OVRTX/NVIDIA project references in the MaterialX reader are the
  issue.
- I found no `attestation` matches in the production MaterialX/Cycles source
  paths inspected.

### 4. Fail-closed boundaries are correct but poorly communicated

The code intentionally refuses unsupported shapes.  Examples include:

- `graph.cpp:4773-11181` is a 6,409-line `validate()` with many `return false`
  boundaries.
- `graph.cpp:12386-12409` rolls back and sets `error_message` when validation
  rejects a node.
- `usdshade_reader.cpp:6671-6675` rejects unsupported Matrix44 nodes with a
  named error.
- `usdshade_reader.cpp:17763-17768` treats missing unconnected optional
  displacement as absent, while malformed connected terminals fail.

Current user-visible path for Blender materials:

- `intern/cycles/blender/shader.cpp:94-111` says return `false` only when
  MaterialX authority was not selected; once selected, malformed/missing fields
  fail closed so an unrelated Blender node tree cannot silently replace the
  authored document.
- Invalid authority logs `LOG_ERROR << "MaterialX authority is invalid: ..."`
  and returns `true` (`intern/cycles/blender/shader.cpp:102-105`).
- Lowering failure logs `LOG_ERROR << "MaterialX authority could not be lowered: ..."`
  and still returns `true` (`intern/cycles/blender/shader.cpp:107-111`).

What the user sees now: unless they inspect logs, the selected MaterialX path can
produce a material with no successful authored Cycles graph rather than a clear
viewport/render error.  The branch avoids falling back to an unrelated node tree,
which is good, but it does not surface the rejection as a material/import problem
in the UI or an inspectable material status.

Actionable fix: propagate structured diagnostics from reader/lowerer through
shader sync into a user-visible report/status: material name, USD path or node
name, rejected NodeDef, reason, and whether Blender used fallback/default
material output.  Add tests for the communication path, not just for rollback.

### 5. Comment density and tone

Rough comment-density scan:

- `graph.cpp`: 1,465 comment-ish lines, 692 blank lines, 24,742 code-ish lines.
- `usdshade_reader.cpp`: 1,497 comment-ish lines, 620 blank lines, 20,384
  code-ish lines.

High comment count is not automatically bad; many comments cite exact MaterialX
NodeDef semantics.  The blocker is audience/tone.  Samples that would need
rewrite:

- `graph.h:26-43`, `graph.h:57-63`, `graph.h:72-82`, `graph.h:96-98`,
  `graph.h:114-122`, `graph.h:158-184`: repeated `Task N` / `Phase N` notes.
- `usdshade_reader.h:20-26` and `usdshade_reader.h:36-55`: task/phase framing.
- `authority_pipeline.cpp:128-132`: "Task 7" and regression notes about tasks
  2-6.
- `usdshade_reader.cpp:17714-17720`: private add-on-specific controlled USD,
  `extensions/materialx_authoring`, and `exact91_ovrtx` references.
- `usdshade_reader.cpp:3069-3072`: refers to the "live scene used by the value
  oracle" rather than a general Blender/Cycles reason.
- `usdshade_reader.cpp:845-846` and `usdshade_reader.cpp:6693`: OVRTX finding
  references in production source.

Actionable fix: rewrite comments for a reader with no project history.  Keep
normative facts such as "MaterialX stdlib_defs.mtlx declares..." and "Cycles has
no socket for..."; remove task numbers, incident history, oracle language,
OVRTX/project docs, and test-batch references.

## Additional observations

- The current `texcoord_attribute_name()` maps MaterialX UV0 to the concrete
  Blender attribute `"UVMap"` (`usdshade_reader.cpp:3067-3073`) while the
  surrounding comment also says Cycles' empty UVMap attribute reads `ATTR_STD_UV`
  (`usdshade_reader.cpp:3059-3065`).  This may be correct for the project oracle,
  but it is fragile for upstream unless tied to Blender's existing USD UV naming
  options.  Repository evidence: USD export optionally renames active UV maps to
  `"st"` for USD/MaterialX compatibility (`source/blender/io/usd/intern/usd_writer_mesh.cc:271-274`).
- The MaterialX code is guarded by optional dependency patterns in some places
  (`intern/cycles/test/CMakeLists.txt:61-65`, `source/blender/io/usd/CMakeLists.txt:219-222`).
  Any upstream proposal should preserve those gates and not make Cycles shader
  sync require MaterialX unless the build option is explicitly enabled.

## Minimum unblock plan

1. Delete or isolate the authority/manifest/digest pipeline from production
   Cycles code; keep it only as tests/tools if it remains useful for validation.
2. Split `graph.cpp` and `usdshade_reader.cpp` by family, starting with the
   14.5k-line `lower()` and 6.4k-line `validate()` functions.
3. Replace project comments with durable technical comments and remove OVRTX,
   exact91, task/phase, and value-oracle references from production code.
4. Add a real diagnostic path for unsupported MaterialX nodes that is visible to
   the user and covered by tests.
5. If any runtime MaterialX default lookup is reintroduced, replace it with a
   generated checked-in table plus verification test, or get explicit module-owner
   approval for runtime library loading (**UNVERIFIED** acceptance criterion).
