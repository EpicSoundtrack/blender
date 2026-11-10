/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/common.h>

#include <memory>

namespace blender {

struct bNode;
struct bNodeTree;

struct Main;
struct Scene;
struct World;

namespace io::usd {

struct USDExportParams;
struct USDImportParams;

/* This struct contains all DomeLight attribute needed to
 * create a world environment */
struct USDImportDomeLightData {
  float intensity;
  pxr::GfVec3f color;
  pxr::SdfAssetPath tex_path;
  pxr::TfToken pole_axis;

  bool has_color;
  bool has_tex;
};

/**
 * Journal the node-tree edits made by a DomeLight import without replacing preexisting IDs,
 * nodes or links. Call finish_dome_light() immediately after conversion and restore() on failure.
 *
 * The World must remain alive until this snapshot is destroyed. An invalid snapshot must not
 * protect a World mutation. restore() refuses a tree that has changed since conversion, such as
 * an unsupported import-hook edit.
 */
class USDWorldStateSnapshot {
 public:
  USDWorldStateSnapshot(World *world, Main *bmain);
  ~USDWorldStateSnapshot();

  USDWorldStateSnapshot(const USDWorldStateSnapshot &) = delete;
  USDWorldStateSnapshot &operator=(const USDWorldStateSnapshot &) = delete;

  bool is_valid() const;
  bool finish_dome_light();
  bool restore();

 private:
  struct State;
  std::unique_ptr<State> state_;
};

/**
 * If the Blender scene has an environment texture,
 * export it as a USD dome light.
 */
void world_material_to_dome_light(const USDExportParams &params,
                                  const Scene *scene,
                                  pxr::UsdStageRefPtr stage);

void dome_light_to_world_material(const USDImportParams &params,
                                  Scene *scene,
                                  Main *bmain,
                                  const USDImportDomeLightData &dome_light_data,
                                  const pxr::UsdPrim &prim,
                                  const pxr::UsdTimeCode time = 0.0);

}  // namespace io::usd
}  // namespace blender
