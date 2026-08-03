#pragma once

#include <ork/application/application.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/renderer/probe_sh.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>

#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/asset/Asset.inl>
#include <ork/profiling.inl>

namespace ork::lev2::pbr {

struct ForwardPbrNodeImpl;

struct ForwardPass {
  //ForwardNode* _node                 = nullptr;
  ForwardPbrNodeImpl* _impl          = nullptr;
  CompositorDrawData* _drawdata      = nullptr;
  std::string _fwd_pass_layer        = "std_forward";
  std::string _dpp_pass_layer        = "depth_prepass";
  bool _single_pass_stereo           = false;
  rtgroup_ptr_t _rtg_out;
  rtgroup_ptr_t _rtg_depth_copy;
  rtgroup_ptr_t _rtg_depth_copy_linear;
  bool _renderingPROBE = false;
};
using forward_pass_ptr_t = std::shared_ptr<ForwardPass>;

struct ForwardPbrNodeImpl {
  static const int KMAXLIGHTS = 32;
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ForwardPbrNodeImpl(ForwardNode* node);
  ~ForwardPbrNodeImpl();
  void init(lev2::Context* context, int iw, int ih);
  void _buildPrimaryRtgs(lev2::Context* context, int iw, int ih); // MSAA-aware primary RtgSet (re-buildable)
  void _render_prologue(CompositorDrawData& drawdata); // once per composited frame (lights/shadows/probes)
  void _render_top(CompositorDrawData& drawdata);
  void _render_dppskyssaocolor(forward_pass_ptr_t fpass);
  void _render_dpp(forward_pass_ptr_t fpass);
  void _render_skybox(forward_pass_ptr_t fpass);
  void _render_ssao_linearize_depth(forward_pass_ptr_t fpass);
  void _render_ssao_prepass(forward_pass_ptr_t fpass);
  void _render_colorpass(forward_pass_ptr_t fpass);
  void _update_env_probes(CompositorDrawData& drawdata);
  void _update_shadow_maps();
  void _update_sun_cascades(); // SKYLIGHT lane A — sun UBO write + cascade depth passes
  // S2a — draw the in-flight snapshot's next batch of bands, and publish (flip)
  // when the last one lands. Called from _update_sun_cascades only.
  void _render_sun_snapshot_bands(int bands_this_frame, int fade_frames, float fade_secs);
  // OUTGOING-snapshot blend weight for the flip crossfade: 1 on the flip
  // frame, 0 at the end of the window. Clock-ramped when a wall-clock length
  // is declared, frame-ramped otherwise.
  float _sun_fade_weight() const;
  // CLOUD SHADOWS — fill the sun cookie by drawing the "sun_cookie" role layer
  // (the cloud decks) from a sun-aligned ortho camera. Runs BEFORE
  // _update_sun_cascades so the cookie the ublk_sun write publishes is this
  // frame's, not last frame's.
  void _update_sun_cookie();
  void _update_sky_luts();     // SKYLIGHT lane B — Hillaire LUT chain (inert with no atmosphere)
  void _update_sky_ibl();      // SKYLIGHT lane B — equirect snapshot + sliced refilter, at cycle start only
  void _update_sky_sh();       // W4-S8 — project the frozen snapshot onto the sky probe's L2 basis
  void _initProbeBlitMaterial(Context* ctx);
  void _setupCubeFaceCamera(lightprobe_ptr_t probe, int iface);
  void _renderProbeWithSSAA(lightprobe_ptr_t probe, CompositorDrawData& drawdata,
                            CompositingPassData& cubemapCPD);
  void _renderProbeWithTAA(lightprobe_ptr_t probe, CompositorDrawData& drawdata,
                           CompositingPassData& cubemapCPD);
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ForwardNode* _node;
  std::string _camname;
  enumeratedlights_ptr_t _enumeratedLights;

