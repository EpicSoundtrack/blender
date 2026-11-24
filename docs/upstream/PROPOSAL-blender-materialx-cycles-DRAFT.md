# DRAFT - Proposal: native MaterialX / UsdShade support in Cycles

STATUS: internal draft. NOT submitted or posted anywhere. Not reviewed by a human.
Anything marked UNVERIFIED could not be confirmed from a page opened during drafting.

## 1. Process research (what was actually opened)

Pages successfully opened (fetched during drafting):
- Cycles module owner comments on MaterialX in Cycles:
  https://devtalk.blender.org/t/material-x-adopted-as-an-official-standard-by-asf-possibilities-for-cycles-eevee/19731
- Render & Cycles meeting notes (Aug 2022) on MaterialX/Hydra:
  https://devtalk.blender.org/t/2022-08-23-render-cycles-meeting/25591
- MaterialX is a Blender dependency (external_materialx, patch materialx_2866.diff):
  https://github.com/blender/blender/blob/main/build_files/build_environment/cmake/materialx.cmake
- Pinned versions: MaterialX 1.39.4, USD 26.08:
  https://github.com/blender/blender/blob/main/build_files/build_environment/cmake/versions.cmake
- Existing shader-node to MaterialX export code lives in source/blender/nodes/shader/materialx (files group_nodes, material, node_graph, node_item, node_parser; purposes inferred from names only):
  https://github.com/blender/blender/tree/main/source/blender/nodes/shader/materialx
- The USD importer's material code is built around UsdPreviewSurface; no MaterialX import was seen in the visible part of the file (truncated, so not conclusive):
  https://github.com/blender/blender/blob/main/source/blender/io/usd/intern/usd_reader_material.cc
- Blender 5.2 LTS: initial release July 14, 2026, supported until July 2028; the page does not mention MaterialX or USD:
  https://www.blender.org/releases/5-2/

What prior discussion says (from the devtalk pages above):
- The Cycles module owner stated the main benefit of MaterialX is interoperability and that it is mostly a subset of Cycles/EEVEE nodes. He suggested adding the most important MaterialX nodes and closures with conversion in both directions. He was "not personally convinced" about closer integration into Cycles/EEVEE and opposed "two parallel shader node evaluation systems with different limitations".
- The 2022 meeting notes said rendering MaterialX directly in Cycles/EEVEE would need many low-level nodes or conversion to OSL/GLSL, was a lot of work, and should not block initial Hydra work. Long term, Cycles/EEVEE could support low-level nodes with current high-level nodes becoming node groups.
- These are 2021-2022 statements. The current positions of the module owners are UNVERIFIED.

