/* SPDX-FileCopyrightText: 2023 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_asset_utils.hh"
#include "usd.hh"

#include <pxr/usd/ar/asset.h>
#include <pxr/usd/ar/packageUtils.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/ar/writableAsset.h>
#include <pxr/usd/usd/common.h>
#include <pxr/usd/usd/stage.h>

#include "BKE_appdir.hh"
#include "BKE_idprop.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"

#include "BLI_fileops.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"
#include "BLI_string_utils.hh"
#include "BLI_time.hh"

#include "WM_api.hh"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string_view>
#include <utility>

namespace blender::io::usd {

constexpr char UDIM_PATTERN[] = "<UDIM>";
constexpr char UDIM_PATTERN2[] = "%3CUDIM%3E";

/* Maximum range of UDIM tiles, per the
 * UsdPreviewSurface specifications.  See
 * https://graphics.pixar.com/usd/release/spec_usdpreviewsurface.html#texture-reader
 */
constexpr int UDIM_START_TILE = 1001;
constexpr int UDIM_END_TILE = 1100;

AssetImportTransaction::~AssetImportTransaction()
{
  if (state_ == State::Active && !rollback()) {
    WM_global_reportf(RPT_ERROR, "USD asset import: Couldn't restore all copied asset files");
  }
  else if (state_ == State::Committed && !commit()) {
    WM_global_reportf(RPT_WARNING, "USD asset import: Couldn't remove all asset backup files");
  }
}

bool AssetImportTransaction::prepare_directory(const std::string &path, ReportList *reports)
{
  if (state_ != State::Active || failed_ || path.size() >= FILE_MAX ||
      !BLI_path_is_abs_from_cwd(path.c_str()) || pxr::ArIsPackageRelativePath(path))
  {
    BKE_reportf(reports, RPT_ERROR, "Cannot transactionally create import directory %s", path.c_str());
    failed_ = true;
    return false;
  }

  const std::filesystem::path directory = std::filesystem::u8path(path);
  if (!directory.is_absolute()) {
    BKE_reportf(reports, RPT_ERROR, "Import directory is not local and absolute: %s", path.c_str());
    failed_ = true;
    return false;
  }

  std::filesystem::path current = directory.root_path();
  std::error_code error;
  const std::filesystem::file_status root_status = std::filesystem::symlink_status(current, error);
  if (error || !std::filesystem::is_directory(root_status)) {
    BKE_reportf(reports, RPT_ERROR, "Cannot inspect import directory root for %s", path.c_str());
    failed_ = true;
    return false;
  }

  for (const std::filesystem::path &component : directory.relative_path()) {
    if (component == ".") {
      continue;
    }
    if (component == "..") {
      BKE_reportf(reports, RPT_ERROR, "Import directory contains parent traversal: %s", path.c_str());
      failed_ = true;
      return false;
    }
    current /= component;

    error.clear();
    const std::filesystem::file_status status = std::filesystem::symlink_status(current, error);
    if (!error && std::filesystem::is_directory(status)) {
      continue;
    }
    if ((!error || error == std::errc::no_such_file_or_directory) &&
        !std::filesystem::exists(status))
    {
      error.clear();
      if (std::filesystem::create_directory(current, error)) {
        created_directories_.push_back(current);
        continue;
      }
      /* Another process may have created it first. Such a directory is never ours. */
      error.clear();
      const std::filesystem::file_status raced_status = std::filesystem::symlink_status(current,
                                                                                       error);
      if (!error && std::filesystem::is_directory(raced_status)) {
        continue;
      }
    }
    BKE_reportf(reports, RPT_ERROR, "Cannot safely create import directory %s", path.c_str());
    failed_ = true;
    return false;
  }
  return true;
}

