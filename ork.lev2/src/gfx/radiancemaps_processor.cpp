////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <algorithm>
#include <atomic>
#include <unistd.h>
#include <chrono>
#include <map>
#include <mutex>
#include <thread>
#include <boost/filesystem.hpp>
#include <ork/pch.h>
#include <ork/util/logger.h>
#include <ork/lev2/gfx/radiancemaps_processor.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/xir_format.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/txi.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gpumicrotask.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>
#include <ork/lev2/gfx/targetinterfaces.h>
#include <ork/lev2/gfx/image.h>
#include <ork/asset/Asset.inl>
#include <ork/kernel/timer.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/async_tracker.h>
#include <ork/file/file.h>

namespace ork::lev2 {

static logchannel_ptr_t logchan_gen = logger()->configureChannel("ENVMAPGEN", fvec3(0.8, 0.8, 0.1), true);

extern context_ptr_t gloadercontext;

////////////////////////////////////////////////////////////////////////////////
// Shader path for environment filtering (copied from material_pbr_gen.cpp)
////////////////////////////////////////////////////////////////////////////////

static file::Path filterenv_shader_path() {
  return file::Path("orkshader://pbr_filterenv.fxv2");
}

////////////////////////////////////////////////////////////////////////////////
// TileParams implementation
////////////////////////////////////////////////////////////////////////////////

fvec4 TileParams::getNDC(int tex_width, int tex_height) const {
  float ndc_x = (float)x / tex_width * 2.0f - 1.0f;
  float ndc_y = (float)y / tex_height * 2.0f - 1.0f;
  float ndc_w = (float)width / tex_width * 2.0f;
  float ndc_h = (float)height / tex_height * 2.0f;
  return fvec4(ndc_x, ndc_y, ndc_w, ndc_h);
}

fvec4 TileParams::getUV(int tex_width, int tex_height) const {
  float uv_x = (float)x / tex_width;
  float uv_y = (float)y / tex_height;
  float uv_w = (float)width / tex_width;
  float uv_h = (float)height / tex_height;
  return fvec4(uv_x, uv_y, uv_w, uv_h);
}

////////////////////////////////////////////////////////////////////////////////
// Tile rendering methods
////////////////////////////////////////////////////////////////////////////////

void EnvMapProcessor::renderSpecularTile(
    Context* ctx,
    const TileParams& tile,
    texture_ptr_t src_tex,
    rtbuffer_ptr_t target_buffer,
    float roughness,
    int num_samples) {

  // This will be called from a task on the GPU thread
  // Render a single tile of the specular filtered environment map

  auto dwi       = ctx->DWI();
  int tex_width  = target_buffer->_width;
  int tex_height = target_buffer->_height;

  // Calculate NDC and UV coordinates for this tile
  fvec4 ndc = tile.getNDC(tex_width, tex_height);
  fvec4 uv  = tile.getUV(tex_width, tex_height);

  // Render the tile using quad2D
  // dwi->quad2D(ndc, uv, fvec4(0, 0, 0, 0));
}

////////////////////////////////////////////////////////////////////////////////
// MT2 (JUL05_GPUMICROTASK §2.6): shared per-level filtering primitives.
//
// The filtering math is deterministic + stateless per tile (T12): the SAME
// helpers below drive BOTH the burst taskgraph (createFilteringTaskGraph, one
// TaskPhase per level on gloadercontext) AND the sliced microtask
// (RadiancePrefilterMicrotask, one Context::executeInlineGpuJob per tile batch
// and per readback, on any context). Shared code = byte-identical output (§5
// determinism gate), regardless of which context filters or how the submits are
// sequenced — mt2_radiance_byteid GATE-B is the standing measurement of that.
//
// A8: the filterenv material's tweakable inputs (roughness / sample count /
// image dims) ride bindParam* UBO uniforms, NEVER inlined into shader text.
////////////////////////////////////////////////////////////////////////////////

namespace {

struct EnvFilterState {
  // inputs (frozen for the whole job — T12: no per-slice-varying state)
  texture_ptr_t _rawenvmap;
  bool          _is_equirectangular = false;
  int           _tex_width          = 0;
  int           _tex_height         = 0;
  std::string   _tex_name;
  EBufferFormat _capture_format       = EBufferFormat::RGBA16F;
  EBufferFormat _render_target_format = EBufferFormat::RGBA32F;
  int                _num_roughness_levels = 10;
  // COMFORT-1: importance-sample counts, per job. Defaulted to the bake-time
  // values so every existing caller (and every baked .xir) is byte-unchanged;
  // the procedural sky feed lowers them (see SkyAtmosphereData).
  int                _specular_samples = EnvMapProcessor::kBakedSpecularSamples;
  std::vector<float> _roughness_values;
  // materials (built once by initFilterMaterials)
  std::shared_ptr<FreestyleMaterial> _specular_material;
  // outputs (appended one entry per rendered level, in level order)
  rtgroup_list_t                   _spec_rtgroups;
  rtbuffer_list_t                  _spec_rtbuffers;
  std::vector<captureasync_ptr_t>  _spec_futures;
  std::vector<capturebuffer_ptr_t> _spec_capbufs;
  image_list_t                     _debug_spec_images;
  // COMFORT-2 package staging. The packaging runs as a SERIES of slices, so
  // its intermediates have to outlive any one of them; filled in cursor order:
  // serialize (per level) -> container open / add per level / emit per stream
  // -> container reopen -> per-level upload decode + budgeted chain slices ->
  // publish.
  std::vector<datablock_ptr_t> _spec_datablocks;
  xir::xirarraywriter_ptr_t    _package_writer;
  datablock_ptr_t              _xir_datablock;
  xir::xirarrayreader_ptr_t    _publish_reader;
  std::vector<image_ptr_t>     _publish_spec_images;
  std::vector<compressedmipchain_ptr_t> _publish_spec_chains;
  mipchainbuilder_ptr_t        _publish_chain_builder;
  // The RESOLVED mip-chain slice budget in output pixels, written once by the
  // microtask ctor (which resolves property/env/default) and read by the chain
  // slices. It rides the state because the SAME number has to size the step plan
  // and pace each step: a plan cut for one budget and stepped at another either
  // publishes a SHORT array (too few steps) or burns no-op slices. 0 = never
  // written, which only happens on the burst taskgraph path — it has no publish
  // chain at all.
  size_t _mipchain_budget_px = 0;
  // THE AMBIENT (W4-S9) — the nine L2 coefficients and the available-light
  // measure this cycle publishes, projected from specular level 0 the moment
  // that level is decoded (preparePublishSpecularLevel). Invalid until then;
  // carried here so the publish can hand it to the RadianceMaps it swaps in.
  pbr::RadianceSH _sh;
};

void initEnvFilterState(EnvFilterState& st, texture_ptr_t rawenvmap, bool is_equirectangular) {
  st._rawenvmap            = rawenvmap;
  st._is_equirectangular   = is_equirectangular;
  st._tex_width            = rawenvmap->_width;
  st._tex_height           = rawenvmap->_height;
  st._tex_name             = file::Path(rawenvmap->_debugName).toBFS().stem().string();
  // Render RGBA32F, capture RGBA16F — for EVERY source. The filtered radiance
  // is HDR regardless of how the source was encoded (a filter of an 8-bit
  // source still feeds a scene with exposure and a tone curve), and the RGBA8
  // capture this replaced baked a permanent 1.0 ceiling into the .xir.
  st._capture_format       = EBufferFormat::RGBA16F;
  st._render_target_format = EBufferFormat::RGBA32F;
  st._num_roughness_levels = 10;
  const float roughness_power = 0.5f;
  for (int i = 0; i < st._num_roughness_levels; i++)
    st._roughness_values.push_back(powf(float(i) / 9.0f, roughness_power));
}

////////////////////////////////////////////////////////////////////////////////
// ONE filter material per Context, for the life of the process.
//
// The material is a pure function of its shader and a rasterstate that never
// varies, so per-cycle construction bought nothing — and cost a device-lost.
// The engine's pipeline caches are keyed by RAW MATERIAL POINTER and are never
// evicted (FxPipelineCacheImpl::_fxcachemap), so a refilter cycle whose fresh
// material landed on a freed predecessor's address inherited that
// predecessor's FxPipelineCache: pipelines holding a raw backref to the dead
// material (FxPipeline::_material_ptr, plus the state lambda's captured `mtl`)
// which re-bind its freed textures on the next draw. Constructing once removes
// the address churn here; ~FreestyleMaterial's cache eviction removes the
// defect class. It also erases the per-cycle stranded-cache leak.
//
// Deliberately leaked (never torn down): a function-local static of shared_ptrs
// would destruct at static-destruction time, i.e. AFTER the Context it was
// gpuInit'd on. Process-lifetime is the honest lifetime for a per-Context
// singleton with no context-death hook to hang eviction on.
////////////////////////////////////////////////////////////////////////////////

static bool _filterMaterialTrace() {
  static bool t = (std::getenv("ORKID_MT_TRACE") != nullptr);
  return t;
}

void initFilterMaterials(Context* ctx, EnvFilterState& st) {
  using material_map_t = std::map<Context*, std::shared_ptr<FreestyleMaterial>>;
  static std::mutex      gmutex;
  static material_map_t* gmaterials = new material_map_t;

  std::lock_guard<std::mutex> lock(gmutex);

  bool built = false;
  auto it    = gmaterials->find(ctx);
  if (it == gmaterials->end()) {
    auto specular_material = std::make_shared<FreestyleMaterial>();
    specular_material->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
    specular_material->_rasterstate->setDepthTest(EDepthTest::OFF);
    specular_material->_rasterstate->setCullTest(ECullTest::OFF);
    specular_material->gpuInit(ctx, filterenv_shader_path());
    it    = gmaterials->emplace(ctx, specular_material).first;
    built = true;
  }

  st._specular_material = it->second;

  if (_filterMaterialTrace()) {
    fprintf(
        stderr,
        "[RPMT] filter-material %s ctx<%p> mtl<%p>\n",
        built ? "BUILT" : "reused",
        (void*)ctx,
        (void*)it->second.get());
    fflush(stderr);
  }
}

////////////////////////////////////////////////////////////////////////////////
// COMFORT-2 tile batching. A level's tile grid is the unit of work a slice can
// be cut on: tiles are disjoint, cover the target exactly once, and (blending
// and depth off) each writes every channel of every pixel it covers — so the
// pixels are the same however the tiles are distributed over submits. What is
// NOT free is the submit itself (measured ~0.9-1.4ms of CPU-visible round trip
// per executeInlineGpuJob at any size), hence a batch COUNT per level rather
// than a tile count per slice: the split stays bounded no matter how many tiles
// the source extent implies (a 4k bake is 512 tiles per level).
//
// The DEFAULT of one batch is a measured answer to an extent-dependent
// question, hence the knob: at a 256x128 procedural snapshot a whole specular
// level's tile work is ~1.2ms — less than the submit that halving it would
// cost. Splitting only pays where a level's tile work exceeds the submit
// overhead, which the snapshot extent (IblSnapshotWidth/Height, 512x256 by
// default) decides.
////////////////////////////////////////////////////////////////////////////////

// FALLBACK, not the only source: SkyAtmosphereData::_iblLevelBatches above zero
// wins (resolved once in the microtask ctor). Bake callers pass nothing and land
// here, as they did before the property existed.
int levelBatchCount() {
  static const int count = []() -> int {
    if (const char* v = std::getenv("ORKID_MT_IBL_LEVEL_BATCHES"))
      return std::max(1, atoi(v));
    return 1;
  }();
  return count;
}

////////////////////////////////////////////////////////////////////////////////
// How many of THIS job's slices one frame may run (GpuMicrotask::
// _maxSlicesPerFrame). The scheduler's frame budget is the only other throttle
// and it does not exist on an UNBOUNDED context, which is every offscreen and
// loader context: measured, all 56 slices of a 256x128 sky-IBL cycle landed in
// ONE offscreen frame, 89-134ms of it, once per cycle. Slicing that a job is
// not allowed to actually spread across frames is not slicing.
//
// The default is ONE — the finest grain the plan has, and what the header has
// always claimed the job runs at. Raising it trades frame smoothness for cycle
// latency (a cycle is ~58 slices at the default snapshot extent, so N per frame
// is ~58/N frames per cycle).
////////////////////////////////////////////////////////////////////////////////

// FALLBACK: SkyAtmosphereData::_iblSlicesPerFrame above zero wins. The 0 the
// property uses for "unset" is NOT this function's answer — it never returns 0,
// which is _maxSlicesPerFrame's "unbounded".
int slicesPerFrame() {
  static const int count = []() -> int {
    if (const char* v = std::getenv("ORKID_MT_IBL_SLICES_PER_FRAME"))
      return std::max(1, atoi(v));
    return 1;
  }();
  return count;
}

////////////////////////////////////////////////////////////////////////////////
// COMFORT-3: how much of a published roughness level's CPU mip chain one slice
// may build, in DOWNSAMPLED OUTPUT PIXELS.
//
// The chain is the most expensive CPU thing the package phase does and the one
// with no GPU in it at all: a 4x4 kernel evaluated in double per channel per
// output pixel, with a half<->float conversion per tap for the RGBA16F capture
// format. Measured 0.19us per output pixel here, and a 512x256 level is 43.7k
// output pixels — 8.5ms, which is a dropped VR frame per level. 12288 buys a
// ~2.3ms slice, half the comfort ceiling, which is the headroom the gate's
// ceiling convention asks for. Like the tile batch count this is an extent-
// dependent measurement, hence the knob.
////////////////////////////////////////////////////////////////////////////////

// FALLBACK: SkyAtmosphereData::_iblMipChainBudgetPx above zero wins. Both users
// of the budget (the step plan and each chain slice) read the ONE resolved value
// off EnvFilterState — never this function directly — so they cannot disagree.
size_t mipChainSliceBudgetPx() {
  static const size_t budget = []() -> size_t {
    if (const char* v = std::getenv("ORKID_MT_IBL_MIPCHAIN_BUDGET_PX"))
      return std::max(1, atoi(v));
    return 12288;
  }();
  return budget;
}

int tileGridCount(int width, int height, int tile_size) {
  return ((width + tile_size - 1) / tile_size) * ((height + tile_size - 1) / tile_size);
}

// tiles covered by batch `bi` of `nbatches` over `ntiles` — [begin,end).
void tileBatchRange(int ntiles, int nbatches, int bi, int& begin, int& end) {
  int per   = (ntiles + nbatches - 1) / nbatches;
  begin     = std::min(ntiles, bi * per);
  end       = std::min(ntiles, begin + per);
}

// how many batches a level of `ntiles` tiles is actually cut into; the caller
// passes the already-resolved batch count (see the microtask ctor) so the
// property/env/default choice is made in exactly one place
int batchesForLevel(int ntiles, int level_batches) {
  return std::min(std::max(1, ntiles), std::max(1, level_batches));
}

void renderSpecularLevelTiles(Context* ctx, EnvFilterState& st, int rough_idx, int tile_begin, int tile_end, bool capture) {
  const int   tex_width  = st._tex_width;
  const int   tex_height = st._tex_height;
  const float roughness  = st._roughness_values[rough_idx];

  const int tile_size   = EnvMapProcessor::SPECULAR_TILE_SIZE;
  const int num_tiles_x = (tex_width + tile_size - 1) / tile_size;
  const int num_tiles   = tileGridCount(tex_width, tex_height, tile_size);
  const bool first_batch = (0 == tile_begin);

  rtgroup_ptr_t  rtgroup;
  rtbuffer_ptr_t rtbuffer;
  if (first_batch) {
    rtgroup             = std::make_shared<RtGroup>(ctx, tex_width, tex_height, MsaaSamples::MSAA_1X);
    rtbuffer            = rtgroup->createRenderTarget(st._render_target_format);
    rtbuffer->_debugName = FormatString("%s-spc-rtb-%d", st._tex_name.c_str(), rough_idx);
    rtgroup->_name       = FormatString("%s-spc-rtg-%d", st._tex_name.c_str(), rough_idx);
    // A level split across submits must not re-clear on the later ones (each
    // submit begins its own render pass): LOAD keeps what the earlier batches
    // wrote. Legal on the first batch too — the grid overwrites every pixel.
    // A single-batch level keeps the historical clear.
    rtgroup->_autoclear = (tile_end >= num_tiles);
    st._spec_rtgroups.push_back(rtgroup);
    st._spec_rtbuffers.push_back(rtbuffer);
  } else {
    rtgroup  = st._spec_rtgroups[rough_idx];
    rtbuffer = st._spec_rtbuffers[rough_idx];
  }

  auto RCFD = std::make_shared<RenderContextFrameData>(ctx);
  auto fbi  = ctx->FBI();
  auto dwi  = ctx->DWI();

  fbi->PushRtGroup(rtgroup.get());

  auto technique_name = st._is_equirectangular                          //
                            ? "tek_filterSpecularMapEquirectangular"    //
                            : "tek_filterSpecularMapStandard";
  auto material  = st._specular_material;
  auto technique = material->technique(technique_name);
  OrkAssert(technique);
  material->begin(technique, RCFD);

  auto param_mvp                   = material->param("mvp");
  auto param_pfm                   = material->param("prefiltmap");
  auto param_ruf                   = material->param("roughness");
  auto param_imgdim                = material->param("imgdim");
  auto param_numsamples            = material->param("numsamples");
  auto param_viewport_size         = material->param("ViewportSize");
  auto param_inv_viewport_size_vtx = material->param("InvViewportSize");
  auto param_inv_viewport_size_frg = material->param("InvViewportSizeFrg");

  OrkAssert(param_mvp);
  OrkAssert(param_pfm);
  OrkAssert(param_ruf);
  OrkAssert(param_imgdim);
  OrkAssert(param_numsamples);

  material->bindParamMatrix(param_mvp, fmtx4::Identity());
  material->bindParamTexture(param_pfm, st._rawenvmap.get());
  material->bindParamFloat(param_ruf, roughness);
  material->bindParamVec2(param_imgdim, fvec2(tex_width, tex_height));
  material->bindParamU32(param_numsamples, uint32_t(st._specular_samples));
  if (param_viewport_size)
    material->bindParamVec2(param_viewport_size, fvec2(tex_width, tex_height));
  if (param_inv_viewport_size_vtx)
    material->bindParamVec2(param_inv_viewport_size_vtx, fvec2(1.0f / tex_width, 1.0f / tex_height));
  if (param_inv_viewport_size_frg)
    material->bindParamVec2(param_inv_viewport_size_frg, fvec2(1.0f / tex_width, 1.0f / tex_height));

  material->commit();

  for (int ti = tile_begin; ti < tile_end; ti++) {
    TileParams tile;
    tile.x         = (ti % num_tiles_x) * tile_size;
    tile.y         = (ti / num_tiles_x) * tile_size;
    tile.width     = std::min(tile_size, tex_width - tile.x);
    tile.height    = std::min(tile_size, tex_height - tile.y);
    tile.roughness = roughness;
    fvec4 ndc      = tile.getNDC(tex_width, tex_height);
    fvec4 uv       = tile.getUV(tex_width, tex_height);
    dwi->quad2D(ndc, uv, fvec4(0, 0, 0, 0));
  }

  material->end(RCFD);
  fbi->PopRtGroup();

  if (capture) {
    auto capbuf = std::make_shared<CaptureBuffer>();
    st._spec_capbufs.push_back(capbuf);
    auto future = fbi->captureAsFormat(rtbuffer.get(), capbuf, st._capture_format);
    st._spec_futures.push_back(future);
  }
}

// COMFORT-2: the readback is its own step because it is the single most
// expensive thing a filter level does and the one part of it that does NOT
// subdivide — measured 2.0-3.5ms for a 256x128 level (a per-capture staging
// VkBuffer allocation plus an uncached 512KB device->host read of the RGBA32F
// target) against ~1.2ms for all of that level's tile rendering. Sharing a
// slice with tile work only adds to the worst slice, never divides it.
void captureSpecularLevel(Context* ctx, EnvFilterState& st, int rough_idx) {
  auto capbuf = std::make_shared<CaptureBuffer>();
  st._spec_capbufs.push_back(capbuf);
  auto future = ctx->FBI()->captureAsFormat(st._spec_rtbuffers[rough_idx].get(), capbuf, st._capture_format);
  st._spec_futures.push_back(future);
}

void renderSpecularLevel(Context* ctx, EnvFilterState& st, int rough_idx) {
  int ntiles = tileGridCount(st._tex_width, st._tex_height, EnvMapProcessor::SPECULAR_TILE_SIZE);
  renderSpecularLevelTiles(ctx, st, rough_idx, 0, ntiles, true);
}

static int captureBytesPerChannel(EBufferFormat fmt) {
  return (fmt == EBufferFormat::RGBA16F) ? 2 : (fmt == EBufferFormat::RGBA32F) ? 4 : 1;
}

// COMFORT-2: the packaging is per-level so the sliced path can spend one slice
// on one level. Serializes captured specular level `i` into its own
// single-level XTX datablock, appended in level order (a failed capture
// contributes nothing, exactly as the monolithic loop did).
void serializeSpecularLevel(EnvFilterState& st, size_t i) {
  auto capbuf = st._spec_capbufs[i];
  auto future = st._spec_futures[i];
  future->wait(nullptr);
  if (future->_failed || (not capbuf->_image))
    return;
  CompressedImageMipChain single_level_chain;
  CompressedImage base_level;
  base_level._width           = capbuf->width();
  base_level._height          = capbuf->height();
  base_level._depth           = 1;
  base_level._format          = st._capture_format;
  base_level._numcomponents   = 4;
  base_level._bytesPerChannel = captureBytesPerChannel(st._capture_format);
  base_level._data            = capbuf->_image->_data;

  single_level_chain._width         = base_level._width;
  single_level_chain._height        = base_level._height;
  single_level_chain._depth         = 1;
  single_level_chain._format        = base_level._format;
  single_level_chain._numcomponents = 4;
  single_level_chain._levels        = {base_level};

  auto roughness_datablock = std::make_shared<DataBlock>();
  single_level_chain.writeXTX(roughness_datablock);
  st._spec_datablocks.push_back(roughness_datablock);
  st._debug_spec_images.push_back(capbuf->_image);
}

// COMFORT-3: the container assembly is three step kinds, not one, because at
// the 512x256 snapshot it is ~12MB copied TWICE (payload into the chunk stream,
// streams into the container) — measured 18.7ms in a single slice, the worst
// thing the whole cycle did. openPackageWriter opens the container; one
// addPackageSpecularLevel per roughness level feeds it
// (~0.5ms each); one emitPackageStep per container stream writes it out
// (~1.1ms each). XIRArrayWriter guarantees the bytes are what the one-shot
// writer produced.
void openPackageWriter(EnvFilterState& st) {
  // The container's DIFFUSE STREAM stays in the format and is written EMPTY
  // (W4-S9): nothing prefilters an irradiance equirect any more, and every
  // .xir already on disk — each carrying a full chain nothing reads — must keep
  // loading through the same reader.
  CompressedImageMipChain diffuse_mipchain;
  auto diffuse_datablock = std::make_shared<DataBlock>();
  diffuse_mipchain.writeXTX(diffuse_datablock);

  st._xir_datablock  = std::make_shared<DataBlock>();
  st._package_writer = std::make_shared<xir::XIRArrayWriter>( //
      diffuse_datablock,
      st._roughness_values,
      st._spec_datablocks.size());
}

void addPackageSpecularLevel(EnvFilterState& st, size_t i) {
  if (i < st._spec_datablocks.size())
    st._package_writer->addSpecularLevel(i, st._spec_datablocks[i]);
}

// one container stream out; false once the container is complete
bool emitPackageStep(EnvFilterState& st) {
  return st._package_writer->emitNext(st._xir_datablock);
}

// the assembly run to completion (burst path — one call, no slicing)
void assembleFilterResult(EnvFilterState& st) {
  openPackageWriter(st);
  for (size_t i = 0; i < st._spec_datablocks.size(); i++)
    addPackageSpecularLevel(st, i);
  while (emitPackageStep(st)) {
  }
}

// Wait for every capture, then package the roughness array into the
// array-format XIR datablock (burst path — one call, no slicing).
datablock_ptr_t packageFilterResult(EnvFilterState& st) {
  for (auto& future : st._spec_futures)
    future->wait(nullptr);
  for (size_t i = 0; i < st._spec_capbufs.size(); i++)
    serializeSpecularLevel(st, i);
  assembleFilterResult(st);
  return st._xir_datablock;
}

////////////////////////////////////////////////////////////////////////////////
// In-scene live swap (MT2 §2.6). Decodes the freshly-packaged XIR datablock the
// EXACT way the load-time swap machinery does (radiancemaps_asset.cpp
// _loadFromXIR — "the hardest part exists") so the published maps are
// byte-identical to a normal load, then assigns-new every field on the
// `target` RadianceMaps and hands the outgoing textures to the N-frame deferred
// destroy (§1.6 rule 3, kDelayFrames=3). Runs on the render thread (ctx owner)
// — the microtask already GPU-completed every capture via executeInlineGpuJob,
// but the assignment itself is deferred to the uploads (see STAGE, DON'T SWAP).
//
// The field assignments are synchronous but the two texture UPLOADS they publish
// are not: both the loadreq path and the texture-array path record a one-shot
// transfer CB that folds into whatever primary CB is open and only becomes
// sampleable when its completion semaphore is polled at a later frame boundary.
// So "the publish call returned" is NOT "the maps are on the GPU" — a capture
// taken in that window shares its submission with the transfer and photographs
// the pre-swap frame. `on_uploads_complete` is invoked from the LAST upload's
// completion callback, which is what makes SkyIblState::_generation mean
// "sampleable"; and each in-flight upload holds a "texture_upload" async-tracker
// marker (same tag/contract as submitLoadingPhase in gfxctx.cpp and the opq path
// in txi_xtx.cpp) so a settle/exit poll cannot grab a mid-publish frame either.
//
// COMFORT-2 splits the DECODE out of the publish (decodePublishInputs +
// preparePublishSpecularLevel, one slice each) but not the ISSUE: both uploads
// and the field swap stay in this one function, so the issue guard, the
// completion counting and the swap keep exactly the atomicity described above.
// COMFORT-3 splits those two further (per-level container extraction, budgeted
// mip-chain slices) — again, everything ahead of the ISSUE.
//
// STAGE, DON'T SWAP. The new fields are filled into a STAGING RadianceMaps and
// copied onto `target` from inside the upload-completion continuation, never at
// issue time. Assigning at issue put textures whose transfer CB had not run yet
// into the live set (this slice runs after the frame's one-shot drain, so the
// uploads execute in the NEXT frame's CB and complete the frame after that):
// until then their sampling image is null and per-draw binding substitutes the
// BLACK default textures, so lit geometry went dark for ~2 frames — per-draw
// selectively, since only draws whose descriptors were rewritten inside the gap
// saw the null. The crossfade cannot cover that window because it IS the
// completion continuation: the fade only starts once the maps are sampleable.
// Deferring the assignment keeps the OLD resident maps bound through the gap and
// puts the visible swap exactly where the fade begins.
//
// Nothing may reach the continuation through the EnvFilterState: the scheduler
// drops the microtask (and its state) as soon as the publish slice returns, so
// the staging set carries every published value BY VALUE.
////////////////////////////////////////////////////////////////////////////////

static bool _publishTrace() {
  static bool t = (std::getenv("ORKID_MT_TRACE") != nullptr);
  return t;
}

// Decode step: the container header and the roughness values. The roughness
// levels are NOT read here — each is a megabyte and gets extracted by its own
// prepare slice (COMFORT-3; reading all ten here measured 4.5ms, the second
// worst slice of the cycle at 512x256).
void decodePublishInputs(EnvFilterState& st) {
  st._publish_reader = std::make_shared<xir::XIRArrayReader>(st._xir_datablock);
  auto& data         = st._publish_reader->_data;
  if (!data._valid || !data._is_array_format)
    return; // publishStagedToTarget owns the fail-loud + completion path
}

////////////////////////////////////////////////////////////////////////////////
// The publish chain's mip downsample fans row chunks onto a queue and JOINS on
// the render thread, inside a scheduler slice whose wall time is the number the
// comfort gate scores. On the shared concurrentQueue that join measures queue
// LATENCY, not filtering: the pool is also carrying readback conversions (each
// itself a fan-join) and any cold shader compile, and a chunk that waits behind
// them turns a 2.3ms slice into a 40-50ms one with no more pixels filtered.
// Fixing the operation rather than the pool: its own queue, whose only producer
// is this chain and whose worker never enqueues or blocks, so the join can
// neither starve nor be starved.
//
// One thread is the right width here rather than a copy of the pool: a chain
// slice's band is budgeted to ~48 rows of a 512-wide level, well under the
// chunk size, so the fan is a single chunk anyway.
////////////////////////////////////////////////////////////////////////////////
static opq::opq_ptr_t _chainDownsampleQueue() {
  static opq::opq_ptr_t q = std::make_shared<opq::OperationsQueue>(1, "iblChainDownsample", opq::EPerformaceProfile::BALANCED);
  return q;
}

// One specular array slice's worth of upload preparation: container extraction,
// XTX decode, image conversion, and the opening of the UPLOAD'S MIP CHAIN.
//
// That last one is the whole point of these steps. The texture array wants a
// CPU-downsampled chain per slice, which is scalar per-pixel work — measured
// 8.1-8.7ms for ONE 512x256 level, i.e. more than a frame each and ~85ms of a
// cycle. So the chain is built by a MipChainBuilder a bounded number of output
// pixels at a time (advancePublishSpecularChain) and handed to the TXI
// directly, rather than left for initTextureArray2DFromData to build inline.
void preparePublishSpecularLevel(EnvFilterState& st, int level) {
  auto reader = st._publish_reader;
  if (!reader || !reader->_data._valid)
    return;
  reader->readSpecularLevel(level);
  if (level >= int(reader->_data._specular_datablocks.size()))
    return;
  auto cmipchain = std::make_shared<CompressedImageMipChain>();
  cmipchain->readXTX(reader->_data._specular_datablocks[level]);
  auto image = std::make_shared<Image>();
  cmipchain->_levels[0].convertToImage(*image);
  st._publish_spec_images.push_back(image);
  // THE AMBIENT (W4-S9). Level 0 is the prefilter's identity level, so it IS
  // the radiance this cycle filtered — projected here, in the slice that
  // already paid for decoding it, rather than in the publish (which must stay
  // one indivisible issue).
  if (0 == level)
    st._sh = pbr::projectRadianceSH(*image);
  st._publish_chain_builder = std::make_shared<MipChainBuilder>(*image);
  st._publish_chain_builder->_downsample_queue = _chainDownsampleQueue();
}

// one budgeted slice of the level opened above; the completed chain is what the
// upload consumes
void advancePublishSpecularChain(EnvFilterState& st) {
  if (nullptr == st._publish_chain_builder)
    return;
  // the budget the PLAN was cut with, not a fresh read — a step paced differently
  // from the plan that scheduled it goes short or long (see _mipchain_budget_px).
  OrkAssertI(st._mipchain_budget_px > 0, "publish chain slice ran with no resolved mip-chain budget");
  if (st._publish_chain_builder->step(st._mipchain_budget_px))
    return;
  st._publish_spec_chains.push_back(st._publish_chain_builder->_chain);
  st._publish_chain_builder = nullptr;
}

void publishStagedToTarget(
    Context* ctx,
    EnvFilterState& st,
    pbr::radiancemaps_ptr_t target,
    const std::string& base_name,
    void_lambda_t on_uploads_complete = nullptr) {
  auto reader   = st._publish_reader;
  bool staged   = reader                       //
                and reader->_data._valid       //
                and reader->_data._is_array_format
                // a level whose chain slices did not all run would publish a
                // SHORT array — the swap must not see a partial package
                and (int(st._publish_spec_chains.size()) == reader->_data._num_roughness_levels);
  if (not staged) {
    logchan_gen->log("RadiancePrefilter: refilter datablock invalid — refusing to publish (fail loud)");
    OrkAssert(false);
    // Nothing was uploaded, so the continuation must still run: a cycle whose
    // completion never fires leaves the feed's in-flight gate latched forever.
    if (on_uploads_complete)
      on_uploads_complete();
    return;
  }

  // ISSUE GUARD (the leading 1): held until every upload below has been handed
  // to the TXI, so a backend that completes an upload inline cannot fire the
  // continuation while a later upload is still to be issued. Released at the
  // bottom of this function. Both the counter and every completion callback run
  // on the render thread (VkContext::_doBeginFrame polls the one-shot
  // semaphores), which is also this function's thread — hence a plain int.
  auto pending_uploads = std::make_shared<int>(1);

  // The staging slot: everything this cycle publishes, held OUT of the live set
  // until the uploads it belongs to have completed.
  auto staging = std::make_shared<pbr::RadianceMaps>();

  // ctx is captured raw — legally so: the completion callbacks are driven by
  // this very context's frame poll, so it outlives every firing.
  auto uploadComplete = [pending_uploads, on_uploads_complete, ctx, target, staging]() {
    if (0 != --(*pending_uploads))
      return;

    // THE SWAP, on the render thread, with every published texture sampleable.
    // Outgoing textures are snapshotted here (not at issue) and their last
    // reference deferred past MAX_FRAMES_IN_FLIGHT — a synchronous vkDestroy of
    // large in-flight maps is its own black frame (§1.6 rule 3).
    constexpr int kDelayFrames = 3;
    auto old_specular = target->_filtenvSpecularMapArray;
    if (old_specular)
      ctx->enqueueDelayedDestroy([old_specular]() {}, kDelayFrames);

    target->_numRoughnessLevels      = staging->_numRoughnessLevels;
    target->_specularRoughnessValues = staging->_specularRoughnessValues;
    // S9 ambient + available light cross at the swap; an invalid staged
    // projection never publishes a black sky over a good one.
    if (staging->_shValid) {
      for (int i = 0; i < 9; i++)
        target->_shCoeffs[i] = staging->_shCoeffs[i];
      target->_shValid          = true;
      target->_measuredLuminance = staging->_measuredLuminance;
    }
    target->_filtenvSpecularMapArray  = staging->_filtenvSpecularMapArray;
    target->_brdfIntegrationMapGGX    = staging->_brdfIntegrationMapGGX;
    target->_brdfIntegrationMapVelvet = staging->_brdfIntegrationMapVelvet;
    target->_brdfIntegrationMapGGXRIM = staging->_brdfIntegrationMapGGXRIM;
    target->_brdfIntegrationMapBlinn  = staging->_brdfIntegrationMapBlinn;
    target->_brdfIntegrationMapPhong  = staging->_brdfIntegrationMapPhong;

    if (_publishTrace()) {
      fprintf(stderr, "[RPMT] publish uploads complete (frame %d) -> staged swap + cycle completion\n",
              ctx->GetTargetFrame());
      fflush(stderr);
    }
    // AFTER the swap: this is what flips _ready/_generation and starts the fade,
    // both of which must describe the set the renderer is now bound to.
    if (on_uploads_complete)
      on_uploads_complete();
  };
  auto trackedUploadComplete = [uploadComplete]() {
    asyncWorkEnd("texture_upload");
    uploadComplete();
  };

  // Specular roughness texture array. The chains were built by the prepare
  // slices and are handed over as chains: the _subimg form of a slice would
  // have the TXI build them inline, which is the multi-frame cost those slices
  // exist to spread. _images is what that form would have recorded.
  int num_roughness_levels = int(st._publish_spec_chains.size());
  auto specular_texarray             = std::make_shared<TextureArray>();
  specular_texarray->_tex->_debugName = base_name + ".iblspec_array";
  TextureArrayInitData TID;
  TID._slices.resize(num_roughness_levels);
  for (int i = 0; i < num_roughness_levels; i++) {
    uint32_t usage_id          = CrcString(FormatString("roughness_%d", i).c_str()).hashed();
    TID._slices[i]._usage      = usage_id;
    TID._slices[i]._cmipchain  = st._publish_spec_chains[i];
    specular_texarray->_images[i] = st._publish_spec_images[i];
  }
  TID._on_gpu_upload_complete = trackedUploadComplete;
  (*pending_uploads)++;
  asyncWorkBegin("texture_upload");
  ctx->TXI()->initTextureArray2DFromData(specular_texarray.get(), TID);
  specular_texarray->_tex->TexSamplingMode()._texAddrModeS = TextureAddressMode::WRAP;
  specular_texarray->_tex->TexSamplingMode()._texAddrModeT = TextureAddressMode::CLAMP;
  specular_texarray->_tex->TexSamplingMode()._texAddrModeR = TextureAddressMode::CLAMP;
  ctx->TXI()->ApplySamplingMode(specular_texarray->_tex.get());

  // Fill the STAGING set (blackout-fix model preserved across S9). Nothing here
  // touches `target` — the renderer keeps sampling the resident maps until
  // uploadComplete performs the swap. S9: no prefiltered diffuse any more; the
  // ambient is the SH projection off this cycle's level-0 prepare slice.
  staging->_numRoughnessLevels      = num_roughness_levels;
  staging->_specularRoughnessValues = reader->_data._roughness_values;
  pbr::assignRadianceSH(staging, st._sh);
  if (st._sh._valid)
    staging->_measuredLuminance = st._sh._luminance;
  staging->_filtenvSpecularMapArray  = specular_texarray;
  staging->_brdfIntegrationMapGGX    = PBRMaterial::brdfIntegrationMap(ctx, "GGX");
  staging->_brdfIntegrationMapVelvet = PBRMaterial::brdfIntegrationMap(ctx, "GGXVELVET");
  staging->_brdfIntegrationMapGGXRIM = PBRMaterial::brdfIntegrationMap(ctx, "GGXRIM");
  staging->_brdfIntegrationMapBlinn  = PBRMaterial::brdfIntegrationMap(ctx, "BLINN");
  staging->_brdfIntegrationMapPhong  = PBRMaterial::brdfIntegrationMap(ctx, "PHONG");


  if (_publishTrace()) {
    fprintf(stderr, "[RPMT] publish issued %d async uploads (frame %d)\n",
            (*pending_uploads) - 1, ctx->GetTargetFrame());
    fflush(stderr);
  }

  // Release the issue guard — with every upload now in flight this is a plain
  // decrement; if there were none it is what fires the continuation.
  uploadComplete();
}

////////////////////////////////////////////////////////////////////////////////
// RadiancePrefilterMicrotask (MT2 §2.6) — first REAL microtask client.
//
// OPPORTUNISTIC. One slice = ONE entry of the step plan built in the ctor:
//   MATERIALS      : build the filter material
//   SPEC_TILES     : one batch of one specular roughness level's tiles
//   SPEC_CAPTURE   : that level's readback
//   PKG_SERIALIZE  : one captured specular level -> its XTX datablock
//   PKG_OPEN       : the XIR container opened
//   PKG_ADD        : one specular level fed into the container
//   PKG_EMIT       : one container stream written out
//   PKG_DECODE     : the container's header back (live only)
//   PKG_PREPARE    : one specular level extracted + its upload chain opened
//   PKG_CHAIN      : one budgeted slice of that chain
//   PKG_PUBLISH    : issue both uploads + the live swap
// runSlice runs exactly one step and returns true until the plan is exhausted.
// The render/capture steps go through Context::executeInlineGpuJob (T7 option
// "a": own submit + fence wait) so each slice's measured CPU-wall reflects its
// real GPU cost and the §2.3 budget loop throttles it across frames.
//
// COMFORT-2: the plan replaces an index cascade because the two package phases
// and the tile batching made the arithmetic the source of truth for four
// different step populations. The package steps carry their OWN cost key: they
// are a different cost population from the filter steps (CPU serialize/decode
// vs GPU render+fence), and a registry entry that averages the two misprices
// both — which is the same reason COMFORT-1 gave the final step its own seed.
//
// COMFORT-3 subdivides the package steps again, because the 512x256 snapshot
// quadrupled their bytes: the container assembly went from one 18.7ms step to
// an open + one add per level + one emit per stream, the container decode
// dropped its per-level extraction into the prepare steps, and each level's
// upload mip chain (8.5ms of scalar per-pixel CPU) became a run of budgeted
// PKG_CHAIN slices. Nothing MOVED across the completeCycle boundary — the whole
// subdivision sits ahead of PKG_PUBLISH, which is still one indivisible issue.
////////////////////////////////////////////////////////////////////////////////

struct RadiancePrefilterMicrotask final : public GpuMicrotask {

