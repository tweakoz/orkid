#pragma once 

#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/compositormaterial.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/kernel/varmap.inl>
#include <ork/kernel/datacache.h>
#include <ork/kernel/timer.h>

// scenegraph::Scene forward decl comes via lev2_types.h (pulled in
// transitively through rtgroup.h included above).

namespace ork::lev2::pbr {

///////////////////////////////////////////////////////////////////////////////

template <typename T> inline bool doRangesOverlap(T amin, T amax, T bmin, T bmax) {
  return std::max(amin, bmin) <= std::min(amax, bmax);
}

///////////////////////////////////////////////////////////////////////////////

struct PointLight {

  PointLight() {
  }
  fvec3 _pos;
  fvec3 _dst;
  fvec3 _color;
  float _radius;
  int _counter   = 0;
  float dist2cam = 0;
  AABox _aabox;
  fvec3 _aamin, _aamax;
  int _minX, _minY;
  int _maxX, _maxY;
  float _minZ, _maxZ;

  void next() {
    float x  = float((rand() & 0x3fff) - 0x2000);
    float z  = float((rand() & 0x3fff) - 0x2000);
    float y  = float((rand() & 0x1fff) - 0x1000);
    _dst     = fvec3(x, y, z);
    _counter = 256 + rand() & 0xff;
  }
};

///////////////////////////////////////////////////////////////////////////////

struct RadianceMaps {

  texturearray_ptr_t _filtenvSpecularMapArray;  // New: texture array with roughness slices
  texture_ptr_t _brdfIntegrationMapGGX;
  texture_ptr_t _brdfIntegrationMapVelvet;
  texture_ptr_t _brdfIntegrationMapGGXRIM;
  texture_ptr_t _brdfIntegrationMapBlinn;
  texture_ptr_t _brdfIntegrationMapPhong;
  std::vector<float> _specularRoughnessValues;  // Roughness value for each array slice
  int _numRoughnessLevels = 0;  // Number of roughness levels in array
  asset::loadrequest_ptr_t _loadRequest;

  // A RadianceMaps handed out by the XIR loader is EMPTY until its publish
  // lands — the samplers/coefficients are the state. POST-S9: the prefiltered
  // diffuse equirect no longer exists; ambient completeness is the SH
  // projection, so "published" = specular chain present AND (SH valid). Nothing may assume a publish
  // landed just because the object exists (the warm-start drain waits on this).
  bool isPublished() const {
    return (nullptr != _filtenvSpecularMapArray) and _shValid;
  }

  // THE DIFFUSE AMBIENT OF THIS MAP SET (W4-S9), as nine L2 coefficients
  // projected from specular roughness level 0 — the prefilter chain's identity
  // level, i.e. the authored radiance itself. There is ONE ambient pipeline
  // now: a baked scene's sky is projected at LOAD and reconstructed per
  // fragment exactly as the procedural sky's probe is, and the prefiltered
  // diffuse equirect that used to stand in for it does not exist any more.
  //
  // Units are the MAP's: a procedural set's coefficients carry the capture
  // pre-scale, and the one decode seam is at the read (envSHCoeffs).
  //
  // _shValid false = nothing projectable has published into this set yet. A
  // consumer must not read that as a black sky.
  fvec4 _shCoeffs[9] = {};
  bool _shValid      = false;

  // AVAILABLE LIGHT, measured — the sphere-mean radiance of this map set,
  // which is the L0 coefficient above times Y00 (the mean of a field IS its
  // zeroth moment, so this needs no pass, no readback and no second sampling
  // of anything). Rec.709 luminance of that mean, in the same undecoded units
  // as the coefficients it comes from; the pre-scale is divided out at the
  // read, exactly as the shaders decode.

