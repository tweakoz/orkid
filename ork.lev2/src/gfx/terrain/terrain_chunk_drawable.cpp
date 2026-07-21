////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// terrain_chunk_drawable.cpp (E.6 → D.5) — see terrain_chunk_drawable.h. The buffer-side
// port of terrain/gpu_chunk.py's ComputeDrawable consumer contract; the layout arithmetic
// MUST stay in lockstep with the Python class (the GLSL side's single source of truth).
//
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/terrain/terrain_chunk_drawable.h>
#include <ork/lev2/gfx/renderer/compute_drawable.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>           // terrain texture-bake (ORKID_TERRAIN_TEXBAKE_DUMP)
#include <ork/lev2/gfx/fbi.h>
#include <ork/lev2/gfx/gbi.h>
#include <ork/lev2/gfx/mtxi.h>
#include <ork/lev2/gfx/txi.h>
#include <ork/lev2/gfx/fx_pipeline.h>
#include <ork/lev2/gfx/targetinterfaces.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h> // [M] read _terrainMaterialMode off pbr_common
#include <ork/lev2/gfx/image.h>             // CaptureBuffer::_image->writeToFile (PNG dump)
#include <ork/lev2/gfx/live_field_buffer.h> // S4 — renderer live-accept of progressive re-bake planes
#include <ork/util/crc.h>
#include <ork/util/logger.h>
#include <ork/kernel/async_tracker.h>     // register the stored-mode texbake as pending async work
#include <ork/kernel/datablock.h>         // terrain-products digest (atlas currency)
#include <filesystem>
#include <cstring>

#include <rapidjson/document.h>
#include <OpenImageIO/imageio.h>

#include <fstream>
#include <sstream>

namespace ork::lev2::terrain {

static logchannel_ptr_t logchan_tcd = logger()->configureChannel("TERRAIN", fvec3(0.4, 0.9, 0.4));

///////////////////////////////////////////////////////////////////////////////

void TerrainChunkDrawableData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("hf_asset", &TerrainChunkDrawableData::_hf_asset_name);
  clazz->directProperty("material_asset", &TerrainChunkDrawableData::_material_asset_name);
  clazz->directVectorProperty("debug_material_assets", &TerrainChunkDrawableData::_debug_material_assets);
  clazz->directProperty("chunk", &TerrainChunkDrawableData::_chunk);
  clazz->directProperty("layout_dim_cap", &TerrainChunkDrawableData::_layout_dim_cap);
  clazz->directProperty("render_dimension", &TerrainChunkDrawableData::_render_dimension);
  clazz->directProperty("capture_mode", &TerrainChunkDrawableData::_capture_mode);
  clazz->directProperty("capture_res", &TerrainChunkDrawableData::_capture_res);
  clazz->directProperty("capture_dir", &TerrainChunkDrawableData::_capture_dir);
  clazz->directVectorProperty("capture_targets", &TerrainChunkDrawableData::_capture_targets);
}

TerrainChunkDrawableData::TerrainChunkDrawableData() {
}
TerrainChunkDrawableData::~TerrainChunkDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////