  rtgset_ptr_t  _rtgs_primary;
  rtgroup_ptr_t _rtg_primary;
  rtgroup_ptr_t _rtg_primary_depth_copy;
  rtgroup_ptr_t _rtg_primary_depth_copy_linear;
  rtgroup_ptr_t _rtg_ambocc_accum;
  rtgroup_ptr_t _rtg_ambocc_accum2;
  rtgroup_ptr_t _rtg_cube1_depth_copy;
  // GENERIC AUX CHANNELS (E2B item D) — one lazily-created RTG per declared
  // channel (RGBA16F, primary dims, no depth); rendered as an extra pass in
  // _render_colorpass and published into the CompositorDrawData properties
  // under crc("aux_<name>") for postfx consumption.
  std::map<std::string, rtgroup_ptr_t> _aux_rtgs;
  rtgset_ptr_t _rtgs_resolve_msaa;
  fmtx4 _viewOffsetMatrix;
  pbrmaterial_ptr_t _skybox_material;
  freestyle_mtl_ptr_t _ssao_material;
  fxpipelinecache_constptr_t _skybox_fxcache;
  fxpipelinecache_constptr_t _ssao_fxcache;
  textureassetptr_t _whiteTexture;
  cameramatrices_ptr_t _SHADOWCAM;
  cameramatrices_ptr_t _CUBECAM;
  cameramatrices_ptr_t _SUNCAM; // per-cascade ortho camera (reused sequentially like _SHADOWCAM)
  cameramatrices_ptr_t _COOKIECAM; // sun-aligned ortho camera the cloud decks are drawn from
  cameramatrices_ptr_t _CULLCAM; // cascade-cull fix: UNION sun ortho enclosing ALL cascade slices; the
                                 // per-frame sun-shadow cull (Scene::shadowCull) frustum. Superset of
                                 // every slice, so its survivors cover every cascade's casters.
  // SUN-CASCADE SNAPSHOT state (DirectionalLightData::_shadowSnapshotInterval).
  // The held unit is fit+cull+depth-passes. The declared interval is the ONLY
  // cadence; what is recorded here is the STRUCTURAL premise set — the caster
  // and the rig — because those change the storage or the geometry the maps
  // are drawn into, and a snapshot cannot be held across them at any interval.
  Timer _sun_snap_timer;                  // Start()ed on each taken snapshot
  bool _sun_snap_valid          = false;  // false = nothing held yet (first-ever render refits)
  DirectionalLight* _sun_snap_caster = nullptr; // which light the snapshot was fit FOR (sun->moon flip)
  int _sun_snap_body            = 0;      // its _skyBody, in case a Light address is recycled
  int _sun_snap_map_dim         = 0;      // rig the snapshot was fit with — any change refits THAT
  int _sun_snap_cascades        = 0;      //  frame: ensureSunCascades and the shadow-params write both
  float _sun_snap_max_dist      = 0.0f;   //  live inside the held unit, so deferring a resize would
  float _sun_snap_band_radius   = 0.0f;   //  sample maps at dimensions nothing rendered into
  float _sun_snap_band_ratio    = 0.0f;
  float _sun_snap_bias          = 0.0f;
  float _sun_snap_pcf           = 0.0f;
  float _sun_snap_res_ratio     = 0.0f;   // per-band resolution step (1 = uniform dims)
  // Per-snapshot jitter sequence index — advanced once per STARTED snapshot,
  // never per frame: the whole point is that the offset is constant for the
  // life of a snapshot and only changes across a crossfade.
  int _sun_snap_jitter_index    = 0;
  int _sun_snap_sets            = 0;      // 1 = single-buffered (shipped), 2 = double-buffered
  // SUN-CASCADE SNAPSHOT JOB (S2a) — the amortized depth render. A snapshot is
  // ONE unit fit against ONE frozen premise set (anchor, light direction, cull
  // box); BandsPerFrame only decides how many frames the DRAWING is spread
  // over. Everything the deferred frames need is copied in here at job start,
  // because by the time band 3 renders the live camera has moved and the live
  // sun has turned — reading either again would publish a half-refit world.
  //
  // The shader-facing writes (matrices, splits, anchor, params) happen at
  // COMPLETION, never at job start: new matrices over not-yet-rendered depth
  // is precisely the swimming-shadow artifact the hold exists to avoid.
  struct SunCascadeJob {
    bool _active     = false;
    int _next_band   = 0; // next band to draw (== _band_count -> ready to publish)
    int _band_count  = 0;
    int _target_set  = 0; // slice base = _target_set * kSunCascadeStorage
    fmtx4 _view[LightManager::kSunCascadeStorage];
    fmtx4 _proj[LightManager::kSunCascadeStorage];
    fmtx4 _shmtx[LightManager::kSunCascadeStorage]; // proj*view, the shader-facing form
    fvec4 _splits;
    fvec4 _params;
    fvec3 _anchor;
    // PER-BAND RESOLUTION (S2b). _dim[i] is the band's rendered viewport edge
    // inside its (full-dim) slice, and _texel carries 1/_dim[i] to the shader —
    // one vec4, so the read side derives both its uv scale (params.w/_texel[i])
    // and its world texel size from it. All entries equal params.w = the
    // uniform-dim path, bit for bit.
    int _dim[LightManager::kSunCascadeStorage];
    fvec4 _texel;
  };
  SunCascadeJob _sun_job;
  int _sun_live_set = 0; // which set the shader is sampling (published, atomic wrt the frame)
  // The PUBLISHED snapshot, kept CPU-side so a flip can hand the outgoing fit
  // to the shader as the crossfade's prev half (the UBO is write-combined —
  // reading it back to recover what we just wrote would be a stall).
  bool _sun_pub_valid   = false;
  int _sun_pub_cascades = 0;
  fmtx4 _sun_pub_shmtx[LightManager::kSunCascadeStorage];
  fvec4 _sun_pub_splits;
  fvec3 _sun_pub_anchor  = fvec3(0, 0, 0);
  int _sun_fade_total     = 0; // declared crossfade window, frames
  int _sun_fade_remaining = 0; // 0 = no fade running (shader takes the current set only)
  int _sun_fade_prev_set  = 0;
  // the window's WALL-CLOCK length (0 = ride the frame count) and the clock it
  // is measured on, restarted at the flip. A frame count is not a duration: the
  // same 12 frames are 24 ms offscreen and 200 ms at 60 Hz, so without this the
  // blend duty was a function of the frame rate.
  float _sun_fade_secs = 0.0f;
  Timer _sun_fade_timer;
  // one-shot latch for the "declared cadence is under the snapshot+fade floor"
  // complaint: the condition recurs on every frame of every fade, so a per-frame
  // line would be a spam channel. Not reset — the first detection is the report.
  bool _sun_cadence_floor_warned = false;
  FreestyleMaterial _blit2screenmtl;
  const FxShaderTechnique* _fxtechnique1x1;
  const FxShaderParam* _fxpMVP;
  const FxShaderParam* _fxpP;
  const FxShaderParam* _fxpInvP;
  const FxShaderParam* _fxpColorMap;