  //
  // Negative = nothing has published into this set yet; a consumer must not
  // read it as darkness (see CommonStuff::availableLightLuminance).
  float _measuredLuminance = -1.0f;


};

///////////////////////////////////////////////////////////////////////////////
// THE AMBIENT PROJECTION for one map set (W4-S9), in ONE place because there
// are three publish sites (the .xir load, the prefilter publish, the procedural
// gradient) and a set whose coefficients came from a different convention than
// its neighbour's would light two halves of a scene differently.
//
// Input is specular roughness level 0 — the prefilter chain's identity level,
// which is the source equirect's radiance at the array's extent, in the one
// bearing probe_sh.h documents. Output is what RadianceMaps carries: the nine
// coefficients and the sphere-mean luminance L0 implies.
//
// projectRadianceSH is pure and thread-safe: the .xir path runs it on the
// loader's decode thread, and only the tiny assignment lands on the render
// thread with the rest of the publish.
//
// assignRadianceSH publishes the COEFFICIENTS ONLY. The available-light measure
// is deliberately NOT published with them: S1's adaptation reads a NEGATIVE
// _measuredLuminance as "nothing has been measured yet, seed instead", and that
// is exactly the state a scene with no refilter cycle was in before this slice
// existed. Only the prefilter publish — the one thing that actually measures a
// sky — writes it (see radiancemaps_processor publishStagedToTarget).
///////////////////////////////////////////////////////////////////////////////

struct RadianceSH {
  fvec4 _coeffs[9]  = {};
  float _luminance  = -1.0f;
  bool _valid       = false;
};

RadianceSH projectRadianceSH(const Image& equirect_level0);
void assignRadianceSH(radiancemaps_ptr_t maps, const RadianceSH& sh);
// project + assign, for a caller already on the thread that owns `maps`
void publishRadianceMapsSH(radiancemaps_ptr_t maps, image_ptr_t equirect_level0);

///////////////////////////////////////////////////////////////////////////////
// SKYLIGHT L6 — the sky is a PER-SCENE choice, not a migration: baked equirect
// envmaps stay fully supported with no parity window.
//
// The byte-identity half of that law is RETIRED FOR AMBIENT (owner ruling 10,
// W4-S9): a baked scene's diffuse ambient now comes from the SAME
// cosine-convolved SH reconstruction the procedural sky feeds, so a baked
// render SHIFTS to the accurate diffuse rather than reproducing the
// prefiltered GGX-lobe map it used to sample. Everything else about a baked
// scene — the specular chain, the skybox, the sun — is untouched.
///////////////////////////////////////////////////////////////////////////////

enum class SkySource {
  BAKED = 0,  // equirect envmap slice-0 through FWD_SKYBOX_MO (default)
  PROCEDURAL, // Hillaire sky-view LUT + analytic sun disc through FWD_SKYBOX_PROC
};

///////////////////////////////////////////////////////////////////////////////
// DIRECT DIFFUSE BRDF — which lobe the ANALYTIC lights (point, spot cookie,
// sun) shade their diffuse with. OREN_NAYAR is the default: rough dielectrics
// — rock, bark, dirt, cloth — flatten toward their albedo at grazing sun
// instead of falling off on a cosine, which is what Lambert gets wrong on
// exactly the surfaces a landscape is made of.
//
// The AMBIENT/IBL diffuse is NOT selected by this and stays Lambert: it is a
// cosine-convolved SH irradiance, and matching it would mean re-convolving the
// probe, not scaling the read (see the comment at the env site in fwdtools.i2).
//
// TWO CURRENCIES, one registry. The crc of the enumerant's NAME is the identity
// the scene DSL, the pyext and any saved state traffic in — same machinery as
// CommonStuff::_brdftype / setBRDF(tokens.GGX). The enum's VALUE is a small
// ordinal, and that ordinal is what the shader UBO carries, so no crc literal
// is ever baked into shader text (lib_brdf::diffuseBRDF).
///////////////////////////////////////////////////////////////////////////////

enum class DiffuseBrdfModel : int {
  LAMBERT    = 0, // albedo/pi — what the engine shaded with before the selector
  OREN_NAYAR = 1, // qualitative A/B microfacet retro-reflection (DEFAULT)
  BURLEY     = 2, // Disney: Schlick grazing retro-reflection, roughness-driven f90
};

// crc(name) -> model. False for an unknown crc, out_ untouched — every caller
// refuses out loud and names the valid set rather than shading with a guess.
bool diffuseBrdfModelFromCrc(uint64_t crc, DiffuseBrdfModel& out_);
// Same registry through the spelling: name -> crc -> model. The name IS the
// token spelling ("OREN_NAYAR"), so a string in an .ecs and a tokens.X in a
// scene resolve identically.
bool diffuseBrdfModelFromName(const std::string& name, DiffuseBrdfModel& out_);
const char* diffuseBrdfModelName(DiffuseBrdfModel model);
std::string diffuseBrdfModelValidSet(); // "LAMBERT, OREN_NAYAR, BURLEY"

///////////////////////////////////////////////////////////////////////////////
// SKYLIGHT slice B3 — cycle state for the procedural IBL feed (spec §2 lagged
// tier). ONE snapshot texture and ONE job at a time: the SNAPSHOT LAW says the
// sliced prefilter's source must stay immutable for the job's whole multi-frame
// life, so a cycle may never begin while another is in flight. That hard gate
// IS the compliance mechanism — GpuMicrotaskScheduler::enqueue does not dedup,
// and two jobs sharing one snapshot would tear it.
//
// The refilter publishes into _maps, a SECOND RadianceMaps kept apart from the
// baked set: the L6 per-scene switch then costs nothing in either direction,
// and the env accessors can keep serving the baked maps until the first
// procedural cycle has actually published (never a half-filled object).
//
// THREADING: every field is written from the context-owner (render) thread —
// the prologue starts cycles there, and the microtask's completion callback
// runs there too (the scheduler drains inside Context::beginFrame). The atomics
// are for READERS elsewhere (python gates, HUD), not for writer contention.
///////////////////////////////////////////////////////////////////////////////

struct SkyIblState {

