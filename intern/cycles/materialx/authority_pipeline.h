/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "materialx/authority.h"

CCL_NAMESPACE_BEGIN

class ShaderGraph;

namespace materialx {

/**
 * Validate and lower one canonical in-memory USDShade authority source through
 * the shared MaterialX graph IR into a transient Cycles graph.
 */
bool lower_usdshade_authority(const Authority &authority,
                              ShaderGraph *graph,
                              string *error_message = nullptr);

/**
 * Validate one canonical in-memory USDShade authority source and resolve
 * its manifest-bound `selected_outputs` (Phase 1 generic admission, Task 2)
 * into the shared graph IR.
 *
 * The authority contract (graph SHA-256, material path, render context, and
 * per-output NodeDef/output-name/type descriptors) is fully authenticated
 * before and during parsing; a tampered digest, invalid manifest, or any
 * selected-output mismatch fails closed without mutating `graph`/`results`.
 */
bool resolve_usdshade_authority_outputs(const Authority &authority,
                                        Graph *graph,
                                        vector<Link> *results,
                                        string *error_message = nullptr);

}  // namespace materialx

CCL_NAMESPACE_END
