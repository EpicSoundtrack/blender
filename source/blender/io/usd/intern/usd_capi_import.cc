/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "IO_types.hh"
#include "usd.hh"
#include "usd_asset_utils.hh"
#include "usd_hook.hh"
#include "usd_light_convert.hh"
#include "usd_reader_domelight.hh"
#include "usd_reader_geom.hh"
#include "usd_reader_prim.hh"
#include "usd_reader_stage.hh"

#include "BKE_cachefile.hh"
#include "BKE_collection.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"

#include "BLI_listbase.hh"
#include "BLI_map.hh"
#include "BLI_path_utils.hh"
#include "BLI_set.hh"
#include "BLI_string.hh"
#include "BLI_timeit.hh"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "DNA_cachefile_types.h"
#include "DNA_collection_types.h"
#include "DNA_key_types.h"
#include "DNA_layer_types.h"
#include "DNA_listBase.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"

#include "ED_undo.hh"

#include "MEM_guardedalloc.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/metrics.h>

#include <fmt/core.h>
#include <memory>

#include "CLG_log.h"

namespace blender {

static CLG_LogRef LOG = {"io.usd"};

namespace io::usd {

static CacheArchiveHandle *handle_from_stage_reader(USDStageReader *reader)
{
  return reinterpret_cast<CacheArchiveHandle *>(reader);
}

static USDStageReader *stage_reader_from_handle(CacheArchiveHandle *handle)
{
  return reinterpret_cast<USDStageReader *>(handle);
}

static bool gather_objects_paths(const pxr::UsdPrim &object,
                                 ListBaseT<CacheObjectPath> *object_paths)
{
  if (!object.IsValid()) {
    return false;
  }

  for (const pxr::UsdPrim &childPrim : object.GetChildren()) {
    gather_objects_paths(childPrim, object_paths);
  }

  CacheObjectPath *usd_path = MEM_new<CacheObjectPath>("CacheObjectPath");

  STRNCPY(usd_path->path, object.GetPrimPath().GetString().c_str());
  BLI_addtail(object_paths, usd_path);

  return true;
}

enum {
  USD_NO_ERROR = 0,
  USD_ARCHIVE_FAIL,
  USD_HOOK_FAIL,
  USD_ASSET_FAIL,
};

/**
 * Bounded rollback of the synchronous native import. Ownership is recorded from reader/material
 * roots and their new dependencies BEFORE invoking material or scene import hooks. In particular,
 * an ID created by a hook is not owned merely because it appeared after the import started.
 *
 * This is not a general Python transaction: deleting existing IDs or editing arbitrary existing
 * datablock contents cannot be undone here. Required links that cannot be restored make deletion
 * fail closed, rather than allowing BKE_id_multi_delete to delete a preexisting ID transitively.
 */
class ImportRollbackState {
 private:
  struct ObjectData {
    Object *object;
    ID *data;
    ObjectType type;
  };

  Main *bmain_;
  Scene *scene_;
  ViewLayer *view_layer_;
  Map<ID *, unsigned int> initial_ids_;
  Map<ID *, unsigned int> owned_ids_;
  Vector<ID *> pending_ids_;
  Vector<ObjectData> object_data_;
  Set<Object *> selected_objects_;
  Object *active_object_ = nullptr;
  Collection *active_collection_ = nullptr;
  World *world_ = nullptr;
  std::unique_ptr<USDWorldStateSnapshot> world_state_;
  int frame_start_ = 0;
  int frame_end_ = 0;
  int frame_current_ = 0;
  float subframe_ = 0.0f;

  static Map<ID *, unsigned int> main_identities(Main *bmain)
  {
    Map<ID *, unsigned int> result;
    ID *id;
    FOREACH_MAIN_ID_BEGIN (bmain, id) {
      result.add_new(id, id->session_uid);
    }
    FOREACH_MAIN_ID_END;
    return result;
  }

  static bool same_identity(const Map<ID *, unsigned int> &a,
                            const Map<ID *, unsigned int> &b,
                            ID *id)
  {
    const unsigned int *a_uid = a.lookup_ptr(id);
    const unsigned int *b_uid = b.lookup_ptr(id);
    return a_uid && b_uid && *a_uid == *b_uid;
  }

  void own_new_id(ID *id)
  {
    if (!id || (id->flag & ID_FLAG_EMBEDDED_DATA) || (id->tag & ID_TAG_NO_MAIN) ||
        initial_ids_.contains(id) || owned_ids_.contains(id))
    {
      return;
    }
    owned_ids_.add_new(id, id->session_uid);
    pending_ids_.append(id);
  }