  void beginCycle();    // fail-loud snapshot-law guard, then marks in-flight
  // from the microtask's completion callback. fade_max_secs is the WALL-CLOCK
  // bound on the window fade_frames sizes; 0 = frames only.
  void completeCycle(Context* ctx, int fade_frames, float fade_max_secs);
  void tickCrossfade(Context* ctx); // once per frame, from the forward prologue

  bool proceduralMapsReady() const {
    return _ready.load() and (_maps != nullptr);
  }

  // 1.0 whenever no fade is running — see CommonStuff::envCrossfadeWeight for
  // the accessor the render path actually binds.
  //
  // TWO BOUNDS, whichever finishes first: the frame window (frames remaining of
  // the sized total) and the WALL-CLOCK budget (SkyAtmosphereData
  // ::_iblCrossfadeMaxSecs, clocked from the publish). The weight is the larger
  // progress of the two, so a fast frame loop rides the clock and a slow one
  // rides the frames — a fade is a DURATION either way, never a frame count.
  float fadeWeight() const;

  // CONTINUOUS CHAINING support. fadeSettled() is the "weight is pinned at 1"
  // query the trigger gates a chained cycle on: starting one while a fade is
  // live would need a third map set blended in.
  //
  // autoFadeFrames sizes the next window from the MEASURED length of the last
  // cycle (frames between its beginCycle and its publish, counted by
  // tickCrossfade — the one call that runs on every frame). It is deliberately
  // NOT the publish-to-publish interval: a chained cycle cannot start until the
  // fade settles, so a window sized to that interval would include the previous
  // window and every cycle would stretch the next one until it pinned at the
  // clamp. Cycle length is the stable measure, and it is the span the fade has
  // to cover for the next publish to land as the current one finishes fading.
  // `fallback` is what an unmeasured first cycle uses; a fallback of 0 stays 0
  // (an explicit fade-disable is a user decision the auto-size must not
  // override).
  bool fadeSettled() const {
    return _fade_frames_remaining.load() <= 0;
  }
  int autoFadeFrames(int fallback) const;

  // CHAIN CADENCE CEILING (SkyAtmosphereData::_iblChainMaxHz). Wall-clock, not
  // frames: the ceiling has to mean the same thing whatever the frame rate is,
  // which is the entire point of capping a feed whose natural rate is "as often
  // as the loop allows". max_hz <= 0 = uncapped (the pre-knob behavior), and a
  // feed that has never run a cycle is never held back.
  bool chainCadenceElapsed(float max_hz) const;

