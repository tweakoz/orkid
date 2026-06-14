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
#include <ork/util/logger.h>
#include <limits>

namespace ork::lev2::hypermesh {

static logchannel_ptr_t logchan_hmdrw = logger()->configureChannel("HYPERMESH", fvec3(0.9, 0.9, 0.2));

///////////////////////////////////////////////////////////////////////////////

void HypermeshDrawableData::describeX(object::ObjectClass* clazz) {
  clazz->directObjectProperty("graph", &HypermeshDrawableData::_graphdata);
  clazz->directProperty("material_asset", &HypermeshDrawableData::_material_asset_name);
  clazz->directProperty("animated", &HypermeshDrawableData::_animated);
  clazz->directProperty("face_viz", &HypermeshDrawableData::_face_viz);
  clazz->directProperty("tag_viz", &HypermeshDrawableData::_tag_viz);
  clazz->directProperty("wireframe", &HypermeshDrawableData::_wireframe);
  clazz->directProperty("vtx_budget", &HypermeshDrawableData::_vtx_budget);
  clazz->directVectorProperty("instance_matrices", &HypermeshDrawableData::_instance_matrices);
  clazz->directProperty("instance_source", &HypermeshDrawableData::_instance_source_name);
  // E.3 — per-gid material bindings (gid-as-string -> material asset name)
  clazz->directMapProperty("gid_materials", &HypermeshDrawableData::_gid_material_assets);
  // E.4 — per-view GPU instance cull
  clazz->directProperty("cull", &HypermeshDrawableData::_cull);
  clazz->directProperty("cull_bound", &HypermeshDrawableData::_cull_bound);
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
};
} // namespace

drawable_ptr_t HypermeshDrawableData::createDrawable() const {
  auto drw   = std::make_shared<ComputeDrawable>();
  auto state = std::make_shared<HmBootstrap>();
  // self_alias: non-owning alias so the closure can reach the (host-mutated) runtime fields;
  // the DrawableData outlives its drawables by the scene contract.
  auto self = this;

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
      if (not self->_graphdata) {
        logchan_hmdrw->log("HypermeshDrawable: NULL graphdata — drawable is inert");
        state->_built = true; // nothing will ever change; stop re-checking
        return;
      }
      //////////////////////////////////////////////////////////////////
      // materializeLive + the make_drawable port (all callees already C++)
      //////////////////////////////////////////////////////////////////
      auto live = materializeLive(self->_graphdata, ctx, self->_vtx_budget);
      OrkAssert(live and live->_mesh);
      self->_live = live;
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
      std::vector<int> bound_gids;
      for (const auto& [gid, gm] : self->_resolved_gid_materials)
        bound_gids.push_back(gid);
      // E.4 — the cull bound: AUTO (w<=0) computes the object-space sphere once
      // from a position readback of the materialized mesh (+5% pad). Animated
      // meshes that outgrow their static bounds should override cull_bound.
      fvec4 cull_bound = self->_cull_bound;
      if (self->_cull and instanced and cull_bound.w <= 0.0f) {
        int nv   = live->_mesh->_num_verts;
        auto pch = live->_mesh->channel(MeshChannel::POSITION);
        if (nv > 0 and pch) {
          auto fxi = ctx->FXI();
          std::vector<float> P(size_t(nv) * 4);
          auto m = fxi->mapStorageBuffer(pch->_ssbo, 0, size_t(nv) * 16, BufferMapAccess::READ_ONLY);
          std::memcpy(P.data(), m->_mappedaddr, size_t(nv) * 16);
          fxi->unmapStorageBuffer(m.get());
          fvec3 bmin(P[0], P[1], P[2]), bmax = bmin;
          for (int i = 1; i < nv; i++) {
            fvec3 p(P[i * 4], P[i * 4 + 1], P[i * 4 + 2]);
            bmin = fvec3(std::min(bmin.x, p.x), std::min(bmin.y, p.y), std::min(bmin.z, p.z));
            bmax = fvec3(std::max(bmax.x, p.x), std::max(bmax.y, p.y), std::max(bmax.z, p.z));
          }
          fvec3 c = (bmin + bmax) * 0.5f;
          float r = (bmax - bmin).length() * 0.5f * 1.05f;
          cull_bound = fvec4(c, r);
          logchan_hmdrw->log("HypermeshDrawable: auto cull bound c<%.2f %.2f %.2f> r<%.2f>", c.x, c.y, c.z, r);
        }
      }
      auto handles = setupMeshRender(
          cdd.get(), live, ctx, self->_animated, face_viz, tag_viz, self->_wireframe,
          inst_count, self->_instance_matrices, bound_gids,
          self->_cull and instanced, cull_bound);
      // E.3 — one extra indexed-indirect draw per bound gid: same index buffer,
      // args offset = gid slot * 20 bytes, OWN material + OWN storage-block list
      // (block handles are per-shader; first 5 entries = the vertex channels,
      // the live refresh updates them by index — same contract as the main draw).
      for (const auto& [gid, gm] : self->_resolved_gid_materials) {
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
        }
        cdd->_bucketDraws.push_back(bucket);
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
  };

  auto draw_raw = drw.get();
  drw->setRenderLambda([draw_raw](RenderContextInstData& RCID) { draw_raw->_renderIndirect(RCID); });
  return drw;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::hypermesh

ImplementReflectionX(ork::lev2::hypermesh::HypermeshDrawableData, "HypermeshDrawableData");