  static int capture_dependency(LibraryIDLinkCallbackData *cb)
  {
    auto &state = *static_cast<ImportRollbackState *>(cb->user_data);
    if (!(cb->cb_flag & (IDWALK_CB_LOOPBACK | IDWALK_CB_INTERNAL))) {
      state.own_new_id(*cb->id_pointer);
    }
    return IDWALK_RET_NOP;
  }

 public:
  ImportRollbackState(Main *bmain, Scene *scene, ViewLayer *view_layer)
      : bmain_(bmain), scene_(scene), view_layer_(view_layer), initial_ids_(main_identities(bmain))
  {
    for (Object &object : bmain_->objects) {
      object_data_.append({&object, object.data, object.type});
    }
    if (!scene_) {
      return;
    }
    frame_start_ = scene_->r.sfra;
    frame_end_ = scene_->r.efra;
    frame_current_ = scene_->r.cfra;
    subframe_ = scene_->r.subframe;
    world_ = scene_->world;
    if (view_layer_) {
      BKE_view_layer_synced_ensure(*bmain_, scene_, view_layer_);
      for (const Base &base : *BKE_view_layer_object_bases_get(view_layer_)) {
        if (base.flag & BASE_SELECTED) {
          selected_objects_.add(base.object);
        }
      }
      if (Base *active = BKE_view_layer_active_base_get(view_layer_)) {
        active_object_ = active->object;
      }
      if (view_layer_->active_collection) {
        active_collection_ = view_layer_->active_collection->collection;
      }
    }
  }

  bool prepare_world()
  {
    if (!world_) {
      return true;
    }
    world_state_ = std::make_unique<USDWorldStateSnapshot>(world_, bmain_);
    return world_state_->is_valid();
  }

  USDWorldStateSnapshot *world_snapshot() const
  {
    return world_state_.get();
  }

  void own_collection(Collection *collection)
  {
    if (collection) {
      own_new_id(&collection->id);
    }
  }

  /* Called immediately around the native-only create_proto_collections operation. */
  void own_proto_collections(const Set<Collection *> &before)
  {
    for (Collection &collection : bmain_->collections) {
      if (!before.contains(&collection)) {
        own_collection(&collection);
      }
    }
  }

  void seal(USDStageReader &archive, CacheFile *cache_file)
  {
    for (const USDPrimReader *reader : archive.readers()) {
      if (Object *object = reader->object()) {
        own_new_id(&object->id);
      }
    }
    /* Includes unbound materials from import_all_materials and collision-mode variants. */
    for (Material *material : archive.settings().mat_name_to_mat.values()) {
      own_new_id(reinterpret_cast<ID *>(material));
    }
    for (Material *material : archive.settings().usd_path_to_mat.values()) {
      own_new_id(reinterpret_cast<ID *>(material));
    }
    if (cache_file) {
      own_new_id(&cache_file->id);
    }
    if (scene_ && scene_->world) {
      own_new_id(&scene_->world->id);
      /* Native DomeLight import can add images to an existing World's embedded tree. */
      BKE_library_foreach_ID_link(
          bmain_, &scene_->world->id, capture_dependency, this, IDWALK_READONLY);
    }
    for (int i = 0; i < pending_ids_.size(); ++i) {
      BKE_library_foreach_ID_link(
          bmain_, pending_ids_[i], capture_dependency, this, IDWALK_READONLY);
    }
    pending_ids_.clear();
  }

