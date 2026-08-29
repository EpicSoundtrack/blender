/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "materialx/graph.h"
#include "util/string.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

namespace materialx {

inline constexpr char authority_enabled_property[] = "materialx_authoring.cycles_native_authority";
inline constexpr char document_uuid_property[] = "materialx_authoring.document_uuid";
inline constexpr char document_digest_property[] = "materialx_authoring.document_digest";
inline constexpr char document_usda_text_property[] =
    "materialx_authoring.document_usda_text_name";
inline constexpr char material_path_property[] = "materialx_authoring.document_material_path";
inline constexpr char usda_text_prefix[] = ".materialx_usdshade_";

struct Authority {
  string document_uuid;
  string digest;
  string usda_text_name;
  string material_path;
  string usda;

  /**
   * Phase 1 manifest-bound admission (Task 2): the exact render context
   * ("" for the universal/preview context, or a named context such as
   * "mtlx") and the ordered, typed output ports authenticated during
   * parsing. Empty `selected_outputs` preserves the legacy whole-material
   * OpenPBR terminal pipeline (`lower_usdshade_authority`) unchanged.
   */
  string render_context;
  vector<SelectedOutput> selected_outputs;
};

/** Compute the canonical digest property for the exact USDA Text bytes. */
string usda_sha256_digest(const string &usda);

/**
 * Validate the renderer-neutral ownership contract passed from Blender.
 *
 * The digest binds the authority metadata to the exact USDA Text bytes. The
 * USD parser remains responsible for validating the actual stage and graph.
 */
bool is_valid(const Authority &authority);

/**
 * Structural validation for manifest-bound output-port descriptors alone
 * (Phase 1). Does not authenticate against parsed USD content -- that
 * happens during parsing via `resolve_manifest_outputs`.
 */
bool is_valid_manifest(const vector<SelectedOutput> &selected_outputs);

}  // namespace materialx

CCL_NAMESPACE_END
