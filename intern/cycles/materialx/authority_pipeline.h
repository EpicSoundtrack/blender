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

/**
 * Task 7: fixture-bound authentication (texture bytes).
 *
 * Given a resolved `Graph` (as produced by `resolve_manifest_outputs`/
 * `read_usdshade_graph`) and a manifest-declared map of exact asset path
 * (matching `Node::asset_inputs["file"]`) -> required `"sha256:<hex>"`
 * digest, authenticate every image/texture node's referenced file against
 * its declared digest, reading the file's actual raw bytes from disk.
 * Fails closed -- with a named error identifying the exact asset path --
 * on a fixture path with no manifest-declared digest, an unreadable file,
 * or a digest mismatch. An empty `fixture_digests` with at least one
 * fixture-bearing node in `graph` also fails closed (no fixture is
 * implicitly trusted); an empty `graph` with no fixture-bearing nodes at
 * all trivially succeeds.
 */
bool authenticate_resolved_fixture_bytes(const Graph &graph,
                                         const unordered_map<string, string> &fixture_digests,
                                         string *error_message = nullptr);

}  // namespace materialx

CCL_NAMESPACE_END