bool AssetImportTransaction::prepare_destination(const std::string &path, ReportList *reports)
{
  if (state_ != State::Active) {
    BKE_report(reports, RPT_ERROR, "USD asset transaction cannot copy after finalization");
    return false;
  }
  if (failed_) {
    BKE_report(reports, RPT_ERROR, "USD asset transaction cannot copy after a failed asset copy");
    return false;
  }

  const std::filesystem::path file_path = std::filesystem::u8path(path);
  if (path.size() >= FILE_MAX || !BLI_path_is_abs_from_cwd(path.c_str()) ||
      !file_path.is_absolute() || pxr::ArIsPackageRelativePath(path))
  {
    BKE_reportf(reports, RPT_ERROR, "Cannot roll back non-local asset path %s", path.c_str());
    failed_ = true;
    return false;
  }

  for (const Destination &destination : destinations_) {
    if (BLI_path_cmp_normalized(destination.path.c_str(), path.c_str()) == 0) {
      return true;
    }
  }

  /* Inspect the path itself rather than following a symlink. A dangling link must not be mistaken
   * for a new file, and replacing a link could write outside the recorded destination. */
  std::error_code error;
  const std::filesystem::file_status status = std::filesystem::symlink_status(file_path, error);
  if (error && error != std::errc::no_such_file_or_directory) {
    BKE_reportf(reports, RPT_ERROR, "Cannot inspect asset destination %s", path.c_str());
    failed_ = true;
    return false;
  }
  const bool existed = std::filesystem::exists(status);
  if (existed && !std::filesystem::is_regular_file(status)) {
    BKE_reportf(reports, RPT_ERROR, "Asset destination is not a regular file: %s", path.c_str());
    failed_ = true;
    return false;
  }

  Destination destination{path, {}};
  if (existed) {
    char dir[FILE_MAX];
    BLI_path_split_dir_part(path.c_str(), dir, sizeof(dir));

    static std::atomic<uint64_t> backup_serial{0};
    const int64_t backup_time = BLI_time_now_seconds_i();
    bool moved = false;
    for (int attempt = 0; attempt < 100; ++attempt) {
      const std::string name = ".usd_asset_backup_" + std::to_string(backup_time) + "_" +
                               std::to_string(backup_serial.fetch_add(1));
      if (strlen(dir) + name.size() + 1 >= FILE_MAX) {
        break;
      }
      char backup_path[FILE_MAX];
      BLI_path_join(backup_path, sizeof(backup_path), dir, name.c_str());
      if (BLI_path_cmp_normalized(path.c_str(), backup_path) == 0) {
        continue;
      }
      if (BLI_rename(path.c_str(), backup_path) == 0) {
        destination.backup_path = backup_path;
        moved = true;
        break;
      }
      if (!BLI_exists(backup_path)) {
        break;
      }
    }
    if (!moved) {
      BKE_reportf(reports, RPT_ERROR, "Cannot back up asset destination %s", path.c_str());
      failed_ = true;
      return false;
    }
  }

  destinations_.push_back(std::move(destination));
  return true;
}

bool AssetImportTransaction::commit(ReportList *reports)
{
  if (state_ == State::RolledBack) {
    BKE_report(reports, RPT_ERROR, "Cannot commit a rolled back USD asset transaction");
    return false;
  }
  if (failed_) {
    BKE_report(reports, RPT_ERROR, "Cannot commit a failed USD asset transaction");
    return false;
  }
  state_ = State::Committed;

  bool success = true;
  for (Destination &destination : destinations_) {
    if (!destination.backup_path.empty()) {
      if (BLI_delete(destination.backup_path.c_str(), false, false) != 0) {
        BKE_reportf(reports,
                    RPT_WARNING,
                    "Cannot remove USD asset backup %s",
                    destination.backup_path.c_str());
        success = false;
      }
      else {
        destination.backup_path.clear();
      }
    }
  }
  return success;
}