  enum class StepKind : uint8_t {
    MATERIALS = 0,
    SPEC_TILES,
    SPEC_CAPTURE,
    PKG_SERIALIZE,
    PKG_OPEN,
    PKG_ADD,
    PKG_EMIT,
    PKG_DECODE,
    PKG_PREPARE,
    PKG_CHAIN,
    PKG_PUBLISH,
  };

  struct Step {
    StepKind _kind;
    int      _level      = 0; // roughness level / array slice
    int      _tile_begin = 0;
    int      _tile_end   = 0;
  };

  RadiancePrefilterMicrotask(
      std::shared_ptr<EnvFilterState> state,
      pbr::radiancemaps_ptr_t target,
      std::function<void(datablock_ptr_t)> on_complete,
      int level_batches      = 0,
      int slices_per_frame   = 0,
      int mipchain_budget_px = 0,
      bool cold_start        = false)
      : _state(state)
      , _target(target)
      , _on_complete(on_complete) {
    _name     = "RadiancePrefilter:" + state->_tex_name;
    _class    = MicrotaskClass::OPPORTUNISTIC;
    // GRANULARITY RESOLUTION — the ONE place the three-way choice is made, for
    // all three consumers below (per-frame quota, spec tile batches, chain step
    // plan + chain slices). A caller-supplied value above
    // zero wins; 0 means the caller expressed no opinion, which is what every
    // bake caller does, and the fallback is the env var then the measured default.
    const int    res_level_batches = (level_batches > 0) ? level_batches : levelBatchCount();
    const int    res_slices        = (slices_per_frame > 0) ? slices_per_frame : slicesPerFrame();
    const size_t res_chain_budget  = (mipchain_budget_px > 0) ? size_t(mipchain_budget_px) : mipChainSliceBudgetPx();
    // this job spans frames BY CONSTRUCTION, not by whatever the budget happens
    // to allow — see slicesPerFrame(). The resolution above happens FIRST on
    // purpose: 0 here would mean "no per-task limit", so the unset sentinel must
    // never reach this field.
    //
    // COLD START inverts exactly that, and nothing else: the FIRST bake of a
    // feed has no previous maps to keep showing, so pacing it is not smoothness,
    // it is fifty frames of placeholder radiance. Quota OFF (the deliberate 0)
    // plus budget exemption = drain to completion in the frame it starts in.
    // Scoped to this instance, so the flip back is structural: the next cycle is
    // constructed with cold_start false and resolves res_slices like any other,
    // with no state anywhere for a stale cold window to leak through. The
    // granularity RESOLUTION above is untouched — cold start changes the budget,
    // never which grain the scene declared.
    _maxSlicesPerFrame = cold_start ? 0 : res_slices;
    _unboundedDrain    = cold_start;
    // COMFORT-1: what this job's slices cost is dominated by the source extent
    // and the sample counts, NOT by which texture it is — so the learned scale
    // is keyed on those and shared by every cycle of the same shape. A 4k bake
    // and a 512x256 sky feed keep separate keys, as they must — and so do two
    // sky feeds either side of an IblSnapshotWidth edit.
    _costKey  = FormatString(
        "RadiancePrefilter:%dx%d:%d",
        state->_tex_width,
        state->_tex_height,
        state->_specular_samples);
    // The package steps' cost tracks the extent and the level count only — no
    // sampling happens in them.
    _packageCostKey = FormatString(
        "RadiancePrefilterPkg:%dx%d:%d",
        state->_tex_width,
        state->_tex_height,
        state->_num_roughness_levels);

    auto step = [this](StepKind k, int level, int tb = 0, int te = 0) {
      _steps.push_back(Step{k, level, tb, te});
    };

    step(StepKind::MATERIALS, 0);
    const int spec_tiles = tileGridCount(state->_tex_width, state->_tex_height, EnvMapProcessor::SPECULAR_TILE_SIZE);
    for (int ri = 0; ri < state->_num_roughness_levels; ri++) {
      int nb = batchesForLevel(spec_tiles, res_level_batches);
      for (int bi = 0; bi < nb; bi++) {
        int tb = 0, te = 0;
        tileBatchRange(spec_tiles, nb, bi, tb, te);
        step(StepKind::SPEC_TILES, ri, tb, te);
      }
      step(StepKind::SPEC_CAPTURE, ri);
    }
    for (int ri = 0; ri < state->_num_roughness_levels; ri++)
      step(StepKind::PKG_SERIALIZE, ri);
    step(StepKind::PKG_OPEN, 0);
    for (int ri = 0; ri < state->_num_roughness_levels; ri++)
      step(StepKind::PKG_ADD, ri);
    // an UPPER bound: a capture that failed leaves fewer levels, hence fewer
    // streams, and the surplus emit steps run as no-ops rather than the plan
    // going short of a complete container.
    int emits = int(xir::XIRArrayWriter::emitStepCount(state->_num_roughness_levels));
    for (int ei = 0; ei < emits; ei++)
      step(StepKind::PKG_EMIT, ei);
    // published to the state so the chain SLICES pace themselves with the same
    // number this plan is cut with (advancePublishSpecularChain)
    state->_mipchain_budget_px = res_chain_budget;
    if (_target) {
      step(StepKind::PKG_DECODE, 0);
      int chain_steps = int(mipChainStepCount(state->_tex_width, state->_tex_height, state->_mipchain_budget_px));
      for (int ri = 0; ri < state->_num_roughness_levels; ri++) {
        step(StepKind::PKG_PREPARE, ri);
        for (int ci = 0; ci < chain_steps; ci++)
          step(StepKind::PKG_CHAIN, ri);
      }
      step(StepKind::PKG_PUBLISH, 0);
    }
    _numSteps = int(_steps.size());
    // the resolved triple, as ONE line: which granularity a run actually used is
    // otherwise invisible (three sources, four consumers, one plan). Behind the
    // TU's existing trace flag — a per-cycle line unguarded is a spam channel.
    if (_trace())
      logchan_gen->log(
          "RadiancePrefilter granularity: level_batches<%d> slices_per_frame<%d> mipchain_budget_px<%zu> steps<%d> "
          "cold_start<%d> quota<%d>",
          res_level_batches,
          res_slices,
          res_chain_budget,
          _numSteps,
          int(cold_start),
          _maxSlicesPerFrame);
  }