NOT verified (HTTP 403 from projects.blender.org and developer.blender.org in this environment, so these pages were NOT opened):
- Developer handbook contributing page: https://developer.blender.org/docs/handbook/contributing/ . A web-search snippet (not an opened page) indicates: open a Design task first for larger design questions; reviews cover design fit and technical quality; a "WIP:" title prefix holds a PR pending design agreement; ask in developer chat for extra reviews. UNVERIFIED against the page itself.
- Design task T100569 (USD layers, MaterialX, Hydra add-on): https://developer.blender.org/T100569 - UNVERIFIED (search summary only).
- MaterialX export from shader nodes, issue 112864: https://projects.blender.org/blender/blender/issues/112864 - UNVERIFIED.
- The AMD/Blender MaterialX add-on (T100894; WIP PR 104594 in blender-addons): seen only as search result titles; details UNVERIFIED.
- Current module ownership for Cycles and USD/IO, the coding-style document, test requirements, the right devtalk category, and the GPL-2.0-or-later contribution license: UNVERIFIED here and to be checked by hand before posting. (GPL-2.0-or-later is believed to be Blender's convention from general knowledge, not confirmed this session.)
- Exact current MaterialX handling in 5.x USD import/export beyond the files listed above: UNVERIFIED.

Release cadence: a roadmap result found via search (not an opened primary source) lists 5.1 (March), 5.2 LTS (July) and 5.3 (November) for 2026. LTS branches receive fixes only (per search snippets, unverified), so new features would target main for the next non-LTS release.

Where code would live: intern/cycles/ (a new materialx subdirectory, as in our tree), a hook in intern/cycles/blender/ for sync, and a UsdShade reader either in source/blender/io/usd or on the Cycles side (open question 3).

## 2. Problem and user value

Artists receive USD assets whose materials are authored as UsdShade MaterialX networks. Blender's USD importer is UsdPreviewSurface-centred, so such materials are lost or degraded, and Blender exports MaterialX only for Hydra delegates. A native path would let Cycles render MaterialX/OpenPBR content as authored, with fail-closed diagnostics for anything unsupported.

## 3. Scope and non-goals

In scope (staged): MaterialX 1.39 standard-library nodedefs (math, convert, logic, channel, procedural, texture, geometric, adjustment, compositing, color transform), standard_surface and OpenPBR lowered to Cycles closures, the UsdShade import path (authored UsdShade networks and UsdMtlx references), nodegraph interfaces.

Non-goals: a second user-facing shader evaluation system or node editor; EEVEE support; MDL; MaterialX writing (export already exists); replacing Blender shader nodes; unrelated GPU kernel changes (separate PRs with their own justification).

## 4. Architecture and alternatives

Pipeline: MaterialX/UsdShade source -> reader -> intermediate graph -> validate() (reject unsupported constructs, fail closed) -> lower() into Cycles ShaderGraph nodes -> normal Cycles compilation.

Alternatives, with trade-offs:
1. Convert to Blender shader nodes in the importer. Pro: matches the module owner's stated preference; nodes stay Blender's own and editable; EEVEE benefits; no parallel system. Con: Blender has no node for many low-level MaterialX operations, so the result is a node-group explosion or lossy; round trip is hard; the importer must own a large mapping layer.
2. Native lowering into the Cycles ShaderGraph (our implementation). Pro: direct mapping to existing Cycles nodes, precise validation, no UI surface. Con: Cycles-only, very large new code volume, and it touches the "two systems" concern. Acceptable only as an internal translation layer.
3. OSL via MaterialX ShaderGen. Pro: reuses MaterialX's own generator and reference behaviour; far less hand-written code. Con: OSL in Cycles is CPU-oriented (GPU support status UNVERIFIED), needs MaterialXGenOsl and library data files shipped, and closure mapping to Cycles still needs custom work. Not evaluated in depth by us.

Question for owners: keep native lowering as an internal layer behind the importer, or target Blender nodes/groups for part of the library.

## 5. Dependencies

MaterialX is already built as a Blender dependency (1.39.4; verified above). Our CMake links MaterialXCore and MaterialXFormat under WITH_MATERIALX and uses USD under WITH_USD. Whether Blender's prebuilt libraries on every platform ship the components and standard-library data files needed here: UNVERIFIED.

## 6. State of the code today (honest)

Line counts measured with wc -l on the local development tree:
- intern/cycles/materialx/graph.cpp: 28,741 lines (validate() starts near line 5303; lower() is roughly 15k lines and validate() roughly 7k per earlier project measurements, not re-measured in detail)
- intern/cycles/materialx/usdshade_reader.cpp: 28,122 lines
- intern/cycles/blender/shader.cpp: 2,037 lines (the MaterialX authority path)

This cannot be reviewed as is. Before submission it must be restructured:
- Split graph.cpp and usdshade_reader.cpp into per-node-family files with a registration table; the monolithic lower() must be broken up.
- A silent empty-graph fallback when lowering fails is being fixed on a separate branch; failures must be visible.
- Leftover debug logging is being removed on a separate branch.
- Unrelated generic Cycles kernel changes are mixed in (CUDA sinf/cosf accuracy, anisotropic glass, Diffuse/Metallic SVM weight+tint, triangle attribute path, NaN compare). Each must become its own PR or be dropped.
- One stock bug is identified: MixNode::is_linear_operation looked up "Factor" instead of "Fac". It is a standalone fix and the first PR.
- Existing tests are gtests in intern/cycles/test (e.g. materialx_graph_test.cpp, materialx_usdshade_reader_test.cpp) plus Blender-side Python smokes; they are not yet in the tests/render framework form.

Validation status, stated plainly: the project ledger covers the 802 MaterialX 1.39 nodedefs. 587 are claimed as paired parity between Cycles and NVIDIA OVRTX, and only 150 of those have final (not proposed) certificates. No human review has occurred. OVRTX parity is not conformance with the MaterialX reference implementation; reference comparison is still to be done.

## 7. Proposed patch sequence

1. Standalone stock fix: MixNode::is_linear_operation "Fac" lookup, with a test.
2. Design task first (no code): this proposal, narrowed to the decisions in section 4.
3. Infrastructure PR: CMake option, intermediate graph, reader skeleton, validate()/lower() framework with explicit failure reporting; gtests only.
4. Surface closures PR: standard_surface and OpenPBR mapped to Principled and existing closures, with tests.
5. Node-family PRs, one family per PR in separate files (math, convert, channel, logic, procedural, texture, geometric, ...), each with tests and render references.
6. USD importer hook-up and UI, after owners decide where the reader belongs.
Generic kernel changes go in separate, independent PRs.

## 8. Testing plan

- gtests for reader, validation rejections and lowering structure.
- Cycles render regression cases in Blender's tests/render framework (tests/python/cycles_render_tests.py exists) with small reference images per node family.
- Value comparison against MaterialX reference results where feasible (tooling not yet built).
- Negative tests proving unsupported graphs fail loudly rather than rendering a fallback.

## 9. Maintenance commitment

To be defined by the contributing team before submission: named maintainers, response times, and tracking of MaterialX library upgrades. Not yet agreed.

## 10. Open questions for module owners

1. Is native lowering acceptable as an internal translation layer, or must MaterialX map to Blender shader nodes/groups?
2. Is OSL via MaterialX ShaderGen preferred for the long tail of nodes?
3. Where should the UsdShade MaterialX reader live: io/usd or Cycles?
4. What is an acceptable first-release subset, and how should partial support be surfaced to users?
5. What test and reference-image expectations apply, and what maintenance is expected for this volume of new code?
6. Is there appetite for this, given earlier concerns about parallel node systems?