  bool rollback(ReportList *reports)
  {
    const Map<ID *, unsigned int> live = main_identities(bmain_);
    bool restored = true;
    /* Restore required object-data links BEFORE considering deletion. Hooks may have assigned
     * imported geometry to an existing object (including the startup cube). */
    for (const ObjectData &saved : object_data_) {
      ID *object_id = reinterpret_cast<ID *>(saved.object);
      if (!same_identity(initial_ids_, live, object_id) ||
          (saved.data && !same_identity(initial_ids_, live, saved.data)))
      {
        restored = false;
        continue;
      }
      Object *object = saved.object;
      /* A hook which converts the original object to another type needs a full object undo,
       * not an obdata relink. Avoid the relink API's same-ID-type assertion in that case. */
      if (object->type != saved.type ||
          (object->data && saved.data && GS(object->data->name) != GS(saved.data->name)))
      {
        restored = false;
        continue;
      }
      if (object->data != saved.data) {
        if (object->data) {
          BKE_libblock_relink_ex(bmain_,
                                object,
                                object->data,
                                saved.data,
                                ID_REMAP_SKIP_USER_CLEAR | ID_REMAP_FORCE_OBDATA_IN_EDITMODE);
        }
        else {
          object->data = saved.data;
          if (saved.data) {
            id_us_plus(saved.data);
          }
        }
        BKE_object_materials_sync_length(bmain_, object, saved.data);
        DEG_id_tag_update(&object->id, ID_RECALC_GEOMETRY);
      }
    }

    const bool scene_alive = scene_ &&
                             same_identity(initial_ids_, live, reinterpret_cast<ID *>(scene_));
    if (scene_alive) {
      scene_->r.sfra = frame_start_;
      scene_->r.efra = frame_end_;
      scene_->r.cfra = frame_current_;
      scene_->r.subframe = subframe_;
      if (!world_ || same_identity(initial_ids_, live, reinterpret_cast<ID *>(world_))) {
        if (scene_->world != world_) {
          if (scene_->world) {
            id_us_min(&scene_->world->id);
          }
          scene_->world = world_;
          if (world_) {
            id_us_plus(&world_->id);
          }
        }
        if (world_state_) {
          if (!world_state_->restore()) {
            restored = false;
          }
        }
      }
      else {
        restored = false;
      }
    }
    else if (scene_) {
      restored = false;
    }

    Set<ID *> to_delete;
    for (const auto item : owned_ids_.items()) {
      if (same_identity(owned_ids_, live, item.key)) {
        to_delete.add(item.key);
      }
    }

    /* The deletion implementation has three ways to grow its set: libraries, shape keys, and
     * NEVER_NULL users. Prove closure for all three before calling it. No links are changed by
     * this pass; an unexpected dependency leaves the imported IDs in place for recovery. */
    bool safe_to_delete = true;
    for (ID *id : to_delete) {
      Key *key = BKE_key_from_id(id);
      if (GS(id->name) == ID_LI || (key && !to_delete.contains(&key->id))) {
        safe_to_delete = false;
      }
    }
    struct RequiredLinkCheck {
      const Set<ID *> *owned;
      bool safe = true;
    } check{&to_delete};
    for (const auto item : live.items()) {
      if (!to_delete.contains(item.key)) {
        BKE_library_foreach_ID_link(
            bmain_,
            item.key,
            [](LibraryIDLinkCallbackData *cb) {
              auto &check = *static_cast<RequiredLinkCheck *>(cb->user_data);
              if ((cb->cb_flag & IDWALK_CB_NEVER_NULL) &&
                  check.owned->contains(*cb->id_pointer))
              {
                check.safe = false;
              }
              return IDWALK_RET_NOP;
            },
            &check,
            IDWALK_READONLY | IDWALK_DO_INTERNAL_RUNTIME_POINTERS | IDWALK_INCLUDE_UI);
      }
    }
    safe_to_delete &= check.safe;
    if (safe_to_delete) {
      BKE_id_multi_delete(bmain_, to_delete);
    }
    else {
      BKE_report(reports,
                 RPT_ERROR,
                 "USD import rollback retained imported IDs because deletion would affect "
                 "non-imported data");
      restored = false;
    }

    if (scene_alive && view_layer_ && BLI_findindex(&scene_->view_layers, view_layer_) != -1) {
      BKE_view_layer_synced_ensure(*bmain_, scene_, view_layer_);
      for (Base &base : *BKE_view_layer_object_bases_get(view_layer_)) {
        base.flag &= ~BASE_SELECTED;
        if (selected_objects_.contains(base.object) &&
            same_identity(initial_ids_, live, &base.object->id))
        {
          base.flag |= BASE_SELECTED;
        }
      }
      view_layer_->basact = nullptr;
      if (active_object_ &&
          same_identity(initial_ids_, live, reinterpret_cast<ID *>(active_object_)))
      {
        view_layer_->basact = BKE_view_layer_base_find(view_layer_, active_object_);
      }
      if (active_collection_ &&
          (active_collection_ == scene_->master_collection ||
           same_identity(initial_ids_, live, reinterpret_cast<ID *>(active_collection_))))
      {
        if (LayerCollection *layer = BKE_layer_collection_first_from_scene_collection(
                view_layer_, active_collection_))
        {
          view_layer_->active_collection = layer;
        }
        else {
          restored = false;
        }
      }
      else if (active_collection_) {
        restored = false;
      }
      DEG_id_tag_update(&scene_->id, ID_RECALC_BASE_FLAGS);
    }
    else if (scene_ && view_layer_) {
      restored = false;
    }
    DEG_relations_tag_update(bmain_);
    if (!restored) {
      BKE_report(reports, RPT_ERROR, "USD import rollback could not restore all original state");
    }
    return restored;
  }
};

struct ImportJobData {
  bContext *C;
  Main *bmain;
  Scene *scene;
  wmWindowManager *wm;