  static bool _isPackage(StepKind k) {
    return k >= StepKind::PKG_SERIALIZE;
  }

  // only the two capture-awaiting kinds are ever named (see the stall log)
  static const char* _stepName(const Step& S) {
    switch (S._kind) {
      case StepKind::PKG_SERIALIZE:
        return "pkg_serialize";
      case StepKind::PKG_OPEN:
        return "pkg_open";
      default:
        return "step";
    }
  }

  const std::string& sliceCostKey() const override {
    bool package = (_cursor < _numSteps) and _isPackage(_steps[_cursor]._kind);
    return package ? _packageCostKey : _costKey;
  }

  int64_t sliceEstimateUs() const override {
    // FIXED seeds, and deliberately NOT _lastMeasuredUs: feeding back the raw
    // last-measured cost lets a ONE-TIME pipeline-warmup slice (first specular
    // level ~180ms on a cold pipeline) pin the estimate above MAX_BUDGET, after
    // which the scheduler defers the task forever (only the first slice is
    // budget-exempt) and it never completes.
    //
    // COMFORT-1 changes what MULTIPLIES the seed, not the seed itself: the
    // scheduler's estimate scale is now seeded per cost key from the
    // MicrotaskCostRegistry (an EMA that rises fast and decays), so cycle N+1
    // starts out knowing roughly what cycle N's slices cost instead of sailing
    // through the est>budget gate at 1x every single cycle. That is NOT the
    // pinning failure above: the EMA recovers from an outlier, and the
    // scheduler's deferral escape guarantees the job completes even if it
    // doesn't.
    //
    // COMFORT-2: the two populations now differ by key, not by seed — the
    // package steps are per-level like the filter steps, so they share the same
    // few-ms seed and let their own learned scale do the discriminating.
    return 3000;
  }

