////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

// fwd — VdbLevelSetRendererData carries a reflected PbrMaterialGenData (the
// serializable material recipe); the full type lives in lev2/gfx/asset_gen.h.
namespace ork::lev2 {
struct PbrMaterialGenData;
using pbr_material_gendata_ptr_t = std::shared_ptr<PbrMaterialGenData>;
}

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
/////////////////////////////////////////

using streak_vtx_t           = SVtxV12N12B12T16;
using sprite_vtx_t           = SVtxV12N12B12T16;

using sprite_vertex_writer_t = lev2::VtxWriter<sprite_vtx_t>;
using streak_vertex_writer_t = lev2::VtxWriter<streak_vtx_t>;

using vtx_set_sprite_t = std::function<void( sprite_vertex_writer_t& vw, //
                                       const BasicParticle* ptc, //
                                       float fang, //
                                       float size, //
                                       uint32_t ucolor )>;
using vtx_set_streak_t = std::function<void( streak_vertex_writer_t& vw, //
                                             const BasicParticle* ptc, //
                                             fvec2 LW, 
                                             fvec3 obj_nrmz )>;

struct MaterialBase : public ork::Object {
  DeclareAbstractX(MaterialBase, ork::Object);
public:
  virtual void gpuInit(const RenderContextInstData& RCID) = 0;
  // PRE-PASS (render thread, no active render pass) one-shot gpuInit — the ONLY legal home
  // for the shader load. Renderers call it from their drawable onGpuUpdate hook; _render
  // asserts rather than initializing mid-pass.
  void gpuInitIfNeeded(ork::lev2::Context* ctx);
  virtual void update(const RenderContextInstData& RCID){}
  // PRE-RENDER hook (render thread) — GradientMaterial overrides it to re-sample its gradient
  // into _gradientSamples (CPU only, dirty-gated) so the renderer can write the LUT into the
  // per-frame SSBO. Default no-op.
  virtual void onGpuUpdate(ork::lev2::Context* ctx){}
  MaterialBase();
  fxpipeline_ptr_t pipeline(const RenderContextInstData& RCID, bool streaks);
  // Gradient color LUT (256 entries) delivered to the shader via the per-frame particle SSBO
  // (NOT a texture — the particle render path is entirely inside a render pass, so a texture
  // upload can't land). Defaults to WHITE (set in the MaterialBase ctor) so non-gradient
  // materials pass frg_clr through unchanged; GradientMaterial fills it from its gradient.
  fvec4 _gradientSamples[256];

  freestyle_mtl_ptr_t _material;
  fxpipeline_ptr_t _pipeline;

  fxtechnique_constptr_t _tek_sprites = nullptr;
  fxtechnique_constptr_t _tek_streaks = nullptr;

  fxtechnique_constptr_t _tek_streaks_stereoCI = nullptr;
  fxtechnique_constptr_t _tek_sprites_stereoCI = nullptr;

  // SINGLE-PASS STEREO: resolve the "<mono>_ST" technique peers and install the
  //  ublk_stereo producer on _pipeline. Call from gpuInit AFTER _tek_sprites/_tek_streaks
  //  and _pipeline are set. Without it a stereo pass draws the mono matrix into BOTH eye
  //  layers — zero parallax, invisible to validation.
  void _wireStereoTechniques();
  
  vtx_set_sprite_t _vertexSetterSprite;
  vtx_set_streak_t _vertexSetterStreak;
  fvec4 _color;
  fvec4 _averageColor;
  EDepthTest _depthtest = EDepthTest::OFF;
  BlendingMacro _blending = BlendingMacro::OFF;