  // DECLARED CADENCE (SkyAtmosphereData::_iblSnapshotInterval). Same start-to-start
  // clock as chainCadenceElapsed, expressed as a PERIOD instead of a rate because
  // that is the unit a scene author declares in. Sibling, not a replacement: the
  // ceiling paces a sun-driven trigger, this one IS the trigger.
  //
  // It is NOT the whole trigger: the caller conjoins fadeSettled(), which floors
  // the achieved cadence at cadenceFloorSecs() below. A declared rate that
  // outruns the floor is not achievable at that snapshot config, and the caller
  // says so out loud rather than clamping in silence.
  bool snapshotIntervalElapsed(float secs) const;

  // The MEASURED floor on any declared cadence, in seconds: cycle start to fade
  // SETTLED, off the same start-to-start clock, so it needs no frame-period
  // estimate and no constant — it is whatever this snapshot extent, sample count
  // and machine actually delivered. Written at fade end, and at publish (a
  // fade-disabled config settles there), so on a first detection it may still
  // hold the publish-only value and the true floor is >= what it reports. 0 =
  // nothing measured yet.
  double cadenceFloorSecs() const {
    return _last_settle_secs.load();
  }

  void _retireMaps(Context* ctx, radiancemaps_ptr_t maps);

  radiancemaps_ptr_t _maps;    // procedural target; created at the first cycle
  rtgroup_ptr_t _snapshot_rtg; // the equirect snapshot the running job consumes
  std::atomic<bool> _inflight           = {false};
  std::atomic<bool> _ready              = {false}; // a cycle has PUBLISHED at least once
  std::atomic<uint64_t> _generation     = {0};     // completed cycles
  std::atomic<uint64_t> _cycles_started = {0};
  fvec3 _last_snapshot_dir_to_sun     = fvec3(0, 1, 0);
  uint64_t _last_snapshot_medium_hash = 0;
  // the PRESENTATION half of the same stamp (SkyAtmosphereData
  // ::hazePresentationHash): the snapshot bakes the artist haze layer into the
  // sky it captures, so a haze edit has to start a cycle even though it is not a
  // medium edit and must never re-bake a LUT.
  uint64_t _last_snapshot_haze_hash = 0;
  bool _ever_snapped                  = false;

  // CROSSFADE. A publish replaces every filtered map at once, which lit
  // geometry sees as a single-frame ambient/specular pop. _prevMaps holds the
  // OUTGOING set alive across a fade window so the shaders can blend out of it;
  // _pendingPrevMaps is that set snapshotted at cycle start (a copy: the
  // publish mutates _maps in place). The publish's own delayed-destroy keeps
  // the textures alive for its MAX_FRAMES_IN_FLIGHT window; this reference
  // extends that to the end of the fade, and the retire at fade end restores
  // the same in-flight delay before the last reference drops.
  radiancemaps_ptr_t _prevMaps;
  radiancemaps_ptr_t _pendingPrevMaps;
  std::atomic<int> _fade_frames_remaining = {0};
  std::atomic<int> _fade_frames_total     = {0};

  // the fade's wall-clock bound: the budget this fade was started with (0 = no
  // clock bound) and the clock it is measured against, restarted at the publish.
  // Both are what makes the window a duration rather than a frame count.
  std::atomic<float> _fade_max_secs = {0.0f};
  Timer _fade_timer;

  // CADENCE. Frames elapsed inside the running cycle (reset at beginCycle,
  // ticked by tickCrossfade) and the span the last completed cycle took. That
  // span is the cycle cost as the frame loop actually experienced it — snapshot,
  // every prefilter slice, and the upload wait — which is the window the next
  // fade has to cover.
  std::atomic<int> _frames_this_cycle = {0};
  std::atomic<int> _cycle_frames      = {0}; // 0 = not measured yet

  // cycle start -> fade settled, in SECONDS (cadenceFloorSecs above reads it).
  std::atomic<double> _last_settle_secs = {0.0};

  // one-shot latch for the below-the-floor complaint: the condition recurs every
  // frame of every fade, and a per-frame log line is a spam channel, not a
  // warning. Not reset — the first detection is the report.
  bool _interval_floor_warned = false;

  // start-to-start clock for the chain cadence ceiling; restarted by beginCycle.
  Timer _chain_timer;

