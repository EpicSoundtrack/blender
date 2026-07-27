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

}  // namespace materialx

CCL_NAMESPACE_END