  // GENERIC AUX-CHANNEL techniques (E2B item D). A material may provide a
  // technique pair for any named aux channel (e.g. "heat" — written into
  // the forward node's aux RT during the AUX subpass). Keyed by the
  // CrcString hash of the channel name (matches the RCFD "AUX_CHANNEL"
  // user property the aux pass publishes). pipeline() returns the entry's
  // pipeline during an AUX subpass, or nullptr when the material doesn't
  // write the active channel — renderers SKIP the draw on nullptr.
  struct AuxTekSet {
    fxtechnique_constptr_t _tek_sprites = nullptr;
    fxtechnique_constptr_t _tek_streaks = nullptr;
    fxpipeline_ptr_t       _pipeline;   // own rasterstate (e.g. additive, no-Z)
  };
  std::map<uint64_t, AuxTekSet> _aux_teks;

  // SSBO binding-slot metadata. The actual particle vertex SSBO is owned
  // per-renderer-instance (StreakRendererInst / SpriteRendererInst) — was
  // previously here on the material, but that caused multiple particle
  // instances using the same material to race on a single SSBO.
  const FxShaderStorageBlock* _cu_storage_block  = nullptr;
  const FxComputeShader* _streakcu_shader              = nullptr;
  const FxComputeShader* _spritecu_shader              = nullptr;


};

using basematerial_ptr_t = std::shared_ptr<MaterialBase>;

/////////////////////////////////////////

struct FlatMaterial : public MaterialBase {
  DeclareConcreteX(FlatMaterial, MaterialBase);
public:
  static std::shared_ptr<FlatMaterial> createShared();
  FlatMaterial();
  void update(const RenderContextInstData& RCID) final;
  void gpuInit(const RenderContextInstData& RCID) final;
};

using flatmaterial_ptr_t = std::shared_ptr<FlatMaterial>;

/////////////////////////////////////////

struct GradientMaterial : public MaterialBase {
  DeclareConcreteX(GradientMaterial, MaterialBase);
public:
  static std::shared_ptr<GradientMaterial> createShared();
  GradientMaterial();
  void update(const RenderContextInstData& RCID) final;
  void gpuInit(const RenderContextInstData& RCID) final;
  void onGpuUpdate(ork::lev2::Context* ctx) final; // (re)upload the gradient LUT, dirty-gated
  fxparam_constptr_t _param_mod_texture;
  gradient_fvec4_ptr_t _gradient;
  freestyle_mtl_ptr_t _grad_render_mtl;
  fxpipeline_ptr_t _grad_render_pipeline;
  texture_ptr_t _gradient_texture;
  rtgroup_ptr_t _gradient_rtgroup;
  asset::asset_ptr_t _modulation_texture_asset;
  texture_ptr_t _modulation_texture;
  bool _gradient_resampled = false;       // false until the first onGpuUpdate re-sample
  float _gradientAlphaIntensity = 1.0f;
  float _gradientColorIntensity = 1.0f;
};

using gradientmaterial_ptr_t = std::shared_ptr<GradientMaterial>;

/////////////////////////////////////////

// GradientAtlasMaterial — samples a user-supplied 2D texture as a
// "gradient atlas." Frag shader looks up vec2(unit_age, aux.x), so each
// row of the atlas is a complete 1D gradient and a particle's aux.x picks
// which row. The atlas is provided directly (no runtime gradient→texture
// bake step like GradientMaterial does). Standard blending/depth/color
// intensity properties match the sibling class.
struct GradientAtlasMaterial : public MaterialBase {
  DeclareConcreteX(GradientAtlasMaterial, MaterialBase);
public:
  static std::shared_ptr<GradientAtlasMaterial> createShared();
  GradientAtlasMaterial();
  void update(const RenderContextInstData& RCID) final;
  void gpuInit(const RenderContextInstData& RCID) final;

  fxparam_constptr_t _param_atlas       = nullptr;
  fxparam_constptr_t _param_mod_texture = nullptr;
  texture_ptr_t      _atlas;                 // the gradient atlas (2D)
  texture_ptr_t      _modulation_texture;    // sibling concept, optional
  float              _gradientAlphaIntensity = 1.0f;
  float              _gradientColorIntensity = 1.0f;
};