  char filepath[FILE_MAX];
  USDImportParams params;

  USDStageReader *archive;

  char error_code;
  bool was_canceled;
  bool import_ok;
  bool is_background_job;
  timeit::TimePoint start_time;

  CacheFile *cache_file;
  std::unique_ptr<ImportRollbackState> rollback_state;
  std::unique_ptr<AssetImportTransaction> asset_transaction;
  bool asset_copy_failed = false;
};

static void report_job_duration(const ImportJobData *data)
{
  timeit::Nanoseconds duration = timeit::Clock::now() - data->start_time;
  fmt::print("USD import of '{}' took ", data->filepath);
  timeit::print_duration(duration);
  fmt::print("\n");
}

static void import_startjob(void *customdata, wmJobWorkerStatus *worker_status)
{
  ImportJobData *data = static_cast<ImportJobData *>(customdata);
  data->was_canceled = false;
  data->archive = nullptr;
  data->start_time = timeit::Clock::now();
  data->cache_file = nullptr;

  data->params.worker_status = worker_status;
  data->params.asset_transaction = nullptr;
  data->params.asset_copy_failed = nullptr;
  if (!data->is_background_job && data->params.import_textures_mode == TexImportMode::Copy) {
    data->asset_transaction = std::make_unique<AssetImportTransaction>();
    data->params.asset_transaction = data->asset_transaction.get();
    data->params.asset_copy_failed = &data->asset_copy_failed;
  }

  if (data->wm) {
    WM_locked_interface_set(data->wm, true);
  }
  G.is_break = false;

  BLI_path_abs(data->filepath, BKE_main_blendfile_path_from_global());

  pxr::UsdStagePopulationMask pop_mask;
  for (const std::string &mask_token : pxr::TfStringTokenize(data->params.prim_path_mask, ",;")) {
    pxr::SdfPath prim_path(mask_token);
    if (!prim_path.IsEmpty()) {
      pop_mask.Add(prim_path);
    }
  }

  pxr::UsdStageRefPtr stage = pop_mask.IsEmpty() ?
                                  pxr::UsdStage::Open(data->filepath) :
                                  pxr::UsdStage::OpenMasked(data->filepath, pop_mask);

  if (!stage) {
    BKE_reportf(worker_status->reports,
                RPT_ERROR,
                "USD Import: unable to open stage to read %s",
                data->filepath);
    data->import_ok = false;
    data->error_code = USD_ARCHIVE_FAIL;
    return;
  }

  worker_status->progress = 0.05f;
  worker_status->do_update = true;
  if (G.is_break) {
    data->was_canceled = true;
    return;
  }

  double scene_scale = data->params.scale;
  if (data->params.apply_unit_conversion_scale) {
    scene_scale *= pxr::UsdGeomGetStageMetersPerUnit(stage);
  }

  /* Callback function to lazily create a cache file when converting
   * time varying data. */
  auto get_cache_file = [data, scene_scale]() {
    if (!data->cache_file) {
      data->cache_file = static_cast<CacheFile *>(
          BKE_cachefile_add(data->bmain, BLI_path_basename(data->filepath)));

      /* Decrement the ID ref-count because it is going to be incremented for each
       * modifier and constraint that it will be attached to, so since currently
       * it is not used by anyone, its use count will off by one. */
      id_us_min(&data->cache_file->id);

      data->cache_file->is_sequence = data->params.is_sequence;
      data->cache_file->scale = scene_scale;
      STRNCPY(data->cache_file->filepath, data->filepath);
      if (data->params.relative_path && !BLI_path_is_rel(data->cache_file->filepath)) {
        BLI_path_rel(data->cache_file->filepath, BKE_main_blendfile_path_from_global());
      }
    }
    return data->cache_file;
  };

  USDStageReader *archive = new USDStageReader(stage, data->params, get_cache_file);
  data->archive = archive;

  /* Ensure Python types for invoking hooks are registered. */
  register_hook_converters();

  if (!archive->find_material_import_hook_sources()) {
    data->error_code = USD_HOOK_FAIL;
    data->import_ok = false;
    return;
  }

  /* Eligibility hooks have finished. Native reader allocations below can now be attributed to
   * this synchronous import. Background jobs need a separate concurrency-aware transaction. */
  if (!data->is_background_job) {
    data->rollback_state = std::make_unique<ImportRollbackState>(
        data->bmain, data->scene, CTX_data_view_layer(data->C));
  }

  worker_status->progress = 0.1f;
  worker_status->do_update = true;
  if (G.is_break) {
    data->was_canceled = true;
    return;
  }

  archive->collect_readers();

  worker_status->progress = 0.15f;
  worker_status->do_update = true;
  if (G.is_break) {
    data->was_canceled = true;
    return;
  }

  if (data->params.import_lights && data->params.create_world_material &&
      !archive->dome_light_readers().is_empty())
  {
    /* NOTE: Since Blender does not have dome light objects, we need to modify the Scene directly
     * in order to setup environment lighting. */
    if (data->scene) {
      if (data->rollback_state && !data->rollback_state->prepare_world()) {
        BKE_report(worker_status->reports,
                   RPT_ERROR,
                   "USD import could not preserve the existing World for rollback");
        data->error_code = USD_HOOK_FAIL;
        data->import_ok = false;
        return;
      }
      USDDomeLightReader *dome_light_reader = archive->dome_light_readers().first();
      dome_light_reader->create_object(data->scene,
                                       data->bmain,
                                       data->rollback_state ? data->rollback_state->world_snapshot() :
                                                              nullptr);
      if (data->rollback_state && data->rollback_state->world_snapshot() &&
          !data->rollback_state->world_snapshot()->is_valid())
      {
        BKE_report(worker_status->reports,
                   RPT_ERROR,
                   "USD import could not journal the DomeLight World change for rollback");
        data->error_code = USD_HOOK_FAIL;
        data->import_ok = false;
        return;
      }
    }
  }

  if (data->params.import_materials && data->params.import_all_materials) {
    archive->import_all_materials(data->bmain);
  }

  worker_status->progress = 0.2f;
  worker_status->do_update = true;

  /* Sort readers by name: when creating a lot of objects in Blender,
   * it is much faster if the order is sorted by name. */
  archive->sort_readers();

  worker_status->progress = 0.25f;
  worker_status->do_update = true;

  const float size = float(archive->readers().size());
  size_t i = 0;

  /* Create blender objects. */
  for (USDPrimReader *reader : archive->readers()) {
    reader->create_object(data->bmain);

    worker_status->progress = 0.25f + 0.25f * (++i / size);
    worker_status->do_update = true;

    if (G.is_break) {
      data->was_canceled = true;
      return;
    }
  }

  /* Setup parenthood and read actual object data. */
  i = 0;
  for (USDPrimReader *reader : archive->readers()) {
    Object *ob = reader->object();
    reader->read_object_data(data->bmain, 0.0);

    USDPrimReader *parent = reader->parent();
    if (parent == nullptr) {
      ob->parent = nullptr;
    }
    else {
      ob->parent = parent->object();
    }

    worker_status->progress = 0.5f + 0.5f * (++i / size);
    worker_status->do_update = true;

    if (G.is_break) {
      data->was_canceled = true;
      return;
    }
  }

  if (data->params.import_skeletons) {
    archive->process_armature_modifiers();
  }

  data->import_ok = !data->was_canceled;

  worker_status->progress = 1.0f;
  worker_status->do_update = true;
}

static void import_endjob(void *customdata)
{
  ImportJobData *data = static_cast<ImportJobData *>(customdata);
  bool ownership_sealed = false;
  if (data->asset_transaction && data->asset_transaction->has_failed()) {
    data->asset_copy_failed = true;
  }
  if (data->asset_copy_failed) {
    data->error_code = USD_ASSET_FAIL;
    data->import_ok = false;
  }

  /* Retain legacy background cancellation behavior. Synchronous cleanup uses the journal below. */
  if (data->was_canceled && data->archive && data->is_background_job) {
    for (const USDPrimReader *reader : data->archive->readers()) {
      /* It's possible that cancellation occurred between the creation of
       * the reader and the creation of the Blender object. */
      if (Object *ob = reader->object()) {
        BKE_id_free_us(data->bmain, ob);
      }
    }
  }
  else if (!data->was_canceled && data->archive && data->error_code == USD_NO_ERROR) {
    Collection *collection_dst = nullptr;

    if (data->scene) {
      /* Set scene animation range. */
      if (data->params.set_frame_range) {
        const pxr::UsdStageRefPtr stage = data->archive->stage();
        data->scene->r.sfra = stage->GetStartTimeCode();
        data->scene->r.efra = stage->GetEndTimeCode();
      }

      ViewLayer *view_layer = CTX_data_view_layer(data->C);

      /* Create a new collection if required. */
      if (data->params.create_collection) {
        char display_name[MAX_ID_NAME - 2];
        BLI_path_to_display_name(
            display_name, sizeof(display_name), BLI_path_basename(data->filepath));
        Collection *import_collection = BKE_collection_add(
            data->bmain, data->scene->master_collection, display_name);
        if (data->rollback_state) {
          data->rollback_state->own_collection(import_collection);
        }

        DEG_id_tag_update(&import_collection->id, ID_RECALC_SYNC_TO_EVAL);
        DEG_relations_tag_update(data->bmain);

        BKE_view_layer_synced_ensure(*data->bmain, data->scene, view_layer);
        view_layer->active_collection = BKE_layer_collection_first_from_scene_collection(
            view_layer, import_collection);
      }

      BKE_view_layer_base_deselect_all(*data->bmain, data->scene, view_layer);

      LayerCollection *lc = BKE_layer_collection_get_active_editable(view_layer);
      if (!ID_IS_EDITABLE(lc->collection)) {
        BKE_report(
            data->params.worker_status->reports,
            RPT_WARNING,
            "Could not find an editable collection in current scene, imported data will not "
            "be instantiated");
      }

      collection_dst = lc->collection;
    }

    /* Create prototype collections for instancing. */
    Set<Collection *> collections_before;
    if (data->rollback_state) {
      for (Collection &collection : data->bmain->collections) {
        collections_before.add(&collection);
      }
    }
    data->archive->create_proto_collections(data->bmain, collection_dst);
    if (data->rollback_state) {
      data->rollback_state->own_proto_collections(collections_before);
    }

    /* Add all objects to the collection. */
    for (const USDPrimReader *reader : data->archive->readers()) {
      if (reader->is_in_proto()) {
        /* Skip prototype prims, as these are added to prototype collections. */
        continue;
      }
      Object *ob = reader->object();
      if (!ob) {
        continue;
      }
      BKE_collection_object_add(data->bmain, collection_dst, ob);
    }

    /* Sync and do the view layer operations. */
    if (data->scene) {
      ViewLayer *view_layer = CTX_data_view_layer(data->C);
      BKE_view_layer_synced_ensure(*data->bmain, data->scene, view_layer);

      bool has_instantiated_object = false;
      bool has_uninstantiated_object = false;
      for (const USDPrimReader *reader : data->archive->readers()) {
        Object *ob = reader->object();
        if (!ob) {
          continue;
        }
        Base *base = BKE_view_layer_base_find(view_layer, ob);
        if (!base) {
          /* Object not instantiated in current viewlayer. */
          has_uninstantiated_object = true;
          continue;
        }
        has_instantiated_object = true;
        /* TODO: is setting active needed? */
        BKE_view_layer_base_select_and_set_active(view_layer, base);

        DEG_id_tag_update(&collection_dst->id, ID_RECALC_SYNC_TO_EVAL);
        DEG_id_tag_update_ex(data->bmain,
                             &ob->id,
                             ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION |
                                 ID_RECALC_BASE_FLAGS);
      }
      if (has_instantiated_object && has_uninstantiated_object) {
        CLOG_ERROR(&LOG, "Some imported objects were not instantiated, while others were");
      }

      DEG_id_tag_update(&data->scene->id, ID_RECALC_BASE_FLAGS);
    }

    DEG_relations_tag_update(data->bmain);

    if (data->params.import_materials && data->params.import_all_materials) {
      data->archive->fake_users_for_unused_materials();
    }

    if (data->rollback_state) {
      data->rollback_state->seal(*data->archive, data->cache_file);
      ownership_sealed = true;
    }

    const bool material_hooks_ok = data->archive->call_material_import_hooks(data->bmain);
    if (!material_hooks_ok) {
      if (data->rollback_state) {
        data->error_code = USD_HOOK_FAIL;
        data->import_ok = false;
      }
      else {
        BKE_report(data->params.worker_status->reports,
                   RPT_WARNING,
                   "USD material import hook failed; background import rollback is not available");
      }
    }

    if (data->asset_transaction && data->asset_transaction->has_failed()) {
      data->asset_copy_failed = true;
    }

    if (material_hooks_ok && !data->asset_copy_failed &&
        !call_import_hooks(data->archive, data->params.worker_status->reports))
    {
      if (data->rollback_state) {
        data->error_code = USD_HOOK_FAIL;
        data->import_ok = false;
      }
      else {
        /* Preserve existing background-job behavior until it has a concurrency-aware journal.
         * In particular, do not report a clean cancellation while leaving a dirty scene. */
        BKE_report(data->params.worker_status->reports,
                   RPT_WARNING,
                   "USD scene import hook failed; background import rollback is not available");
      }
    }

    if (data->is_background_job) {
      /* Blender already returned from the import operator, so we need to store our own extra undo
       * step. */
      ED_undo_push(data->C, "USD Import Finished");
    }
  }

  if (!data->is_background_job) {
    ReportList *reports = data->params.worker_status->reports;
    if (data->asset_transaction && data->asset_transaction->has_failed()) {
      data->asset_copy_failed = true;
    }
    if (data->asset_copy_failed) {
      data->error_code = USD_ASSET_FAIL;
      data->import_ok = false;
      BKE_report(reports, RPT_ERROR, "USD import failed because an asset could not be copied");
    }
    if (data->was_canceled || data->error_code != USD_NO_ERROR || !data->import_ok) {
      if (data->rollback_state && data->archive) {
        /* A cancellation or native copy failure may precede the normal pre-hook ownership seal.
         * Never reseal after hooks: their newly created IDs do not belong to this journal. */
        if (!ownership_sealed) {
          data->rollback_state->seal(*data->archive, data->cache_file);
        }
        data->rollback_state->rollback(reports);
        data->archive->clear_readers();
        data->cache_file = nullptr;
      }
      /* Release imported IDs and restore their original links before restoring asset files. */
      if (data->asset_transaction && !data->asset_transaction->rollback(reports)) {
        BKE_report(reports, RPT_ERROR, "USD import could not restore all copied asset files");
      }
    }
    else if (data->asset_transaction && !data->asset_transaction->commit(reports)) {
      /* commit() retains the imported files even when removing a backup fails. The successful
       * scene import must not be rolled back after that point. */
      BKE_report(reports,
                 RPT_WARNING,
                 "USD import succeeded, but some asset backups could not be removed");
    }
  }

  if (data->wm) {
    WM_locked_interface_set(data->wm, false);
  }

  switch (data->error_code) {
    default:
    case USD_NO_ERROR:
      data->import_ok = !data->was_canceled;
      break;
    case USD_ARCHIVE_FAIL:
      BKE_report(data->params.worker_status->reports,
                 RPT_ERROR,
                 "Could not open USD archive for reading, see console for detail");
      break;
    case USD_HOOK_FAIL:
    case USD_ASSET_FAIL:
      data->import_ok = false;
      break;
  }

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);
  report_job_duration(data);
}

static void import_freejob(void *user_data)
{
  ImportJobData *data = static_cast<ImportJobData *>(user_data);

  delete data->archive;
  delete data;
}

bool USD_import(bContext *C,
                const char *filepath,
                const USDImportParams *params,
                bool as_background_job,
                ReportList *reports)
{
  /* Using new here since `MEM_*` functions do not call constructor to properly initialize data. */
  ImportJobData *job = new ImportJobData();
  job->C = C;
  job->bmain = CTX_data_main(C);
  job->scene = CTX_data_scene(C);  // May be null
  job->wm = CTX_wm_manager(C);     // May be null
  job->import_ok = false;
  job->is_background_job = as_background_job;
  STRNCPY(job->filepath, filepath);

  job->error_code = USD_NO_ERROR;
  job->was_canceled = false;
  job->archive = nullptr;

  job->params = *params;

  G.is_break = false;

  bool import_ok = false;
  if (as_background_job) {
    wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                                CTX_wm_window(C),
                                CTX_data_scene(C),
                                "Importing USD...",
                                WM_JOB_PROGRESS,
                                WM_JOB_TYPE_USD_IMPORT);

    /* setup job */
    WM_jobs_customdata_set(wm_job, job, import_freejob);
    WM_jobs_timer(wm_job, 0.1, NC_SCENE, NC_SCENE);
    WM_jobs_callbacks(wm_job, import_startjob, nullptr, nullptr, import_endjob);

    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }
  else {
    wmJobWorkerStatus worker_status = {};
    /* Use the operator's reports in non-background case. */
    worker_status.reports = reports;

    import_startjob(job, &worker_status);
    import_endjob(job);
    import_ok = job->import_ok;

    import_freejob(job);
  }