namespace {
struct TcBootstrap {
  bool _built  = false;
  bool _warned = false;
  bool _baked  = false;                       // ORKID_TERRAIN_TEXBAKE_DUMP one-shot guard
  bool _stale_atlas = false;                  // proc mode: products snapshot mismatched at materialize -> re-capture
  std::shared_ptr<ComputeDrawableData> _cdd;  // keeps configured state alive
  // stashed by _liveRecompute for the texture-bake one-shot (terrainTexBake)
  FxShaderStorageBuffer* _ssbo = nullptr;
  int   _dim = 0, _chunk = 0, _nchunk = 0, _vpc = 0;
  float _extent_m = 0.0f;
  float _ymin = 0.0f, _ymax = 0.0f;   // actual field height range METERS (scanned @materialize)
  std::vector<float> _heights;        // the bake's DENSE height array (BAKE-res, stride 1 — both modes)
  std::vector<uint32_t> _frame;       // relax: the bake's stride-5 PACKED frame [uv.x/y fp32-bits, half2(nrm.xz), half2(bn.xy), half2(bn.z,0)] (BAKE-res); empty = mono
  std::shared_ptr<RtGroup> _bake_rtg; // stored mode: the baked atlas, bound onto the material (kept alive)
  // S4 live-accept (renderer consumes LIVE artifacts during a sliced re-bake) — armed
  // at materialize with the height product path's named LiveFieldBuffer; per-frame the
  // drawable consumes any NEWER generation into the presenting SSBO (heights + bounds).
  live_field_buffer_ptr_t _live;
  uint64_t _live_gen    = 0;          // last consumed generation (init = current at arm time)
  bool     _live_warned = false;
  std::vector<float> _live_scratch;   // consume copy-out target (reused)
  size_t _heights_off = 0, _chunky_off = 0, _yb_off = 0; // stashed layout (mirror materialize)
  int _render_dim = 0, _render_cps = 0;
};

///////////////////////////////////////////////////////////////////////////////
// S4 live-accept — progressive display of an in-flight sliced re-bake (JUL13 §E5/S4).
// The cook publishes whole, frame-coherent height planes to the named LiveFieldBuffer at
// each viewable-node checkpoint; this consumes the NEWEST one into the ALREADY-PRESENTING
// terrain SSBO (dense heights + per-chunk Y bounds + global ybounds, so cull stays honest
// for the morphing surface). GPU-write-hazard note: a new generation only exists on a
// frame where the cook advanced a node, and every node's dispatch triplet ends in
// submit+WAIT on the shared queue — all previously submitted GPU work (including the
// prior frame's render reading this SSBO) has completed before this map/write runs. The
// plane itself is always COMPLETE (the double buffer flips under its lock), so the mesh
// never shows a half-updated field. Physics/scatter do NOT take this path — they REQUIRE
// final and hold-last-final (BulletTerrainImpl::consumePendingReload / scatter-at-bake).
///////////////////////////////////////////////////////////////////////////////
static void s4LiveAccept(Context* ctx, TcBootstrap* state) {
  if (not state->_live or not state->_ssbo)
    return;
  if (state->_live->generation() == state->_live_gen) // lock-free peek: nothing new
    return;
  int lw = 0, lh = 0;
  state->_live_gen = state->_live->consume(
      state->_live_gen, [&](int w, int h, const float* data, uint64_t) {
        state->_live_scratch.assign(data, data + size_t(w) * size_t(h));
        lw = w;
        lh = h;
      });
  if (lw <= 0 or lw != lh)
    return;
  const int rdim           = state->_render_dim;
  std::vector<float>* hp   = &state->_live_scratch;
  std::vector<float> resampled;
  if (lw != rdim) {
    if (lw < rdim) { // live plane coarser than the render grid — decline, loudly once
      if (not state->_live_warned) {
        logchan_tcd->log(
            "TerrainChunkDrawable: S4 live plane %dx%d < render_dim %d — ignoring live updates",
            lw, lh, rdim);
        state->_live_warned = true;
      }
      return;
    }
    // downsample with the SAME filtered convention materialize + the collider use
    Image simg;
    simg.initWithFormat(lw, lh, EBufferFormat::R32F);
    std::memcpy((void*)simg._data->data(), hp->data(), hp->size() * sizeof(float));
    Image dimg;
    dimg.resampledOf(simg, rdim, rdim, Image::ResampleFilter::TRIANGLE);
    resampled.resize(size_t(rdim) * rdim);
    std::memcpy(resampled.data(), dimg._data->data(), resampled.size() * sizeof(float));
    hp = &resampled;
  }
  auto fxi = ctx->FXI();
  { // the dense heights @_heights_off (terr_pos reads heights[cz*u_dim+cx])
    auto m = fxi->mapStorageBuffer(state->_ssbo, state->_heights_off, hp->size() * 4, BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, hp->data(), hp->size() * 4);
    fxi->unmapStorageBuffer(m.get());
  }
  { // per-chunk world-Y bounds + global u_ybounds (HZB/frustum cull correctness)
    const int   chunk = state->_chunk;
    const int   cps   = state->_render_cps;
    const auto& hh    = *hp;
    std::vector<float> chunkY(size_t(cps) * cps * 2);
    float gmn = 1e30f, gmx = -1e30f;
    for (int ccz = 0; ccz < cps; ccz++)
      for (int ccx = 0; ccx < cps; ccx++) {
        float hmn = 1e30f, hmx = -1e30f;
        int x1 = std::min(rdim, (ccx + 1) * chunk + 1);
        int z1 = std::min(rdim, (ccz + 1) * chunk + 1);
        for (int z = ccz * chunk; z < z1; z++)
          for (int x = ccx * chunk; x < x1; x++) {
            float h = hh[size_t(z) * rdim + x];
            hmn     = std::min(hmn, h);
            hmx     = std::max(hmx, h);
          }
        int ci             = ccz * cps + ccx;
        chunkY[ci * 2 + 0] = hmn;
        chunkY[ci * 2 + 1] = hmx;
        gmn = std::min(gmn, hmn);
        gmx = std::max(gmx, hmx);
      }
    auto m = fxi->mapStorageBuffer(state->_ssbo, state->_chunky_off, chunkY.size() * 4, BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, chunkY.data(), chunkY.size() * 4);
    fxi->unmapStorageBuffer(m.get());
    float yb[2] = {gmn, gmx};
    auto ym     = fxi->mapStorageBuffer(state->_ssbo, state->_yb_off, sizeof(yb), BufferMapAccess::WRITE_ONLY);
    std::memcpy(ym->_mappedaddr, yb, sizeof(yb));
    fxi->unmapStorageBuffer(ym.get());
    state->_ymin = gmn;
    state->_ymax = gmx;
  }
  logchan_tcd->log("TerrainChunkDrawable: S4 live-accept gen<%llu> plane<%dx%d> -> render_dim<%d>",
                   (unsigned long long)state->_live_gen, lw, lh, rdim);
}

///////////////////////////////////////////////////////////////////////////////
// PHASE-0 SPIKE — bake the terrain proctex (FWD_SSBO_CUSTOM_CAPTURE) over the planar UV domain into a
// PBR MRT atlas + dump PNGs (baked-vs-live eyeball). Mirrors the impostor bake (hmdflow_render.cpp) but
// collapses the hemi-oct camera grid to a SINGLE top-down orthographic pass: surface() reads
// frg_wpos / frg_uv0 / the world normal (all camera-INDEPENDENT — m=identity, frg_wpos=m*position), so
// only gl_Position needs the camera; and the planar uv0 == worldXZ/extent+0.5, so a top-down ortho over
// the extent rasterizes EXACTLY the planar parameterization (atlas u = uv0.x, v = 1-uv0.y with up=-Z).
// Reuses the terrain's OWN SSBO (heights already uploaded) — CPU-fills the visible-chunk list to draw ALL
// chunks (no frustum/HZB cull). The capture pipe copies the material's _bound_params, so channel samplers
// (erodeflow FlowMetrics / xxx3 FlowMap) + proctex params bake losslessly. Gated by ORKID_TERRAIN_TEXBAKE_DUMP;
// the material must be authored with capture=True (so FWD_SSBO_CUSTOM_CAPTURE exists).
///////////////////////////////////////////////////////////////////////////////
static std::string terrainTexBakeDumpDir() {
  const char* e   = getenv("ORKID_TERRAIN_TEXBAKE_DUMP");
  std::string dir = (e and std::strlen(e) > 1) ? std::string(e) : std::string("/tmp/orkid_terrabake");
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir;
}

// Digest of the terrain PRODUCTS' .cookhash sidecars (the input-derived cook keys the
// hfdflow bake writes next to each product EXR). The baked atlas samples those products,
// but the python-side cap_dir key can only see the terrain's AUTHORED inputs — a C++
// cook-salt re-key (e.g. an fbm version bump) changes the products without changing the
// dir name, so the warm bind would hold a stale atlas forever. This digest is the
// missing content half of the key; captured as a snapshot at bake time, compared on
// every warm bind.
static std::string terrainProductsDigest(const std::string& manifest_path) {
  auto dir = std::filesystem::path(manifest_path).parent_path();
  std::map<std::string, std::string> sidecars; // filename-sorted for stability
  std::error_code ec;
  for (auto& de : std::filesystem::directory_iterator(dir, ec)) {
    if (de.path().extension() == ".cookhash") {
      std::ifstream f(de.path());
      std::stringstream ss;
      ss << f.rdbuf();
      sidecars[de.path().filename().string()] = ss.str();
    }
  }
  if (sidecars.empty())
    return ""; // nothing to compare against — currency check degrades to a no-op
  auto ch = DataBlock::createHasher();
  ch->accumulateItem<int>(0x74706431); // 'tpd1' snapshot format salt
  for (auto& item : sidecars) {
    ch->accumulateString(item.first);
    ch->accumulateString(item.second);
  }
  ch->finish();
  char buf[64];
  snprintf(buf, sizeof(buf), "tpd1-%016llx-n%zu", (unsigned long long)ch->result(), sidecars.size());
  return std::string(buf);
}

static bool terrainTexBake(Context* ctx, const TerrainChunkDrawableData* self, TcBootstrap* st) {
  if (st->_baked)
    return true;
  if (not st->_built or not st->_ssbo)
    return false; // wait for _liveRecompute to build + upload the SSBO
  // PROC-mode atlas currency: python bound a cached atlas at materialize because its
  // (input-derived) cap_dir key matched — but the products snapshot check at
  // materialize (where the bake stashes are decided) may have found the CONTENT half
  // of the key stale (product re-key: cook salts, graph edits). Current -> the warm
  // bind stands, no bake. Stale -> fall through and re-capture with full stored
  // semantics (bake -> write cache+snapshot -> bind-back overrides the stale
  // samplers python bound).
  bool force_recapture = false;
  if (self->_capture_mode == "proc" and not getenv("ORKID_TERRAIN_TEXBAKE_DUMP")) {
    if (not st->_stale_atlas) {
      st->_baked = true;
      return true;
    }
    force_recapture = true;
  }
  auto mtl  = self->_resolved_material;
  auto cfs  = mtl ? mtl->_as_freestyle : nullptr;
  auto ctek = mtl ? mtl->_tek_FWD_SSBO_CUSTOM_CAPTURE : nullptr;
  if (not cfs or not ctek) {
    logchan_tcd->log(
        "TERRAIN-TEXBAKE: material<%s> has no FWD_SSBO_CUSTOM_CAPTURE — author it with capture=True",
        self->_material_asset_name.c_str());
    st->_baked = true;
    return true;
  }
  st->_baked      = true;
  auto fxi        = ctx->FXI();
  const int   dim = st->_dim, nchunk = st->_nchunk, vpc = st->_vpc;
  const float extent = st->_extent_m;
  //////////////////////////////////////////////////////////////////
  // 0. resolution + the N EXPLICIT capture targets (the material's self.capture(target,...) groups, in
  //    codegen/MRT order). N MRT -> N atlas textures. ORKID_TERRAIN_TEXBAKE_DUMP writes them to /tmp for
  //    inspection (no bind); otherwise (stored) the textures are bound BACK onto this material so its
  //    surface_stored() samples them this frame (same-session bake-then-bind, the impostor pattern).
  //////////////////////////////////////////////////////////////////
  const bool stored = (self->_capture_mode == "stored") or force_recapture;
  const bool dump   = (getenv("ORKID_TERRAIN_TEXBAKE_DUMP") != nullptr);
  int atlas = stored ? std::max(64, self->_capture_res) : 2048;
  if (const char* r = getenv("ORKID_TERRAIN_TEXBAKE_RES")) { int v = atoi(r); if (v >= 64) atlas = v; }
  const std::vector<std::string>& targets = self->_capture_targets;
  const int N = int(targets.size());
  if (N == 0) {
    logchan_tcd->log("TERRAIN-TEXBAKE: material<%s> declares no captures (need self.capture(...)+surface_stored) — skip",
                     self->_material_asset_name.c_str());
    return true;
  }
  if (N > 8) {
    logchan_tcd->log("TERRAIN-TEXBAKE: %d capture targets > 8 MRT (multi-pass not yet implemented) — skip", N);
    return true;
  }
  //////////////////////////////////////////////////////////////////
  // 1. DEDICATED bake SSBO that draws ALL chunks — NOT the shared one. The per-frame cull compute writes the
  //    shared SSBO's v_list AFTER this one-shot (submit+wait), so reusing it captured only the frustum-visible
  //    chunks. A separate buffer the cull never touches => the WHOLE terrain bakes (all culling disabled).
  //    Layout MUST mirror gpu_chunk.py (ARGS @160, VIS @176, u_ybounds @192, chunk_y @200, ...).
  //////////////////////////////////////////////////////////////////
  // heights[] is the trailing RUNTIME array; the fixed-cap per-chunk arrays precede it (chunk_y first,
  // 8-aligned for vec2). MUST mirror gpu_chunk.py — maxnc = the LAYOUT CAP's chunk count
  // (layout_dim_cap when pinned, else the manifest dim; same formula as the render path).
  const int    cap_dim     = (self->_layout_dim_cap > 0) ? self->_layout_dim_cap : dim;
  const int    cap_cps     = (cap_dim + self->_chunk - 1) / self->_chunk;
  const int    maxnc       = cap_cps * cap_cps;
  const size_t ARGS_OFF    = 160;
  const size_t VIS_OFF     = 176;
  const size_t CHUNKY_OFF  = 200;                                 // vec2 u_ybounds @192 precedes; chunk_y (unused by bake)
  const size_t VLIST_OFF   = CHUNKY_OFF + size_t(maxnc) * 8;      // uint v_list[MAXNC]
  const size_t HEIGHTS_OFF = VLIST_OFF + size_t(maxnc) * 4;       // float heights[] (runtime, last; DENSE stride 1)
  const size_t TOTAL       = HEIGHTS_OFF + size_t(dim) * size_t(dim) * 4;
  if (st->_heights.size() != size_t(dim) * size_t(dim)) {
    logchan_tcd->log("TERRAIN-TEXBAKE: heights not stashed (size<%zu> expected<%zu>) — abort",
                     st->_heights.size(), size_t(dim) * size_t(dim));
    return true;
  }
  auto bakeSSBO = fxi->createStorageBuffer(TOTAL);
  { // ARGS @160 {a_vc,a_ic,a_fv,a_fi} + VIS header @176 {v_count,v_frustum_count,u_dim,v_total_count}.
    // The bake draws ALL chunks (no cull): a_vc = nchunk*vpc, v_count = nchunk. u_dim = dim — the bake VS
    // reads the runtime grid dim from here, exactly like the render path.
    uint32_t ctrl[8] = {0u};
    ctrl[0] = uint32_t(nchunk) * uint32_t(vpc); // a_vc (vertexCount)
    ctrl[1] = 1u;                               // a_ic (instanceCount)
    ctrl[4] = uint32_t(nchunk);                 // v_count
    ctrl[6] = uint32_t(dim);                    // u_dim (runtime grid dim)
    ctrl[7] = uint32_t(nchunk);                 // v_total_count (debug total)
    auto m = fxi->mapStorageBuffer(bakeSSBO, ARGS_OFF, sizeof(ctrl), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, ctrl, sizeof(ctrl));
    fxi->unmapStorageBuffer(m.get());
  }
  { // v_list = ALL chunks [0..nchunk-1] @VLIST_OFF — a SEPARATE upload now (no longer contiguous with
    // ctrl: chunk_y/heights were reordered so v_list sits past the chunk_y cap, not right after VIS).
    std::vector<uint32_t> vlist(size_t(nchunk), 0u);
    for (int i = 0; i < nchunk; i++)
      vlist[i] = uint32_t(i);
    auto m = fxi->mapStorageBuffer(bakeSSBO, VLIST_OFF, vlist.size() * 4, BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, vlist.data(), vlist.size() * 4);
    fxi->unmapStorageBuffer(m.get());
  }
  { // heights[] (terr_pos reads them) — the stashed copy from _liveRecompute
    auto m = fxi->mapStorageBuffer(bakeSSBO, HEIGHTS_OFF, st->_heights.size() * 4, BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, st->_heights.data(), st->_heights.size() * 4);
    fxi->unmapStorageBuffer(m.get());
  }
  // relax: the FULL-res frame array (sif_terra_frame, its own buffer) — the cap VS reads ruv/frame
  // from it so the atlas rasterizes in full-res relaxed space. Mono: empty, no buffer, no bind.
  FxShaderStorageBuffer* bakeFrame = nullptr;
  if (not st->_frame.empty()) {
    OrkAssert(st->_frame.size() == size_t(dim) * size_t(dim) * 5); // stride-5 packed (WS4 fp16)
    bakeFrame = fxi->createStorageBuffer(st->_frame.size() * 4);
    auto m = fxi->mapStorageBuffer(bakeFrame, 0, st->_frame.size() * 4, BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, st->_frame.data(), st->_frame.size() * 4);
    fxi->unmapStorageBuffer(m.get());
  }
  // bake-only intermediates: released at EVERY exit below. The atlas draw is RECORDED into
  // this frame's CB (executes at submit), so GPU destruction defers past the frames in
  // flight; the host stashes are one-shot uploads (a hypothetical re-bake self-aborts
  // loudly on the missing stash instead of mis-baking). ~272MB GPU + ~144MB host @2048² relaxed.
  auto release_bake = [ctx, fxi, bakeSSBO, bakeFrame, st]() {
    ctx->enqueueDelayedDestroy([fxi, bakeSSBO, bakeFrame]() {
      fxi->destroyStorageBuffer(bakeSSBO);
      if (bakeFrame)
        fxi->destroyStorageBuffer(bakeFrame);
    }, 3);
    st->_heights = std::vector<float>();
    st->_frame   = std::vector<uint32_t>(); // fp16-packed frame (stride-5 u32) since 190963a75
  };
  //////////////////////////////////////////////////////////////////
  // 2. MRT atlas — N targets (one per capture group) + depth, auto-mipped, trilinear. Terrain is opaque +
  //    fills the whole atlas (coverage=1), so MSAA_1X suffices. Target t = the capture FS's out_<targets[t]>.
  //////////////////////////////////////////////////////////////////
  static int s_id   = 0;
  std::string aname = "terraBakeAtlas" + std::to_string(s_id++);
  auto rtg          = std::make_shared<RtGroup>(ctx, atlas, atlas, MsaaSamples::MSAA_1X);
  rtg->_name        = aname;
  std::vector<rtbuffer_ptr_t> bufs;
  for (int t = 0; t < N; t++) {
    auto rb = rtg->createRenderTarget(EBufferFormat::RGBA8); // one MRT per capture target (packed RGBA)
    bufs.push_back(rb);
    rb->_mipgen     = RtBuffer::EMG_AUTOCOMPUTE;
    rb->_clearColor = fvec4(0, 0, 0, 0);
    if (auto tex = rtg->texture(t)) {
      tex->_debugName    = aname + "_" + targets[t];
      auto& sm           = tex->TexSamplingMode();
      sm._texFiltModeMin = ETextureMinifyFilterMode::LINEAR_MIPMAP_LINEAR; // sample the mip chain
      sm._texFiltModeMag = ETextureMagnifyFilterMode::LINEAR;
      sm._texAddrModeS   = TextureAddressMode::CLAMP;
      sm._texAddrModeT   = TextureAddressMode::CLAMP;
      sm._maxMipLevel    = 16;
    }
  }
  rtg->createDepthBuffer(EBufferFormat::Z32F, true);
  rtg->_autoclear = true;
  //////////////////////////////////////////////////////////////////
  // 3. capture pipeline (freestyle forced-technique — the PBR cache asserts on the capture frame-type). Bind
  //    the terrain SSBO + the matrices vs_ptex_ssbo uses (no CPD -> they fall back to the pushed ortho V/P +
  //    identity M), then COPY the material's stamped _bound_params so the channel samplers + proctex params
  //    bake exactly as they render live.
  //////////////////////////////////////////////////////////////////
  FxPipelinePermutation permu;
  permu._is_vertex_ssbo   = true;
  permu._forced_technique = ctek;
  auto pipe               = cfs->pipelineCache()->findPipeline(permu);
  if (not pipe) {
    logchan_tcd->log("TERRAIN-TEXBAKE: no capture pipeline");
    release_bake();
    return true;
  }
  if (auto blk = cfs->storageBlock("sif_ptex_vtx"))
    pipe->bindStorage(blk, bakeSSBO); // the all-chunks bake buffer (not the cull-overwritten shared one)
  if (bakeFrame) {                    // relax: the cap VS also pulls the frame SSBO
    auto blk = cfs->storageBlock("sif_terra_frame");
    if (blk)
      pipe->bindStorage(blk, bakeFrame);
    else {
      logchan_tcd->log("TERRAIN-TEXBAKE: relax frame stashed but material<%s> lacks sif_terra_frame — abort",
                       self->_material_asset_name.c_str());
      release_bake();
      return true;
    }
  }
  if (auto p = cfs->param("mvp"))
    pipe->bindParam(p, "RCFD_Camera_MVP_Mono"_crcsh);
  if (auto p = cfs->param("m"))
    pipe->bindParam(p, "RCFD_M"_crcsh);
  if (auto p = cfs->param("mrot"))
    pipe->bindParam(p, "RCFD_Model_Rot"_crcsh);
  for (auto item : mtl->_bound_params) // samplers (FlowMap/FlowMetrics/...) + proctex params, as live
    pipe->bindParam(item.first, item.second);
  //////////////////////////////////////////////////////////////////
  // 4. one top-down orthographic pass covering the extent -> the atlas IS the planar UV domain.
  //////////////////////////////////////////////////////////////////
  auto fbi   = ctx->FBI();
  auto gbi   = ctx->GBI();
  auto mtxi  = ctx->MTXI();
  auto txi   = ctx->TXI();
  auto rcfd  = std::make_shared<RenderContextFrameData>(ctx);
  const float half = extent * 0.5f;
  const float eyeY = st->_ymax + extent + 1.0f; // safely above the highest terrain (actual max, meters)
  fvec3 eye(0, eyeY, 0), center(0, 0, 0);
  fmtx4 V, P;
  V.lookAt(eye, center, fvec3(0, 0, -1));     // straight down; up=-Z -> atlas u=uv0.x, v=1-uv0.y
  P.ortho(-half, half, half, -half, 1.0f, (eyeY - st->_ymin) + 10.0f); // far reaches below the lowest point
  fbi->PushRtGroup(rtg.get());
  mtxi->PushPMatrix(P);
  mtxi->PushVMatrix(V);
  mtxi->PushMMatrix(fmtx4::Identity());
  ViewportRect vp(0, 0, atlas, atlas);
  fbi->pushViewport(vp);
  fbi->pushScissor(vp);
  RenderContextInstData RCID(rcfd);
  RCID._isSSBOSourced = true;
  pipe->wrappedDrawCall(RCID, [&]() { gbi->DrawIndirectEML(PrimitiveType::TRIANGLES, bakeSSBO, ARGS_OFF); });
  fbi->popScissor();
  fbi->popViewport();
  mtxi->PopPMatrix();
  mtxi->PopVMatrix();
  mtxi->PopMMatrix();
  fbi->PopRtGroup();
  //////////////////////////////////////////////////////////////////
  // 4b. DISK CACHE (cold): write each target to <_capture_dir>/<target>.png BEFORE generateMipMaps, so the
  //     mip-gen below re-establishes SHADER_READ (captureAsFormat ends in render-target) and the same-session
  //     bind still samples. Warm runs never reach the bake (Python binds the cached atlas via sampler_textures
  //     at materialize, capture_mode="proc"). The readback is async; keep rtg/pipe/rcfd alive in the lambda.
  //////////////////////////////////////////////////////////////////
  if (stored and not dump and not self->_capture_dir.empty()) {
    std::filesystem::path cacheDir(self->_capture_dir);
    std::error_code ec;
    std::filesystem::create_directories(cacheDir, ec);
    for (int t = 0; t < N; t++) {
      auto capbuf      = std::make_shared<CaptureBuffer>();
      std::string path = (cacheDir / (targets[t] + ".png")).string();
      auto buf = bufs[t];
      auto keep = rtg;
      fbi->captureAsFormat(buf.get(), capbuf, EBufferFormat::RGBA8, [capbuf, path, keep, pipe, rcfd]() {
        capbuf->_image->writeToFile(file::Path(path.c_str()));
        printf("TERRAIN-TEXBAKE: cached %s\n", path.c_str());
      });
    }
    // snapshot the product cook keys this atlas was captured against — the warm-bind
    // currency check above compares against this on every proc-mode run.
    auto digest = terrainProductsDigest(self->_resolved_manifest);
    if (not digest.empty()) {
      std::ofstream snapf(cacheDir / "products.cookhash", std::ios::trunc);
      snapf << digest << "\n";
    }
  }
  for (int t = 0; t < N; t++)
    if (auto tex = rtg->texture(t)) {
      txi->generateMipMaps(tex.get());
      txi->ApplySamplingMode(tex.get()); // build the trilinear sampler AFTER init (else point/mip-0 -> aliasing)
    }
  //////////////////////////////////////////////////////////////////
  // 5. DUMP (inspection) -> write each target PNG to /tmp; otherwise (stored) BIND each baked texture back
  //    onto THIS material's sampler<target> so surface_stored() samples it (deferred stamp, applied by the
  //    forward beginBlock this frame). Keep the RtGroup alive (stash on the bootstrap).
  //////////////////////////////////////////////////////////////////
  if (dump) {
    std::string base = terrainTexBakeDumpDir() + "/" + (self->_hf_asset_name.empty() ? "terra" : self->_hf_asset_name);
    for (int t = 0; t < N; t++) {
      auto capbuf      = std::make_shared<CaptureBuffer>();
      std::string path = base + "_" + targets[t] + ".png";
      auto buf  = bufs[t];
      auto keep = rtg;
      fbi->captureAsFormat(buf.get(), capbuf, EBufferFormat::RGBA8, [capbuf, path, keep, pipe, rcfd]() {
        capbuf->_image->writeToFile(file::Path(path.c_str()));
        printf("TERRAIN-TEXBAKE: wrote %s\n", path.c_str());
      });
    }
  } else {
    for (int t = 0; t < N; t++)
      if (auto p = cfs->param(targets[t]))
        mtl->bindParam(p, rtg->texture(t)); // texture_ptr_t -> varval_t; stamps the material (2.12 rebind)
    st->_bake_rtg = rtg;                     // keep the atlas alive for the material's lifetime
  }
  logchan_tcd->log("TERRAIN-TEXBAKE: %dx%d atlas, %d targets (first<%s>) mtl<%s> -> %s",
                   atlas, atlas, N, targets[0].c_str(), self->_material_asset_name.c_str(),
                   dump ? "DUMPED" : "BOUND");
  release_bake();
  return true;
}
} // namespace

