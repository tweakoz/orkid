////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// hm_drawable.cpp (HYPERECS D.3) — HypermeshDrawableData: the C++ port of the Python
// make_drawable orchestration. createDrawable() returns a ComputeDrawable whose GPU side
// materializes LAZILY on the first onGpuUpdate (the first render-thread moment a Context
// exists — also after the AssetSystem's gpu-init has had a chance to materialize the
// material artifact this drawable references by name).
//
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/properties/DirectTypedVector.hpp> // std::vector<float> instance matrices
#include <ork/reflect/properties/DirectTypedMap.hpp>    // E.3: gid->material-asset string map
#include <ork/lev2/gfx/hypermesh/hm_drawable.h>
#include <ork/lev2/gfx/renderer/compute_drawable.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/dataflow/module.inl> // typedInputNamed (E.6/2.12 sink plug read)
#include <ork/kernel/async_tracker.h> // O3 stage 3: stored-mode section bake as pending async work
#include <ork/util/logger.h>
#include <limits>

namespace ork::lev2::hypermesh {

static logchannel_ptr_t logchan_hmdrw = logger()->configureChannel("HYPERMESH", fvec3(0.9, 0.9, 0.2));

///////////////////////////////////////////////////////////////////////////////

void HypermeshDrawableData::describeX(object::ObjectClass* clazz) {
  clazz->directObjectProperty("graph", &HypermeshDrawableData::_graphdata);
  // Phase 3c — DISTANCE LOD: parallel arrays (coarser graph per ascending distance boundary).
  clazz->directObjectVectorProperty("lod_graphs", &HypermeshDrawableData::_lod_graphs);
  clazz->directVectorProperty("lod_distances", &HypermeshDrawableData::_lod_distances);
  clazz->directVectorProperty("impostor_lods", &HypermeshDrawableData::_impostor_lods); // extra-tier idx -> billboard tier
  clazz->directProperty("impostor_grid", &HypermeshDrawableData::_impostor_grid);       // atlas view count (grid×grid)
  clazz->directProperty("impostor_tile", &HypermeshDrawableData::_impostor_tile);       // per-view atlas tile pixels
  clazz->directProperty("impostor_ssaa", &HypermeshDrawableData::_impostor_ssaa);       // bake supersample factor
  clazz->directProperty("impostor_msaa", &HypermeshDrawableData::_impostor_msaa);       // bake multisample count
  clazz->directMapProperty("lod_materials", &HypermeshDrawableData::_lod_material_assets); // LOD idx str -> mtl name
  clazz->directProperty("material_asset", &HypermeshDrawableData::_material_asset_name);
  // O3 stage 3 — stored-mode per-section texture-array bake (opt-in). Round-trips tojson->player.
  clazz->directProperty("section_bake", &HypermeshDrawableData::_section_bake);
  clazz->directProperty("section_bake_res", &HypermeshDrawableData::_section_bake_res);
  clazz->directProperty("section_mips", &HypermeshDrawableData::_section_mips); // A8: trilinear mip chains (default ON)
  clazz->directVectorProperty("section_targets", &HypermeshDrawableData::_section_targets);
  clazz->directProperty("animated", &HypermeshDrawableData::_animated);
  clazz->directProperty("face_viz", &HypermeshDrawableData::_face_viz);
  clazz->directProperty("tag_viz", &HypermeshDrawableData::_tag_viz);
  clazz->directProperty("wireframe", &HypermeshDrawableData::_wireframe);
  clazz->directProperty("vtx_budget", &HypermeshDrawableData::_vtx_budget);
  clazz->directVectorProperty("instance_matrices", &HypermeshDrawableData::_instance_matrices);
  clazz->directProperty("instance_source", &HypermeshDrawableData::_instance_source_name);
  // LOD/Phase 2 — drawable-level instance source (portable asset+sink+type, or direct ogeo)
  clazz->directProperty("instance_scatter_asset", &HypermeshDrawableData::_instance_scatter_asset);
  clazz->directProperty("instance_sink", &HypermeshDrawableData::_instance_sink);
  clazz->directProperty("instance_ogeo_path", &HypermeshDrawableData::_instance_ogeo_path);
  clazz->directProperty("instance_type_id", &HypermeshDrawableData::_instance_type_id);
  // E.3 — per-gid material bindings (gid-as-string -> material asset name)
  clazz->directMapProperty("gid_materials", &HypermeshDrawableData::_gid_material_assets);
  // E.4 — per-view GPU instance cull
  clazz->directProperty("cull", &HypermeshDrawableData::_cull);
  clazz->directProperty("cull_bound", &HypermeshDrawableData::_cull_bound);
  clazz->directProperty("cull_slabs", &HypermeshDrawableData::_cull_slabs);
  clazz->directProperty("cull_tightness", &HypermeshDrawableData::_cull_tightness);
  clazz->directProperty("cull_distance", &HypermeshDrawableData::_cull_distance);
}

HypermeshDrawableData::HypermeshDrawableData() {
}
HypermeshDrawableData::~HypermeshDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////
// the lazy bootstrap state — lives in the drawable's _liveRecompute closure. The OUTER
// closure stays installed for the drawable's whole life (reassigning the std::function
// we're executing inside would be UB); per-frame it forwards to the setupMeshRender-built
// inner hook once the build has happened.
///////////////////////////////////////////////////////////////////////////////

namespace {
struct HmBootstrap {
  bool _built       = false;
  bool _warned_mtl  = false;
  std::shared_ptr<ComputeDrawableData> _cdd; // keeps the configured data (and its closures) alive
  std::function<void(Context*, ComputeDrawable*)> _inner;
  // E.6/2.12 — MaterialParamSink wiring, resolved once at build: each sink's
  // pokeable "value" DATA plug + every (material, param-handle) pair whose
  // generated shader declares sink->_param_name. Drained per-frame below.
  struct SinkBinding {
    materialparamsinkdata_ptr_t _data;
    dflow::floatinpplug_ptr_t _plug;
    std::vector<std::pair<pbrmaterial_ptr_t, fxparam_constptr_t>> _targets;
    float _last = std::numeric_limits<float>::quiet_NaN(); // NaN != anything -> first frame always applies
  };
  std::vector<SinkBinding> _sinks;
  // O3 stage 3 — the stored-mode section-array COLD-bake poll (mirrors the Python cold_wait state): the
  // in-flight bake job + the content key/targets it will assemble+cache+rebind onto the sampler material.
  sectionbakejob_ptr_t     _sectionJob;
  std::string              _sectionKey;
  std::vector<std::string> _sectionTargets;
  int                      _sectionRes    = 256;
  int                      _sectionLayers = 0;
  bool                     _sectionRebound = false;
  bool                     _sectionAsyncPending = false; // asyncWorkBegin fired -> exactly one asyncWorkEnd
  void endSectionAsync() {
    if (_sectionAsyncPending) {
      _sectionAsyncPending = false;
      asyncWorkEnd("hypermesh_section_bake");
    }
  }
};

// O3 stage 3 — bind one assembled TextureArray per capture target onto the stored SAMPLER material's
// sampler2DArray uniform (same name as the capture target). The 2.12 rebind stamp propagates the value
// into every cached pipeline at its next beginBlock — so a cold placeholder->real swap is live.
static void bindSectionArrays(pbrmaterial_ptr_t mtl, const std::vector<std::string>& targets,
                              const std::vector<texturearray_ptr_t>& arrays) {
  if (not mtl)
    return;
  auto fs = mtl->_as_freestyle;
  if (not fs)
    return;
  for (size_t t = 0; t < targets.size() and t < arrays.size(); t++)
    if (auto par = fs->param(targets[t]))
      mtl->bindParam(par, arrays[t]);
}
} // namespace

drawable_ptr_t HypermeshDrawableData::createDrawable() const {
  auto drw   = std::make_shared<ComputeDrawable>();
  auto state = std::make_shared<HmBootstrap>();
  // self_alias: non-owning alias so the closure can reach the (host-mutated) runtime fields;
  // the DrawableData outlives its drawables by the scene contract.
  auto self = this;

  // O3 stage 3 — register the stored-mode section bake as pending ASYNC WORK FROM STAGE TIME (mirrors the
  // terrain texbake), so an offscreen player waiter (--offscreen / --snapshot) knows the bake is pending
  // BEFORE the first onGpuUpdate even runs (the marker can't wait on the render-thread build to fire it).
  // Ended exactly once: WARM-bind, COLD rebind, or an inert (no-section) build — see endSectionAsync().
  if (self->_section_bake) {
    asyncWorkBegin("hypermesh_section_bake");
    state->_sectionAsyncPending = true;
  }

  drw->_liveRecompute = [self, state](Context* ctx, ComputeDrawable* drawable) {
    if (not state->_built) {
      //////////////////////////////////////////////////////////////////
      // resolve the material — direct assignment wins; else poll the resolver
      // (the AssetSystem may materialize artifacts a frame after stage time).
      //////////////////////////////////////////////////////////////////
      if (not self->_resolved_material and self->_material_resolver)
        self->_resolved_material = self->_material_resolver();
      if (not self->_resolved_material) {
        if (not state->_warned_mtl) {
          logchan_hmdrw->log(
              "HypermeshDrawable: material asset<%s> not resolved yet — skipping frames until it is",
              self->_material_asset_name.c_str());
          state->_warned_mtl = true;
        }
        return;
      }
      // E.3 — resolve EVERY per-gid material (same retry contract); the build
      // waits for the full set so buckets never flicker in one at a time.
      for (const auto& [gid_str, mtl_name] : self->_gid_material_assets) {
        int gid = atoi(gid_str.c_str());
        if (self->_resolved_gid_materials.count(gid))
          continue;
        pbrmaterial_ptr_t gm;
        if (self->_material_resolver_named)
          gm = self->_material_resolver_named(mtl_name);
        if (not gm) {
          if (not state->_warned_mtl) {
            logchan_hmdrw->log(
                "HypermeshDrawable: gid<%d> material asset<%s> not resolved yet — skipping frames",
                gid, mtl_name.c_str());
            state->_warned_mtl = true;
          }
          return;
        }
        self->_resolved_gid_materials[gid] = gm;
      }
      // per-LOD material overrides (same retry contract): resolve each before building so tier draws
      // never flicker their material in.
      for (const auto& [lod_str, mtl_name] : self->_lod_material_assets) {
        int lod = atoi(lod_str.c_str());
        if (self->_resolved_lod_materials.count(lod))
          continue;
        pbrmaterial_ptr_t lm;
        if (self->_material_resolver_named)
          lm = self->_material_resolver_named(mtl_name);
        if (not lm) {
          if (not state->_warned_mtl) {
            logchan_hmdrw->log(
                "HypermeshDrawable: LOD<%d> material asset<%s> not resolved yet — skipping frames",
                lod, mtl_name.c_str());
            state->_warned_mtl = true;
          }
          return;
        }
        self->_resolved_lod_materials[lod] = lm;
      }
      if (not self->_graphdata) {
        logchan_hmdrw->log("HypermeshDrawable: NULL graphdata — drawable is inert");
        state->endSectionAsync(); // never baking -> release the offscreen waiter
        state->_built = true; // nothing will ever change; stop re-checking
        return;
      }
      //////////////////////////////////////////////////////////////////
      // materializeLive + the make_drawable port (all callees already C++)
      //////////////////////////////////////////////////////////////////
      auto live = materializeLive(self->_graphdata, ctx, self->_vtx_budget);
      OrkAssert(live and live->_mesh);
      self->_live = live;
      // LOD/Phase 2 — a DRAWABLE-LEVEL instance source resolves the baked ScatterSet HERE
      // (decoupled from the geometry graph), so one shared InstanceSet routes to N LOD meshes.
      // Overrides any graph-carried ScatterSource. Resolved once, on this first build.
      if (not self->_instance_sink.empty() or not self->_instance_ogeo_path.empty()) {
        auto iset = std::make_shared<dflowgfx::InstanceSetInst>(nullptr);
        fillInstanceSetFromScatter(
            ctx, iset, self->_instance_scatter_asset, self->_instance_sink,
            self->_instance_ogeo_path, self->_instance_type_id);
        live->_instances = iset;
      }
      auto mtl   = self->_resolved_material;
      auto fsmtl = mtl->_as_freestyle;
      OrkAssert(fsmtl); // ptex3d materials are freestyle-backed (generated fxv2)
      bool tag_viz  = self->_tag_viz;
      bool face_viz = self->_face_viz or tag_viz; // tag viz indexes __tags by the per-tri face id
      int inst_count = int(self->_instance_matrices.size() / 16);
      // E.2: a graph-carried InstanceSet (ScatterSource) takes precedence over reflected floats
      bool instanced = (inst_count > 1) or (live->_instances and live->_instances->_count > 0);

      auto cdd        = std::make_shared<ComputeDrawableData>();
      cdd->_material  = mtl;
      cdd->_instanced = instanced;
      // vertex channels -> graphics-storage 0..4 (the live hook refreshes these BY INDEX, so
      // the order here is a contract with hmdflow_render.cpp's refresh loop)
      static const char* kChanBlocks[5] = {"sif_ptex_vtx", "sif_N", "sif_B", "sif_uv", "sif_clr"};
      for (int i = 0; i < 5; i++) {
        auto block = fsmtl->storageBlock(kChanBlocks[i]);
        auto chan  = live->_mesh->channel(MeshChannel(i));
        if (block and chan)
          cdd->addGraphicsStorage(block, chan->_ssbo);
      }
      // O3 stage 3 — STORED-MODE bake: _gid_material_assets is the per-gid BAKE MAP, not draw buckets.
      // The mesh draws ONCE with the sampler material (_resolved_material); the per-section content is
      // baked into a texture array the sampler samples at ctx.layer. So NO gid buckets in stored mode.
      const bool stored = self->_section_bake;
      std::vector<int> bound_gids;
      if (not stored)
        for (const auto& [gid, gm] : self->_resolved_gid_materials)
          bound_gids.push_back(gid);
      // E.4 — the cull bound is AUTO (object-space sphere from a one-time mesh position readback)
      // inside setupMeshRender when _cull_bound.w<=0; an explicit _cull_bound overrides it (e.g. an
      // animated mesh that outgrows its static bounds). Same path as the python make_drawable.
      // Phase 3c — materialize each LOD tier graph to its own live (static distinct mesh). Parallel to
      // _lod_distances; passed to setupMeshRender as tiers 1..N. Empty -> single tier (legacy).
      std::vector<livehypermesh_ptr_t> lod_lives;
      for (auto& g : self->_lod_graphs) {
        auto llive = g ? materializeLive(g, ctx, self->_vtx_budget) : nullptr;
        lod_lives.push_back(llive);
      }
      auto handles = setupMeshRender(
          cdd.get(), live, ctx, self->_animated, face_viz, tag_viz, self->_wireframe,
          inst_count, self->_instance_matrices, bound_gids,
          self->_cull and instanced, self->_cull_bound,
          self->_cull_slabs, self->_cull_tightness, self->_cull_distance,
          lod_lives, self->_lod_distances, self->_impostor_lods,
          self->_resolved_gid_materials, self->_impostor_grid, self->_impostor_tile,
          self->_impostor_ssaa, self->_impostor_msaa);
      // E.3 — one extra indexed-indirect draw per bound gid: same index buffer,
      // args offset = gid slot * 20 bytes, OWN material + OWN storage-block list
      // (block handles are per-shader; first 5 entries = the vertex channels,
      // the live refresh updates them by index — same contract as the main draw).
      // STORED MODE skips this entirely (the gid materials are the bake map, not buckets).
      for (const auto& [gid, gm] : (stored ? std::map<int, pbrmaterial_ptr_t>{} : self->_resolved_gid_materials)) {
        auto gfs = gm->_as_freestyle;
        OrkAssert(gfs);
        ComputeDrawable::BucketDraw bucket;
        bucket._material   = gm;
        bucket._argsOffset = size_t(gid) * 20;
        for (int i = 0; i < 5; i++) {
          auto block = gfs->storageBlock(kChanBlocks[i]);
          auto chan  = live->_mesh->channel(MeshChannel(i));
          if (block and chan)
            bucket._graphicsStorage.push_back({block, chan->_ssbo});
        }
        if (instanced and handles._instMtx) {
          if (auto blk = gfs->storageBlock("storage_inst_mtx"))
            bucket._graphicsStorage.push_back({blk, handles._instMtx});
          if (handles._instAttr)
            if (auto blk = gfs->storageBlock("storage_inst_attr"))
              bucket._graphicsStorage.push_back({blk, handles._instAttr});
          // cascade-cull fix: gid-bucket shadow instance override (this bucket material's block handles).
          if (handles._instMtxShadow) {
            if (auto blk = gfs->storageBlock("storage_inst_mtx"))
              bucket._shadowStorageOverrides.push_back({blk, handles._instMtxShadow});
            if (handles._instAttrShadow)
              if (auto blk = gfs->storageBlock("storage_inst_attr"))
                bucket._shadowStorageOverrides.push_back({blk, handles._instAttrShadow});
          }
        }
        cdd->_bucketDraws.push_back(bucket);
      }
      // Phase 3b — LOD TIER DRAWS: each extra distance tier redraws the mesh (main material + every gid
      // material, mirroring tier 0) from its OWN indirect args (the per-tier fanout stamped its
      // instanceCount=VIS[t]) and its tier mesh's index, binding the SHARED OUT_M/attrs at the tier's
      // byte offset via the graphics sub-range bind (VS reads the slice from gl_InstanceIndex==0).
      for (size_t lt = 0; lt < handles._lodTiers.size(); lt++) {
        const auto& tier = handles._lodTiers[lt];
        // the tier draws its OWN (coarser) mesh — its index buffer references the TIER mesh's verts, so
        // its vertex channels MUST come from the tier mesh, not the main (live) mesh (same-mesh 3b hid
        // this; distinct LOD meshes expose it -> the far tier read the wrong verts -> nothing rendered).
        auto tier_mesh = (lt < lod_lives.size() and lod_lives[lt]) ? lod_lives[lt]->_mesh : nullptr;
        auto add_tier_bucket = [&](material_ptr_t mat, freestyle_mtl_ptr_t fs, size_t argsOff) {
          if (not fs or not tier_mesh)
            return;
          ComputeDrawable::BucketDraw b;
          b._material       = mat;
          b._argsOffset     = argsOff;
          b._indexSSBO      = tier._index;
          b._argsSSBO       = tier._args;
          b._instByteOffset = tier._instByteOffset;
          for (int i = 0; i < 5; i++) {
            auto block = fs->storageBlock(kChanBlocks[i]);
            auto chan  = tier_mesh->channel(MeshChannel(i));
            if (block and chan)
              b._graphicsStorage.push_back({block, chan->_ssbo});
          }
          if (instanced and handles._instMtx) { // bound at the tier offset (NOT in _graphicsStorage)
            b._instMtxBlock = fs->storageBlock("storage_inst_mtx");
            b._instMtxBuf   = handles._instMtx;
            if (handles._instAttr) {
              b._instAttrBlock = fs->storageBlock("storage_inst_attr");
              b._instAttrBuf   = handles._instAttr;
            }
          }
          cdd->_bucketDraws.push_back(b);
        };
        // PER-LOD material override: draw the WHOLE tier mesh with one material (gid slot 0; the cull's
        // per-tier fanout stamps instanceCount into every bound slot, but only slot 0 is drawn here).
        // Otherwise mirror tier 0 — the main material + each gid bucket.
        auto lod_over = self->_resolved_lod_materials.find(int(lt));
        if (lod_over != self->_resolved_lod_materials.end()) {
          add_tier_bucket(lod_over->second, lod_over->second->_as_freestyle, 0);
        } else {
          add_tier_bucket(mtl, fsmtl, 0); // the main material (gid slot 0)
          for (const auto& [gid, gm] : self->_resolved_gid_materials)
            add_tier_bucket(gm, gm->_as_freestyle, size_t(gid) * 20);
        }
      }
      if (face_viz and handles._faceid)
        cdd->addGraphicsStorage(fsmtl->storageBlock("sif_triface"), handles._faceid);
      if (tag_viz) {
        auto tags = live->_mesh->face("__tags");
        if (tags)
          cdd->addGraphicsStorage(fsmtl->storageBlock("sif_seltags"), tags->_ssbo);
      }
      if (instanced and handles._instMtx) { // bound LAST (the live refresh loop only touches <= slot 6)
        cdd->addGraphicsStorage(fsmtl->storageBlock("storage_inst_mtx"), handles._instMtx);
        if (handles._instAttr)              // E.2: typed per-instance data -> the VS attrs block
          cdd->addGraphicsStorage(fsmtl->storageBlock("storage_inst_attr"), handles._instAttr);
        // cascade-cull fix: the sun-cascade depth passes re-bind these instance blocks at the SHADOW
        // OUT_M/OUT_A (this material's block handles) so they draw the union-sun survivor set.
        if (handles._instMtxShadow) {
          if (auto blk = fsmtl->storageBlock("storage_inst_mtx"))
            cdd->_shadowStorageOverrides.push_back({blk, handles._instMtxShadow});
          if (handles._instAttrShadow)
            if (auto blk = fsmtl->storageBlock("storage_inst_attr"))
              cdd->_shadowStorageOverrides.push_back({blk, handles._instAttrShadow});
        }
      }
      if (self->_wireframe and self->_resolved_overlay_material) {
        // the overlay (LINES) pull-VS reads P at overlay slot 0, N at slot 1 (refresh contract)
        auto omtl = self->_resolved_overlay_material;
        cdd->_overlayMaterial = omtl;
        auto ofs = std::dynamic_pointer_cast<PBRMaterial>(omtl)
                       ? std::dynamic_pointer_cast<PBRMaterial>(omtl)->_as_freestyle
                       : std::dynamic_pointer_cast<FreestyleMaterial>(omtl);
        if (ofs) {
          if (auto ch = live->_mesh->channel(MeshChannel::POSITION))
            cdd->addOverlayGraphicsStorage(ofs->storageBlock("sif_ptex_vtx"), ch->_ssbo);
          if (auto ch = live->_mesh->channel(MeshChannel::NORMAL))
            cdd->addOverlayGraphicsStorage(ofs->storageBlock("sif_N"), ch->_ssbo);
          if (instanced and handles._instMtx) {
            cdd->addOverlayGraphicsStorage(ofs->storageBlock("storage_inst_mtx"), handles._instMtx);
            if (handles._instAttr)
              cdd->addOverlayGraphicsStorage(ofs->storageBlock("storage_inst_attr"), handles._instAttr);
          }
        }
      }
      //////////////////////////////////////////////////////////////////
      // O3 stage 3 — STORED-MODE section-array bake driver (mirrors terrainTexBake's shape). The mesh
      // draws ONCE with the sampler material (mtl); the per-section surfaces are baked (per-gid bake map)
      // into one texture array per capture target, sampled at ctx.layer. COLD = in-frame GPU bake +
      // content-addressed cache write + placeholder->rebind; WARM = load cache. Registered BEFORE the graft
      // so the bake one-shot (installed on cdd->_oneShotRender by prepareSectionBakeMapped) is copied below.
      //////////////////////////////////////////////////////////////////
      if (stored) {
        auto layerGids = sectionUnwrapLayerGids(live, ctx);
        if (layerGids.empty()) {
          logchan_hmdrw->log(
              "HypermeshDrawable: section_bake set but no SectionUnwrap layer->gid table — bake skipped "
              "(the last graph op must be section_unwrap on a gid-partitioned mesh)");
          state->endSectionAsync();
        } else {
          int numLayers = int(layerGids.size());
          int bakeRes   = std::max(8, self->_section_bake_res);
          std::vector<std::string> targets =
              self->_section_targets.empty() ? std::vector<std::string>{"SectionAlbedo"} : self->_section_targets;
          std::map<int, std::string> gidNames;
          for (const auto& [gs, nm] : self->_gid_material_assets)
            gidNames[atoi(gs.c_str())] = nm;
          std::string key = sectionBakeContentKey(
              self->_graphdata, self->_material_asset_name, gidNames, layerGids, bakeRes);
          bool warm = sectionArrayCacheWarm(key, targets, bakeRes, numLayers);
          if (warm) {
            bindSectionArrays(mtl, targets,
                              loadSectionArraysFromCache(ctx, key, targets, bakeRes, numLayers, self->_section_mips));
            logchan_hmdrw->log(
                "HypermeshDrawable: section bake WARM (%d layers, %zu targets) — cache loaded",
                numLayers, targets.size());
            state->endSectionAsync();
          } else {
            // COLD: a valid gray placeholder per target (the sampler2DArray must be shader-readable during
            // the few-frame async bake), then kick the per-gid bake + register the completion poll (tail).
            std::vector<texturearray_ptr_t> ph;
            for (size_t t = 0; t < targets.size(); t++)
              ph.push_back(placeholderSectionArray(ctx, numLayers, bakeRes));
            bindSectionArrays(mtl, targets, ph);
            state->_sectionJob = prepareSectionBakeMapped(
                ctx, cdd.get(), live, mtl, self->_resolved_gid_materials, layerGids, bakeRes, int(targets.size()));
            state->_sectionKey     = key;
            state->_sectionTargets = targets;
            state->_sectionRes     = bakeRes;
            state->_sectionLayers  = numLayers;
            logchan_hmdrw->log(
                "HypermeshDrawable: section bake COLD (%d layers, %zu targets @ %dpx) — GPU bake kicked",
                numLayers, targets.size(), bakeRes);
          }
        }
      }
      //////////////////////////////////////////////////////////////////
      // graft the configured state onto the LIVE drawable (same copy set as
      // ComputeDrawableData::createDrawable, minus the render lambda we already
      // own and minus _liveRecompute — the inner hook forwards via state->_inner).
      //////////////////////////////////////////////////////////////////
      drawable->_passes          = cdd->_passes;
      drawable->_camParamsSSBO   = cdd->_camParamsSSBO;
      drawable->_camParamsOffset = cdd->_camParamsOffset;
      drawable->_material        = cdd->_material;
      drawable->_pipeline        = cdd->_pipeline;
      drawable->_graphicsStorage = cdd->_graphicsStorage;
      drawable->_argsSSBO        = cdd->_argsSSBO;
      drawable->_argsOffset      = cdd->_argsOffset;
      drawable->_indexSSBO       = cdd->_indexSSBO;
      drawable->_primtype        = cdd->_primtype;
      drawable->_indexSize       = cdd->_indexSize;
      drawable->_instanced       = cdd->_instanced;
      drawable->_overlayMaterial        = cdd->_overlayMaterial;
      drawable->_overlayPipeline        = cdd->_overlayPipeline;
      drawable->_overlayGraphicsStorage = cdd->_overlayGraphicsStorage;
      drawable->_overlayArgsSSBO        = cdd->_overlayArgsSSBO;
      drawable->_overlayArgsOffset      = cdd->_overlayArgsOffset;
      drawable->_overlayIndexSSBO       = cdd->_overlayIndexSSBO;
      drawable->_overlayPrimtype        = cdd->_overlayPrimtype;
      drawable->_overlayIndexSize       = cdd->_overlayIndexSize;
      drawable->_bucketDraws            = cdd->_bucketDraws;
      drawable->_perViewCompute         = cdd->_perViewCompute; // E.4: the per-view cull hook
      drawable->_perViewComputeShadow   = cdd->_perViewComputeShadow;   // cascade-cull fix
      drawable->_argsSSBOShadow         = cdd->_argsSSBOShadow;         // cascade-cull fix
      drawable->_shadowStorageOverrides = cdd->_shadowStorageOverrides; // cascade-cull fix
      drawable->_oneShotRender          = cdd->_oneShotRender;  // A2: the one-shot impostor-bake hook
      //////////////////////////////////////////////////////////////////
      // E.6/2.12 — collect MaterialParamSinks: resolve each sink's param
      // name against EVERY material this drawable binds (main + gid
      // buckets); a name matching nothing is LOUD (the sink would be a
      // silent no-op otherwise). Drained per-frame after the inner hook.
      //////////////////////////////////////////////////////////////////
      for (size_t im = 0; im < self->_graphdata->numModules(); im++) {
        auto sinkdata = std::dynamic_pointer_cast<MaterialParamSinkData>(self->_graphdata->module(im));
        if (not sinkdata)
          continue;
        HmBootstrap::SinkBinding b;
        b._data = sinkdata;
        b._plug = sinkdata->typedInputNamed<dflow::FloatPlugTraits>("value");
        auto offer = [&](pbrmaterial_ptr_t m) {
          if (not m or not m->_as_freestyle)
            return;
          if (auto par = m->_as_freestyle->param(sinkdata->_param_name))
            b._targets.push_back({m, par});
        };
        offer(self->_resolved_material);
        for (const auto& [gid, gm] : self->_resolved_gid_materials)
          offer(gm);
        if (b._targets.empty() or not b._plug) {
          logchan_hmdrw->log(
              "HypermeshDrawable: MaterialParamSink param<%s> matches NO param on any bound material — sink is INERT",
              sinkdata->_param_name.c_str());
          continue;
        }
        state->_sinks.push_back(std::move(b));
      }
      state->_inner = cdd->_liveRecompute;
      state->_cdd   = cdd;
      state->_built = true;
      logchan_hmdrw->log(
          "HypermeshDrawable: materialized (mtl<%s> animated<%d> faces<%d> verts<%d> inst<%d>)",
          self->_material_asset_name.c_str(),
          int(self->_animated),
          live->_mesh->_num_faces,
          live->_mesh->_num_verts,
          inst_count > 1 ? inst_count : 1);
    }
    if (state->_inner)
      state->_inner(ctx, drawable);
    // E.6/2.12 — drain MaterialParamSinks: read the pokeable DATA plug (NOT an
    // inst copy, so a poke is live even on a STATIC graph that never recomputes)
    // and bindParam on change; the material's stamp then propagates the value
    // into EVERY cached pipeline (color, depth-prepass, gid buckets) at its
    // next beginBlock. Runs on the render thread (onGpuUpdate), same thread
    // as the draws that consume it.
    for (auto& sink : state->_sinks) {
      float v = *(sink._plug->_value);
      if (v != sink._last) {
        sink._last = v;
        for (const auto& [m, par] : sink._targets)
          m->bindParam(par, v);
      }
    }
    // NB: no per-frame clock feed here. A VS-wind material (GpuMeshRenderSource(displace=Wind())) reads
    // the standard `Time` uniform, whose value is the RCFD_TIME provider — bound ONCE at material build
    // (asset_gen.cpp) and supplied every frame BY THE ENGINE through fx_pipeline's named-param providers
    // (same path as MatMVP/modcolor). Works identically on the main material and every gid bucket, in
    // the viewer, a scene, and the zero-Python player — no displace-specific code in this drawable.
    //
    // O3 stage 3 — COLD section-bake completion poll (mirrors the Python cold_wait -> rebind). Once the
    // in-frame GPU bake's async captures drain, assemble one texture array per target, WRITE the content-
    // addressed cache, and REBIND the real arrays onto the sampler material (the placeholder drops out via
    // the 2.12 rebind stamp). Fires exactly once; then asyncWorkEnd clears the offscreen waiter's marker.
    if (state->_sectionJob and not state->_sectionRebound and state->_sectionJob->isReady()) {
      auto arrays = assembleSectionArraysFromJob(
          ctx, state->_sectionJob, state->_sectionKey, state->_sectionTargets, state->_sectionRes,
          self->_section_mips);
      bindSectionArrays(self->_resolved_material, state->_sectionTargets, arrays);
      state->_sectionRebound = true;
      state->_sectionJob     = nullptr;
      state->endSectionAsync();
      logchan_hmdrw->log(
          "HypermeshDrawable: section bake COMPLETE (%d layers, %zu targets) — arrays rebound + cached",
          state->_sectionLayers, arrays.size());
    }
  };

  auto draw_raw = drw.get();
  drw->setRenderLambda([draw_raw](RenderContextInstData& RCID) { draw_raw->_renderIndirect(RCID); });
  return drw;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::hypermesh

ImplementReflectionX(ork::lev2::hypermesh::HypermeshDrawableData, "HypermeshDrawableData");