  float progress() const override {
    return (_numSteps > 0) ? float(_cursor) / float(_numSteps) : -1.0f;
  }

  static bool _trace() {
    static bool t = (std::getenv("ORKID_MT_TRACE") != nullptr);
    return t;
  }

  //////////////////////////////////////////////////////////////////////////////
  // COOPERATIVE CAPTURE WAIT.
  //
  // A slice must NEVER block on a capture. A capture's host-side completion is
  // not finished by the GPU fence — the readback's format conversion is handed
  // to an opq worker (VkContext::_processPendingCaptures), and the DRAIN that
  // hands it over is the frame's own. A slice that blocking-waits inside
  // beginFrame therefore holds the frame open against the progress it is
  // waiting for: reproduced on MoltenVK as a permanent wedge parked in
  // CaptureAsync::wait's sleep loop, with linux only getting away with it on
  // timing.
  //
  // So the step is simply NOT RUN: runSlice returns with the cursor unmoved and
  // the scheduler resumes at the same step next drain. That is a RUN slice
  // measured at ~0us, NOT a deferral — it never enters the est>budget
  // accounting nor the deferral escape.
  //
  // The poll paces itself to one per frame by SAYING SO (_yieldFrame), not by
  // relying on the per-frame quota to stop the scheduler re-picking it: the
  // cold-start drain runs with no quota, and there the same poll would be a
  // render-thread spin against a worker thread — the frame-holding shape this
  // whole mechanism exists to avoid, minus only the lock that would make it
  // permanent.
  //////////////////////////////////////////////////////////////////////////////