using gradientatlasmaterial_ptr_t = std::shared_ptr<GradientAtlasMaterial>;

/////////////////////////////////////////

struct TextureMaterial : public MaterialBase {
  DeclareConcreteX(TextureMaterial, MaterialBase);

public:
  static std::shared_ptr<TextureMaterial> createShared();
  TextureMaterial();
  void update(const RenderContextInstData& RCID) final;
  void gpuInit(const RenderContextInstData& RCID) final;
  texture_ptr_t _texture;            // runtime (resolved from _texture_asset when null)
  asset::asset_ptr_t _texture_asset; // REFLECTED — the serializable form
  fxparam_constptr_t _paramColorMap;
  fxparam_constptr_t _parammodcolor;
};

using texturematerial_ptr_t = std::shared_ptr<TextureMaterial>;

/////////////////////////////////////////

struct TexGridMaterial : public MaterialBase {
  DeclareConcreteX(TexGridMaterial, MaterialBase);

public:
  static std::shared_ptr<TexGridMaterial> createShared();
  TexGridMaterial();
  void update(const RenderContextInstData& RCID) final;
  void gpuInit(const RenderContextInstData& RCID) final;
  texture_ptr_t _texture;            // runtime (resolved from _texture_asset when null)
  asset::asset_ptr_t _texture_asset; // REFLECTED — the serializable form
  fxparam_constptr_t _paramColorMap;
  fxparam_constptr_t _paramGridDim;
  fxparam_constptr_t _parammodcolor;
  float _gridDim = 1.0;
};

using texgridmaterial_ptr_t = std::shared_ptr<TexGridMaterial>;

/////////////////////////////////////////
// FreestyleParticleMaterial — the composable particle material:
//   flipbook cookie (texgrid recipe) x gradient ramp over unit_age,
//   authored for PREMA (premultiplied) compositing so ONE chain morphs
//   additive fire (gradient w ~ 0) into absorptive smoke (w > 0) per
//   particle. Geometry-agnostic (sprite + streak technique pair from one
//   fragment shader). `shader_path` overrides the stock orkshader://particle
//   with any fxv2 providing tfreestyleparticle_{sprites,streaks} — the
//   slot DSL-generated shaders (and aux-channel/heat variants) plug into.
//   ALL authored state is reflected (model-B: live-only state drops on the
//   embedded-graph round-trip and renders nothing in the C++ host).

struct FreestyleParticleMaterial : public MaterialBase {
  DeclareConcreteX(FreestyleParticleMaterial, MaterialBase);

public:
  static std::shared_ptr<FreestyleParticleMaterial> createShared();
  FreestyleParticleMaterial();
  void update(const RenderContextInstData& RCID) final;
  void gpuInit(const RenderContextInstData& RCID) final;

  // reflected (the serializable contract)
  gradient_fvec4_ptr_t _gradient;              // ramp over unit_age (rgb=color, w=PREMA opacity)
  asset::asset_ptr_t _texture_asset;           // the flipbook cookie sheet
  float _gridDim                = 1.0f;        // flipbook grid dimension (1 = static cookie)
  float _gradientColorIntensity = 1.0f;        // HDR scale on ramp rgb
  float _gradientAlphaIntensity = 1.0f;        // scale on ramp opacity
  std::string _shader_path;                    // empty = stock orkshader://particle
  // emission-light shaping (item E) — AUTHORED look knobs (reflected; the
  // DSL owns the feel, the C++ compute just obeys):
  //   smoothing: EMA time-constant (s) on the published light color/pos —
  //              0 = raw per-tick (max flicker), ~0.2 = breathing glow
  //   lum_power: per-particle weight = lum^p * (1-occlusion). p=1 lets the
  //              brief bright ignition flash dominate (whiter, twitchier);
  //              p<1 biases toward the flame BODY (oranger, steadier)
  //   tint:      direct multiplier on the published light color
  float _emission_smoothing = 0.0f;
  float _emission_lum_power = 1.0f;
  fvec3 _emission_tint      = fvec3(1, 1, 1);
  // SOFT-PARTICLE DEPTH FADE distance, in eye-space units: how far in FRONT of
  // the scene surface a sprite has to sit before it reaches full opacity. 0 =
  // off. Only bites when the shader opted in (ctx.soft_fade() in the fragment
  // DSL declares SoftFadeDistance; the stock shader has no fade), and only
  // when the DEPTH PREPASS ran — see update().
  float _soft_fade_distance = 0.0f;