  const FxUniformBlock* _par_ublk_std_matrices = nullptr;  
  FxUniformBuffer* _ubuf_std_matrices = nullptr;

  const FxShaderTechnique* _tek_ssao_prepass;
  const FxShaderTechnique* _tek_ssao_lindepth;

  const FxShaderParam* _fxpSSAONumSamples;
  const FxShaderParam* _fxpSSAONumSteps;
  const FxShaderParam* _fxpSSAOBias;
  const FxShaderParam* _fxpSSAORadius;
  const FxShaderParam* _fxpSSAOWeight;
  const FxShaderParam* _fxpSSAOPower;
  const FxShaderParam* _fxpSSAOFeedback;
  const FxShaderParam* _fxpSSAOKernel;
  const FxShaderParam* _fxpSSAOScrNoise;
  const FxShaderParam* _fxpSSAOMapDepth;
  const FxShaderParam* _fxpSSAOTexelSize;
  const FxShaderParam* _fxpSSAOInvViewportSize;
  const FxShaderParam* _fxpSSAOMVP;
  const FxShaderParam* _fxpSSAOPREV;
  const FxShaderParam* _fxpZndc2eye;

  const DrawQueue* _currentDrawQueue = nullptr;
  ViewData _currentViewData;
  rcfd_ptr_t _currentRCFD;
  lev2::IRenderer* _currentIRenderer = nullptr;
  Context* _currentContext       = nullptr;
  compositorimpl_ptr_t _currentCIMPL = nullptr;
  int _currentWidth              = 0;
  int _currentHeight             = 0;
  int _msaa_level_built          = -1;  // the --msaa LEVEL _rtgs_primary was built at (-1 = never)
  // Context::GetTargetFrame() stamp of the last _render_prologue run. The
  // prologue must run exactly ONCE per target frame and _render_top refuses
  // to render without a same-frame prologue (stale lights/shadows/probes die
  // loudly). NOT _node->_frameIndex — that increments per Render() (per eye).
  int _prologueTargetFrame       = -1;
  // rtg key -> Context::GetTargetFrame() of the frame that FIRST recorded that key's
  // depth passes since (re)build/resize. The frame-start HZB build samples LAST frame's
  // depth — until a key is seeded that depth image is UNDEFINED and must not be sampled.
  // The stamp is load-bearing: seeding marks the passes RECORDED, not SUBMITTED, and the
  // HZB dispatch rides its own queue submit that precedes the frame's graphics submit. A
  // second _render_top in the SAME frame (stereo/multi-compositor) would therefore hand
  // the driver a depth image that is still UNDEFINED to it, while every engine-side
  // layout check passes (CPU tracking already reads SHADER_READ_ONLY). Sampling is only
  // legal once the recording frame has ended — hence the strict frame comparison.
  std::unordered_map<uint64_t, int> _hzb_seeded_rtgs;
  // rtg key -> the LATEST frame that recorded that key's depth passes. Distinct from the
  // seed map above on purpose: the seed stamp is the admission test's comparand (first
  // recording frame, deliberately sticky), this one is the pyramid's PROVENANCE (which
  // frame's depth the build actually sampled) and is published on HZBBuilder for the
  // cull-oracle's leg. Reporting only — nothing culls or admits on it.
  std::unordered_map<uint64_t, int> _hzb_recorded_rtgs;