  // A capture that never lands is a defect, not a wait — say so, once. The
  // threshold is WALL CLOCK, not a frame count: the failure this catches (a
  // wedged conversion worker) also collapses the frame rate, so a frame-count
  // trigger arrives minutes late or, when the loop stops entirely, never.
  static constexpr double kCaptureStallSecs = 10.0;

  static bool _captureReady(const captureasync_ptr_t& f) {
    return (nullptr == f) or f->isReady();
  }

  bool _capturesPending(EnvFilterState& st, const Step& S) const {
    switch (S._kind) {
      case StepKind::PKG_SERIALIZE:
        return (size_t(S._level) < st._spec_futures.size()) and not _captureReady(st._spec_futures[S._level]);
      case StepKind::PKG_OPEN:
        return false; // the container open waits on nothing
      default:
        return false;
    }
  }

  bool runSlice(MicrotaskContext& mctx) override {
    Context* ctx     = mctx._gfxctx;
    auto&    st      = *_state;

    if (_capturesPending(st, _steps[_cursor])) {
      if (0 == _capturePolls) {
        _captureTimer.Start();
        _captureStallLogged = false;
      }
      _capturePolls++;
      double awaited = _captureTimer.SecsSinceStart();
      if ((awaited > kCaptureStallSecs) and not _captureStallLogged) {
        _captureStallLogged = true;
        logchan_gen->log(
            "RadiancePrefilter <%s>: step %d/%d (%s) has awaited its capture for %.1f seconds (%llu polls) — the readback "
            "never completed",
            _name.c_str(),
            _cursor,
            _numSteps,
            _stepName(_steps[_cursor]),
            awaited,
            (unsigned long long)_capturePolls);
      }
      if (_trace() and (0 == (_capturePolls % 60))) {
        fprintf(stderr, "[RPMT] await_capture step=%d/%d polls=%llu\n",
                _cursor, _numSteps, (unsigned long long)_capturePolls);
        fflush(stderr);
      }
      _yieldFrame = true;
      return true; // resume at this same step
    }
    _capturePolls = 0;

    int      step    = _cursor++;
    const Step& S    = _steps[step];

    // COMFORT-1 per-slice instrumentation. The inline job's fence wait IS the
    // slice's GPU cost by design, so CPU wall around the step is the whole
    // story; the scheduler measures the same span for its budget, this one
    // attributes it to a NAMED step (which one hitches is the question a
    // comfort investigation actually asks).
    Timer slice_timer;
    slice_timer.Start();
    auto trace_slice = [&](const char* what) {
      if (not _trace())
        return;
      fprintf(
          stderr,
          "[RPMT] %s[%d] step=%d/%d frame=%llu %.2fms\n",
          what,
          S._level,
          step,
          _numSteps,
          (unsigned long long)mctx._frameIndex,
          slice_timer.SecsSinceStart() * 1000.0);
      fflush(stderr);
    };

    switch (S._kind) {
      case StepKind::MATERIALS: {
        initFilterMaterials(ctx, st); // CPU-side shader init; cached after first build
        trace_slice("materials");
        break;
      }
      case StepKind::SPEC_TILES: {
        int ri = S._level, tb = S._tile_begin, te = S._tile_end;
        ctx->executeInlineGpuJob([ctx, &st, ri, tb, te]() { renderSpecularLevelTiles(ctx, st, ri, tb, te, false); });
        trace_slice("spec");
        break;
      }
      case StepKind::SPEC_CAPTURE: {
        int ri = S._level;
        ctx->executeInlineGpuJob([ctx, &st, ri]() { captureSpecularLevel(ctx, st, ri); });
        trace_slice("spec_capture");
        break;
      }
      case StepKind::PKG_SERIALIZE: {
        if (S._level < int(st._spec_capbufs.size()))
          serializeSpecularLevel(st, size_t(S._level));
        trace_slice("pkg_serialize");
        break;
      }
      case StepKind::PKG_OPEN: {
        openPackageWriter(st);
        trace_slice("pkg_open");
        break;
      }
      case StepKind::PKG_ADD: {
        addPackageSpecularLevel(st, size_t(S._level));
        trace_slice("pkg_add");
        break;
      }
      case StepKind::PKG_EMIT: {
        emitPackageStep(st);
        // bake path (no live target): the datablock IS the product, and nothing
        // is uploaded, so there is nothing to wait on — complete on the emit
        // that closes the container.
        if ((nullptr == _target) and st._package_writer->complete()) {
          if (_on_complete)
            _on_complete(st._xir_datablock);
          trace_slice("pkg_emit");
          return false;
        }
        trace_slice("pkg_emit");
        break;
      }
      case StepKind::PKG_DECODE: {
        OrkAssert(st._package_writer->complete()); // nothing decodes a partial container
        decodePublishInputs(st);
        trace_slice("pkg_decode");
        break;
      }
      case StepKind::PKG_PREPARE: {
        preparePublishSpecularLevel(st, S._level);
        trace_slice("pkg_prepare");
        break;
      }
      case StepKind::PKG_CHAIN: {
        advancePublishSpecularChain(st);
        trace_slice("pkg_chain");
        break;
      }
      case StepKind::PKG_PUBLISH: {
        // The completion is DEFERRED to the publish's GPU uploads (see
        // publishStagedToTarget): _on_complete is what bumps
        // SkyIblState::_generation, and a generation that outran its uploads let
        // a capture gate photograph the pre-swap frame. Copied by value because
        // this task is dropped by the scheduler as soon as runSlice returns
        // false, well before the uploads complete.
        auto on_complete = _on_complete;
        auto xir         = st._xir_datablock;
        publishStagedToTarget(ctx, st, _target, st._tex_name, [on_complete, xir]() {
          if (on_complete)
            on_complete(xir);
        });
        trace_slice("pkg_publish");
        return false; // complete
      }
    }
    return true;
  }