bool AssetImportTransaction::rollback(ReportList *reports)
{
  if (state_ == State::Committed) {
    BKE_report(reports, RPT_ERROR, "Cannot roll back a committed USD asset transaction");
    return false;
  }
  if (state_ == State::RolledBack) {
    return true;
  }

  bool success = true;
  for (size_t i = destinations_.size(); i > 0; --i) {
    const Destination &destination = destinations_[i - 1];
    bool restored = true;
    if (!destination.backup_path.empty()) {
      if (!BLI_is_file(destination.backup_path.c_str()) ||
          BLI_is_dir(destination.path.c_str()) ||
          BLI_rename_overwrite(destination.backup_path.c_str(), destination.path.c_str()) != 0)
      {
        BKE_reportf(reports,
                    RPT_ERROR,
                    "Cannot restore USD asset backup %s to %s",
                    destination.backup_path.c_str(),
                    destination.path.c_str());
        restored = false;
      }
    }
    else if (BLI_exists(destination.path.c_str()) &&
             (!BLI_is_file(destination.path.c_str()) ||
              BLI_delete(destination.path.c_str(), false, false) != 0))
    {
      BKE_reportf(reports, RPT_ERROR, "Cannot remove imported asset %s", destination.path.c_str());
      restored = false;
    }
    if (restored) {
      destinations_.erase(destinations_.begin() + (i - 1));
    }
    else {
      success = false;
    }
  }
  /* Removing an empty directory cannot remove files added by another owner. Work backwards so
   * children are removed before parents, and keep failed entries for an explicit retry. */
  for (size_t i = created_directories_.size(); i > 0; --i) {
    const std::filesystem::path &directory = created_directories_[i - 1];
    std::error_code error;
    const std::filesystem::file_status status = std::filesystem::symlink_status(directory, error);
    bool removed = false;
    if (error == std::errc::no_such_file_or_directory ||
        (!error && !std::filesystem::exists(status)))
    {
      removed = true;
    }
    else if (!error && std::filesystem::is_directory(status)) {
      error.clear();
      removed = std::filesystem::remove(directory, error);
    }
    if (removed) {
      created_directories_.erase(created_directories_.begin() + (i - 1));
    }
    else {
      BKE_report(reports, RPT_WARNING, "USD import left a nonempty or changed import directory");
      success = false;
    }
  }
  if (success) {
    state_ = State::RolledBack;
  }
  return success;
}

/**
 * The following is copied from `_SplitUdimPattern()` in
 * USD library source file `materialParamsUtils.cpp`.
 * Split a UDIM file path such as `/someDir/myFile.<UDIM>.exr` into a
 * prefix `/someDir/myFile.` and suffix `.exr`.
 */
static std::pair<std::string, std::string> split_udim_pattern(const std::string &path)
{
  std::string_view patterns[]{UDIM_PATTERN, UDIM_PATTERN2};
  for (const std::string_view pattern : patterns) {
    const std::string::size_type pos = path.find(pattern);
    if (pos != std::string::npos) {
      return {path.substr(0, pos), path.substr(pos + pattern.size())};
    }
  }

  return {};
}

/* Return the asset file base name, with special handling of
 * package relative paths. */
static std::string get_asset_base_name(const std::string &src_path, ReportList *reports)
{
  char base_name[FILE_MAXFILE];

  if (pxr::ArIsPackageRelativePath(src_path)) {
    std::pair<std::string, std::string> split = pxr::ArSplitPackageRelativePathInner(src_path);
    if (split.second.empty()) {
      BKE_reportf(reports,
                  RPT_WARNING,
                  "%s: Couldn't determine package-relative file name from path %s",
                  __func__,
                  src_path.c_str());
      return src_path;
    }
    BLI_path_split_file_part(split.second.c_str(), base_name, sizeof(base_name));
  }
  else {
    BLI_path_split_file_part(src_path.c_str(), base_name, sizeof(base_name));
  }

  return base_name;
}

/* Copy an asset to a destination directory. */
static std::string copy_asset_to_directory(const std::string &src_path,
                                           const char *dest_dir_path,
                                           TexNameCollisionMode name_collision_mode,
                                           ReportList *reports,
                                           AssetImportTransaction *transaction)
{
  std::string base_name = get_asset_base_name(src_path, reports);

  char dest_file_path[FILE_MAX];
  BLI_path_join(dest_file_path, sizeof(dest_file_path), dest_dir_path, base_name.c_str());
  BLI_path_normalize(dest_file_path);

  if (name_collision_mode == TexNameCollisionMode::UseExisting && BLI_is_file(dest_file_path)) {
    return dest_file_path;
  }

  if (!copy_asset(src_path, dest_file_path, name_collision_mode, reports, transaction)) {
    BKE_reportf(reports,
                RPT_WARNING,
                "%s: Couldn't copy file %s to %s",
                __func__,
                src_path.c_str(),
                dest_file_path);
    return src_path;
  }

  return dest_file_path;
}