  ///////////////////////////////////////////////////////////////////////////
  // THE SKY SH PROBE (W4-S8). ONE probe, fed by the equirect snapshot each
  // cycle freezes, projected onto the L2 basis by ProbeSHProjector and read
  // back here. The DIFFUSE ambient of every forward fragment is reconstructed
  // from these nine coefficients — the prefiltered equirect diffuse map no
  // longer reaches a shaded fragment on the procedural path.
  //
  // The coefficients carry DECODED radiance: the capture pre-scale is divided
  // out inside the projection kernel, once, so nothing downstream decodes
  // again. Units are the SH radiance integral (see probe_sh.h) — the cosine
  // convolution is applied at read time in the shader.
  //
  // CADENCE: exactly the maps' own. _sh_project_pending is raised at cycle
  // start (the snapshot has just been RENDERED and cannot be sampled by a
  // dispatch phase in the same frame), serviced on a later prologue into
  // _sh_staged, and promoted into _sh_cur at the frame the cycle PUBLISHES —
  // the same instant the prefiltered maps swap and the crossfade opens, so one
  // EnvBlendWeight covers both.
  ///////////////////////////////////////////////////////////////////////////
  bool _sh_project_pending = false;
  int _sh_projected_frame  = -1; // Context frame the pending snapshot was rendered on
  fvec4 _sh_staged[9]      = {};
  bool _sh_staged_valid    = false;
  fvec4 _sh_cur[9]         = {};
  fvec4 _sh_prev[9]        = {};
  bool _sh_valid           = false; // a projection has been PROMOTED at least once
  uint64_t _sh_published_gen = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct CommonStuff : public ork::Object {
  DeclareConcreteX(CommonStuff, ork::Object);

  CommonStuff();

  void _readEnvTexture(asset::asset_ptr_t& tex) const;
  void _writeEnvTexture(asset::asset_ptr_t const& tex);
  void setEnvTexturePath(file::Path path);

  void assignEnvTexture(asset::asset_ptr_t texasset);
  // Which RadianceMaps the frame's IBL actually comes from: the procedural feed
  // once (and only once) it has published, the baked set otherwise. Every read
  // of a maps FIELD that pairs with the bound env textures (roughness-level
  // count, brdf integration maps) must go through here, or the sampler and its
  // metadata describe two different maps.
  radiancemaps_ptr_t activeRadianceMaps() const;
  lev2::texturearray_ptr_t envSpecularTexture() const;
  // The OUTGOING procedural set during a refilter crossfade, and the blend
  // weight that fades it out (1.0 = the new set only). With no fade running
  // this ALIASES the accessor above and the weight is 1.0 — the sampler set is
  // shared by every PBR consumer, so the prev slot must never be left null or
  // stale, and every baked scene must keep binding exactly what it bound
  // before the crossfade existed.
  lev2::texturearray_ptr_t envSpecularTexturePrev() const;
  float envCrossfadeWeight() const;
  // The DECODE half of the procedural capture pre-scale: 1/_iblCaptureScale
  // while the bound maps are the procedural set, exactly 1.0 otherwise. Gated
  // on the SAME predicate activeRadianceMaps() branches on, so the factor and
  // the textures can never describe different captures. Both maps in a
  // crossfade are procedural (a baked -> procedural handover snaps rather than
  // fades), so one scalar covers the pair.
  float envCaptureScaleInv() const;
  // AVAILABLE LIGHT for the frame, in DECODED radiance units: the active
  // maps' measured floor-level mean (RadianceMaps::_measuredLuminance) with
  // the capture pre-scale divided out, exactly as a shader read of those maps
  // would decode it.
  //
  // NEGATIVE = nothing has published yet and there is nothing to measure. A
  // consumer must not read that as darkness; it seeds itself instead, off
  // skySunElevationSin() below, for the handful of warm-up frames it lasts.
  float availableLightLuminance() const;
  // sin(sun elevation) as the atmosphere currently has it, +1 (overhead) when
  // the scene declares no atmosphere. THE ELEVATION SEED, and nothing else:
  // it exists so a consumer can pick a starting point before the first
  // measurement lands, and no per-frame look may be a function of it.
  float skySunElevationSin() const;
  // THE DIFFUSE AMBIENT SOURCE — the ONE ambient pipeline (W4-S8, unified by
  // W4-S9). Writes the nine L2 coefficients the fragment shader reconstructs
  // irradiance from, in DECODED radiance, from whichever sky this frame
  // actually has:
  //   * the procedural sky's own probe once it has promoted a projection —
  //     already crossfaded between the outgoing and incoming cycles by the
  //     same weight the maps fade on;
  //   * otherwise the ACTIVE map set's own coefficients (RadianceMaps::
  //     _shCoeffs), projected from its authored equirect at load. That is the
  //     baked scene's ambient, and it is also what covers the procedural
  //     warm-up window — the maps bound in that window carry a projection of
  //     exactly the sky they were filtered from.
  // False = there is no sky of either kind yet (nothing has published into the
  // bound maps). A caller must fail loudly rather than shade against zero.
  bool envSHCoeffs(fvec4 out[9]) const;
  // null unless a fade is running against maps this frame is actually lit by
  // (the ONE place the aliasing rule above is decided).
  radiancemaps_ptr_t _crossfadePrevMaps() const;

  lev2::texture_ptr_t ssaoKernel(lev2::Context* ctx, int seed);
  lev2::texture_ptr_t ssaoScrNoise(lev2::Context* ctx, int seed, int w, int h);

  float environmentIntensity() const {
    return _environmentIntensity;
  }
  float environmentMipBias() const {
    return _environmentMipBias;
  }
  float environmentMipScale() const {
    return _environmentMipScale;
  }
  float diffuseLevel() const {
    return _diffuseLevel;
  }
  float specularLevel() const {
    return _specularLevel;
  }
  fvec3 ambientLevel() const {
    return _ambientLevel;
  }
  float skyboxLevel() const {
    return _skyboxLevel;
  }
  float depthFogDistance() const {
    return _depthFogDistance;
  }
  float depthFogPower() const {
    return _depthFogPower;
  }

  void requestAndRefSkyboxTexture(asset::loadrequest_ptr_t load_req);
  // BAKED IBL COLD START. The skybox load is a decode on the concurrent queue,
  // an upload on the loader thread, and a publish deferred to the render thread
  // behind the loader's own fence poll — a chain whose completion is bounded by
  // the LOADER's frame pump, not by ours. Nothing about the render loop's speed
  // makes it land sooner, so "render N frames first" is a timing assumption, not
  // a guarantee: a fast start (warm shader cache) reaches the first lit frame
  // with the set still empty and lights the whole scene off a black IBL, and a
  // capture taken on a frame count keeps that frame forever.
  //
  // So the FIRST frame that would be lit by an outstanding baked set drains it
  // in-frame instead, exactly as the procedural feed's first cycle does. State-
  // checked (the request's own pending count, then isPublished), never timed.
  // Called from the forward prologue; a no-op — one pointer test — from the
  // second frame on, and for any scene with no baked skybox request at all.
  void drainPendingRadianceMapLoad(lev2::Context* ctx);
  // MT2 (§2.6): re-filter the IBL from a raw env map and live-swap it into the
  // currently-bound _radiance_maps (in-scene HDRI/dynamic-sky refresh). Routes
  // through the shared RadianceMapCache::refilter → WINDOW-context scheduler.
  void refilterSkybox(const AssetPath& raw_source_path);
  static radiancemaps_ptr_t requestRadianceMaps(const AssetPath& texture_path);
  static radiancemaps_ptr_t requestRadianceMapsAsync(const AssetPath& texture_path);
  // Blocking variant: returns only after the radiance maps are fully
  // GPU-resident. Caller passes the context whose _deferredOps the
  // loader's terminal swap-op will land on (post Phase 6.3 Variant B:
  // typically the render context). This routine pumps that context's
  // deferred queue while polling the load counter.
  static radiancemaps_ptr_t requestRadianceMapsSync(const AssetPath& texture_path, lev2::Context* ctx);

  // Allocate procedural RadianceMaps (black). Populate via updateRadianceMapsGradient.
  // The update reuses the GPU textures, so calling every frame is safe.
  // A single-stop gradient is equivalent to a solid color.
  static radiancemaps_ptr_t makeProceduralRadianceMaps(lev2::Context* ctx);
  static void updateRadianceMapsGradient(
      radiancemaps_ptr_t maps,
      const std::vector<std::pair<float, fvec3>>& stops,
      lev2::Context* ctx);

  void onGpuInit(lev2::Context* ctx);

  radiancemaps_ptr_t _radiance_maps;

  // one-shot latch for the cold-start drain's failure report: a load that ended
  // WITHOUT publishing never recovers, and the condition recurs every frame.
  bool _baked_cold_start_failed = false;

  asset::asset_ptr_t _environmentTextureAsset;
  std::unordered_map<uint64_t, lev2::texture_ptr_t> _ssaoKernels;
  std::unordered_map<uint64_t, lev2::texture_ptr_t> _ssaoScrNoise;

  float _environmentIntensity = 1.0f;
  float _environmentMipBias   = 0.0f;
  float _environmentMipScale  = 1.0f;
  float _diffuseLevel         = 1.0f;
  float _specularLevel        = 1.0f;
  float _specularMipBias      = 0.0f;
  float _skyboxLevel          = 1.0f;
  float _depthFogDistance     = 1000.0f;
  float _depthFogPower        = 1.0f;
  float _roughnessPower       = 1.2f;
  fvec3 _ambientLevel;
  fvec4 _clearcolor;
  int _ssaoNumSamples = 0;
  int _ssaoNumSteps = 4;
  float _ssaoRadius = 0.05;
  float _ssaoBias = 0.0;
  float _ssaoWeight = 0.0;
  float _ssaoPower = 1.0;
  float _ssaoFeedback = 0.5;
  bool _useDepthPrepass = true;
  bool _useFloatColorBuffer = false;
  // PBR2 P3.D — runtime SSSS gate. Default true. When false, PostFxNodeSSSS
  // passes target0 straight through to its output (no blur, no composite);
  // the chain downstream sees the lit composite without the subsurface
  // delta. Useful as a live A/B comparison and as a key-toggle hook.
  bool _enable_SSSS = true;
  // M-key terrain material-override mode (0=declared 1=normals 2=slope 3=white). Runtime-only
  // (never reflected), like _enable_SSSS. Set by the SG system's SetTerrainMaterialMode notify;
  // read per-frame by the terrain drawable's render lambda (reached via the RCFD "PBR_COMMON"
  // userProperty) to pick a forced debug technique. 0 = no override = the declared path.
  int _terrainMaterialMode = 0;
  uint64_t _brdftype = 0;
  // THE DIRECT DIFFUSE LOBE (see DiffuseBrdfModel above). Bound on every PBR
  // draw as an ordinal; live-editable (the player's POST page row reaches it
  // through the SceneGraphSystem's UpdatePbrCommon notify).
  DiffuseBrdfModel _diffuseBrdfModel = DiffuseBrdfModel::OREN_NAYAR;
  float _dppZbias = 1.0e-3f;
  bool _enable_skybox = true;

  // SKYLIGHT lane B — procedural atmosphere. Unreflected runtime slot (same
  // precedent as _enable_skybox/_defaultBG); the DATA it points at is the
  // reflected object. NULL is the armed/disarmed switch: with no atmosphere
  // attached the compositor prologue's LUT step is skipped entirely, so every
  // baked-envmap scene renders bit-for-bit as before.
  skyatmospheredata_ptr_t _atmosphere;

  // SKYLIGHT lane B slice B2 — which sky reaches the SCREEN. Unreflected
  // runtime toggle (the _enable_skybox precedent), set per scene from the
  // "SkySource" scenegraph param or the pyext property. PROCEDURAL implies an
  // atmosphere: the prologue attaches the earth-like default rather than
  // handing the technique an empty LUT. _enable_skybox still gates BOTH
  // sources — a scene with the skybox off draws no sky of either kind.
  SkySource _sky_source = SkySource::BAKED;

  // SKYLIGHT lane B slice B3 — the procedural IBL feed's cycle state (see
  // SkyIblState). Always present; it stays entirely inert (and allocates no
  // maps) until a procedural-sky scene's prologue starts the first cycle.
  skyiblstate_ptr_t _sky_ibl;

  texture_ptr_t _texCubeBlack;
  texture_ptr_t _texCubeWhite;
  texturearray_ptr_t _texBlackArray;
  texturearray_ptr_t _texWhiteLightMapArray;
  texturearray_ptr_t _texBlackLightMapArray;
  texture_ptr_t _texBlack;
  texture_ptr_t _texWhite;
  bool _needsGpuInit = true;

  std::string _name;

  // Non-owning back-pointer to the scenegraph::Scene that owns this
  // CommonStuff (via its shared_ptr). Used by the forward compositor
  // to reach Scene::layersForRole() for layer-role-based scene
  // composition. Nullable — can be unset for compositors not backed
  // by a scenegraph. Scene outlives this so raw pointer is safe.
  scenegraph::Scene* _scene = nullptr;
};


///////////////////////////////////////////////////////////////////////////////

struct RadianceMapCache {
  /// Get or load radiance maps by resolved path (thread-safe, cached).
  radiancemaps_ptr_t get(const AssetPath& path);
  /// Clear all cached entries (e.g., after re-baking probes).
  void clear();
  /// MT2 (JUL05_GPUMICROTASK §2.6): re-filter `raw_source_path` (a raw
  /// HDR/EXR/PNG env map) and LIVE-swap the result into `target` via a
  /// RadiancePrefilterMicrotask enqueued on the WINDOW context's scheduler —
  /// budget-enforced where frames matter (T11). The initial scene-load path
  /// keeps the burst (createFilteringTaskGraph); this is the in-scene refresh
  /// (HDRI / dynamic-sky refresh without a hitch, FUSION §3). No-op targets or
  /// a missing render context fail loudly.
  void refilter(const AssetPath& raw_source_path, radiancemaps_ptr_t target);
  /// SKYLIGHT slice B3: same sliced prefilter, but from a GPU texture that is
  /// ALREADY resident (the procedural sky's equirect snapshot) — no asset load,
  /// no extension sniff, equirect+HDR asserted by the caller's construction.
  /// `on_complete` fires on the context-owner thread AFTER the swap has
  /// published, and is where the caller's cycle gate reopens. The source must
  /// stay immutable until then (§2 snapshot law) — this entry point cannot
  /// enforce that, only its caller can.
  /// COMFORT-1: the importance-sample count is the caller's quality/cost
  /// trade and is REQUIRED here — a live feed that hitches the render thread
  /// has no business inheriting the bake-time count by accident.
  /// `steady_state` declares THIS refilter's async-tracker tag recurring
  /// (asyncWorkMarkSteady): a chaining sky feed re-arms every cycle, and a
  /// settle/drain waiter that counted it would never see the census reach zero.
  /// Only the caller knows whether its feed has already published once, so the
  /// decision is passed in rather than inferred here.
  /// The three granularity ints are the caller's SkyAtmosphereData knobs passed
  /// straight through to the microtask factory, UNRESOLVED: 0 on any of them
  /// means the caller declared nothing and the ORKID_MT_IBL_* env var / shipped
  /// default applies. This cache resolves nothing itself — there is exactly one
  /// resolution site (the RadiancePrefilterMicrotask ctor) and it must stay that
  /// way, or a feed and its plan can disagree about the grain they ran at.
  /// `cold_start` says THIS refilter is the feed's first — nothing usable is
  /// published, so the job drains in one frame instead of being paced (see the
  /// factory). Read off the same `_ready` edge as `steady_state` at the sky call
  /// site, but a separate contract: one tags the async census, the other sets a
  /// budget, and a caller can want either without the other.
  void refilterFromTexture(
      texture_ptr_t source,                            //
      radiancemaps_ptr_t target,                       //
      lev2::Context* ctx,                              //
      std::function<void(datablock_ptr_t)> on_complete,
      int specular_samples,                            //
      bool steady_state         = false,               //
      int level_batches         = 0,                   //
      int slices_per_frame      = 0,                   //
      int mipchain_budget_px    = 0,                   //
      bool cold_start           = false);
private:
  std::mutex _mutex;
  std::map<std::string, radiancemaps_ptr_t> _cache;
};

using radiancemap_cache_ptr_t = std::shared_ptr<RadianceMapCache>;

/// Process-wide radiance map cache (lazy singleton).
radiancemap_cache_ptr_t getRadianceMapCache();

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::pbr {