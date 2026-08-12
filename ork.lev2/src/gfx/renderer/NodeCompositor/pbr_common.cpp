////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

//#include <format>
//#include <print> // mac also ahead on this...
#include <algorithm>
#include <chrono>
#include <thread>
#include <ork/pch.h>
#include <ork/rtti/Class.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/mutex.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/application/application.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/sky_atmosphere.h>
#include <ork/kernel/datacache.h>
#include <ork/kernel/async_tracker.h>
#include <ork/gfx/brdf.inl>
#include <ork/gfx/dds.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/util/logger.h>
#include <ork/math/misc_math.h>

#include <ork/lev2/gfx/radiancemaps_asset.h>
#include <ork/lev2/gfx/radiancemaps_processor.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/xir_format.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/renderer/probe_sh.h>

#include <ork/profiling.inl>
#include <ork/asset/Asset.inl>

ImplementReflectionX(ork::lev2::pbr::CommonStuff, "pbr::CommonStuff");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::pbr {

constexpr size_t KNUMSSAONOISEFRAMES = 60;

static logchannel_ptr_t logchan_pbrcom = logger()->configureChannel("PBRCOM", fvec3(0.8, 0.8, 0.5), false);

///////////////////////////////////////////////////////////////////////////////
// DIRECT DIFFUSE BRDF registry — ONE table, read by the crc resolver, the name
// resolver (which is the crc resolver plus a hash) and the error text, so a
// model added here is immediately reachable from the scene DSL, the pyext and
// the editor row with nothing else to update.
///////////////////////////////////////////////////////////////////////////////
namespace {
struct DiffuseBrdfEntry {
  const char* _name;
  DiffuseBrdfModel _model;
};
static const DiffuseBrdfEntry kDiffuseBrdfTable[] = {
    {"LAMBERT", DiffuseBrdfModel::LAMBERT},
    {"OREN_NAYAR", DiffuseBrdfModel::OREN_NAYAR},
    {"BURLEY", DiffuseBrdfModel::BURLEY},
};
} // namespace
///////////////////////////////////////////////////////////////////////////////
bool diffuseBrdfModelFromCrc(uint64_t crc, DiffuseBrdfModel& out_) {
  for (const auto& item : kDiffuseBrdfTable) {
    if (CrcString(item._name).hashed() == crc) {
      out_ = item._model;
      return true;
    }
  }
  return false;
}
///////////////////////////////////////////////////////////////////////////////
bool diffuseBrdfModelFromName(const std::string& name, DiffuseBrdfModel& out_) {
  return diffuseBrdfModelFromCrc(CrcString(name.c_str()).hashed(), out_);
}
///////////////////////////////////////////////////////////////////////////////
const char* diffuseBrdfModelName(DiffuseBrdfModel model) {
  for (const auto& item : kDiffuseBrdfTable)
    if (item._model == model)
      return item._name;
  return "?";
}
///////////////////////////////////////////////////////////////////////////////
std::string diffuseBrdfModelValidSet() {
  std::string rval;
  for (const auto& item : kDiffuseBrdfTable) {
    if (not rval.empty())
      rval += ", ";
    rval += item._name;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
CommonStuff::CommonStuff() {

  _radiance_maps = std::make_shared<RadianceMaps>();
  _sky_ibl        = std::make_shared<SkyIblState>();
  _clearcolor     = fvec4(0, 0, 0, 1);
}
///////////////////////////////////////////////////////////////////////////////
// SKYLIGHT slice B3 — the snapshot law's enforcement point. Reaching beginCycle
// with a job already in flight means the caller's gate is broken and the
// running prefilter is about to have its source repainted underneath it.
void SkyIblState::beginCycle() {
  bool expected = false;
  OrkAssertI(
      _inflight.compare_exchange_strong(expected, true),
      "sky IBL refilter cycle started while another was still in flight - the equirect snapshot "
      "would mutate under a running microtask (SKYLIGHT snapshot law)");
  _cycles_started++;
  _frames_this_cycle.store(0); // the span the auto-sized fade window is read from
  _chain_timer.Start();        // start-to-start, so the ceiling bounds the cycle RATE

  // The set this cycle is about to replace, captured BY VALUE: the publish
  // assigns the new textures into the very object _maps points at. A first
  // cycle (empty target) leaves this null, which is what makes the baked ->
  // procedural handover snap rather than fade out of black maps.
  _pendingPrevMaps = nullptr;
  if (_maps and _maps->_filtenvSpecularMapArray)
    _pendingPrevMaps = std::make_shared<RadianceMaps>(*_maps);
}
///////////////////////////////////////////////////////////////////////////////
// Runs after publishStagedToTarget has swapped the maps AND their GPU uploads
// have completed (the publish defers this continuation to the last upload's
// completion semaphore), so neither _ready nor _generation ever announces an
// object the GPU cannot sample yet.
void SkyIblState::completeCycle(Context* ctx, int fade_frames, float fade_max_secs) {

  ////////////////////////////////////////
  // crossfade start (the swap frame)
  ////////////////////////////////////////

  if (_pendingPrevMaps) {
    // a cycle that completed while the previous fade was still running: the
    // older set is dropped here rather than blended three ways.
    if (_prevMaps)
      _retireMaps(ctx, _prevMaps);
    _prevMaps        = _pendingPrevMaps;
    _pendingPrevMaps = nullptr;
    int frames       = std::max(fade_frames, 0);
    _fade_frames_total.store(frames);
    _fade_frames_remaining.store(frames);
    // the fade's other bound, started on the swap frame: the window ENDS at
    // whichever of the two arrives first, so the clock has to run from the same
    // instant the frame count does.
    _fade_max_secs.store(std::max(fade_max_secs, 0.0f));
    _fade_timer.Start();
    if (0 == frames) { // fade disabled - the historical hard swap
      _retireMaps(ctx, _prevMaps);
      _prevMaps = nullptr;
    }
  }

  // how long THIS cycle took, in frames the render loop actually ran.
  _cycle_frames.store(_frames_this_cycle.load());

  // provisional cadence floor: with the fade disabled the cycle settles right
  // here, so this is the whole floor. With a fade running, tickCrossfade
  // overwrites it at fade end with the fuller measurement.
  _last_settle_secs.store(_chain_timer.SecsSinceStart());

  _generation++;
  _ready.store(true);
  _inflight.store(false);
}
///////////////////////////////////////////////////////////////////////////////
// One frame of the fade window, from the forward prologue (render thread).
void SkyIblState::tickCrossfade(Context* ctx) {
  // the frame clock the cycle-span measurement rides on — outside the fade
  // early-out below, since a cycle spans frames where no fade is running.
  _frames_this_cycle++;
  int remaining = _fade_frames_remaining.load();
  if (remaining <= 0)
    return;
  remaining--;
  // WALL-CLOCK BOUND. The frame window is sized from a measured frame SPAN, so
  // at a high frame rate it outlasts any duration a viewer would call a fade
  // (hundreds of frames offscreen); the budget ends it on time whatever the
  // rate. Retiring here rather than merely pinning the weight is what lets the
  // chained trigger's fadeSettled() see the fade as over.
  float budget = _fade_max_secs.load();
  if ((budget > 0.0f) and (_fade_timer.SecsSinceStart() >= double(budget)))
    remaining = 0;
  _fade_frames_remaining.store(remaining);
  if (0 == remaining) {
    _retireMaps(ctx, _prevMaps);
    _prevMaps = nullptr;
    // the full cadence floor: cycle start (when _chain_timer restarted) through
    // the frame the fade finished on — the earliest a next cycle may begin
    // without blending a third map set.
    _last_settle_secs.store(_chain_timer.SecsSinceStart());
  }
}
///////////////////////////////////////////////////////////////////////////////
// The chained fade window: as long as the last cycle took, so the fade is still
// interpolating right up to the frame the next publish lands on and the feed
// reads as piecewise-linear keyframe interpolation rather than 2-degree steps.
// Clamped low (a 1-frame "fade" is the pop it exists to hide) and high (a
// stalled cycle must not leave the outgoing maps blended in for seconds).
int SkyIblState::autoFadeFrames(int fallback) const {
  if (fallback <= 0) // explicit fade-disable — the hard swap the knob asked for
    return 0;
  int cycle_frames = _cycle_frames.load();
  if (cycle_frames <= 0)
    return fallback;
  return std::clamp(cycle_frames, 2, 60);
}
///////////////////////////////////////////////////////////////////////////////
// The chain cadence ceiling. Chaining's own pacing is the fade plus the angle
// floor, neither of which is a rate: on a fast day cycle over a cheap frame the
// feed will start a cycle every few milliseconds and spend the frame loop on
// IBL. This is the only clock in the trigger.
bool SkyIblState::chainCadenceElapsed(float max_hz) const {
  if (max_hz <= 0.0f) // uncapped — the continuous behavior the knob sits on top of
    return true;
  if (_cycles_started.load() == 0) // nothing has started, so nothing to pace against
    return true;
  return _chain_timer.SecsSinceStart() >= (1.0 / double(max_hz));
}
///////////////////////////////////////////////////////////////////////////////
// The declared cadence. Reads the same start-to-start clock the ceiling does, so
// the interval a scene declares is measured cycle-START to cycle-START. This is
// only the DECLARED half of the trigger: the caller also requires fadeSettled(),
// which floors the achieved cadence at cadenceFloorSecs() — so the effective
// interval is max(declared, floor), and a declaration under the floor is logged
// once rather than silently clamped. secs <= 0 means the knob is disabled, in
// which case this is never the trigger and the caller keeps the sun-angle policy;
// a feed that has never run a cycle is never held back (the first-ever snapshot
// is immediate in every mode).
bool SkyIblState::snapshotIntervalElapsed(float secs) const {
  if (secs <= 0.0f)
    return false;
  if (_cycles_started.load() == 0)
    return true;
  return _chain_timer.SecsSinceStart() >= double(secs);
}
///////////////////////////////////////////////////////////////////////////////
// The larger of the two progresses, because the fade ENDS at whichever bound
// lands first: riding frames alone stretches the ramp over wall-clock time at a
// high frame rate, riding the clock alone would jump the weight at the budget
// on a slow one. Reading them together keeps the ramp monotone through either
// hand-off.
float SkyIblState::fadeWeight() const {
  int total     = _fade_frames_total.load();
  int remaining = _fade_frames_remaining.load();
  if ((total <= 0) or (remaining <= 0))
    return 1.0f;
  float by_frames = 1.0f - (float(remaining) / float(total));
  float budget    = _fade_max_secs.load();
  float by_clock  = 0.0f;
  if (budget > 0.0f)
    by_clock = float(_fade_timer.SecsSinceStart() / double(budget));
  return std::clamp(std::max(by_frames, by_clock), 0.0f, 1.0f);
}
///////////////////////////////////////////////////////////////////////////////
// The last reference to a retired set drops MAX_FRAMES_IN_FLIGHT later, never
// inline: the frames still in flight may hold descriptors pointing at these
// textures (§1.6 rule 3, same delay the publish path uses).
void SkyIblState::_retireMaps(Context* ctx, radiancemaps_ptr_t maps) {
  if ((nullptr == maps) or (nullptr == ctx))
    return;
  constexpr int kDelayFrames = 3;
  ctx->enqueueDelayedDestroy([maps]() {}, kDelayFrames);
}
///////////////////////////////////////////////////////////////////////////////
void CommonStuff::assignEnvTexture(asset::asset_ptr_t texasset) {
  asset::vars_ptr_t old_varmap;
  if (_environmentTextureAsset) {
    old_varmap = _environmentTextureAsset->_varmap.clone();
    // printf("OLD <%p:%s>\n\n", _environmentTextureAsset.get(),_environmentTextureAsset->name().c_str());
  }
  // printf("NEW <%p:%s>\n\n", texasset.get(),texasset->name().c_str());

  _environmentTextureAsset = texasset;
  if (nullptr == _environmentTextureAsset)
    return;
  //_environmentTextureAsset->_varmap = *_RadianceVars();
}
///////////////////////////////////////////////////////////////////////////////

// Path-string expansion helper: resolves <assetcache>/, <stage>/, etc. to
// real filesystem paths. The scenegraph code that consumes the initial
// SkyboxTexPathStr (scenegraph.cpp ~L266) does this before constructing a
// LoadRequest; the request* functions below historically did NOT, so any
// caller passing a placeholder-bearing path (e.g. "<assetcache>/envmaps2/
// foo.xir") got a silent nullptr from AssetManager::load → null
// _radiance_maps → crash in envSpecularTexture(). Centralizing the
// expansion here keeps callers symmetric with the scenegraph path.
static AssetPath _expandIfNeeded(const AssetPath& p) {
  auto s = p.toStdString();
  if (s.find("<") != std::string::npos) {
    return AssetPath(file::Path::expandPathString(s));
  }
  return p;
}

radiancemaps_ptr_t CommonStuff::requestRadianceMaps(const AssetPath& texture_path) {
  auto resolved = _expandIfNeeded(texture_path);
  // Load XIR file directly using the registered XIR loader
  auto load_req = std::make_shared<asset::LoadRequest>(resolved);

  // Load using generic asset mechanism - the XIR extension will route to RadianceMapsLoader
  auto generic_asset = asset::AssetManager<RadianceMapsAsset>::load(load_req);
  if (generic_asset) {
    // Cast to RadianceMapsAsset
    auto radiancemaps_asset = std::dynamic_pointer_cast<RadianceMapsAsset>(generic_asset);
    if (radiancemaps_asset) {
      //_radiance_maps = radiancemaps_asset->_radiance_maps;
      if(0)printf("RRM: asset<%p> irrmaps<%p>\n", (void*) radiancemaps_asset.get(), (void*) radiancemaps_asset->_radiance_maps.get()  );
      return radiancemaps_asset->_radiance_maps;
    }
  }
  return nullptr;
}
radiancemaps_ptr_t CommonStuff::requestRadianceMapsAsync(const AssetPath& texture_path) {
  auto resolved = _expandIfNeeded(texture_path);
  // Load XIR file directly using the registered XIR loader
  auto load_req = std::make_shared<asset::LoadRequest>(resolved);

  // Load using generic asset mechanism - the XIR extension will route to RadianceMapsLoader
  auto generic_asset = asset::AssetManager<RadianceMapsAsset>::load(load_req);
  if (generic_asset) {
    // Cast to RadianceMapsAsset
    auto radiancemaps_asset = std::dynamic_pointer_cast<RadianceMapsAsset>(generic_asset);
    if (radiancemaps_asset) {
      //_radiance_maps = radiancemaps_asset->_radiance_maps;
      if(0)printf("RRM: asset<%p> irrmaps<%p>\n", (void*) radiancemaps_asset.get(), (void*) radiancemaps_asset->_radiance_maps.get()  );
      return radiancemaps_asset->_radiance_maps;
    }
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

radiancemaps_ptr_t CommonStuff::requestRadianceMapsSync(const AssetPath& texture_path, Context* ctx) {
  auto resolved = _expandIfNeeded(texture_path);
  // Kick off the load. RadianceMapsLoader wraps its async decode + three
  // deferred GPU-upload ops with a terminal deferred op that decrements
  // the LoadRequest's partial-load counter, so the counter hitting zero
  // means every layer has finished and the RadianceMaps are GPU-resident.
  auto load_req = std::make_shared<asset::LoadRequest>(resolved);
  auto generic_asset = asset::AssetManager<RadianceMapsAsset>::load(load_req);
  auto rm_asset = std::dynamic_pointer_cast<RadianceMapsAsset>(generic_asset);
  if (!rm_asset) return nullptr;

  // Self-pump ctx's deferred queue while we wait. Caller is on ctx's
  // owning thread (typically the GPU/render thread) and frame-begin would
  // normally drive the drain, but this routine may be called before the
  // render loop starts. We drain ctx's own queue here — Phase 6.3 Variant
  // B made the queue per-context, so this is now a pump on `ctx` rather
  // than a global drain.
  while (load_req->_partial_load_counter.load() > 0) {
    ctx->processDeferredOps();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return rm_asset->_radiance_maps;
}

///////////////////////////////////////////////////////////////////////////////

namespace {

// Gradient envs are rotation-symmetric about Y, so W only needs to be wide enough for
// stable bilinear filtering; H resolves the latitude gradient. Roughness curve matches
// the disk-IBL bake convention in radiancemaps_processor.cpp.
constexpr int   kProcNumRoughnessLevels = 10;
constexpr float kProcRoughnessPower     = 0.5f;
constexpr int   kProcEnvW               = 32;
constexpr int   kProcEnvH               = 64;

fvec3 sampleGradientAt(const std::vector<std::pair<float, fvec3>>& stops, float t) {
  if (t <= stops.front().first) return stops.front().second;
  if (t >= stops.back().first)  return stops.back().second;
  for (size_t i = 0; i + 1 < stops.size(); ++i) {
    float t0 = stops[i].first, t1 = stops[i + 1].first;
    if (t >= t0 && t <= t1) {
      float frac = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0f;
      return stops[i].second * (1.0f - frac) + stops[i + 1].second * frac;
    }
  }
  return stops.back().second;
}

// Solid-angle-weighted full-sphere average; lerp target for high-roughness slices.
fvec3 sphereAverage(const std::vector<std::pair<float, fvec3>>& stops, int n = 64) {
  fvec3 acc(0); float w_sum = 0.0f;
  for (int i = 0; i < n; ++i) {
    float v = (float(i) + 0.5f) / float(n);
    float w = sinf(v * float(PI));
    acc   = acc + sampleGradientAt(stops, v) * w;
    w_sum = w_sum + w;
  }
  return (w_sum > 0.0f) ? acc * (1.0f / w_sum) : fvec3(0);
}

// RGBA32F image filled with a per-row color via `row_color(v)`.
template<typename F>
image_ptr_t buildRowwiseImage(int w, int h, F&& row_color) {
  auto img              = std::make_shared<Image>();
  img->_format          = EBufferFormat::RGBA32F;
  img->_bytesPerChannel = 4;
  img->init(w, h, 4, 4);
  auto p = (float*)img->_data->data();
  for (int y = 0; y < h; ++y) {
    float v = (h > 1) ? float(y) / float(h - 1) : 0.0f;
    fvec3 c = row_color(v);
    for (int x = 0; x < w; ++x) {
      int idx = (y * w + x) * 4;
      p[idx + 0] = c.x; p[idx + 1] = c.y; p[idx + 2] = c.z; p[idx + 3] = 1.0f;
    }
  }
  return img;
}

// In-place upload — reuses VkImage / TextureArray slices, no descriptor churn.
void uploadRadianceMapsImages(
    radiancemaps_ptr_t maps,
    const std::vector<image_ptr_t>& spec_images,
    Context* ctx) {
  OrkAssert(int(spec_images.size()) == maps->_numRoughnessLevels);
  auto txi = ctx->TXI();
  for (size_t i = 0; i < spec_images.size(); ++i) {
    auto slice = maps->_filtenvSpecularMapArray->slice(i);
    txi->updateTextureArraySlice(slice.get(), spec_images[i]);
  }
}

} // namespace

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// THE AMBIENT PROJECTION (W4-S9). See pbr_common.h for why every publish site
// funnels through here.
///////////////////////////////////////////////////////////////////////////////

RadianceSH projectRadianceSH(const Image& equirect_level0) {
  RadianceSH rval;
  fvec3 coeffs[kProbeSHCoeffs];
  if (not projectEquirectImageSH(equirect_level0, coeffs))
    return rval; // invalid: the caller must leave its set unprojected
  for (int i = 0; i < kProbeSHCoeffs; i++)
    rval._coeffs[i] = fvec4(coeffs[i].x, coeffs[i].y, coeffs[i].z, 0.0f);
  // sphere-mean radiance = L0 * Y00; Rec.709 of it is the available-light
  // measure, in the map's own (undecoded) units.
  fvec3 mean  = coeffs[0] * 0.2820948f;
  rval._luminance = mean.x * 0.2126f + mean.y * 0.7152f + mean.z * 0.0722f;
  rval._valid     = true;
  return rval;
}

void assignRadianceSH(radiancemaps_ptr_t maps, const RadianceSH& sh) {
  if ((nullptr == maps) or (not sh._valid))
    return;
  for (int i = 0; i < 9; i++)
    maps->_shCoeffs[i] = sh._coeffs[i];
  maps->_shValid = true;
}

void publishRadianceMapsSH(radiancemaps_ptr_t maps, image_ptr_t equirect_level0) {
  if ((nullptr == maps) or (nullptr == equirect_level0))
    return;
  assignRadianceSH(maps, projectRadianceSH(*equirect_level0));
}

///////////////////////////////////////////////////////////////////////////////

radiancemaps_ptr_t CommonStuff::makeProceduralRadianceMaps(Context* ctx) {
  auto maps = std::make_shared<RadianceMaps>();
  maps->_numRoughnessLevels = kProcNumRoughnessLevels;
  maps->_specularRoughnessValues.resize(kProcNumRoughnessLevels);
  for (int i = 0; i < kProcNumRoughnessLevels; ++i) {
    maps->_specularRoughnessValues[i] =
        powf(float(i) / float(kProcNumRoughnessLevels - 1), kProcRoughnessPower);
  }

  auto black = buildRowwiseImage(kProcEnvW, kProcEnvH, [](float) { return fvec3(0); });

  auto txi          = ctx->TXI();
  auto specular_arr = std::make_shared<TextureArray>();
  specular_arr->_tex->_debugName = "procRadSpec";
  TextureArrayInitData TID;
  TID._slices.resize(kProcNumRoughnessLevels);
  for (int i = 0; i < kProcNumRoughnessLevels; ++i) {
    uint32_t usage_id = CrcString(FormatString("roughness_%d", i).c_str()).hashed();
    TID._slices[i]    = TextureArrayInitSubItem{usage_id, black};
  }
  txi->initTextureArray2DFromData(specular_arr.get(), TID);

  // Equirectangular: U wraps, V clamps (no pole bleed at V=0/V=1).
  auto setEquirectangularSampling = [&](Texture* tex) {
    tex->TexSamplingMode()._texAddrModeS = TextureAddressMode::WRAP;
    tex->TexSamplingMode()._texAddrModeT = TextureAddressMode::CLAMP;
    tex->TexSamplingMode()._texAddrModeR = TextureAddressMode::CLAMP;
    txi->ApplySamplingMode(tex);
  };
  setEquirectangularSampling(specular_arr->_tex.get());

  maps->_filtenvSpecularMapArray = specular_arr;

  maps->_brdfIntegrationMapGGX    = PBRMaterial::brdfIntegrationMap(ctx, "GGX");
  maps->_brdfIntegrationMapVelvet = PBRMaterial::brdfIntegrationMap(ctx, "GGXVELVET");
  maps->_brdfIntegrationMapGGXRIM = PBRMaterial::brdfIntegrationMap(ctx, "GGXRIM");
  maps->_brdfIntegrationMapBlinn  = PBRMaterial::brdfIntegrationMap(ctx, "BLINN");
  maps->_brdfIntegrationMapPhong  = PBRMaterial::brdfIntegrationMap(ctx, "PHONG");

  return maps;
}

void CommonStuff::updateRadianceMapsGradient(
    radiancemaps_ptr_t maps,
    const std::vector<std::pair<float, fvec3>>& stops,
    Context* ctx) {
  OrkAssert(maps && maps->_filtenvSpecularMapArray);
  OrkAssert(!stops.empty());
  int W = int(maps->_filtenvSpecularMapArray->_width);
  int H = int(maps->_filtenvSpecularMapArray->_height);
  fvec3 avg = sphereAverage(stops);

  // Specular slices: gradient at V, lerped toward sphere-average by the
  // slice's roughness.
  std::vector<image_ptr_t> spec;
  spec.reserve(maps->_specularRoughnessValues.size());
  for (float r : maps->_specularRoughnessValues) {
    spec.push_back(buildRowwiseImage(W, H, [&](float v) {
      fvec3 c = sampleGradientAt(stops, v);
      return c * (1.0f - r) + avg * r;
    }));
  }
  uploadRadianceMapsImages(maps, spec, ctx);
  // The AMBIENT of a gradient env, off the same slice-0 image the reflections
  // read (W4-S9) — the cosine convolution that used to be rasterized into a
  // diffuse map is applied at the read now, from these nine coefficients.
  publishRadianceMapsSH(maps, spec[0]);
}

///////////////////////////////////////////////////////////////////////////////

radiancemap_cache_ptr_t getRadianceMapCache() {
  static radiancemap_cache_ptr_t _instance;
  if (!_instance) {
    _instance = std::make_shared<RadianceMapCache>();
  }
  return _instance;
}

radiancemaps_ptr_t RadianceMapCache::get(const AssetPath& path) {
  // Normalize the key to the EXPANDED filesystem path so a raw
  // "<ork_envmaps2>/x.xir" (the form the v1b eager warm hands us at scene-data
  // wire time) and the already-expanded path the compositor consume builds
  // resolve to the SAME entry — hence the SAME progressively-filled RadianceMaps
  // object. Without this, the two forms key two entries -> two decodes -> the
  // render binds a fresh empty-until-async copy and the first frame is black.
  auto expanded = _expandIfNeeded(path);
  std::lock_guard<std::mutex> lock(_mutex);
  auto key = expanded.toStdString();
  auto it = _cache.find(key);
  if (it != _cache.end()) return it->second;
  auto maps = CommonStuff::requestRadianceMaps(expanded);
  _cache[key] = maps;
  return maps;
}

void RadianceMapCache::clear() {
  std::lock_guard<std::mutex> lock(_mutex);
  _cache.clear();
}

///////////////////////////////////////////////////////////////////////////////

void RadianceMapCache::refilterFromTexture(
    texture_ptr_t source,                             //
    radiancemaps_ptr_t target,                        //
    lev2::Context* ctx,                               //
    std::function<void(datablock_ptr_t)> on_complete, //
    int specular_samples,                             //
    bool steady_state,                                //
    int level_batches,                                //
    int slices_per_frame,                             //
    int mipchain_budget_px,                           //
    bool cold_start) {                                //

  // Fail loud rather than silently skipping a cycle: the caller has already
  // taken the in-flight flag, so a quiet return here would wedge the feed.
  OrkAssertI(source != nullptr, "RadianceMapCache::refilterFromTexture: no source texture");
  OrkAssertI(target != nullptr, "RadianceMapCache::refilterFromTexture: no target radiance maps");
  OrkAssertI(ctx != nullptr, "RadianceMapCache::refilterFromTexture: no render context to schedule on");

  // Same equirect sampling the asset path applies to an .exr/.hdr source: u
  // wraps across the +-180 meridian, v/r clamp so the poles never fold back
  // through the seam. The source is an RTG texture, which is handed the
  // context's base sampler when it is realized — so this push is what actually
  // installs the modes recorded on it.
  source->TexSamplingMode()._texAddrModeS = TextureAddressMode::WRAP;
  source->TexSamplingMode()._texAddrModeT = TextureAddressMode::CLAMP;
  source->TexSamplingMode()._texAddrModeR = TextureAddressMode::CLAMP;
  ctx->TXI()->ApplySamplingMode(source.get());

  // Equirect is STRUCTURAL here (the snapshot is a float equirect by
  // construction), not sniffed from a file extension. The scheduler wraps the
  // task in asyncWorkBegin/End, so the settle/exit gate covers this refilter
  // exactly as it covers the asset-path one — until the caller declares the feed
  // steady, from which point the gate stops treating it as work that finishes.
  auto task = EnvMapProcessor::createRadiancePrefilterMicrotask(
      source, true, target, on_complete, specular_samples, //
      level_batches,
      slices_per_frame,
      mipchain_budget_px,
      cold_start);
  // BEFORE the enqueue (which is what fires asyncWorkBegin under this tag), so a
  // settle/drain census can never sample the recurring feed as one-shot work.
  if (steady_state)
    asyncWorkMarkSteady(task->_name);
  ctx->_microtaskScheduler.enqueue(task);
  logchan_pbrcom->log(
      "RadianceMapCache::refilterFromTexture: enqueued microtask for <%s> %dx%d spec_samples<%d> cold_start<%d>",
      source->_debugName.c_str(),
      source->_width,
      source->_height,
      specular_samples,
      int(cold_start));
}

///////////////////////////////////////////////////////////////////////////////

void RadianceMapCache::refilter(const AssetPath& raw_source_path, radiancemaps_ptr_t target) {
  if (nullptr == target) {
    logchan_pbrcom->log("RadianceMapCache::refilter: null target radiance maps — ignoring");
    return;
  }
  auto* ctx = GfxEnv::mainRenderContext();
  if (nullptr == ctx) {
    logchan_pbrcom->log("RadianceMapCache::refilter: no main render context — cannot schedule refilter");
    OrkAssert(false);
    return;
  }

  // Load the raw env map synchronously and pump its deferred GPU upload so the
  // texture is resident on the render context before the microtask binds it
  // (same pattern as EnvMapProcessor::processToXIRDataBlockAsync). refilter is
  // called on the render thread, so mainSerialQueue's upload lands here.
  auto resolved             = _expandIfNeeded(raw_source_path);
  auto load_req             = std::make_shared<asset::LoadRequest>(resolved);
  load_req->_gpu_load_async = false;
  auto texasset             = asset::AssetManager<TextureAsset>::load(load_req);
  if (!texasset || !texasset->GetTexture()) {
    logchan_pbrcom->log("RadianceMapCache::refilter: could not load raw env map <%s>", resolved.c_str());
    OrkAssert(false);
    return;
  }
  auto rawenvmap = texasset->GetTexture();
  while (opq::mainSerialQueue()->Process()) {}

  auto ext = resolved.getExtension();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  bool is_equirectangular = (ext == "exr" || ext == "hdr");

  if (is_equirectangular) {
    rawenvmap->TexSamplingMode()._texAddrModeS = TextureAddressMode::WRAP;
    rawenvmap->TexSamplingMode()._texAddrModeT = TextureAddressMode::CLAMP;
    rawenvmap->TexSamplingMode()._texAddrModeR = TextureAddressMode::CLAMP;
    ctx->TXI()->ApplySamplingMode(rawenvmap.get());
  }

  // Enqueue on the WINDOW context's scheduler (T11). The scheduler wraps the
  // task in asyncWorkBegin/End for us — the settle/exit gate covers the refilter.
  auto task = EnvMapProcessor::createRadiancePrefilterMicrotask(
      rawenvmap, is_equirectangular, target, nullptr);
  ctx->_microtaskScheduler.enqueue(task);
  logchan_pbrcom->log("RadianceMapCache::refilter: enqueued microtask for <%s> on window scheduler", resolved.c_str());
}

///////////////////////////////////////////////////////////////////////////////

void CommonStuff::refilterSkybox(const AssetPath& raw_source_path) {
  if (nullptr == _radiance_maps) {
    logchan_pbrcom->log("CommonStuff::refilterSkybox: no bound radiance maps to swap into");
    return;
  }
  getRadianceMapCache()->refilter(raw_source_path, _radiance_maps);
}

///////////////////////////////////////////////////////////////////////////////
void CommonStuff::requestAndRefSkyboxTexture(asset::loadrequest_ptr_t load_req) {
  // Route through the shared RadianceMapCache rather than a raw
  // AssetManager::load. AssetManager has no dedup, so a raw load here decodes a
  // FRESH RadianceMaps that is empty until its async decode lands — and in the
  // offscreen player this consume runs at the sim's link rendezvous, AFTER the
  // settle gate has already snapshotted the black sky (#27). The cache lets the
  // v1b eager warm (fired at scene-data wire time, streamed during the settle
  // window) publish the SAME object we bind here, so the first compositor frame
  // is lit. Same bind, same member — only the load is now shared, not re-run.
  _radiance_maps = getRadianceMapCache()->get(load_req->_asset_path);
}

///////////////////////////////////////////////////////////////////////////////
// BAKED IBL COLD START — see the header for why a frame count cannot stand in
// for this. Runs at the top of the forward prologue, before anything reads the
// env samplers.
//
// The wait is on the LOAD REQUEST's pending count, which the XIR chain releases
// at exactly one place per outcome (both decode error returns and the terminal
// render-thread swap), so it means "finished", not "succeeded" — isPublished()
// is then asked separately, and a finished-but-empty set is a loud failure
// rather than a silently black scene.
//
// Pumping ctx's deferred queue is what lets the wait resolve: the loader's fence
// poll lands the publish there, and this thread is the only one that drains it.
// The loader thread makes its own progress independently, so this blocks on
// nothing that blocks on us. The ceiling exists because a wedged loader must
// still yield a frame (slow, wrong lighting, LOUD) instead of a hang.
///////////////////////////////////////////////////////////////////////////////

void CommonStuff::drainPendingRadianceMapLoad(Context* ctx) {
  if ((nullptr == ctx) or (nullptr == _radiance_maps))
    return;
  auto req = _radiance_maps->_loadRequest;
  if (nullptr == req) // no baked skybox requested (procedural-only or unlit scene)
    return;
  if (_radiance_maps->isPublished())
    return;
  if (_baked_cold_start_failed) // reported once; the set never publishes now
    return;

  // wall-clock ceiling on the drain. Data-defined: an 8K HDRI on a loaded box
  // legitimately takes seconds, and the number that must never be hit is a
  // deployment property, not an appearance one.
  static const double kCeilingSecs = []() -> double {
    if (const char* v = std::getenv("ORKID_IBL_COLDSTART_CEILING_SECS"))
      return atof(v);
    return 30.0;
  }();

  Timer timer;
  timer.Start();
  while ((req->_partial_load_counter.load() > 0) and (timer.SecsSinceStart() < kCeilingSecs)) {
    ctx->processDeferredOps(); // the loader's fence poll publishes THROUGH here
    std::this_thread::sleep_for(std::chrono::microseconds(200));
  }
  // the publish op may have been enqueued between the last drain and the
  // counter reaching zero — one more pass, so a successful load never reports.
  ctx->processDeferredOps();

  // printf, not a log channel: this reports a first-frame stall the viewer sees,
  // and its failure half reports a scene that will render black forever. Neither
  // may depend on a channel being switched on.
  double waited = timer.SecsSinceStart();
  if (_radiance_maps->isPublished()) {
    printf(
        "[IBL-COLDSTART] skybox <%s> was still unpublished at frame<%d> (the first lit frame) - drained "
        "in-frame, waited %.3f s (the frame is LIT, not black)\n",
        req->_asset_path.c_str(),
        ctx->GetTargetFrame(),
        waited);
    fflush(stdout);
    return;
  }

  _baked_cold_start_failed = true;
  printf(
      "[IBL-COLDSTART] FAILED: skybox <%s> did not publish after %.3f s (load status <%llx>) - this scene "
      "renders with a BLACK environment. The XIR decode/upload chain terminated without handing back "
      "radiance maps, or the loader thread stopped pumping before its fence poll landed.\n",
      req->_asset_path.c_str(),
      waited,
      (unsigned long long)req->_assetStatus);
  fflush(stdout);
}

///////////////////////////////////////////////////////////////////////////////
lev2::texture_ptr_t CommonStuff::ssaoKernel(lev2::Context* ctx,int noise_seed){

  int seed = 0;//noise_seed%97;
  int num_samples = (_ssaoNumSamples<8) ? 8 : _ssaoNumSamples;
  uint64_t key = uint64_t(seed)<<32 | uint64_t(num_samples);

  auto it = _ssaoKernels.find(key);
  if( it == _ssaoKernels.end() ){
    // make new kernel for size and cache
    std::vector<fvec3> ssaoNoise;
    math::FRANDOMGEN R(seed);
    for (unsigned int i = 0; i < num_samples; i++) {
      glm::vec3 noise(
        R.rangedf(-1,1), 
        R.rangedf(-1,1), 
        0.0f); 
      ssaoNoise.push_back(noise);
    }     
    auto texture = std::make_shared<lev2::Texture>();
    auto txi = ctx->TXI();
    TextureInitData tid;
    tid._w = num_samples;
    tid._h = 1;
    tid._d = 1;
    tid._src_format = EBufferFormat::RGB32F;
    tid._dst_format = EBufferFormat::RGB32F;
    tid._data = ssaoNoise.data();
    tid._truncation_length = ssaoNoise.size() * sizeof(fvec3);
    txi->initTextureFromData(texture.get(), tid);
    _ssaoKernels[key] = texture;
    return texture;
  }
  return it->second;

}
///////////////////////////////////////////////////////////////////////////////
struct NoiseData {
    std::vector<fvec3> _ssaoNoise;
    texture_ptr_t _texture;
};
struct NoiseDataSet{
    std::map<uint64_t, NoiseData> _ssaoKernels;
};
using noisedataset_ptr_t = std::shared_ptr<NoiseDataSet>;
///////////////////////////////////////////////////////////////////////////////
lev2::texture_ptr_t CommonStuff::ssaoScrNoise(lev2::Context* ctx, int noise_seed, int w, int h){

  int seed = noise_seed%KNUMSSAONOISEFRAMES;

  uint64_t key = uint64_t(seed)<<32 | uint64_t(w)<<16 | uint64_t(h);

  auto it = _ssaoKernels.find(key);
  if( it == _ssaoKernels.end() ){
    printf( "spin up ssao screen noise for key<%llu>\n", (ull)key);
    // make new kernel for size and cache
    std::vector<fvec3> ssaoNoise;
    int numsamples = w*h;
    math::FRANDOMGEN R(seed);
    for (unsigned int i = 0; i < numsamples; i++) {
      int x = i % w;
      int y = i / w;
      
      glm::vec3 noise(
        R.rangedf(-1,1), 
        R.rangedf(-1,1), 
        R.rangedf(-1,1));
        
      ssaoNoise.push_back(noise);
    }     
    auto texture = std::make_shared<lev2::Texture>();
    auto txi = ctx->TXI();
    TextureInitData tid;
    tid._w = w;
    tid._h = h;
    tid._d = 1;
    tid._src_format = EBufferFormat::RGB32F;
    tid._dst_format = EBufferFormat::RGB32F;
    tid._data = ssaoNoise.data();
    tid._truncation_length = ssaoNoise.size() * sizeof(fvec3);
    txi->initTextureFromData(texture.get(), tid);
    _ssaoKernels[key] = texture;
    return texture;
  }
  return it->second;

}
///////////////////////////////////////////////////////////////////////////////
void CommonStuff::_writeEnvTexture(asset::asset_ptr_t const& tex) {
  assignEnvTexture(tex);
}
///////////////////////////////////////////////////////////////////////////////
// SKYLIGHT lane B slice B3 — the IBL source branch. PROCEDURAL scenes light
// from the sky's own refiltered snapshot, but only from the moment the FIRST
// procedural cycle has published: through the warm-up window (and through every
// baked scene, L6) this keeps returning the baked maps, so no frame ever renders
// against a black or half-filled IBL.
radiancemaps_ptr_t CommonStuff::activeRadianceMaps() const {
  if ((_sky_source == SkySource::PROCEDURAL) and _sky_ibl and _sky_ibl->proceduralMapsReady())
    return _sky_ibl->_maps;
  return _radiance_maps;
}
///////////////////////////////////////////////////////////////////////////////
lev2::texturearray_ptr_t CommonStuff::envSpecularTexture() const {
  return activeRadianceMaps()->_filtenvSpecularMapArray;
}
///////////////////////////////////////////////////////////////////////////////
// The crossfade's aliasing rule, in ONE place. A fade only exists between two
// PROCEDURAL sets that this frame is actually lit by: a baked scene (or the
// warm-up window, or a scene that just switched back to baked mid-fade) gets
// null here, which sends the prev accessors straight back to the current maps
// and pins the weight at 1.0 - byte-identical to a build without the fade.
radiancemaps_ptr_t CommonStuff::_crossfadePrevMaps() const {
  if ((_sky_source != SkySource::PROCEDURAL) or (nullptr == _sky_ibl))
    return nullptr;
  if (not _sky_ibl->proceduralMapsReady())
    return nullptr;
  if (_sky_ibl->_fade_frames_remaining.load() <= 0)
    return nullptr;
  return _sky_ibl->_prevMaps;
}
///////////////////////////////////////////////////////////////////////////////
lev2::texturearray_ptr_t CommonStuff::envSpecularTexturePrev() const {
  auto prev = _crossfadePrevMaps();
  if (prev and prev->_filtenvSpecularMapArray)
    return prev->_filtenvSpecularMapArray;
  return envSpecularTexture();
}
///////////////////////////////////////////////////////////////////////////////
// The pre-scale's decode side. Same branch as activeRadianceMaps(), so through
// the warm-up window (and in every baked scene) this is exactly 1.0 and the
// shader multiply is a no-op.
float CommonStuff::envCaptureScaleInv() const {
  if ((_sky_source != SkySource::PROCEDURAL) or (nullptr == _sky_ibl) or (nullptr == _atmosphere))
    return 1.0f;
  if (not _sky_ibl->proceduralMapsReady())
    return 1.0f;
  float scale = _atmosphere->_iblCaptureScale;
  return (scale > 0.0f) ? (1.0f / scale) : 1.0f;
}
///////////////////////////////////////////////////////////////////////////////
// AVAILABLE LIGHT. The measurement rides the maps the frame is actually lit by,
// so it decodes through the SAME capture pre-scale those maps were written at —
// reading it against any other set would report a luminance in the wrong units.
// No wait, no fence, no lock: the float was filled on the render thread by the
// publish that swapped these maps in, out of the CPU capture buffer the
// packaging already held.
float CommonStuff::availableLightLuminance() const {
  auto maps = activeRadianceMaps();
  if (nullptr == maps)
    return -1.0f;
  if (maps->_measuredLuminance < 0.0f)
    return -1.0f;
  return maps->_measuredLuminance * envCaptureScaleInv();
}
///////////////////////////////////////////////////////////////////////////////
float CommonStuff::skySunElevationSin() const {
  // The IBL cycle's own record of where the sun was, rather than a re-derivation:
  // the seed exists to describe the sky that HAS NOT published yet, and this is
  // the last thing that looked at it. Defaults to overhead (day) on a scene that
  // has never snapped.
  if (nullptr == _sky_ibl)
    return 1.0f;
  return _sky_ibl->_last_snapshot_dir_to_sun.y;
}
///////////////////////////////////////////////////////////////////////////////
// SKY SH PROBE — same gate as activeRadianceMaps()/envCaptureScaleInv(), so the
// probe can never be served over baked maps or through the warm-up window. The
// crossfade is resolved HERE rather than in the shader: the weight is one scalar
// per frame, and blending nine coefficients on the CPU costs a fragment nothing
// and halves what the UBO has to carry.
bool CommonStuff::envSHCoeffs(fvec4 out[9]) const {
  bool have_probe = (_sky_source == SkySource::PROCEDURAL) //
                and (nullptr != _sky_ibl)                  //
                and _sky_ibl->_sh_valid                    //
                and _sky_ibl->proceduralMapsReady();
  if (have_probe) {
    float w = envCrossfadeWeight();
    for (int i = 0; i < 9; i++)
      out[i] = _sky_ibl->_sh_prev[i] + (_sky_ibl->_sh_cur[i] - _sky_ibl->_sh_prev[i]) * w;
    return true; // the probe already carries decoded radiance
  }
  // THE AUTHORED SKY (W4-S9). The bound map set's own projection — a baked
  // scene's whole ambient, and the procedural warm-up window's. Its
  // coefficients are in the MAP's units, so the capture pre-scale is divided
  // out here, at the same seam and by the same factor a shader read of those
  // maps decodes with.
  auto maps = activeRadianceMaps();
  if ((nullptr == maps) or (not maps->_shValid))
    return false;
  float decode = envCaptureScaleInv();
  for (int i = 0; i < 9; i++)
    out[i] = maps->_shCoeffs[i] * decode;
  return true;
}
///////////////////////////////////////////////////////////////////////////////
float CommonStuff::envCrossfadeWeight() const {
  auto prev = _crossfadePrevMaps();
  if (nullptr == prev)
    return 1.0f;
  return _sky_ibl->fadeWeight();
}
///////////////////////////////////////////////////////////////////////////////
void CommonStuff::_readEnvTexture(asset::asset_ptr_t& tex) const {
  tex = _environmentTextureAsset;
}
///////////////////////////////////////////////////////////////////////////////
void CommonStuff::setEnvTexturePath(file::Path path) {
  auto mtl_load_req = std::make_shared<asset::LoadRequest>(path);
  auto envl_asset   = asset::AssetManager<TextureAsset>::load(mtl_load_req);
  OrkAssert(false);
  // TODO - inject asset postload ops ()
}
///////////////////////////////////////////////////////////////////////////////
void CommonStuff::describeX(class_t* c) {
  using namespace asset;
  c->directProperty("ClearColor", &CommonStuff::_clearcolor);
  c->directProperty("AmbientLevel", &CommonStuff::_ambientLevel);
  c->floatProperty("EnvironmentIntensity", float_range{0, 100}, &CommonStuff::_environmentIntensity);
  c->floatProperty("EnvironmentMipBias", float_range{0, 12}, &CommonStuff::_environmentMipBias);
  c->floatProperty("EnvironmentMipScale", float_range{0, 100}, &CommonStuff::_environmentMipScale);
  c->floatProperty("DiffuseLevel", float_range{0, 10}, &CommonStuff::_diffuseLevel);
  c->floatProperty("SpecularLevel", float_range{0, 10}, &CommonStuff::_specularLevel);
  c->floatProperty("SkyboxLevel", float_range{0, 10}, &CommonStuff::_skyboxLevel);
  c->floatProperty("DepthFogDistance", float_range{0.1, 5000}, &CommonStuff::_depthFogDistance);
  c->floatProperty("DepthFogPower", float_range{0.01, 100.0}, &CommonStuff::_depthFogPower);

  /*c->accessorProperty(
       "EnvironmentTexture", //
       &CommonStuff::_readEnvTexture,
       &CommonStuff::_writeEnvTexture)
      ->annotate<ConstString>("editor.class", "ged.factory.assetlist")
      ->annotate<ConstString>("editor.assettype", "lev2tex")
      ->annotate<ConstString>("editor.assetclass", "lev2tex")
      ->annotate<asset::vars_gen_t>(
          "asset.deserialize.vargen", //
          [](ork::object_ptr_t obj) -> asset::vars_ptr_t {
            auto _this = std::dynamic_pointer_cast<CommonStuff>(obj);
            OrkAssert(_this);
            OrkAssert(false);
            return _RadianceVars();
          });*/
}
void CommonStuff::onGpuInit(Context* ctx) {
  if(not _needsGpuInit){
    return;
  }
  _needsGpuInit = false;
  auto TXI = ctx->TXI();
  _texBlack = TXI->createColorTextureV3(fvec3(0, 0, 0), 64, 64);
  _texWhite = TXI->createColorTextureV3(fvec3(1, 1, 1), 64, 64);
  _texCubeBlack = TXI->createColorCubeTexture(fvec4(0, 0, 0, 1), 64,64);
  _texCubeWhite = TXI->createColorCubeTexture(fvec4(1, 1, 1, 1), 64,64);
  _texBlackArray = TXI->createColorTextureV3Array(fvec3(0, 0, 0), 64, 64, 32);
  _texWhiteLightMapArray = TXI->createColorTextureV3Array(fvec3(1, 1, 1), 64, 64, 32);
  _texBlackLightMapArray = TXI->createColorTextureV3Array(fvec3(0,0,0), 64, 64, 32);
  _texBlack->_debugName = "black";
  _texWhite->_debugName = "white";
  _texCubeBlack->_debugName = "black_cube";
  _texCubeWhite->_debugName = "white_cube";
  _texBlackArray->_debugName = "black_array";
  _texWhiteLightMapArray->_debugName = "white_lightmap_array";
  _texBlackLightMapArray->_debugName = "white_lightmap_array";
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::pbr