static std::string copy_udim_asset_to_directory(const std::string &src_path,
                                                const char *dest_dir_path,
                                                TexNameCollisionMode name_collision_mode,
                                                ReportList *reports,
                                                AssetImportTransaction *transaction)
{
  /* Get prefix and suffix from udim pattern. */
  std::pair<std::string, std::string> splitPath = split_udim_pattern(src_path);
  if (splitPath.first.empty() || splitPath.second.empty()) {
    BKE_reportf(
        reports, RPT_ERROR, "%s: Couldn't split UDIM pattern %s", __func__, src_path.c_str());
    return src_path;
  }

  /* Copy the individual UDIM tiles.  Since there is currently no way to query the contents
   * of a directory using the USD resolver, we must take a brute force approach.  We iterate
   * over the allowed range of tile indices and copy any tiles that exist.  The USDPreviewSurface
   * specification stipulates "a maximum of ten tiles in the U direction" and that
   * "the tiles must be within the range [1001, 1100] (as of specification version 2.5)". See
   * https://graphics.pixar.com/usd/release/spec_usdpreviewsurface.html#texture-reader
   */
  for (int i = UDIM_START_TILE; i <= UDIM_END_TILE; ++i) {
    const std::string src_udim = splitPath.first + std::to_string(i) + splitPath.second;
    if (asset_exists(src_udim)) {
      const std::string copied_path = copy_asset_to_directory(
          src_udim, dest_dir_path, name_collision_mode, reports, transaction);
      if (transaction && copied_path == src_udim) {
        return src_path;
      }
    }
  }

  const std::string src_file_name = get_asset_base_name(src_path, reports);
  char ret_udim_path[FILE_MAX];
  BLI_path_join(ret_udim_path, sizeof(ret_udim_path), dest_dir_path, src_file_name.c_str());

  /* Blender only recognizes the <UDIM> pattern, not the
   * alternative UDIM_PATTERN2, so we make sure the returned
   * path has the former. */
  splitPath = split_udim_pattern(ret_udim_path);
  if (splitPath.first.empty() || splitPath.second.empty()) {
    BKE_reportf(reports, RPT_ERROR, "%s: Couldn't split UDIM pattern %s", __func__, ret_udim_path);
    if (transaction) {
      return src_path;
    }
    return ret_udim_path;
  }

  return splitPath.first + UDIM_PATTERN + splitPath.second;
}

bool copy_asset(const std::string &src,
                const std::string &dst,
                TexNameCollisionMode name_collision_mode,
                ReportList *reports,
                AssetImportTransaction *transaction)
{
  if (transaction && transaction->has_failed()) {
    BKE_report(reports, RPT_ERROR, "USD asset transaction cannot copy after a failed asset copy");
    return false;
  }
  const auto fail = [transaction]() {
    if (transaction && transaction->state_ == AssetImportTransaction::State::Active) {
      transaction->failed_ = true;
    }
    return false;
  };

  const pxr::ArResolver &ar = pxr::ArGetResolver();

  if (name_collision_mode != TexNameCollisionMode::Overwrite) {
    if (!ar.Resolve(dst).IsEmpty()) {
      /* The asset exists, so this is a no-op. */
      BKE_reportf(
          reports, RPT_INFO, "%s: Will not overwrite existing asset %s", __func__, dst.c_str());
      return true;
    }
  }

  pxr::ArResolvedPath src_path = ar.Resolve(src);

  if (src_path.IsEmpty()) {
    BKE_reportf(reports, RPT_ERROR, "%s: Cannot resolve path %s", __func__, src.c_str());
    return fail();
  }

  pxr::ArResolvedPath dst_path = ar.ResolveForNewAsset(dst);

  if (dst_path.IsEmpty()) {
    BKE_reportf(
        reports, RPT_ERROR, "%s: Cannot resolve path %s for writing", __func__, dst.c_str());
    return fail();
  }

  if (src_path == dst_path) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Cannot copy %s. The source and destination paths are the same",
                __func__,
                src_path.GetPathString().c_str());
    return fail();
  }

  std::string why_not;
  if (!ar.CanWriteAssetToPath(dst_path, &why_not)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Cannot write to asset %s: %s",
                __func__,
                dst_path.GetPathString().c_str(),
                why_not.c_str());
    return fail();
  }

  std::shared_ptr<pxr::ArAsset> src_asset = ar.OpenAsset(src_path);
  if (!src_asset) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Cannot open source asset %s",
                __func__,
                src_path.GetPathString().c_str());
    return fail();
  }

  const size_t size = src_asset->GetSize();

  if (size == 0) {
    BKE_reportf(reports,
                RPT_WARNING,
                "%s: Will not copy zero size source asset %s",
                __func__,
                src_path.GetPathString().c_str());
    return fail();
  }

  std::shared_ptr<const char> buf = src_asset->GetBuffer();

  if (!buf) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Null buffer for source asset %s",
                __func__,
                src_path.GetPathString().c_str());
    return fail();
  }

  if (transaction && !transaction->prepare_destination(dst_path.GetPathString(), reports)) {
    return fail();
  }

  std::shared_ptr<pxr::ArWritableAsset> dst_asset = ar.OpenAssetForWrite(
      dst_path, pxr::ArResolver::WriteMode::Replace);
  if (!dst_asset) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Cannot open destination asset %s for writing",
                __func__,
                src_path.GetPathString().c_str());
    return fail();
  }

  size_t bytes_written = dst_asset->Write(src_asset->GetBuffer().get(), src_asset->GetSize(), 0);

  if (bytes_written == 0) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Error writing to destination asset %s",
                __func__,
                dst_path.GetPathString().c_str());
  }

  if (!dst_asset->Close()) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Couldn't close destination asset %s",
                __func__,
                dst_path.GetPathString().c_str());
    dst_asset.reset();
    return fail();
  }

  if (transaction && bytes_written != size) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Incomplete write to destination asset %s",
                __func__,
                dst_path.GetPathString().c_str());
    return fail();
  }

  return bytes_written > 0;
}

