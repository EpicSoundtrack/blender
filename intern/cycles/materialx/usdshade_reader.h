/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pxr/usd/usdShade/material.h>

#include "materialx/graph.h"
#include "util/string.h"

CCL_NAMESPACE_BEGIN

namespace materialx {

/**
 * Read the supported portion of an authoritative USDShade MaterialX material
 * into the renderer-independent graph IR.
 *
 * Task 3 (metadata-driven terminal routing): surface, volume, displacement,
 * and light terminals are discovered and validated independently -- a
 * material with only a volume terminal (no surface) is no longer rejected,
 * which is the fix for the previous volume early-return. All co-authored
 * terminal slots are preserved atomically: the destination is replaced only
 * after every authored terminal on the material has independently
 * validated; if any authored terminal fails, none of them are committed.
 * A discovered lightshader terminal is never folded into the Surface
 * output -- it is exposed via `Graph::has_light` for a caller to route
 * through the light path instead of a UsdShade Material output.
 */
bool read_usdshade_graph(const pxr::UsdShadeMaterial &material,
                         Graph *graph,
                         string *error_message = nullptr);

/**
 * Phase 1 generic admission and typed output selection (Task 2).
 *
 * Resolve one or more manifest-bound output ports -- each an exact node
 * path, NodeDef identifier, output name, and declared float/color3/vector2/
 * vector3 type -- from an authenticated USDShade material into the shared
 * graph IR. No NodeDef allowlist is applied at ingress: any NodeDef
 * reachable from the authored material terminal in the exact given render
 * context ("" for the universal/preview context, or a named context such as
 * "mtlx") is admissible as long as its selected output authenticates
 * exactly against the manifest. This is the one typed output resolver that
 * replaces separately invoking the four duplicated float/color3/vector2/
 * vector3 readers.
 *
 * All selected outputs are resolved against the same immutable graph and
 * aggregated, in order, into `graph`/`results`. If any selected output
 * fails to authenticate or resolve -- wrong digest/path/NodeDef/output/
 * type/context, an unreachable output, or an invalid multi-output
 * selection -- `graph` and `results` are left unmodified and the call fails
 * closed; no partial multi-output receipt is produced.
 */
bool resolve_manifest_outputs(const pxr::UsdShadeMaterial &material,
                              const string &render_context,
                              const vector<SelectedOutput> &selected_outputs,
                              Graph *graph,
                              vector<Link> *results,
                              string *error_message = nullptr);

}  // namespace materialx

CCL_NAMESPACE_END