///////////////////////////////////////////////////////////////////////////////
// #88 v2 — in-place display REVISIT: load a baked height product and publish it as one
// whole plane to the buffer the HELD drawable consumes (see terrain_chunk_drawable.h).
// The load MUST match materialize's read (:587-595) exactly — channel-0 as FLOAT, meters —
// so the pushed plane is byte-identical to what a fresh full-swap materialize would upload.
///////////////////////////////////////////////////////////////////////////////
int publishHeightPlaneFromExr(const std::string& held_field_key, const std::string& height_exr_path) {
  if (s4ProgressiveDisabled()) {
    logchan_tcd->log("TerrainChunkDrawable: in-place rebind DECLINED (S4 disabled)");
    return 0;
  }
  // find-only: the held drawable armed this artifact at materialize (liveFieldAcquire).
  // ABSENT => no held drawable listens here => the caller must full-swap (self-defend).
  auto buf = liveFieldFind(liveFieldCanonicalKey(held_field_key));
  if (not buf) {
    logchan_tcd->log("TerrainChunkDrawable: in-place rebind DECLINED (no live buffer <%s>)",
                     held_field_key.c_str());
    return 0;
  }
  auto in = OIIO::ImageInput::open(height_exr_path);
  if (not in) {
    logchan_tcd->log("TerrainChunkDrawable: in-place rebind DECLINED (product MISSING <%s>)",
                     height_exr_path.c_str());
    return 0;
  }
  const auto& spec = in->spec();
  const int w = spec.width, h = spec.height;
  if (w <= 0 or w != h) {
    in->close();
    logchan_tcd->log("TerrainChunkDrawable: in-place rebind DECLINED (non-square plane %dx%d)", w, h);
    return 0;
  }
  std::vector<float> px(size_t(w) * size_t(h) * spec.nchannels);
  in->read_image(0, 0, 0, spec.nchannels, OIIO::TypeDesc::FLOAT, px.data());
  in->close();
  std::vector<float> plane(size_t(w) * size_t(h));
  for (size_t i = 0; i < plane.size(); i++)
    plane[i] = px[i * spec.nchannels]; // channel 0 = height METERS (== materialize's read)
  buf->publish(w, h, plane.data());    // flip + bump generation -> s4LiveAccept consumes next frame
  logchan_tcd->log("TerrainChunkDrawable: in-place plane REBIND published <%dx%d> key<%s> from <%s>",
                   w, h, held_field_key.c_str(), height_exr_path.c_str());
  return w;
}