  return import_ok;
}

/* TODO(makowalski): Extend this function with basic validation that the
 * USD reader is compatible with the type of the given (currently unused) 'ob'
 * Object parameter, similar to the logic in get_abc_reader() in the
 * Alembic importer code. */
static USDPrimReader *get_usd_reader(CacheReader *reader,
                                     const Object * /*ob*/,
                                     const char **r_err_str)
{
  USDPrimReader *usd_reader = reinterpret_cast<USDPrimReader *>(reader);
  pxr::UsdPrim iobject = usd_reader->prim();

  if (!iobject.IsValid()) {
    *r_err_str = RPT_("Invalid object: verify object path");
    return nullptr;
  }

  return usd_reader;
}

USDMeshReadParams create_mesh_read_params(const double motion_sample_time, const int read_flags)
{
  USDMeshReadParams params = {};
  params.motion_sample_time = motion_sample_time;
  params.read_flags = read_flags;
  return params;
}

void USD_read_geometry(CacheReader *reader,
                       const Object *ob,
                       bke::GeometrySet &geometry_set,
                       const USDMeshReadParams params,
                       const char **r_err_str)
{
  USDGeomReader *usd_reader = dynamic_cast<USDGeomReader *>(get_usd_reader(reader, ob, r_err_str));

  if (usd_reader == nullptr) {
    return;
  }

  usd_reader->read_geometry(geometry_set, params, r_err_str);
}

