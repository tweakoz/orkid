////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/renderer/compute_drawable.h>
#include <ork/lev2/gfx/renderphasestats.h> // perf HUD: terrain/hm cull timing
#include <ork/lev2/gfx/renderer/cull_debug.h> // ORKID_DISABLE_FRUSTUM_CULL / _OCCLUSION_CULL debug levers
#include <ork/lev2/gfx/renderer/hzb.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/ci.h>
#include <ork/lev2/gfx/gbi.h>
#include <ork/lev2/gfx/fx_pipeline.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/material_pbr.inl> // [SPVR:CDSEL]: the per-view ublk_stereo bind is a PBRMaterial handle
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/util/logger.h>
#include <cmath>
#include <set>
#include <algorithm>

namespace ork::lev2 {

static logchannel_ptr_t logchan_cdraw = logger()->configureChannel("TERRAIN", fvec3(0.4, 0.9, 0.4));

ComputeDrawable::ComputeDrawable()
    : CallbackDrawable(nullptr) {
}

///////////////////////////////////////////////////////////////////////////////
// per-FRAME (view-independent, ONCE before any viewport): the live dataflow graph re-eval.
// IN-FRAME, manages its own dispatch phase — NOT a new frame. Also refreshes our
// graphics-storage/args to the mesh's CURRENT channels (dynamic topology re-pools).
///////////////////////////////////////////////////////////////////////////////

void ComputeDrawable::onGpuUpdate(Context* ctx) const {
  if (_liveRecompute)
    _liveRecompute(ctx, const_cast<ComputeDrawable*>(this));
}

///////////////////////////////////////////////////////////////////////////////
// per-VP pre-render: write the camera block, then run the compute passes (one dispatch phase =
// one submit+wait; storageBarrier between passes so each pass sees the previous one's writes).
///////////////////////////////////////////////////////////////////////////////

void ComputeDrawable::onPreRender(Context* ctx, const CameraMatrices& cammtx) const {
  auto FXI = ctx->FXI();
  auto CI  = ctx->CI();

  // HZB 1-phase occlusion (terrain cull only — gated on _hzbBlock): the per-frame max-depth pyramid,
  // bound to sif_hzb on every pass below. Fetched here so misc.yzw (base w/h/mips) is written into the
  // CamBlk in the same pass. nullptr => occlusion disabled this frame (cull stays frustum-only).
  FxShaderStorageBuffer* hzbBuf = nullptr;
  int hzb_w = 0, hzb_h = 0, hzb_mips = 0;
  if (_hzbBlock) {
    static const int s_hzbMode = []() { const char* e = getenv("ORKID_HZB_OCCLUSION"); return e ? atoi(e) : 2; }();
    if (s_hzbMode != 0 and not cullOcclusionDisabled()) // ORKID_DISABLE_OCCLUSION_CULL: leave hzbBuf null -> misc.y==0 -> cull stays frustum-only
      if (auto rcfd = ctx->topRenderContextFrameData())
        if (auto v = rcfd->tryUserProperty<uint64_t>("HZB"_crc)) {
          auto* hzb = reinterpret_cast<HZBBuilder*>(uintptr_t(v.value()));
          if (hzb and hzb->_valid and hzb->_ssbo) {
            hzbBuf = hzb->_ssbo;
            hzb_w = hzb->_baseW; hzb_h = hzb->_baseH; hzb_mips = hzb->_mips;
          }
        }
  }

  // CamBlk (std430): mat4 vp; mat4 inv_vp; vec4 eye; vec4 misc; — the cull/gen compute reads this.
  if (_camParamsSSBO) {
    struct CamBlk {
      float vp[16];
      float ivp[16];
      float eye[4];
      float misc[4];
    } blk;
    memcpy(blk.vp, cammtx.GetVPMatrix().asArray(), 64);
    memcpy(blk.ivp, cammtx.GetIVPMatrix().asArray(), 64);
    const float* iv = cammtx.GetIVMatrix().asArray(); // inverse-view translation = eye (col-major)
    blk.eye[0] = iv[12];
    blk.eye[1] = iv[13];
    blk.eye[2] = iv[14];
    blk.eye[3] = 1.0f;
    // CullFrustumScale (frame-global RCFD user prop, stamped in Scene::preRender): carried into the
    // generic-compute cull (e.g. terrain cs_terrain_cull) via misc.x. >1 widen/cull-less, 1.0 exact,
    // <1 narrow/cull-more. The shader applies the reciprocal to the side planes (same sense as the
    // hypermesh MeshInstCull). Same one knob as the instanced cull — read here, applied in-shader.
    float cfs = 1.0f;
    if (auto rcfd = ctx->topRenderContextFrameData())
      if (auto v = rcfd->tryUserProperty<float>("CullFrustumScale"_crc))
        cfs = v.value();
    // ORKID_DISABLE_FRUSTUM_CULL debug lever: stamp the pass-all sentinel (misc.x < 0) so cs_terrain_cull
    // skips the frustum reject (all chunks treated visible; occlusion, if enabled, still applies). Cached
    // bool, no cost when unset. cfs is otherwise > 0, so a negative value is unambiguous.
    if (cullFrustumDisabled())
      cfs = -1.0f;
    blk.misc[0] = cfs;
    // misc.yzw = HZB base w/h/mips for the occlusion test (0 => unavailable, cull skips occlusion).
    blk.misc[1] = float(hzb_w);
    blk.misc[2] = float(hzb_h);
    blk.misc[3] = float(hzb_mips);
    auto m = FXI->mapStorageBuffer(_camParamsSSBO, _camParamsOffset, sizeof(blk), BufferMapAccess::WRITE_ONLY);
    memcpy(m->_mappedaddr, &blk, sizeof(blk));
    m->unmap();
  }

  if (_perViewCompute) { // E.4: e.g. the hypermesh instance cull (own dispatch phase)
    RenderPhaseScope _s("hm-cull"); // perf HUD
    _perViewCompute(ctx, cammtx);
  }

  if (_oneShotRender) { // A2: in-frame pre-pass (impostor bake). Clear only when it reports done — it returns
    if (_oneShotRender(ctx)) // false while the mesh tri is still dirty (capture stale topology guard) -> retry
      _oneShotRender = nullptr;
  }

  // Which compute passes run this view. Pull-VS path: _passes (the cull writes v_list + the indirect
  // args the draw pulls). Taskless mesh-shader path: _passes is EMPTY (the mesh stage self-culls +
  // generates), so nothing writes the CHUNK-level VIS header the terrain-cull perf HUD reads. When the
  // HUD asks for stats (CullStats enabled), run _statsPasses instead — the SAME reset+cull compute,
  // identical VIS semantics — purely to populate that header (off => skipped, so the mesh path stays
  // compute-free). The mesh DRAW never reads v_list/args, so this stat sidecar is invisible to it.
  const std::vector<ComputeDrawablePass>* passes = &_passes;
  if (_passes.empty() and not _statsPasses.empty() and CullStats::instance().enabled())
    passes = &_statsPasses;
  // MODE 2 keeps _passes EMPTY and carries its one compaction in _inlineComputePass instead; return
  // only when there is NO compute of EITHER kind (baseline/mode-1 with nothing to run).
  if (passes->empty() and not _inlineComputePass._shader)
    return;
  // When the cull declares sif_hzb (terrain), it must have a valid buffer bound every pass even when
  // occlusion is off — bind the HZB pyramid if available, else the cam SSBO as an inert stand-in (the
  // shader never reads it because misc.y==0). All terrain passes share cif_terrain, so bind to each.
  FxShaderStorageBuffer* hzbBind = _hzbBlock ? (hzbBuf ? hzbBuf : _camParamsSSBO) : nullptr;

  // MODE 2 (indirect mesh-shader terrain): record the compaction dispatch INLINE onto the frame's
  // primary command buffer (dispatch + producer->consumer barrier), so it rides the ONE frame submit
  // — no dedicated compute submit+fence-wait (the entire per-frame cost on MoltenVK). Correctness of
  // the mesh draw that consumes its args + v_list comes from that barrier, NOT from a fence draining
  // the compute first. Runs at the compute-legal preRender point (outside any render pass).
  if (_inlineComputePass._shader) {
    const auto& p = _inlineComputePass;
    for (const auto& b : p._bindings)
      CI->bindStorageBufferOnBlock(p._shader, b.second, b.first); // (buffer, block); binding auto-resolved
    if (hzbBind)
      CI->bindStorageBufferOnBlock(p._shader, hzbBind, _hzbBlock);
    CI->dispatchComputeInline(p._shader, p._groups_x, p._groups_y, p._groups_z);
  }

  if (not passes->empty()) {
    uint64_t _cull_t0 = ork::Timer::getSystemTick(); // perf HUD
    CI->beginDispatchPhase();
    for (size_t i = 0; i < passes->size(); i++) {
      const auto& p = (*passes)[i];
      for (const auto& b : p._bindings)
        CI->bindStorageBufferOnBlock(p._shader, b.second, b.first); // (buffer, block); binding auto-resolved
      if (hzbBind)
        CI->bindStorageBufferOnBlock(p._shader, hzbBind, _hzbBlock);
      CI->dispatchCompute(p._shader, p._groups_x, p._groups_y, p._groups_z);
      if ((i + 1) < passes->size())
        CI->storageBarrier(); // pass i writes -> pass i+1 reads
    }
    CI->endDispatchPhase(); // submit + WAIT -> results ready for the render pass
    RenderPhaseStats::instance().add(
        _hzbBlock ? "terrain-cull" : "compute-cull", double(ork::Timer::getSystemTick() - _cull_t0) * 1.0e-6); // perf HUD
  }

  // Terrain VIS header @176: v_count(=occlusion-pass) v_frustum_count(=frustum-pass) u_dim
  // v_total_count(=total). Read at the frame boundary from the SSBO the inline compaction wrote LAST
  // frame — LAG-1, host-visible map (the established pattern; NEVER a device-local mid-graph readback
  // that would abort the CB). ONE read feeds BOTH the perf HUD (CullStats) and the mode-1 direct-sized
  // dispatch sizing. Skipped entirely when neither consumer wants it (off => zero readback cost).
  const bool want_hud   = _hzbBlock and _camParamsSSBO and CullStats::instance().enabled();
  const bool want_size  = _meshDirectSized and _camParamsSSBO;
  if (want_hud or want_size) {
    struct Vis { uint32_t v_count, v_frustum_count, u_dim, v_total_count; } vis;
    auto m = FXI->mapStorageBuffer(_camParamsSSBO, 176 /*VIS_OFF*/, sizeof(vis), BufferMapAccess::READ_ONLY);
    memcpy(&vis, m->_mappedaddr, sizeof(vis));
    m->unmap();
    if (want_hud)
      CullStats::instance().addTerrain(vis.v_total_count, vis.v_frustum_count, vis.v_count);
    if (want_size) {
      // LAG-1 sizing: vis.v_count is the count the PREVIOUS frame's compaction produced (this frame's
      // inline compaction is only RECORDED above, not yet executed). Choose this frame's grid-y
      // capacity from it + margin, floored + clamped. The guard in the mesh stage makes any
      // over-dispatch cheap; an UNDER-dispatch (last frame's count exceeded last frame's capacity)
      // is recovered HERE with a full-grid dispatch (one-frame lag), logged once.
      const uint32_t count = vis.v_count;
      const uint32_t total = _meshSizeTotal;
      uint32_t cap;
      bool fallback = false;
      if (not _meshHasHistory) {
        cap = total; // first frame: no history to size from -> full grid
      } else if (count > _meshLastCapacity) {
        cap      = total; // last frame under-dispatched -> recover with the full grid this frame
        fallback = true;
      } else {
        uint32_t est = uint32_t(std::ceil(float(count) * _meshSizeMargin));
        cap          = std::max(est, _meshSizeFloor);
        if (cap > total)
          cap = total;
        if (cap < 1u)
          cap = 1u;
      }
      _meshGroups[0] = _meshSizeMps2;
      _meshGroups[1] = cap;
      _meshGroups[2] = 1;
      // telemetry: log on change (or on a fallback) — the observable proving hidden chunks stopped
      // costing (dispatched vs total workgroups). Default cull granularity is stated so the "apples-
      // to-apples" MESHCULL knob is visible in the same line.
      if (cap != _meshLastCapacity or fallback)
        logchan_cdraw->log(
            "TERRAIN-MESHSHADER: mode-1 sized dispatch cap=%u/%u chunks (visible~%u margin=%.2f "
            "cull=%s%s) -> %u/%u mesh workgroups%s",
            cap, total, count, _meshSizeMargin,
            _meshCullChunk ? "chunk" : "meshlet", _meshCullChunk ? "" : "(default)",
            _meshSizeMps2 * cap, _meshSizeMps2 * total,
            fallback ? " [UNDER-DISPATCH: full-grid recover]" : "");
      _meshLastCapacity = cap;
      _meshHasHistory   = true;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// cascade-cull fix — per-FRAME sun-shadow cull. Runs the shadow variant of the per-view cull with the
// prologue's UNION sun camera, compacting the SHADOW survivor set (its own OUT_M/OUT_A/args). No CamBlk
// / HZB / _passes work here — those are eye-frame concerns; the shadow cull is frustum-only and drives
// its own params. Nests inside Scene::shadowCull's dispatch phase (reentrant begin/end).
///////////////////////////////////////////////////////////////////////////////

void ComputeDrawable::onShadowPreRender(Context* ctx, const CameraMatrices& cammtx) const {
  if (_perViewComputeShadow) {
    RenderPhaseScope _s("hm-shadowcull"); // perf HUD
    _perViewComputeShadow(ctx, cammtx);
  }
}

///////////////////////////////////////////////////////////////////////////////
// POINT-OF-USE ARBITER for the compute-drawable draw (terrain, hypermesh, every
// SSBO-pull consumer). The material-side [SPVR:GENSEL] line says which technique
// the CACHE built; this one says which pipeline this DRAWABLE actually bound, in
// which pass, against which per-view resources — the two claims differ whenever a
// pass takes a peer the material never announced (a DEPTH-PREPASS pipeline, for
// one, is built by a different creator entirely).
//
// The resource IDENTITY is the diagnostic half: a black view-1 predicts a resource
// that is unbound or written for one view only, so the camera block's buffer+offset
// and the presence of the per-view ublk_stereo bind are printed, not just names.
//
// Once per (pipeline, pass-model) — a running scene prints a handful of lines, and
// the depth-prepass and color pipelines of one drawable each get their own.
//
// Grep token: SPVR:CDSEL
///////////////////////////////////////////////////////////////////////////////

static void _announceComputeDraw(
    const RenderContextInstData& RCID, //
    fxpipeline_ptr_t pipe,             //
    material_ptr_t material,           //
    const std::string& family,         //
    FxShaderStorageBuffer* camssbo,    //
    size_t camoffset) {
  if ((nullptr == pipe) or (nullptr == pipe->_technique))
    return;
  static std::set<const void*> s_announced;
  if (not s_announced.insert((const void*)pipe.get()).second)
    return;
  auto RCFD       = RCID.rcfd();
  bool cpd_st     = RCFD->hasCPD() ? RCFD->topCPD().isSinglePassStereo() : false;
  bool is_dpp     = RCFD->_renderingmodel._modelID == "DEPTH_PREPASS"_crcu;
  auto as_pbr     = std::dynamic_pointer_cast<PBRMaterial>(material);
  const char* stb = as_pbr ? (as_pbr->_parStereoBlock ? "bound" : "ABSENT") : "n/a";
  // THE DEFECT NAMES ITSELF. A technique without the _ST suffix bound inside a stereo pass is
  // the mono fallback: one view's transform serving both layers. Reading that off the technique
  // name is a census exercise; saying it in the line is a verdict, and a verdict is greppable.
  const auto& tekname = pipe->_technique->_techniqueName;
  bool is_st          = tekname.size() >= 3 and tekname.compare(tekname.size() - 3, 3, "_ST") == 0;
  const char* arm     = (not cpd_st) ? "mono pass" : (is_st ? "per-view" : "MONO-FALLBACK");
  printf(
      "[SPVR:CDSEL] compute DRAW family<%s> material<%s> technique<%s> pass<%s> pass_stereo<%d> "
      "arm<%s> ssbo<%d> mesh<%d> inst<%d> impostor<%d> camblk<%p+0x%zx> stereoblk<%s>\n",
      family.c_str(),
      material ? material->mMaterialName.c_str() : "<none>",
      tekname.c_str(),
      is_dpp ? "depth_prepass" : "color",
      int(cpd_st),
      arm,
      int(RCID._isSSBOSourced),
      int(RCID._isMeshSourced),
      int(RCID._isInstanced),
      int(RCID._isImpostor),
      (void*)camssbo,
      camoffset,
      stb);
  fflush(stdout);
}

///////////////////////////////////////////////////////////////////////////////
// render: the material/pipeline binds the vertex SSBO (pipe.bindStorage, set in python) + its
// uniforms; the indirect draw pulls the count from the compute-written args buffer.
///////////////////////////////////////////////////////////////////////////////

void ComputeDrawable::_renderIndirect(RenderContextInstData& RCID) const {
  // cascade-cull fix: a sun-cascade depth pass consumes the SHADOW survivor set (union-sun-culled)
  // instead of the eye set; every other pass reads the eye set unchanged (color byte-identity). Keyed
  // off the CPD the cascade pass pushed. A drawable that DID shadow-cull (_perViewComputeShadow set)
  // MUST have its shadow args wired — assert loudly if not (no silent fallback to the eye set).
  bool shadow_pass = false;
  if (auto RCFD = RCID.context()->topRenderContextFrameData())
    shadow_pass = RCFD->topCPD()._sunCascadeShadowPass;
  const bool wired_shadow = bool(_perViewComputeShadow) and not _shadowStorageOverrides.empty();
  if (shadow_pass and wired_shadow)
    OrkAssert(_argsSSBOShadow != nullptr && "sun-cascade pass: shadow cull ran but shadow args set missing");
  const bool use_shadow = shadow_pass and wired_shadow and (_argsSSBOShadow != nullptr);
  // explicit _pipeline wins (FreestyleMaterial demos). Otherwise the STANDARD path: flag the RCID as
  // SSBO-sourced and let the material's cache pick its FWD_SSBO_CUSTOM variant (with forward lighting
  // attached the usual way) — no forced technique, no hand-built pipeline.
  fxpipeline_ptr_t pipe = _pipeline;
  if ((not pipe) and _material) {
    RCID._isSSBOSourced = true;
    RCID._isInstanced   = _instanced;   // instanced+ssbo -> FWD_SSBO_CUSTOM_INSTANCED (matrices via graphics storage)
    // mesh path: _isMeshSourced picks the material's FWD_SSBO_CUSTOM_MESH pair (color + depth
    // prepass) the same flag-driven way _isSSBOSourced picks the pull-VS pair — a PBRMaterial's
    // pipeline creators select by permutation, never by forced technique. The forced technique
    // additionally serves FreestyleMaterial consumers (which DO honor it) and is cleared below.
    if (_meshTechnique) {
      RCID._isMeshSourced = true;
      RCID.forceTechnique(_meshTechnique);
    }
    pipe                = _material->pipelineCache()->findPipeline(RCID);
    // bind our drawable-specific storage (the vertex-source SSBO) onto the cache-built pipeline.
    // Per-frame (not once): there are DISTINCT cached pipes per pass (depth-prepass + color), each
    // needs the binding; bindStorage is an idempotent map insert keyed by block, so this is cheap.
    if (pipe) {
      for (const auto& b : _graphicsStorage)
        pipe->bindStorage(b.first, b.second);
      // shadow pass: re-point the instance blocks at the shadow OUT_M/OUT_A (idempotent map insert
      // keyed by block, so this OVERRIDES the eye instance bind above; vertex channels are shared).
      if (use_shadow)
        for (const auto& b : _shadowStorageOverrides)
          pipe->bindStorage(b.first, b.second);
    }
  }
  if (not pipe)
    return;
  _announceComputeDraw(RCID, pipe, _material, _spvrFamily, _camParamsSSBO, _camParamsOffset);
  auto ctx = RCID.context();
  auto gbi = ctx->GBI();
  auto mainArgs = use_shadow ? _argsSSBOShadow : _argsSSBO;
  pipe->wrappedDrawCall(RCID, [&]() {
    if (_meshTechnique) {
      // cascade-cull fix (W7-S3, mesh path): the sun-cascade pass takes its grid from the SHADOW
      // compaction's command / full-grid fallback, the same way the pull path takes shadow args.
      auto meshArgs        = use_shadow ? _meshArgsSSBOShadow : _meshArgsSSBO;
      const uint32_t* mgrp = use_shadow ? _meshGroupsShadow : _meshGroups;
      if (meshArgs) // GPU-written {x,y,z}: an all-culled view dispatches zero workgroups
        gbi->DrawMeshTasksIndirectEML(meshArgs, _meshArgsOffset);
      else
        gbi->DrawMeshTasksEML(mgrp[0], mgrp[1], mgrp[2]);
    } else if (_indexSSBO)
      gbi->DrawIndexedIndirectEML(_indexSSBO, _primtype, mainArgs, _argsOffset, _indexSize);
    else if (mainArgs)
      gbi->DrawIndirectEML(_primtype, mainArgs, _argsOffset);
  });
  if (_meshTechnique) {
    RCID._isMeshSourced = false;  // the bucket/overlay draws below are pull-VS draws
    RCID.forceTechnique(nullptr); // don't leak the forced technique past this draw
  }

  // E.3 — GID BUCKET DRAWS: same index buffer, per-gid args offset, own material
  // + own storage list (per-shader block handles). Issued right after the main
  // (gid 0 / default-material) draw.
  for (const auto& bucket : _bucketDraws) {
    if (not bucket._material)
      continue;
    // cascade-cull fix: in a sun-cascade depth pass only GID buckets participate — they share the
    // drawable's args (bucket._argsSSBO null) + tier-0 instances (bucket._instByteOffset 0), so the
    // shadow args + shadow instance override apply. LOD/impostor buckets (own args / tier slice) are
    // SKIPPED: the full tier-0 mesh already casts the shadow (no impostor billboards as shadow casters).
    const bool gid_bucket = (bucket._argsSSBO == nullptr) and (bucket._instByteOffset == 0);
    if (use_shadow and not gid_bucket)
      continue;
    RCID._isSSBOSourced = true;
    RCID._isInstanced   = _instanced;
    RCID._isImpostor    = bucket._isImpostor; // -> findPipeline selects FWD_SSBO_CUSTOM_IMPOSTOR (billboard)
    // explicit pipeline override (forced-technique) wins; else the material's RCID-selected pipeline (which
    // for an impostor bucket resolves to the billboard technique + the SAME forward lighting as the mesh).
    auto bpipe          = bucket._pipeline ? bucket._pipeline
                                           : bucket._material->pipelineCache()->findPipeline(RCID);
    if (not bpipe)
      continue;
    _announceComputeDraw(RCID, bpipe, bucket._material, _spvrFamily + ".gid", _camParamsSSBO, _camParamsOffset);
    for (const auto& b : bucket._graphicsStorage)
      bpipe->bindStorage(b.first, b.second);
    // shadow pass (gid bucket only): override instance blocks with the shadow OUT_M/OUT_A.
    if (use_shadow)
      for (const auto& b : bucket._shadowStorageOverrides)
        bpipe->bindStorage(b.first, b.second);
    // LOD tier: re-bind the shared instance buffer(s) at THIS tier's sub-range offset (overrides the
    // offset-0 binding above — bindStorage is an idempotent map insert keyed by block). The VS then
    // reads its tier slice from gl_InstanceIndex==0; instanceCount rides the tier's own args command.
    if (bucket._instByteOffset != 0) {
      if (bucket._instMtxBlock and bucket._instMtxBuf)
        bpipe->bindStorage(bucket._instMtxBlock, bucket._instMtxBuf, bucket._instByteOffset);
      if (bucket._instAttrBlock and bucket._instAttrBuf)
        bpipe->bindStorage(bucket._instAttrBlock, bucket._instAttrBuf, bucket._instByteOffset);
    }
    // PER-VARIANT impostor atlas + params, bound here (BEFORE the draw call, like the per-bucket bindStorage
    // above) — the base PBRMaterial is SHARED across variants, so binding on the material would collapse every
    // impostor to the last-baked atlas. Each bucket binds its OWN atlas, so 16 variants stay 16 distinct trees.
    if (bucket._isImpostor) {
      // bindParam (NOT FXI immediate) -> sets the pipeline's _params, which beginBlock() applies per-draw,
      // same mechanism as the per-bucket bindStorage above. Sequential draws each re-apply their own atlas.
      if (bucket._parImpAlbedo)     bpipe->bindParam(bucket._parImpAlbedo, bucket._impAtlasAlbedo);
      if (bucket._parImpNormal)     bpipe->bindParam(bucket._parImpNormal, bucket._impAtlasNormal);
      if (bucket._parImpMetalRough) bpipe->bindParam(bucket._parImpMetalRough, bucket._impAtlasMetalRough);
      if (bucket._parImpCenter)     bpipe->bindParam(bucket._parImpCenter, bucket._impCenter);
      if (bucket._parImpGrid)       bpipe->bindParam(bucket._parImpGrid, bucket._impGrid);
    }
    // a LOD-tier bucket carries its OWN mesh (distinct topology); else it shares the main draw's buffers.
    auto bidx  = bucket._indexSSBO ? bucket._indexSSBO : _indexSSBO;
    auto bargs = bucket._argsSSBO ? bucket._argsSSBO : (use_shadow ? _argsSSBOShadow : _argsSSBO);
    bpipe->wrappedDrawCall(RCID, [&]() {
      gbi->DrawIndexedIndirectEML(bidx, _primtype, bargs, bucket._argsOffset, _indexSize);
    });
  }
  RCID._isImpostor = false; // don't leak the impostor flag past the bucket loop

  // OPTIONAL OVERLAY draw (e.g. wireframe lines over the fill): its own material/pipeline + index/args.
  // Skipped in sun-cascade depth passes — a wireframe overlay is not a shadow caster (and its eye-culled
  // args would draw the wrong instance set anyway).
  if (not use_shadow and (_overlayMaterial or _overlayPipeline) and _overlayIndexSSBO and _overlayArgsSSBO) {
    fxpipeline_ptr_t opipe = _overlayPipeline;
    if ((not opipe) and _overlayMaterial) {
      RCID._isSSBOSourced = true;
      RCID._isInstanced   = _instanced;   // overlay (wireframe LINES) instances too -> FWD_SSBO_CUSTOM_INSTANCED
      opipe               = _overlayMaterial->pipelineCache()->findPipeline(RCID);
      if (opipe)
        for (const auto& b : _overlayGraphicsStorage)
          opipe->bindStorage(b.first, b.second);
    }
    if (opipe)
      opipe->wrappedDrawCall(RCID, [&]() {
        gbi->DrawIndexedIndirectEML(
            _overlayIndexSSBO, _overlayPrimtype, _overlayArgsSSBO, _overlayArgsOffset, _overlayIndexSize);
      });
  }
}

///////////////////////////////////////////////////////////////////////////////
// ComputeDrawableData
///////////////////////////////////////////////////////////////////////////////

ComputeDrawableData::ComputeDrawableData() {
}

void ComputeDrawableData::addComputePass(
    const FxComputeShader* shader,
    const std::vector<std::pair<const FxShaderStorageBlock*, FxShaderStorageBuffer*>>& bindings,
    uint32_t gx,
    uint32_t gy,
    uint32_t gz) {
  ComputeDrawablePass pass;
  pass._shader   = shader;
  pass._bindings = bindings;
  pass._groups_x = gx;
  pass._groups_y = gy;
  pass._groups_z = gz;
  _passes.push_back(pass);
}

void ComputeDrawableData::addStatsComputePass(
    const FxComputeShader* shader,
    const std::vector<std::pair<const FxShaderStorageBlock*, FxShaderStorageBuffer*>>& bindings,
    uint32_t gx,
    uint32_t gy,
    uint32_t gz) {
  ComputeDrawablePass pass;
  pass._shader   = shader;
  pass._bindings = bindings;
  pass._groups_x = gx;
  pass._groups_y = gy;
  pass._groups_z = gz;
  _statsPasses.push_back(pass);
}

void ComputeDrawableData::setInlineCompute(
    const FxComputeShader* shader,
    const std::vector<std::pair<const FxShaderStorageBlock*, FxShaderStorageBuffer*>>& bindings,
    uint32_t gx,
    uint32_t gy,
    uint32_t gz) {
  _inlineComputePass._shader   = shader;
  _inlineComputePass._bindings = bindings;
  _inlineComputePass._groups_x = gx;
  _inlineComputePass._groups_y = gy;
  _inlineComputePass._groups_z = gz;
}

void ComputeDrawableData::setCameraParams(FxShaderStorageBuffer* ssbo, size_t offset) {
  _camParamsSSBO   = ssbo;
  _camParamsOffset = offset;
}

void ComputeDrawableData::addGraphicsStorage(const FxShaderStorageBlock* block, FxShaderStorageBuffer* ssbo) {
  _graphicsStorage.push_back({block, ssbo});
}

void ComputeDrawableData::setIndirect(
    FxShaderStorageBuffer* args, size_t args_offset, FxShaderStorageBuffer* index, PrimitiveType pt, int index_size) {
  _argsSSBO   = args;
  _argsOffset = args_offset;
  _indexSSBO  = index;
  _primtype   = pt;
  _indexSize  = index_size;
}

void ComputeDrawableData::setMeshDraw(fxtechnique_constptr_t technique, uint32_t gx, uint32_t gy, uint32_t gz) {
  _meshTechnique = technique;
  _meshGroups[0] = gx;
  _meshGroups[1] = gy;
  _meshGroups[2] = gz;
}

void ComputeDrawableData::setOverlayIndirect(
    FxShaderStorageBuffer* args, size_t args_offset, FxShaderStorageBuffer* index, PrimitiveType pt, int index_size) {
  _overlayArgsSSBO   = args;
  _overlayArgsOffset = args_offset;
  _overlayIndexSSBO  = index;
  _overlayPrimtype   = pt;
  _overlayIndexSize  = index_size;
}

void ComputeDrawableData::addOverlayGraphicsStorage(const FxShaderStorageBlock* block, FxShaderStorageBuffer* ssbo) {
  _overlayGraphicsStorage.push_back({block, ssbo});
}

drawable_ptr_t ComputeDrawableData::createDrawable() const {
  auto drw             = std::make_shared<ComputeDrawable>();
  drw->_passes         = _passes;
  drw->_statsPasses    = _statsPasses;
  drw->_inlineComputePass = _inlineComputePass;
  drw->_liveRecompute  = _liveRecompute;
  drw->_camParamsSSBO  = _camParamsSSBO;
  drw->_camParamsOffset = _camParamsOffset;
  drw->_material        = _material;
  drw->_pipeline        = _pipeline;
  drw->_graphicsStorage = _graphicsStorage;
  drw->_bucketDraws    = _bucketDraws;
  drw->_perViewCompute = _perViewCompute;
  drw->_perViewComputeShadow   = _perViewComputeShadow; // cascade-cull fix
  drw->_argsSSBOShadow         = _argsSSBOShadow;       // cascade-cull fix
  drw->_shadowStorageOverrides = _shadowStorageOverrides; // cascade-cull fix
  drw->_oneShotRender  = _oneShotRender; // A2 impostor bake one-shot in-frame hook
  drw->_argsSSBO       = _argsSSBO;
  drw->_argsOffset     = _argsOffset;
  drw->_indexSSBO      = _indexSSBO;
  drw->_primtype       = _primtype;
  drw->_indexSize      = _indexSize;
  drw->_instanced      = _instanced;
  drw->_meshTechnique  = _meshTechnique;
  drw->_meshGroups[0]  = _meshGroups[0];
  drw->_meshGroups[1]  = _meshGroups[1];
  drw->_meshGroups[2]  = _meshGroups[2];
  drw->_overlayMaterial        = _overlayMaterial;
  drw->_overlayPipeline        = _overlayPipeline;
  drw->_overlayGraphicsStorage = _overlayGraphicsStorage;
  drw->_overlayArgsSSBO        = _overlayArgsSSBO;
  drw->_overlayArgsOffset      = _overlayArgsOffset;
  drw->_overlayIndexSSBO       = _overlayIndexSSBO;
  drw->_overlayPrimtype        = _overlayPrimtype;
  drw->_overlayIndexSize       = _overlayIndexSize;
  auto draw_raw        = drw.get();
  drw->setRenderLambda([draw_raw](RenderContextInstData& RCID) { draw_raw->_renderIndirect(RCID); });
  return drw;
}

void ComputeDrawableData::describeX(object::ObjectClass* clazz) {
}

} // namespace ork::lev2

ImplementReflectionX(ork::lev2::ComputeDrawableData, "ComputeDrawableData");