bool asset_exists(const std::string &path)
{
  return !pxr::ArGetResolver().Resolve(path).IsEmpty();
}

std::string import_asset(const std::string &src,
                         const char *import_dir,
                         TexNameCollisionMode name_collision_mode,
                         ReportList *reports,
                         AssetImportTransaction *transaction)
{
  if (transaction && transaction->has_failed()) {
    BKE_report(reports, RPT_ERROR, "USD asset transaction cannot import after a failed asset copy");
    return src;
  }
  if (import_dir[0] == '\0') {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Texture import directory path empty, couldn't import %s",
                __func__,
                src.c_str());
    if (transaction) {
      transaction->failed_ = true;
    }
    return src;
  }

  if (transaction && strlen(import_dir) >= FILE_MAXDIR) {
    BKE_report(reports, RPT_ERROR, "USD texture import directory path is too long");
    transaction->failed_ = true;
    return src;
  }

  char dest_dir_path[FILE_MAXDIR];
  STRNCPY(dest_dir_path, import_dir);

  if (BLI_path_is_rel(import_dir)) {
    const char *basepath = BKE_main_blendfile_path_from_global();
    if (basepath[0] == '\0') {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "%s: import directory is relative "
                  "but the blend file path is empty. "
                  "Please save the blend file before importing the USD "
                  "or provide an absolute import directory path. "
                  "Cannot import %s",
                  __func__,
                  src.c_str());
      if (transaction) {
        transaction->failed_ = true;
      }
      return src;
    }
    char path_temp[FILE_MAX];
    STRNCPY(path_temp, dest_dir_path);
    BLI_path_abs(path_temp, basepath);
    if (transaction && strlen(path_temp) >= FILE_MAXDIR) {
      BKE_report(reports, RPT_ERROR, "Resolved USD texture import directory path is too long");
      transaction->failed_ = true;
      return src;
    }
    STRNCPY(dest_dir_path, path_temp);
  }

  BLI_path_normalize(dest_dir_path);

  const bool dir_created = transaction ? transaction->prepare_directory(dest_dir_path, reports) :
                                         BLI_dir_create_recursive(dest_dir_path);
  if (!dir_created) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s: Couldn't create texture import directory %s",
                __func__,
                dest_dir_path);
    return src;
  }

  if (is_udim_path(src)) {
    return copy_udim_asset_to_directory(
        src, dest_dir_path, name_collision_mode, reports, transaction);
  }

  return copy_asset_to_directory(src, dest_dir_path, name_collision_mode, reports, transaction);
}

/**
 * Returns true if the parent directory of the given path exists on the
 * file system.
 *
 * \param path: input file path
 * \return true if the parent directory exists
 */
static bool parent_dir_exists_on_file_system(const std::string &path)
{
  char dir_path[FILE_MAX];
  BLI_path_split_dir_part(path.c_str(), dir_path, FILE_MAX);
  return BLI_is_dir(dir_path);
}

bool is_udim_path(const std::string &path)
{
  return path.find(UDIM_PATTERN) != std::string::npos ||
         path.find(UDIM_PATTERN2) != std::string::npos;
}

