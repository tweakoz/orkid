////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

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
  virtual void update(const RenderContextInstData& RCID){}
  MaterialBase();
  fxpipeline_ptr_t pipeline(const RenderContextInstData& RCID, bool streaks);

  freestyle_mtl_ptr_t _material;
  fxpipeline_ptr_t _pipeline;

  fxtechnique_constptr_t _tek_sprites = nullptr;
  fxtechnique_constptr_t _tek_streaks = nullptr;

  fxtechnique_constptr_t _tek_streaks_stereoCI = nullptr;
  fxtechnique_constptr_t _tek_sprites_stereoCI = nullptr;
  
  vtx_set_sprite_t _vertexSetterSprite;
  vtx_set_streak_t _vertexSetterStreak;
  fvec4 _color;
  fvec4 _averageColor;
  EDepthTest _depthtest = EDepthTest::OFF;
  BlendingMacro _blending = BlendingMacro::OFF;

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
  fxparam_constptr_t _param_mod_texture;
  gradient_fvec4_ptr_t _gradient;
  freestyle_mtl_ptr_t _grad_render_mtl;
  fxpipeline_ptr_t _grad_render_pipeline;
  texture_ptr_t _gradient_texture;
  rtgroup_ptr_t _gradient_rtgroup;
  asset::asset_ptr_t _modulation_texture_asset;
  texture_ptr_t _modulation_texture;
  fvec4 _gradientSamples[256];
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
  texture_ptr_t _texture;
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
  texture_ptr_t _texture;
  fxparam_constptr_t _paramColorMap;
  fxparam_constptr_t _paramGridDim;
  fxparam_constptr_t _parammodcolor;
  float _gridDim = 1.0;
};

using texgridmaterial_ptr_t = std::shared_ptr<TexGridMaterial>;

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
// lambda via ptcl_context->_rcidlambda in onLink).
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
  lev2::material_ptr_t _material;
  float                _voxelSize = 0.1f;
  VdbLevelSetKernel    _kernel    = VdbLevelSetKernel::WYVILL;
  bool                 _sort      = false;
};

using vdblevelset_module_ptr_t = std::shared_ptr<VdbLevelSetRendererData>;

/////////////////////////////////////////
} //namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////