drawable_ptr_t TerrainChunkDrawableData::createDrawable() const {
  auto drw   = std::make_shared<ComputeDrawable>();
  drw->_drawable_type = "terrain"_crcu; // enumerable via Scene::drawableNodesWithType
  auto state = std::make_shared<TcBootstrap>();
  auto self  = this;

  // lazy bootstrap (the D.3 pattern): the GPU side builds on the first onGpuUpdate,
  // after the wire step has resolved the manifest + material by name.
  drw->_liveRecompute = [self, state](Context* ctx, ComputeDrawable* drawable) {
    if (state->_built) {
      // S4: the renderer MAY accept live artifacts — during a sliced re-bake the
      // held-last-frame terrain progressively morphs as checkpoints publish. No-op
      // unless the named artifact's generation advanced (lock-free peek).
      s4LiveAccept(ctx, state.get());
      return; // terrain is static otherwise (the cull passes run per-VP in onPreRender)
    }
    if (self->_resolved_manifest.empty() or not self->_resolved_material) {
      if (not state->_warned) {
        logchan_tcd->log(
            "TerrainChunkDrawable: unresolved (manifest<%s> material<%s>) — waiting",
            self->_resolved_manifest.c_str(),
            self->_material_asset_name.c_str());
        state->_warned = true;
      }
      return;
    }
    //////////////////////////////////////////////////////////////////
    // 1. the manifest — the self-describing scale contract
    //////////////////////////////////////////////////////////////////
    std::ifstream mf(self->_resolved_manifest);
    if (not mf.good()) {
      logchan_tcd->log("TerrainChunkDrawable: manifest MISSING <%s>", self->_resolved_manifest.c_str());
      state->_built = true;
      return;
    }
    std::stringstream mstrm;
    mstrm << mf.rdbuf();
    std::string mjson = mstrm.str();
    rapidjson::Document doc;
    doc.Parse(mjson.c_str());
    OrkAssert(not doc.HasParseError());
    int dim        = doc["scale"]["dim"].GetInt();
    float extent_m = doc["scale"]["extent_m"].GetFloat();
    OrkAssert(doc["channels"].HasMember("height"));
    std::string hfile = doc["channels"]["height"]["file"].GetString();
    if (hfile.find('/') == std::string::npos) { // relative to the manifest's directory
      auto mdir = self->_resolved_manifest.substr(0, self->_resolved_manifest.find_last_of('/'));
      hfile     = mdir + "/" + hfile;
    }
    //////////////////////////////////////////////////////////////////
    // 2. the heights — channel 0 of the baked EXR
    //////////////////////////////////////////////////////////////////
    auto in = OIIO::ImageInput::open(hfile);
    if (not in) {
      logchan_tcd->log("TerrainChunkDrawable: height image MISSING <%s>", hfile.c_str());
      state->_built = true;
      return;
    }
    const auto& spec = in->spec();
    OrkAssert(spec.width == dim and spec.height == dim);
    std::vector<float> px(size_t(spec.width) * spec.height * spec.nchannels);
    in->read_image(0, 0, 0, spec.nchannels, OIIO::TypeDesc::FLOAT, px.data());
    in->close();
    // HI-RES heights (channel 0) at the BAKE resolution (manifest dim). The material bake (terrainTexBake)
    // consumes these full-res; the render mesh uses a downsampled copy (below).
    const int bake_dim = dim;
    std::vector<float> heights_hi(size_t(bake_dim) * bake_dim);
    for (size_t i = 0; i < heights_hi.size(); i++)
      heights_hi[i] = px[i * spec.nchannels];
    //////////////////////////////////////////////////////////////////
    // 2b. RELAX channels (opt-in via the DSL self.relax_uv(h)): when the manifest carries a relaxed_uv
    //     channel, a SEPARATE stride-5 PACKED frame SSBO [uv fp32-bits ×2, half2(nrm.xz), half2(bn.xy), half2(bn.z,0)] is
    //     built and bound to sif_terra_frame (mirrors gpu_chunk.py relax=True; heights[] stays dense).
    //     relaxed_uv.exr = RGBA(uv.x,uv.y,nrm.x,nrm.z); binormal.exr = RGBA(bn.x,bn.y,bn.z,1).
    //     Both baked at bake_dim; loaded as 4-channel RGBA.
    //////////////////////////////////////////////////////////////////
    const bool relax    = doc["channels"].HasMember("relaxed_uv");
    std::vector<float> ruv_hi, bnm_hi; // bake_dim^2 * 4 (RGBA), only when relax
    auto loadRGBA4 = [&](const char* chan) -> std::vector<float> {
      std::string f = doc["channels"][chan]["file"].GetString();
      if (f.find('/') == std::string::npos) {
        auto mdir = self->_resolved_manifest.substr(0, self->_resolved_manifest.find_last_of('/'));
        f         = mdir + "/" + f;
      }
      auto cin = OIIO::ImageInput::open(f);
      OrkAssert(cin);
      const auto& cs = cin->spec();
      OrkAssert(cs.width == bake_dim and cs.height == bake_dim);
      std::vector<float> raw(size_t(cs.width) * cs.height * cs.nchannels);
      cin->read_image(0, 0, 0, cs.nchannels, OIIO::TypeDesc::FLOAT, raw.data());
      cin->close();
      std::vector<float> out(size_t(bake_dim) * bake_dim * 4, 0.0f);
      int nc = std::min(cs.nchannels, 4);
      for (size_t i = 0; i < size_t(bake_dim) * bake_dim; i++)
        for (int c = 0; c < nc; c++)
          out[i * 4 + c] = raw[i * cs.nchannels + c];
      return out;
    };
    if (relax) {
      ruv_hi = loadRGBA4("relaxed_uv");
      bnm_hi = loadRGBA4("binormal");
    }
    //////////////////////////////////////////////////////////////////
    // 3. the layout arithmetic — MUST mirror gpu_chunk.py
    //////////////////////////////////////////////////////////////////
    int chunk    = self->_chunk;
    int vpc      = chunk * chunk * 6;                    // verts per full chunk
    // The fixed-cap per-chunk arrays are sized at the LAYOUT CAP (maxnc) — the same cap the shader
    // was generated with (gpu_chunk.py bake_dim) — so render + bake share ONE buffer layout.
    // layout_dim_cap > 0 pins the cap independent of the manifest dim (the editor pins it at its
    // max dim, so the shader TEXT is constant across dims and dim changes recompile nothing).
    int cap_dim  = (self->_layout_dim_cap > 0) ? self->_layout_dim_cap : bake_dim;
    OrkAssert(cap_dim >= bake_dim);
    int bake_cps = (cap_dim + chunk - 1) / chunk;
    int maxnc    = bake_cps * bake_cps;
    // RENDER grid: render_dimension <= bake_dim (downsample target). 0 => coupled (== bake_dim).
    int render_dim    = (self->_render_dimension > 0) ? self->_render_dimension : bake_dim;
    OrkAssert(render_dim <= bake_dim);
    int render_cps    = (render_dim + chunk - 1) / chunk;
    int render_nchunk = render_cps * render_cps;
    //////////////////////////////////////////////////////////////////
    // 3b. DOWNSAMPLE hi-res (bake_dim) -> the render grid (render_dim) with the SAME FILTERED
    //     resampler the Bullet collider uses (Image::resampledOf TRIANGLE) — one sampling
    //     convention for physics + visuals. The old hand-rolled bilinear POINT-TAP is only
    //     valid at ratios <= ~1: at NON-INTEGER bake/render ratios its per-texel fractional
    //     phase produces moire banding (height AND the frame channels -> lighting/color
    //     artifacts), and at ratios > 2 it skips source texels outright (aliasing).
    //////////////////////////////////////////////////////////////////
    auto resampleF = [&](const std::vector<float>& src, int C) -> std::vector<float> {
      if (render_dim == bake_dim)
        return src;
      OrkAssert(C == 1 or C == 4);
      Image simg;
      simg.initWithFormat(bake_dim, bake_dim, (C == 1) ? EBufferFormat::R32F : EBufferFormat::RGBA32F);
      std::memcpy((void*)simg._data->data(), src.data(), src.size() * sizeof(float));
      Image dimg;
      dimg.resampledOf(simg, render_dim, render_dim, Image::ResampleFilter::TRIANGLE);
      std::vector<float> out(size_t(render_dim) * render_dim * C);
      std::memcpy(out.data(), dimg._data->data(), out.size() * sizeof(float));
      return out;
    };
    std::vector<float> heights = resampleF(heights_hi, 1);
    //////////////////////////////////////////////////////////////////
    // 3c. RELAX pack — when relaxed, the per-vertex SSBO array is stride-5 uints (WS4 fp16 packing).
    //     The RENDER array uses the DOWNSAMPLED channels (matching the downsampled heights); the BAKE array
    //     (stashed below) uses FULL-res channels so the atlas rasterizes in full-res relaxed space. Mono =>
    //     the array is just the flat heights (stride 1). Mirrors gpu_chunk.py TerrainChunkVertexSource(relax).
    //////////////////////////////////////////////////////////////////
    // pack relaxed_uv(RGBA=uv.xy,nrm.xz) + binormal(RGBA=bn.xyz,1) -> stride-5 UINT at grid gdim.
    // WS4 fp16: uv stays fp32 (bitcast — atlas param; fp16 would quantize ~2 atlas texels near
    // 1.0), normal/binormal pack as fp16 pairs (~1e-4 component error, fine for lighting/TBN).
    // The old slot-0 height is DROPPED (no shader reads it — terr_pos uses the dense heights[]).
    // 32B -> 20B per texel. MUST mirror gpu_chunk.py frame_block/vs_body (stride 5u decode).
    auto pack_frame5 = [](const std::vector<float>& ruv,
                          const std::vector<float>& bnm, int gdim) -> std::vector<uint32_t> {
      auto f2h = [](float f) -> uint32_t { // same semantics as image_fmt_convert float_to_half
        uint32_t bits;
        std::memcpy(&bits, &f, 4);
        uint32_t sign = (bits >> 16) & 0x8000;
        int32_t exp32 = int32_t((bits >> 23) & 0xFF) - 127 + 15;
        uint32_t mant = (bits & 0x007FFFFF);
        if (exp32 <= 0) return sign;            // underflow to zero
        if (exp32 >= 31) return sign | 0x7C00;  // overflow to inf
        return sign | (uint32_t(exp32) << 10) | (mant >> 13);
      };
      auto h2 = [&](float a, float b) -> uint32_t { return f2h(a) | (f2h(b) << 16); };
      std::vector<uint32_t> out(size_t(gdim) * gdim * 5);
      for (size_t i = 0; i < size_t(gdim) * gdim; i++) {
        std::memcpy(&out[i * 5 + 0], &ruv[i * 4 + 0], 4);      // uv.x (fp32 bits)
        std::memcpy(&out[i * 5 + 1], &ruv[i * 4 + 1], 4);      // uv.y (fp32 bits)
        out[i * 5 + 2] = h2(ruv[i * 4 + 2], ruv[i * 4 + 3]);   // normal.x | normal.z
        out[i * 5 + 3] = h2(bnm[i * 4 + 0], bnm[i * 4 + 1]);   // binormal.x | binormal.y
        out[i * 5 + 4] = h2(bnm[i * 4 + 2], 0.0f);             // binormal.z | pad
      }
      return out;
    };
    // relax: the RENDER-res frame array (its own SSBO, bound to sif_terra_frame) — the DOWNSAMPLED
    // channels, matching the downsampled heights. heights[] itself stays DENSE stride-1 in BOTH modes
    // (terr_pos + the depth-prepass keep mono's cache density; the frame is read only by the color VS).
    std::vector<uint32_t> framerender;
    if (relax) {
      auto ruv_r  = resampleF(ruv_hi, 4);
      auto bnm_r  = resampleF(bnm_hi, 4);
      framerender = pack_frame5(ruv_r, bnm_r, render_dim);
    }
    size_t CAM_OFF     = 0;
    size_t ARGS_OFF    = 160; // after CamBlk (64+64+16+16)
    size_t VIS_OFF     = 176;
    // heights[] is the trailing RUNTIME array (render_dim^2); fixed-cap per-chunk arrays precede it
    // (chunk_y first for vec2 8-alignment), sized at maxnc (BAKE chunk count). MUST mirror gpu_chunk.py.
    size_t YB_OFF      = 192;                                               // vec2 u_ybounds (global [min,max] m)
    size_t CHUNKY_OFF  = 200;                                               // vec2 chunk_y[MAXNC] @200
    size_t VLIST_OFF   = CHUNKY_OFF + size_t(maxnc) * 8;                    // uint v_list[MAXNC]
    size_t HEIGHTS_OFF = VLIST_OFF + size_t(maxnc) * 4;                     // float heights[] (runtime, last; DENSE)
    size_t TOTAL       = HEIGHTS_OFF + size_t(render_dim) * render_dim * 4; // render buffer (downsampled)
    (void)vpc;
    (void)extent_m;
    //////////////////////////////////////////////////////////////////
    // 4. the SSBO + heights upload (+ the relax frame SSBO)
    //////////////////////////////////////////////////////////////////
    auto fxi  = ctx->FXI();
    // BAR: GPU reads the dense heights + VIS every pass; the CPU's per-frame touches (CamBlk
    // write in drawable_compute, tiny) stay direct-mapped forward writes. Not DEVICE — that
    // would turn each per-frame CamBlk map into a synchronous staged submit+wait.
    auto ssbo = fxi->createStorageBuffer(TOTAL, StorageBufferUsage::DEFAULT, BufferResidency::BAR);
    { // u_dim (runtime grid dim) -> VIS header slot 2 @VIS_OFF+8. Uploaded ONCE; the per-frame reset
      // compute never touches it (survives reset). The VS/cull read it instead of a baked literal.
      uint32_t udim = uint32_t(render_dim);   // RENDER grid dim (mesh is downsampled to this)
      auto m = fxi->mapStorageBuffer(ssbo, VIS_OFF + 8, 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, &udim, 4);
      fxi->unmapStorageBuffer(m.get());
    }
    { // the DENSE heights @HEIGHTS_OFF — terr_pos reads heights[cz*u_dim+cx] in both modes
      auto m = fxi->mapStorageBuffer(ssbo, HEIGHTS_OFF, heights.size() * 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, heights.data(), heights.size() * 4);
      fxi->unmapStorageBuffer(m.get());
    }
    FxShaderStorageBuffer* frameSSBO = nullptr;
    if (relax) { // stride-5 packed frame @render_dim (WS4 fp16) — bound to sif_terra_frame below
      // DEVICE: written ONCE here (the map below becomes a one-time staged upload), then
      // GPU-read-only every pass — the biggest per-frame PCIe re-read in the terrain path.
      frameSSBO = fxi->createStorageBuffer(framerender.size() * 4, StorageBufferUsage::DEFAULT, BufferResidency::DEVICE);
      auto m = fxi->mapStorageBuffer(frameSSBO, 0, framerender.size() * 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, framerender.data(), framerender.size() * 4);
      fxi->unmapStorageBuffer(m.get());
    }
    //////////////////////////////////////////////////////////////////
    // 4b. per-chunk WORLD-Y bounds for the HZB occlusion AABB (heights are TRUE METERS —
    //     world Y = h directly; y_bias 0 for this path). Scan each chunk's texel span (+1 for
    //     the shared edge the mesh reaches). Tight maxY lets a valley chunk cull behind a
    //     nearer ridge; the shader pads maxY +0.5 so a flat chunk keeps a box. The GLOBAL
    //     [min,max] goes to u_ybounds (the frustum cull's conservative vertical box — there
    //     is no height-scale constant to build one from anymore). Mirror gpu_chunk.py layout.
    //////////////////////////////////////////////////////////////////
    float gmn = 1e30f, gmx = -1e30f; // global field bounds (render-res scan; texBake framing reuses)
    {
      std::vector<float> chunkY(size_t(render_nchunk) * 2);
      for (int ccz = 0; ccz < render_cps; ccz++) {
        for (int ccx = 0; ccx < render_cps; ccx++) {
          float hmn = 1e30f, hmx = -1e30f;
          int x1 = std::min(render_dim, (ccx + 1) * chunk + 1);
          int z1 = std::min(render_dim, (ccz + 1) * chunk + 1);
          for (int z = ccz * chunk; z < z1; z++)
            for (int x = ccx * chunk; x < x1; x++) {
              float h = heights[size_t(z) * render_dim + x];
              hmn = std::min(hmn, h);
              hmx = std::max(hmx, h);
            }
          int ci             = ccz * render_cps + ccx;
          chunkY[ci * 2 + 0] = hmn; // world minY (meters)
          chunkY[ci * 2 + 1] = hmx; // world maxY (meters)
          gmn = std::min(gmn, hmn);
          gmx = std::max(gmx, hmx);
        }
      }
      auto m = fxi->mapStorageBuffer(ssbo, CHUNKY_OFF, chunkY.size() * 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, chunkY.data(), chunkY.size() * 4);
      fxi->unmapStorageBuffer(m.get());
      float yb[2] = {gmn, gmx};
      auto ym = fxi->mapStorageBuffer(ssbo, YB_OFF, sizeof(yb), BufferMapAccess::WRITE_ONLY);
      std::memcpy(ym->_mappedaddr, yb, sizeof(yb));
      fxi->unmapStorageBuffer(ym.get());
    }
    //////////////////////////////////////////////////////////////////
    // 5. the ComputeDrawable consumer contract (viewer2 parity)
    //////////////////////////////////////////////////////////////////
    auto mtl   = self->_resolved_material;
    auto fsmtl = mtl->_as_freestyle;
    OrkAssert(fsmtl);
    auto sif = fsmtl->storageBlock("sif_ptex_vtx");
    OrkAssert(sif);
    auto cdd       = std::make_shared<ComputeDrawableData>();
    cdd->_material = mtl;
    cdd->addGraphicsStorage(sif, ssbo);
    const FxShaderStorageBlock* frame_sif = nullptr;
    if (relax) { // the relax frame SSBO — every VS variant inherits sif_terra_frame (dpp's reads DCE)
      frame_sif = fsmtl->storageBlock("sif_terra_frame");
      if (not frame_sif) {
        logchan_tcd->log(
            "TerrainChunkDrawable: manifest<%s> is RELAXED but material<%s> lacks sif_terra_frame "
            "(stale material — re-materialize the scene) — abort",
            self->_resolved_manifest.c_str(),
            self->_material_asset_name.c_str());
        state->_built = true;
        return;
      }
      cdd->addGraphicsStorage(frame_sif, frameSSBO);
    }
    cdd->setCameraParams(ssbo, CAM_OFF);
    int cull_groups = (render_nchunk + 63) / 64;   // dispatch over the RENDER chunk grid
    struct PassDef { const char* _name; int _gx; };
    // reset -> cull -> sort (near-to-far for early-Z; one group, thread 0 sorts) -> finalize.
    PassDef passes[4] = {{"cs_terrain_reset", 1}, {"cs_terrain_cull", cull_groups},
                         {"cs_terrain_sort", 1}, {"cs_terrain_finalize", 1}};
    for (const auto& p : passes) {
      auto cs = fsmtl->computeShader(p._name);
      if (not cs) {
        logchan_tcd->log(
            "TerrainChunkDrawable: material<%s> lacks compute<%s> — was it authored with "
            "TerrainChunkVertexSource?",
            self->_material_asset_name.c_str(),
            p._name);
        state->_built = true;
        return;
      }
      // relax: the compute interface inherits sif_terra_frame (dense-binding requirement — see
      // gpu_chunk.py cif_terrain), so its pipeline layout includes it; bind the buffer per pass.
      if (frame_sif)
        cdd->addComputePass(cs, {{sif, ssbo}, {frame_sif, frameSSBO}}, p._gx, 1, 1);
      else
        cdd->addComputePass(cs, {{sif, ssbo}}, p._gx, 1, 1);
    }
    cdd->setIndirect(ssbo, ARGS_OFF, nullptr, PrimitiveType::TRIANGLES, 4);
    //////////////////////////////////////////////////////////////////
    // graft onto the live drawable (the D.3 copy block)
    //////////////////////////////////////////////////////////////////
    drawable->_passes          = cdd->_passes;
    drawable->_camParamsSSBO   = cdd->_camParamsSSBO;
    drawable->_camParamsOffset = cdd->_camParamsOffset;
    drawable->_material        = cdd->_material;
    drawable->_graphicsStorage = cdd->_graphicsStorage;
    drawable->_argsSSBO        = cdd->_argsSSBO;
    drawable->_argsOffset      = cdd->_argsOffset;
    drawable->_indexSSBO       = cdd->_indexSSBO;
    drawable->_primtype        = cdd->_primtype;
    drawable->_indexSize       = cdd->_indexSize;
    // HZB 1-phase occlusion: the cull shader's read-only sif_hzb block. ComputeDrawable::onPreRender
    // binds the per-frame HZB pyramid here (from the RCFD) and packs base w/h/mips into CamBlk.misc.yzw.
    // null (block absent/optimized out) => occlusion stays disabled, frustum-only — graceful.
    drawable->_hzbBlock = fsmtl->storageBlock("sif_hzb");
    // stash for the texture-bake one-shot (terrainTexBake) — the SSBO + scale + a heights copy (the
    // bake builds its OWN all-chunks SSBO, since the per-frame cull overwrites the shared v_list).
    state->_ssbo     = ssbo;
    state->_dim      = bake_dim;     // BAKE grid (terrainTexBake draws ALL bake_dim chunks at hi-res)
    state->_chunk    = chunk;
    state->_nchunk   = maxnc;        // bake chunk count (== the array cap)
    state->_vpc      = vpc;
    state->_extent_m = extent_m;
    // actual field range METERS (render-res scan — the texBake ortho framing's +extent
    // margin swallows the sub-texel delta vs the bake-res peaks)
    state->_ymin = gmn;
    state->_ymax = gmx;
    // PROC-mode atlas currency (checked HERE because the bake stashes below are only
    // built when a bake will actually run): the python cap_dir key is input-derived
    // and can't see C++ cook-salt re-keys (e.g. fbm v6) — compare the products
    // snapshot the atlas was captured against with the (post-product-bake, fresh)
    // sidecars. Mismatch or missing snapshot -> the one-shot re-captures.
    if (self->_capture_mode == "proc" and not self->_capture_dir.empty()
        and not getenv("ORKID_TERRAIN_TEXBAKE_DUMP")) {
      auto digest = terrainProductsDigest(self->_resolved_manifest);
      std::string snap;
      {
        std::ifstream f(std::filesystem::path(self->_capture_dir) / "products.cookhash");
        std::getline(f, snap);
      }
      if (not digest.empty() and digest != snap) {
        logchan_tcd->log(
            "TERRAIN-TEXBAKE: STALE atlas (terrain products re-keyed) dir<%s> — will re-capture",
            self->_capture_dir.c_str());
        state->_stale_atlas = true;
      }
    }
    if (self->_capture_mode == "stored" or state->_stale_atlas or getenv("ORKID_TERRAIN_TEXBAKE_DUMP")) {
      // HI-RES (bake_dim^2): dense heights (both modes) + the stride-5 packed frame (relax) — the
      // bake rasterizes the surface into the atlas at the FULL-res relaxed uv from the frame SSBO.
      state->_heights = heights_hi;
      if (relax)
        state->_frame = pack_frame5(ruv_hi, bnm_hi, bake_dim);
    }
    // S4: arm the live-accept — the height product FILE PATH is the artifact name (the
    // same join key the bake's capture path and the Bullet collider's _resPath share).
    // _live_gen starts at the CURRENT generation so only publishes NEWER than this
    // materialize (i.e. the next re-bake's checkpoints) are consumed.
    state->_heights_off = HEIGHTS_OFF;
    state->_chunky_off  = CHUNKY_OFF;
    state->_yb_off      = YB_OFF;
    state->_render_dim  = render_dim;
    state->_render_cps  = render_cps;
    if (not s4ProgressiveDisabled()) {
      state->_live     = liveFieldAcquire(liveFieldCanonicalKey(hfile));
      state->_live_gen = state->_live->generation();
    }
    state->_cdd   = cdd;
    state->_built = true;
    logchan_tcd->log(
        "TerrainChunkDrawable: materialized (bake_dim<%d> render_dim<%d> render_chunks<%dx%d> "
        "ssbo<%.1fMB> mtl<%s>)",
        bake_dim,
        render_dim,
        render_cps,
        render_cps,
        double(TOTAL) / 1e6,
        self->_material_asset_name.c_str());
    // dim-flow trace (the layout half of ORKID_TERRAIN_DIMLOG): every quantity that must
    // agree with the vertex-source text + the uploads — a desync here IS the mesh
    // discontinuity, visible in the log instead of in pixels.
    static const bool s_dimlog = (getenv("ORKID_TERRAIN_DIMLOG") != nullptr);
    if (s_dimlog)
      printf("[terrain-dim] DRAWABLE bake_dim=%d render_dim=%d cap_dim=%d maxnc=%d "
             "HEIGHTS_OFF=%zu TOTAL=%zu heights=%zu chunkY=%d u_dim=%d relax=%d\n",
             bake_dim, render_dim, cap_dim, maxnc, HEIGHTS_OFF, TOTAL,
             heights.size(), render_nchunk, render_dim, int(relax));
  };

  // terrain proctex texture-bake one-shot: stored mode (cached) OR the debug env dump. Material must be
  // authored capture=True so FWD_SSBO_CUSTOM_CAPTURE exists. Returns false until _liveRecompute builds.
  // The stored-mode bake is registered as pending ASYNC WORK (async_tracker) so an offscreen waiter
  // (ork.scene.materialize.py / the player's --offscreen exit) knows when the disk cache has been
  // written, instead of guessing with a frame count. The one-shot returns true exactly once (then it
  // is cleared), so the matching asyncWorkEnd fires once whether the bake succeeded or self-skipped.
  // proc mode with a disk atlas ALSO registers the one-shot: it runs the products-
  // snapshot currency check post-bake and self-clears without baking when current
  // (re-captures when stale — see terrainTexBake).
  const bool cap_check = (self->_capture_mode == "proc" and not self->_capture_dir.empty());
  if (self->_capture_mode == "stored" or cap_check or getenv("ORKID_TERRAIN_TEXBAKE_DUMP")) {
    const bool track = (self->_capture_mode == "stored") or cap_check;
    if (track)
      asyncWorkBegin("terrain_texbake");
    drw->_oneShotRender = [self, state, track](Context* ctx) -> bool {
      bool done = terrainTexBake(ctx, self, state.get());
      if (done and track)
        asyncWorkEnd("terrain_texbake");
      return done;
    };
  }

  auto draw_raw = drw.get();
  // [M] MATERIAL-OVERRIDE: read the mode off pbr_common (RCFD "PBR_COMMON") each frame and,
  // ON CHANGE, swap the drawable's GRAPHICS material to the mode's resolved material (mode 0 =
  // the DECLARED material object — identity). The debug materials share the terrain
  // vertex_source, but bindStorage keys on the BLOCK POINTER (per-material), so we must re-point
  // the graphics-storage blocks to the swapped material's own blocks (same names, same buffers).
  // The COMPUTE passes + SSBO are untouched (identical layout; they keep filling the buffers).
  auto cur_mode = std::make_shared<int>(0);
  drw->setRenderLambda([draw_raw, self, cur_mode](RenderContextInstData& RCID) {
    int mode = 0;
    if (auto rcfd = RCID.rcfd()) {
      auto pbc = rcfd->tryUserProperty<pbr::commonstuff_ptr_t>("PBR_COMMON"_crcu);
      if (pbc) {
        auto cs = pbc.value();
        if (cs)
          mode = cs->_terrainMaterialMode;
      }
    }
    if (mode != *cur_mode and not self->_mode_materials.empty()) {
      // clamp the requested mode against the ACTUAL resolved-material count (data-driven; no magic
      // count). Out-of-range or mode 0 -> the declared material.
      int nmodes = int(self->_mode_materials.size());
      auto want  = (mode >= 1 and mode < nmodes) ? self->_mode_materials[mode] : self->_mode_materials[0];
      if (not want)
        want = self->_mode_materials[0]; // missing debug material -> fall back to declared (no stale)
      if (want and want->_as_freestyle and not draw_raw->_graphicsStorage.empty()) {
        // rebind the vertex-source blocks onto the swapped material's own block objects
        // (order matches _liveRecompute: [0]=sif_ptex_vtx, [1]=sif_terra_frame when relaxed).
        auto fs = want->_as_freestyle;
        auto& gs = draw_raw->_graphicsStorage;
        gs[0].first = fs->storageBlock("sif_ptex_vtx");
        if (gs.size() > 1)
          gs[1].first = fs->storageBlock("sif_terra_frame");
        draw_raw->_material = want;
        *cur_mode          = mode;
      }
    }
    draw_raw->_renderIndirect(RCID);
  });
  return drw;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::terrain

ImplementReflectionX(ork::lev2::terrain::TerrainChunkDrawableData, "TerrainChunkDrawableData");