std::string get_export_textures_dir(const pxr::UsdStageRefPtr stage)
{
  pxr::SdfLayerHandle layer = stage->GetRootLayer();

  if (layer->IsAnonymous()) {
    WM_global_reportf(RPT_WARNING,
                      "%s: Cannot generate a textures directory path for anonymous stage",
                      __func__);
    return "";
  }

  const pxr::ArResolvedPath &stage_path = layer->GetResolvedPath();

  if (stage_path.empty()) {
    WM_global_reportf(RPT_WARNING, "%s: Cannot get resolved path for stage", __func__);
    return "";
  }

  const pxr::ArResolver &ar = pxr::ArGetResolver();

  /* Resolve the `./textures` relative path, with the stage path as an anchor. */
  std::string textures_dir = ar.CreateIdentifierForNewAsset("./textures", stage_path);

  /* If parent of the stage path exists as a file system directory, try to create the
   * textures directory. */
  if (parent_dir_exists_on_file_system(stage_path.GetPathString())) {
    BLI_dir_create_recursive(textures_dir.c_str());
  }

  return textures_dir;
}

bool should_import_asset(const std::string &path)
{
  if (path.empty()) {
    return false;
  }

  if (BLI_path_is_rel(path.c_str())) {
    return false;
  }

  if (pxr::ArIsPackageRelativePath(path)) {
    return true;
  }

  if (is_udim_path(path) && parent_dir_exists_on_file_system(path)) {
    return false;
  }

  return !BLI_is_file(path.c_str()) && asset_exists(path);
}

bool paths_equal(const std::string &path1, const std::string &path2)
{
  BLI_assert_msg(!BLI_path_is_rel(path1.c_str()) && !BLI_path_is_rel(path2.c_str()),
                 "Paths arguments must be absolute");

  const pxr::ArResolver &ar = pxr::ArGetResolver();
  return ar.ResolveForNewAsset(path1) == ar.ResolveForNewAsset(path2);
}

const char *temp_textures_dir()
{
  static bool inited = false;

  static char temp_dir[FILE_MAXDIR] = {'\0'};

  if (!inited) {
    BLI_path_join(temp_dir, sizeof(temp_dir), BKE_tempdir_session(), "usd_textures_tmp", SEP_STR);
    inited = true;
  }

  return temp_dir;
}

bool write_to_path(const void *data, size_t size, const std::string &path, ReportList *reports)
{
  BLI_assert(data);
  if (size == 0) {
    return false;
  }

  const pxr::ArResolver &ar = pxr::ArGetResolver();
  pxr::ArResolvedPath resolved_path = ar.ResolveForNewAsset(path);

  if (resolved_path.IsEmpty()) {
    BKE_reportf(reports, RPT_ERROR, "Cannot resolve path %s for writing", path.c_str());
    return false;
  }

  std::string why_not;
  if (!ar.CanWriteAssetToPath(resolved_path, &why_not)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Cannot write to asset %s: %s",
                resolved_path.GetPathString().c_str(),
                why_not.c_str());
    return false;
  }

  std::shared_ptr<pxr::ArWritableAsset> dst_asset = ar.OpenAssetForWrite(
      resolved_path, pxr::ArResolver::WriteMode::Replace);
  if (!dst_asset) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Cannot open destination asset %s for writing",
                resolved_path.GetPathString().c_str());
    return false;
  }

  size_t bytes_written = dst_asset->Write(data, size, 0);

  if (bytes_written == 0) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Error writing to destination asset %s",
                resolved_path.GetPathString().c_str());
  }

  if (!dst_asset->Close()) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Couldn't close destination asset %s",
                resolved_path.GetPathString().c_str());
    return false;
  }

  return bytes_written > 0;
}

void ensure_usd_source_path_prop(const std::string &path, ID *id)
{
  if (!id || path.empty()) {
    return;
  }

  if (pxr::ArIsPackageRelativePath(path)) {
    /* Don't record package-relative paths (e.g., images in USDZ
     * archives). */
    return;
  }

  IDProperty *idgroup = IDP_EnsureProperties(id);

  if (!idgroup) {
    return;
  }

  const StringRef prop_name = "usd_source_path";

  if (IDP_GetPropertyFromGroup(idgroup, prop_name)) {
    return;
  }

  IDPropertyTemplate val = {0};
  val.string.str = path.c_str();
  /* Note length includes null terminator. */
  val.string.len = path.size() + 1;
  val.string.subtype = IDP_STRING_SUB_UTF8;

  IDProperty *prop = IDP_New(IDP_STRING, &val, prop_name);

  IDP_AddToGroup(idgroup, prop);
}

