# Generic Cycles changes inventory (upstream series v1)

This document inventories the generic (non-MaterialX) Cycles changes present in
the `hermes/upstream-series-v1-20261110` snapshot relative to upstream Blender
`main`. The intent is to split these into justified upstream patches later;
this doc only inventories and recommends, it does not yet split.

## Method

`git diff upstream/main HEAD --stat -- intern/cycles ':!intern/cycles/materialx'`
lists every changed file outside `intern/cycles/materialx`. Because this
snapshot fork has no shared ancestry with upstream `main` (the two roots are
disjoint), a true merge-base diff is not available; the table below was built
by diffing `upstream/main` against `HEAD` directly and confirming each listed
change exists in the snapshot.

Scope note: per the task rules, `graph.cpp` and `usdshade_reader.cpp` were not
touched in this series.

## Inventory

| # | File | What it changes | MaterialX node/test that needed it | Affects non-MaterialX renders? | Recommendation |
|---|------|-----------------|-----------------------------------|-------------------------------|----------------|
| 1 | `intern/cycles/kernel/device/cuda/compat.h` | Redefines `sinf`/`cosf` to accurate large-argument versions (`ccl_sinf_accurate_large`/`ccl_cosf_accurate_large` using double-precision range reduction beyond ±π); keeps `__cosf`/`__sinf` fast path for small angles. | MaterialX accepts finite radian inputs outside [-π, π]; needed so MaterialX trig nodes match the reference on large arguments. | **Yes** — applies to every CUDA GPU render, not just MaterialX. | Keep as its own justified upstream patch (trig accuracy, broadly useful). |
| 2 | `intern/cycles/kernel/closure/bsdf_microfacet.h` | Anisotropic glass sampling/eval changes (GGX/Multi-GGX microfacet). | Anisotropic glass BSDF used by MaterialX glass/metal nodes. | **Yes** — affects any anisotropic glass/microfacet render. | Keep as its own justified upstream patch; or scope under a MaterialX-only code path if upstream reviewers prefer. |
| 3 | `intern/cycles/kernel/closure/bsdf_ashikhmin_shirley.h` | Anisotropic glass/eval tweak (single-line change). | Same as #2 (anisotropic glass). | **Yes** — affects anisotropic glass renders. | Keep as its own justified upstream patch. |
| 4 | `intern/cycles/kernel/svm/closure.h` | Closure/SVM changes supporting anisotropic glass and weight/tint handling. | Supports the Diffuse/Metallic weight and Tint inputs (#5). | **Yes** — SVM closure infra is shared. | Keep as its own justified upstream patch. |
| 5 | `intern/cycles/scene/shader_nodes.cpp` | DiffuseBsdfNode gains a `ClosureWeight` input; MetallicBsdfNode gains a `Tint` input; PrincipledBsdfNode gains `Base Weight` and weight handling changes. | MaterialX Standard Surface diffuse/metallic/sheen/coat weights and tints. | **Partly** — new socket inputs are additive (default-preserving), so existing non-MaterialX materials are unaffected; but the SVM/closure path they feed is shared. | Keep as its own justified upstream patch (additive sockets are upstream-safe). |
| 6 | `intern/cycles/scene/attribute.cpp` | Triangle float-attribute path changes (`size = mesh->num_triangles()` for subd/prim handling); NaN-bit compare via `memcmp` in `step_equals`; attribute type support narrowed (removed Quaternion/PackedSH/GSPLAT standard names). | MaterialX float attribute lookup on triangle meshes; exact attribute comparison. | **Yes** — attribute handling is shared across all renders. | Keep as its own justified upstream patch; review the type-support narrowing carefully (drops Quaternion/PackedSH/GSPLAT standard names). |
| 7 | `intern/cycles/blender/session.cpp` | Removed the four `G21_DIAG` `LOG_INFO` render-session timing lines. | None (diagnostic only). | **No** — diagnostic logging only, no render effect. | Keep as its own justified upstream patch (already committed as "Cycles: remove G21 diagnostic logging"). |

## Notes

- The task body referenced `intern/cycles/scene/node.cpp` (NaN-bit compare) and `engine.cc`/`vk_context.cc` (G21_DIAG); those files/paths do not exist in this snapshot at those locations. The G21_DIAG lines live in `intern/cycles/blender/session.cpp` (commit 2 handled). The NaN-bit compare lives in `intern/cycles/scene/attribute.cpp` (`step_equals`/`memcmp`), included as row 6.
- All socket additions use defaults that preserve existing behavior, so they are safe for upstream submission as additive changes.
- The sinf/cosf redefinition is the highest-impact generic change because it alters every CUDA render's trig results; justify it with the MaterialX large-argument accuracy requirement.