bool USD_mesh_topology_changed(CacheReader *reader,
                               const Object *ob,
                               const Mesh *existing_mesh,
                               const double time,
                               const char **r_err_str)
{
  USDGeomReader *usd_reader = dynamic_cast<USDGeomReader *>(get_usd_reader(reader, ob, r_err_str));

  if (usd_reader == nullptr) {
    return false;
  }

  return usd_reader->topology_changed(existing_mesh, time);
}

CacheReader *CacheReader_open_usd_object(CacheArchiveHandle *handle,
                                         CacheReader *reader,
                                         Object *object,
                                         const char *object_path)
{
  if (object_path[0] == '\0') {
    return reader;
  }

  USDStageReader *archive = stage_reader_from_handle(handle);

  if (!archive || !archive->valid()) {
    return reader;
  }

  if (reader) {
    USD_CacheReader_free(reader);
  }

  pxr::UsdPrim prim = archive->stage()->GetPrimAtPath(pxr::SdfPath(object_path));

  if (!prim) {
    return nullptr;
  }

  /* TODO(makowalski): The handle does not have the proper import params or settings. */
  USDPrimReader *usd_reader = archive->create_reader(prim);

  if (usd_reader == nullptr) {
    /* This object is not supported. */
    return nullptr;
  }
  if (!usd_reader->valid()) {
    /* This object is invalid for some reason. */
    return nullptr;
  }
  usd_reader->object(object);
  usd_reader->incref();

  return reinterpret_cast<CacheReader *>(usd_reader);
}

