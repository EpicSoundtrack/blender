/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "materialx/authority_pipeline.h"

#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdShade/material.h>

#include "materialx/graph.h"
#include "materialx/usdshade_reader.h"
#include "util/path.h"
#include "util/sha256.h"

CCL_NAMESPACE_BEGIN

namespace materialx {

namespace {

void set_error(string *error_message, const string &message)
{
  if (error_message) {
    *error_message = message;
  }
}

}  // namespace

bool lower_usdshade_authority(const Authority &authority,
                              ShaderGraph *graph,
                              string *error_message)
{
  if (graph == nullptr) {
    set_error(error_message, "A destination Cycles graph is required");
    return false;
  }
  if (!is_valid(authority)) {
    set_error(error_message, "MaterialX authority contract is invalid");
    return false;
  }

  const pxr::SdfLayerRefPtr layer = pxr::SdfLayer::CreateAnonymous(".usda");
  if (!layer || !layer->ImportFromString(authority.usda)) {
    set_error(error_message, "MaterialX authority USDA could not be parsed");
    return false;
  }

  const pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(layer);
  if (!stage) {
    set_error(error_message, "MaterialX authority USD stage could not be opened");
    return false;
  }

  const pxr::UsdShadeMaterial material(
      stage->GetPrimAtPath(pxr::SdfPath(authority.material_path)));
  if (!material) {
    set_error(error_message, "MaterialX authority material path is missing or not a material");
    return false;
  }

  Graph source;
  if (!read_usdshade_graph(material, &source, error_message)) {
    return false;
  }
  if (!lower(source, graph)) {
    set_error(error_message, "MaterialX shared graph could not be lowered to Cycles");
    return false;
  }
  return true;
}

bool resolve_usdshade_authority_outputs(const Authority &authority,
                                        Graph *graph,
                                        vector<Link> *results,
                                        string *error_message)
{
  if (graph == nullptr || results == nullptr) {
    set_error(error_message, "Destination graph and results are required");
    return false;
  }
  if (!is_valid(authority)) {
    set_error(error_message, "MaterialX authority contract is invalid");
    return false;
  }
  if (authority.selected_outputs.empty()) {
    set_error(error_message, "MaterialX authority manifest has no selected outputs");
    return false;
  }

  const pxr::SdfLayerRefPtr layer = pxr::SdfLayer::CreateAnonymous(".usda");
  if (!layer || !layer->ImportFromString(authority.usda)) {
    set_error(error_message, "MaterialX authority USDA could not be parsed");
    return false;
  }

  const pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(layer);
  if (!stage) {
    set_error(error_message, "MaterialX authority USD stage could not be opened");
    return false;
  }

  const pxr::UsdShadeMaterial material(
      stage->GetPrimAtPath(pxr::SdfPath(authority.material_path)));
  if (!material) {
    set_error(error_message, "MaterialX authority material path is missing or not a material");
    return false;
  }

  Graph local_graph;
  vector<Link> local_results;
  if (!resolve_manifest_outputs(material,
                                authority.render_context,
                                authority.selected_outputs,
                                &local_graph,
                                &local_results,
                                error_message))
  {
    return false;
  }

  /* Task 7: fixture-bound authentication. Only runs when the authority
   * actually declares fixture digests -- an authority with none behaves
   * exactly as before this task (no regression to Tasks 2-6's tests,
   * every one of which constructs an `Authority` with an implicitly
   * empty/default `fixture_digests`). */
  if (!authority.fixture_digests.empty() &&
      !authenticate_resolved_fixture_bytes(local_graph, authority.fixture_digests, error_message))
  {
    return false;
  }

  *graph = std::move(local_graph);
  *results = std::move(local_results);
  return true;
}

bool authenticate_resolved_fixture_bytes(const Graph &graph,
                                         const unordered_map<string, string> &fixture_digests,
                                         string *error_message)
{
  for (const Node &node : graph.nodes) {
    const auto file = node.asset_inputs.find("file");
    if (file == node.asset_inputs.end()) {
      continue;
    }
    const auto digest = fixture_digests.find(file->second);
    if (digest == fixture_digests.end()) {
      set_error(error_message,
               "No manifest-authenticated fixture digest for asset path: " + file->second);
      return false;
    }
    vector<uint8_t> bytes;
    if (!path_read_binary(file->second, bytes)) {
      set_error(error_message, "Fixture asset could not be read: " + file->second);
      return false;
    }
    const string actual_digest =
        "sha256:" +
        util_sha256_string(string(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
    if (actual_digest != digest->second) {
      set_error(error_message,
               "Fixture digest mismatch for asset path: " + file->second + " (expected " +
                   digest->second + ", got " + actual_digest + ")");
      return false;
    }
  }
  return true;
}

}  // namespace materialx

CCL_NAMESPACE_END