  forward_pass_ptr_t _primary_pass;

  // Probe SSAA/TAA blit support
  FreestyleMaterial _probeBlitMtl;
  const FxShaderTechnique* _tek_probe_blit = nullptr;
  const FxShaderTechnique* _tek_probe_ds[7] = {};    // downsample 1x1 through 7x7
  const FxShaderTechnique* _tek_probe_temporal = nullptr;
  const FxShaderParam* _par_probe_colormap = nullptr;
  const FxShaderParam* _par_probe_accummap = nullptr;
  const FxShaderParam* _par_probe_blendweight = nullptr;
  const FxShaderParam* _par_probe_mvp = nullptr;
  const FxShaderParam* _par_probe_vpdim = nullptr;
  const FxShaderParam* _par_probe_flipy = nullptr;
  const FxShaderParam* _par_probe_flipx = nullptr;
  bool _probeBlitInitDone = false;

  // SKYLIGHT lane B — created lazily on the first frame an atmosphere is
  // attached to pbr::CommonStuff, so scenes without one pay nothing.
  hillairesky_ptr_t _hillaire_sky;

  // SKYLIGHT lane C — created lazily on the first SH_Radiance probe, so scenes
  // with only reflection probes (i.e. everything today) pay nothing. Slots are
  // handed out monotonically and never reclaimed.
  probeshprojector_ptr_t _probeSH;
  int _probeSHSlotCounter = 0;

  // W4-S8 — the sky's own SH probe. Shares the projector (and therefore the SH
  // SSBO) with the placed SH_Radiance probes; its slot is claimed from the same
  // monotonic counter the first time the procedural feed freezes a snapshot.
  int _skySHSlot = -1;

}; // IMPL

} // namespace ork::lev2::pbr