void USD_CacheReader_free(CacheReader *reader)
{
  USDPrimReader *usd_reader = reinterpret_cast<USDPrimReader *>(reader);
  usd_reader->decref();

  if (usd_reader->refcount() == 0) {
    delete usd_reader;
  }
}

CacheArchiveHandle *USD_create_handle(Main * /*bmain*/,
                                      const char *filepath,
                                      ListBaseT<CacheObjectPath> *object_paths)
{
  pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(filepath);

  if (!stage) {
    return nullptr;
  }

  USDImportParams params{};

  USDStageReader *stage_reader = new USDStageReader(stage, params);

  if (object_paths) {
    gather_objects_paths(stage->GetPseudoRoot(), object_paths);
  }

  return handle_from_stage_reader(stage_reader);
}

void USD_free_handle(CacheArchiveHandle *handle)
{
  USDStageReader *stage_reader = stage_reader_from_handle(handle);
  delete stage_reader;
}

void USD_get_transform(CacheReader *reader, float4x4 &r_mat_world, float time, float scale)
{
  if (!reader) {
    return;
  }
  const USDXformReader *usd_reader = reinterpret_cast<USDXformReader *>(reader);
  bool is_constant = false;

  /* Convert from the local matrix we obtain from USD to world coordinates
   * for Blender. This conversion is done here rather than by Blender due to
   * work around the non-standard interpretation of CONSTRAINT_SPACE_LOCAL in
   * BKE_constraint_mat_convertspace(). */
  Object *object = usd_reader->object();
  if (object->parent == nullptr) {
    /* No parent, so local space is the same as world space. */
    usd_reader->read_matrix(r_mat_world, time, scale, &is_constant);
    return;
  }

  float4x4 mat_parent;
  BKE_object_get_parent_matrix(object, object->parent, mat_parent.ptr());

  float4x4 mat_local;
  usd_reader->read_matrix(mat_local, time, scale, &is_constant);

  r_mat_world = mat_parent * float4x4(object->parentinv);
  r_mat_world *= mat_local;
}

}  // namespace io::usd
}  // namespace blender
