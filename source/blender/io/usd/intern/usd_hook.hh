/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <pxr/usd/usd/common.h>
#include <pxr/usd/usdShade/material.h>

namespace blender {

struct Depsgraph;
struct Material;
struct ReportList;

namespace io::usd {

struct USDExportParams;
class USDHierarchyIterator;
struct USDImportParams;
class USDStageReader;

/** Ensure classes and type converters necessary for invoking import and export hooks
 * are registered. */
void register_hook_converters();

/** Call the 'on_export' chaser function defined in the registered #USDHook classes. */
void call_export_hooks(Depsgraph *depsgraph,
                       const USDHierarchyIterator *iter,
                       ReportList *reports);

/** Call the 'on_material_export' hook functions defined in the registered #USDHook classes. */
void call_material_export_hooks(pxr::UsdStageRefPtr stage,
                                const Material *material,
                                const pxr::UsdShadeMaterial &usd_material,
                                const USDExportParams &export_params,
                                ReportList *reports);

/** Call the 'on_import' chaser function defined in the registered USDHook classes.
 * Returns false only when a hook raises; a callback returning false can mean that it did not
 * handle the stage. */
bool call_import_hooks(USDStageReader *archive, ReportList *reports);

enum class MaterialImportHookResult {
  Unhandled,
  Handled,
  Exception,
};

/** Discover whether a hook can convert a material, distinguishing an exception from no handler. */
MaterialImportHookResult have_material_import_hook(pxr::UsdStageRefPtr stage,
                                                   const pxr::UsdShadeMaterial &usd_material,
                                                   const USDImportParams &import_params,
                                                   ReportList *reports);

/** Call the 'on_material_import' hook functions defined in the registered #USDHook classes.
 * An exception takes precedence even if another hook handled the material. */
MaterialImportHookResult call_material_import_hooks(pxr::UsdStageRefPtr stage,
                                                   Material *material,
                                                   const pxr::UsdShadeMaterial &usd_material,
                                                   const USDImportParams &import_params,
                                                   ReportList *reports);

}  // namespace io::usd
}  // namespace blender