  // runtime
  texture_ptr_t _texture;                      // resolved from _texture_asset when null
  // CPU mirror of the ramp (item E) — the sprite renderer derives the
  // per-system emission light from it: PREMA makes the weighting implicit
  // (emissive = the ADDITIVE part: luminance x (1 - occlusion w)), so fire
  // drives the light and smoke contributes nothing — no per-system hook.
  fxparam_constptr_t _param_cookie   = nullptr;
  fxparam_constptr_t _param_gridDim  = nullptr;
  bool _soft_fade_refused = false;             // set per-update when the prepass precondition fails
  bool _soft_fade_warned  = false;
  // the scene depth the fade / the heat pair's manual depth test sample, chosen
  // PER UPDATE: the RCFD DEPTH_MAP when the prepass filled it, else _farDepth.
  // Binding the real one with no prepass is a hard Vulkan layout fault (the
  // color pass still owns the attachment for WRITING), so the substitution is
  // not cosmetic — it is what keeps the refusal from taking the frame down.
  texture_ptr_t _depth_source;
  texture_ptr_t _farDepth;                     // 4x4 white = depth 1.0 = infinitely far
  freestyle_mtl_ptr_t _grad_render_mtl;        // gradient->256x1 RT bake (GradientMaterial recipe)
  fxpipeline_ptr_t _grad_render_pipeline;
  texture_ptr_t _gradient_texture;
  rtgroup_ptr_t _gradient_rtgroup;
};

using freestyleparticlematerial_ptr_t = std::shared_ptr<FreestyleParticleMaterial>;

/////////////////////////////////////////

struct VolTexMaterial : public MaterialBase {
  DeclareConcreteX(VolTexMaterial, MaterialBase);

public:
  static std::shared_ptr<VolTexMaterial> createShared();
  VolTexMaterial();
  void update(const RenderContextInstData& RCID) final;
  void gpuInit(const RenderContextInstData& RCID) final;
  texture_ptr_t _texture;
};

using voltexmaterial_ptr_t = std::shared_ptr<VolTexMaterial>;

/////////////////////////////////////////

struct RendererModuleData : public ParticleModuleData {
  DeclareAbstractX(RendererModuleData, ParticleModuleData);
public:
  RendererModuleData();
  // EXPLICIT draw order across a graph's renderers (lower draws first) —
  // multi-renderer graphs (smoke under fire) must not depend on implicit
  // link order for blend correctness. Reflected; ties keep link order.
  int _draw_order = 0;
};

/////////////////////////////////////////

struct SpriteRendererData : public RendererModuleData {
  DeclareConcreteX(SpriteRendererData, RendererModuleData);
public:
  SpriteRendererData();
  static std::shared_ptr<SpriteRendererData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;

  basematerial_ptr_t _material;
  bool _sort = false;
};

using spritemodule_ptr_t = std::shared_ptr<SpriteRendererData>;

/////////////////////////////////////////

struct StreakRendererData : public RendererModuleData {
  DeclareConcreteX(StreakRendererData, RendererModuleData);
public:
  StreakRendererData();
  static std::shared_ptr<StreakRendererData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
  basematerial_ptr_t _material;
  bool _sort = false;
};

using streakmodule_ptr_t = std::shared_ptr<StreakRendererData>;

/////////////////////////////////////////