std::string get_usd_source_path(ID *id)
{
  if (!id) {
    return "";
  }

  const IDProperty *idgroup = IDP_EnsureProperties(id);
  if (!idgroup) {
    return "";
  }

  const StringRef prop_name = "usd_source_path";
  const IDProperty *prop = IDP_GetPropertyFromGroup(idgroup, prop_name);
  if (!prop) {
    return "";
  }

  return static_cast<const char *>(prop->data.pointer);
}

std::string get_relative_path(const std::string &path, const std::string &anchor)
{
  if (path.empty() || anchor.empty()) {
    return path;
  }

  if (path == anchor) {
    return path;
  }

  if (BLI_path_is_rel(path.c_str())) {
    return path;
  }

  if (pxr::ArIsPackageRelativePath(path)) {
    return path;
  }

  if (BLI_is_file(path.c_str()) && BLI_is_file(anchor.c_str())) {
    /* Treat the paths as standard files. */
    char rel_path[FILE_MAX];
    STRNCPY(rel_path, path.c_str());
    BLI_path_rel(rel_path, anchor.c_str());
    if (!BLI_path_is_rel(rel_path)) {
      return path;
    }
    BLI_string_replace_char(rel_path, '\\', '/');
    return rel_path + 2;
  }

  /* If we got here, the paths may be URIs or files on the file system. */

  /* We don't have a library to compute relative paths for URIs
   * so we use the standard file-system calls to do so. This
   * may not work for all URIs in theory, but is probably sufficient
   * for the subset of URIs we are likely to encounter in practice
   * currently.
   * TODO(makowalski): provide better utilities for this. */

  const pxr::ArResolver &ar = pxr::ArGetResolver();

  std::string resolved_path = ar.Resolve(path);
  std::string resolved_anchor = ar.Resolve(anchor);

  if (resolved_path.empty() || resolved_anchor.empty()) {
    return path;
  }

  std::string prefix = pxr::TfStringGetCommonPrefix(path, anchor);
  if (prefix.empty()) {
    return path;
  }

  std::replace(prefix.begin(), prefix.end(), '\\', '/');

  size_t last_slash_pos = prefix.find_last_of('/');
  if (last_slash_pos == std::string::npos) {
    /* Unexpected: The prefix doesn't contain a slash,
     * so this was not an absolute path. */
    return path;
  }

  /* Replace the common prefix up to the last slash with
   * a fake root directory to allow computing the relative path
   * excluding the URI.  We omit the URI because it might not
   * be handled correctly by the standard file-system path computations. */
  resolved_path = "/root" + resolved_path.substr(last_slash_pos);
  resolved_anchor = "/root" + resolved_anchor.substr(last_slash_pos);

  char anchor_parent_dir[FILE_MAX];
  BLI_path_split_dir_part(resolved_anchor.c_str(), anchor_parent_dir, FILE_MAX);

  if (anchor_parent_dir[0] == '\0') {
    return path;
  }

  char result_path[FILE_MAX];
  STRNCPY(result_path, resolved_path.c_str());
  BLI_path_rel(result_path, anchor_parent_dir);

  if (BLI_path_is_rel(result_path)) {
    /* Strip the Blender relative path marker, and set paths to Unix-style. */
    BLI_string_replace_char(result_path, '\\', '/');
    return std::string(result_path + 2);
  }

  return path;
}

void USD_path_abs(char *path, const char *basepath, bool for_import)
{
  if (!BLI_path_is_rel(path)) {
    pxr::ArResolvedPath resolved_path = for_import ? pxr::ArGetResolver().Resolve(path) :
                                                     pxr::ArGetResolver().ResolveForNewAsset(path);

    const std::string &path_str = resolved_path.GetPathString();

    if (!path_str.empty()) {
      if (path_str.length() < FILE_MAX) {
        BLI_strncpy(path, path_str.c_str(), FILE_MAX);
        return;
      }
      WM_global_reportf(RPT_ERROR,
                        "In %s: resolved path %s exceeds path buffer length.",
                        __func__,
                        path_str.c_str());
    }
  }

  /* If we got here, the path couldn't be resolved by the ArResolver, so we
   * fall back on the standard Blender absolute path resolution. */
  BLI_path_abs(path, basepath);
}

}  // namespace blender::io::usd