  std::shared_ptr<EnvFilterState>      _state;
  pbr::radiancemaps_ptr_t              _target;
  std::function<void(datablock_ptr_t)> _on_complete;
  std::vector<Step>                    _steps;
  std::string                          _packageCostKey;
  int                                  _cursor   = 0;
  int                                  _numSteps = 0;
  uint64_t                             _capturePolls = 0; // consecutive frames awaiting the cursor step's capture
  Timer                                _captureTimer;     // wall clock across that same await
  bool                                 _captureStallLogged = false;
};

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// TaskGraph-based filtering implementation (burst path — bake-time / load
// screen, UNBOUNDED). Unchanged behavior; now delegates each level to the
// shared helpers above so its output is byte-identical to the microtask path.
////////////////////////////////////////////////////////////////////////////////

taskgraph_ptr_t EnvMapProcessor::createFilteringTaskGraph(texture_ptr_t rawenvmap, bool is_equirectangular) {

  auto graph = std::make_shared<TaskGraph>();
  auto state = std::make_shared<EnvFilterState>();
  initEnvFilterState(*state, rawenvmap, is_equirectangular);

  auto gpu_executor     = gloadercontext->createContextExecutor();
  auto primary_executor = TaskExecutor::createSerial(); // on primary execution thread

  // retain the state (+ the raw envmap) with the graph
  graph->_varmap.atomicOp([=](varmap::VarMap& unlocked) {
    unlocked.set<texture_ptr_t>("rawenvmap", rawenvmap);
    unlocked.set<std::shared_ptr<EnvFilterState>>("env_filter_state", state);
  });

  ///////////////////////////////////////
  // Phase 1: setup + one-frame barrier
  ///////////////////////////////////////

  auto setup_phase = TaskGraph::phase(graph, state->_tex_name + ".setup", gpu_executor);
  setup_phase->task("initialize_materials", [state](taskgraph_wkptr_t g) { //
    initFilterMaterials(gloadercontext.get(), *state);
  });
  ContextExecutor::emptyFrame(graph, state->_tex_name + ".setup-barrier", gpu_executor);

  ///////////////////////////////////////
  // Phase 2: specular filtering — one phase (one submit) per roughness level
  ///////////////////////////////////////

  for (int rough_idx = 0; rough_idx < state->_num_roughness_levels; rough_idx++) {
    std::string phase_name = state->_tex_name + ".specular_roughness_" + std::to_string(rough_idx);
    auto        phase      = TaskGraph::phase(graph, phase_name, gpu_executor);
    phase->task("spec_roughness_" + std::to_string(rough_idx), [state, rough_idx](taskgraph_wkptr_t g) {
      renderSpecularLevel(gloadercontext.get(), *state, rough_idx);
    });
    ContextExecutor::emptyFrame(graph, "specular-barrier", gpu_executor);
  }

  ContextExecutor::emptyFrame(graph, state->_tex_name + ".final-frame-barrier", gpu_executor);

  ///////////////////////////////////////
  // Phase 5: wait for captures + package datablocks (primary thread)
  ///////////////////////////////////////

  auto package_phase = TaskGraph::phase(graph, state->_tex_name + ".package_datablocks", primary_executor);
  package_phase->task("package_results", [state](taskgraph_wkptr_t g) {
    logchan_gen->log("EnvMapProcessor: Waiting for async captures spc<%zu> to complete...",
                     state->_spec_futures.size());
    auto xir_datablock = packageFilterResult(*state);
    logchan_gen->log("EnvMapProcessor: Packaging complete! xir<%zu bytes> spc<%zu>",
                     xir_datablock ? xir_datablock->length() : 0,
                     state->_debug_spec_images.size());
    g.lock()->_varmap.atomicOp([=](varmap::VarMap& vmap) {
      vmap.set<datablock_ptr_t>("xir_datablock", xir_datablock);
      vmap.set<image_list_t>("debug_spec_images", state->_debug_spec_images);
    });
  });

  return graph;
}

////////////////////////////////////////////////////////////////////////////////
// createRadiancePrefilterMicrotask (MT2 §2.6 factory)
////////////////////////////////////////////////////////////////////////////////

gpumicrotask_ptr_t EnvMapProcessor::createRadiancePrefilterMicrotask(
    texture_ptr_t rawenvmap,
    bool is_equirectangular,
    pbr::radiancemaps_ptr_t target,
    std::function<void(datablock_ptr_t)> on_complete,
    int specular_samples,
    int level_batches,
    int slices_per_frame,
    int mipchain_budget_px,
    bool cold_start) {

  OrkAssertIFMT(specular_samples > 0, "radiance prefilter sample count must be positive (spec<%d>)", specular_samples);
  auto state = std::make_shared<EnvFilterState>();
  initEnvFilterState(*state, rawenvmap, is_equirectangular);
  state->_specular_samples = specular_samples;
  // forwarded UNRESOLVED (zeros included): the ctor is the single place the
  // property/env/default choice is made, for the bake callers and the sky feed
  // alike — cold_start rides the same forwarding, and is resolved against the
  // granularity in that same one place.
  return std::make_shared<RadiancePrefilterMicrotask>(
      state, target, on_complete, level_batches, slices_per_frame, mipchain_budget_px, cold_start);
}

////////////////////////////////////////////////////////////////////////////////
// processToXIRDataBlockAsync
//////////////////////////////////////////////////////////////////////////////////

xirprocessfuture_ptr_t EnvMapProcessor::processToXIRDataBlockAsync(const file::Path& input_path) {

  // Create the future
  auto future = std::make_shared<XIRProcessFuture>();

  // Load source texture
  auto load_req = std::make_shared<asset::LoadRequest>(input_path);
  load_req->_gpu_load_async = false; // Load synchronously for now
  auto texasset = asset::AssetManager<TextureAsset>::load(load_req);
  if (!texasset || !texasset->GetTexture()) {
    future->setResult(nullptr);
    return future;
  }

  auto rawenvmap = texasset->GetTexture();

  // Ensure the texture is fully uploaded to GPU before proceeding.
  // _loadXTXTexture defers GPU upload to mainSerialQueue, so we must
  // process pending ops before using the texture in the filtering pipeline.
  while (opq::mainSerialQueue()->Process()) {}

  // Extract texture name for debug purposes
  std::string texture_name = input_path.getName();

  // Determine format from extension
  auto ext_str = input_path.getExtension();
  // Convert to lowercase manually
  std::transform(ext_str.begin(), ext_str.end(), ext_str.begin(), ::tolower);
  // getExtension returns without dot, so compare without dot
  bool is_equirectangular = (ext_str == "exr" || ext_str == "hdr");

  // CRITICAL FIX: For equirectangular maps, U-axis must wrap to prevent seam at ±180°
  if (is_equirectangular) {
    rawenvmap->TexSamplingMode()._texAddrModeS = TextureAddressMode::WRAP;  // U-axis wraps
    rawenvmap->TexSamplingMode()._texAddrModeT = TextureAddressMode::CLAMP; // V-axis clamps at poles
    rawenvmap->TexSamplingMode()._texAddrModeR = TextureAddressMode::CLAMP;

    // Apply the sampling mode to the GPU texture object
    auto txi = gloadercontext->TXI();
    txi->ApplySamplingMode(rawenvmap.get());

    logchan_gen->log("EnvMapProcessor: Set WRAP mode on U-axis for equirectangular texture %s",
                     texture_name.c_str());
  }

  // Create the TaskGraph for filtering

  // Execute on a worker thread with ContextExecutor
  opq::concurrentQueue()->enqueue([=]() {
    auto taskgraph = createFilteringTaskGraph(rawenvmap, is_equirectangular);

    auto on_graph_complete = [future](taskgraph_wkptr_t g) {
      // When graph completes, extract results and package as XIR
      datablock_ptr_t result_data;
      image_list_t    debug_spec_images;

      g.lock()->_varmap.atomicOp([&](varmap::VarMap& unlocked) {
        result_data       = unlocked.typedValueForKey<datablock_ptr_t>("xir_datablock").value();
        debug_spec_images = unlocked.typedValueForKey<image_list_t>("debug_spec_images").value();
      });
      // Set debug images in the future
      future->setDebugImages(debug_spec_images);
      future->setResult(result_data);
    };
    TaskGraph::execute(taskgraph, on_graph_complete);
  });

  return future;
}

////////////////////////////////////////////////////////////////////////////////
// processToXIRDataBlockAsyncViaMicrotask (MT2 §3 gate b byte-identity harness)
//
// Bakes `input_path` to XIR via the SLICED microtask path (enqueued on
// gloadercontext's scheduler, UNBOUNDED — the loader thread drains it exactly
// like the burst) instead of the burst taskgraph. Same shared helpers → its
// XIR datablock must be byte-identical to processToXIRDataBlockAsync's.
//////////////////////////////////////////////////////////////////////////////////

xirprocessfuture_ptr_t EnvMapProcessor::processToXIRDataBlockAsyncViaMicrotask(const file::Path& input_path) {

  auto future = std::make_shared<XIRProcessFuture>();

  auto load_req             = std::make_shared<asset::LoadRequest>(input_path);
  load_req->_gpu_load_async = false;
  auto texasset             = asset::AssetManager<TextureAsset>::load(load_req);
  if (!texasset || !texasset->GetTexture()) {
    future->setResult(nullptr);
    return future;
  }
  auto rawenvmap = texasset->GetTexture();
  while (opq::mainSerialQueue()->Process()) {}

  auto ext_str = input_path.getExtension();
  std::transform(ext_str.begin(), ext_str.end(), ext_str.begin(), ::tolower);
  bool is_equirectangular = (ext_str == "exr" || ext_str == "hdr");

  if (is_equirectangular) {
    rawenvmap->TexSamplingMode()._texAddrModeS = TextureAddressMode::WRAP;
    rawenvmap->TexSamplingMode()._texAddrModeT = TextureAddressMode::CLAMP;
    rawenvmap->TexSamplingMode()._texAddrModeR = TextureAddressMode::CLAMP;
    gloadercontext->TXI()->ApplySamplingMode(rawenvmap.get());
  }

  // No live target (bake, not in-scene refilter) — the completion just fulfills
  // the future with the packaged datablock.
  auto task = createRadiancePrefilterMicrotask(
      rawenvmap, is_equirectangular, nullptr, [future](datablock_ptr_t xir) { future->setResult(xir); });

  gloadercontext->_microtaskScheduler.enqueue(task);
  return future;
}

////////////////////////////////////////////////////////////////////////////////
// processToXIR
//////////////////////////////////////////////////////////////////////////////////

bool EnvMapProcessor::processToXIR(const file::Path& input_path, const file::Path& output_path) {

  auto future   = processToXIRDataBlockAsync(input_path);
  auto xir_data = future->get(); // Block waiting for result
  if (!xir_data) {
    return false;
  }

  // Write to file
  auto result = File::saveDatablock(output_path, xir_data);
  return (result == EFEC_FILE_OK);
}

////////////////////////////////////////////////////////////////////////////////

static std::atomic<int> gXIRProcessFutureCount{0};

XIRProcessFuture::XIRProcessFuture() {
  int count = gXIRProcessFutureCount.fetch_add(1);
  logchan_gen->log("XIRProcessFuture<%p> Ctor count<%d>", this, count + 1);
}
XIRProcessFuture::~XIRProcessFuture() {
  int count = gXIRProcessFutureCount.fetch_sub(1);
  logchan_gen->log("XIRProcessFuture<%p> Dtor count<%d>", this, count - 1);
}

} // namespace ork::lev2