struct LightRendererData : public RendererModuleData {
  DeclareConcreteX(LightRendererData, RendererModuleData);
public:
  LightRendererData();
  static std::shared_ptr<LightRendererData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
  basematerial_ptr_t _material;
  bool _sort = false;
};

using lightmodule_ptr_t = std::shared_ptr<LightRendererData>;

/////////////////////////////////////////

// Kernel shapes for VdbLevelSetRendererData splat. Each describes how a
// particle's contribution falls off with distance r from its center, in
// units of Radius. All are zero at r >= Radius (compact support) except
// GAUSSIAN which decays smoothly to ~0 by ~3 sigma.
//
// Underlying values are the CRC of the kernel name (via the _crcu
// user-defined literal) so that `kernel = tokens.WYVILL` from Python
// works via a simple `VdbLevelSetKernel(crc->hashed())` cast — same
// pattern used by BlendingMacro / EDepthTest elsewhere in orkid.
enum class VdbLevelSetKernel : uint64_t {
  WYVILL   = "WYVILL"_crcu,   // (1 - r²)³  — smooth, default
  CUBIC    = "CUBIC"_crcu,    // (1 - r)³   — cheaper, sharper falloff
  QUARTIC  = "QUARTIC"_crcu,  // (1 - r²)²  — sharper than Wyvill
  GAUSSIAN = "GAUSSIAN"_crcu, // exp(-α r²) — no compact support, very soft
};

// VdbLevelSetRenderer — chain terminus that splats live particles into a
// per-graphinst OpenVDB FloatGrid as a density field, runs marching cubes
// (openvdb::tools::volumeToMesh) at the IsoLevel threshold, and draws the
// resulting triangle mesh via an internally-owned RigidPrimitive. Matches
// the existing per-particle-renderer module shape (registers a _render
// lambda via ptcl_context->setRenderLambda(this, ...) in onLink).
//
// Plugs (FloatXf, uniform-rate):
//   Radius    — kernel falloff radius in world units (per particle)
//   Strength  — kernel amplitude multiplier (density contribution per particle)
//   IsoLevel  — marching-cubes threshold (density value at the iso-surface)
//
// Properties (set at construction, not bindable):
//   _voxelSize — VDB grid voxel size (world units). Smaller = sharper but
//                quadratic memory cost per particle's splat sphere.
//   _kernel    — falloff kernel shape (VdbLevelSetKernel enum).
//   _material  — render material (any FreestyleMaterial / PBR / etc.).
struct VdbLevelSetRendererData : public RendererModuleData {
  DeclareConcreteX(VdbLevelSetRendererData, RendererModuleData);
public:
  VdbLevelSetRendererData();
  static std::shared_ptr<VdbLevelSetRendererData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
  // General lev2 material (PBRMaterial / FreestyleMaterial / etc.) — NOT
  // a particle-specific MaterialBase. The extracted triangle mesh is
  // drawn via RigidPrimitive::renderEML through this material's pipeline,
  // same pattern vdb_sculpt.py uses with shaders.createPbrMaterialWithColor.
  // RUNTIME-only (a live GPU material can't serialize) — the imperative override.
  lev2::material_ptr_t _material;
  // SERIALIZABLE material RECIPE (model B): a reflected PbrMaterialGenData. When the
  // live _material is null (the post-deserialize case — it silently dropped, which used
  // to render the blobs INVISIBLE), the renderer inst materializes this lazily at first
  // render (the D.1 C++ materializer; ctx available there). The DSL authors BOTH: the
  // live material for in-process parity, the gen for the round-trip.
  pbr_material_gendata_ptr_t _material_gen;
  float                _voxelSize = 0.1f;
  VdbLevelSetKernel    _kernel    = VdbLevelSetKernel::WYVILL;
  bool                 _sort      = false;
};

using vdblevelset_module_ptr_t = std::shared_ptr<VdbLevelSetRendererData>;

/////////////////////////////////////////
} //namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////
