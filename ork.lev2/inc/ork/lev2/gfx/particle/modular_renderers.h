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
  using streak_vertex_writer_t = lev2::VtxWriter<streak_vtx_t>;

using vtx_set_t = std::function<void( vertex_writer_t& vw, //
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
  fxtechnique_constptr_t _tek_sprites;
  fxtechnique_constptr_t _tek_streaks;
  fxtechnique_constptr_t _tek_sprites_stereo;
  fxtechnique_constptr_t _tek_streaks_stereoCI;
  fxtechnique_constptr_t _tek_streaks_stereo;
  
  vtx_set_t _vertexSetter;
  vtx_set_streak_t _vertexSetterStreak;
  fvec4 _color;
  EDepthTest _depthtest = EDepthTest::OFF;
  Blending _blending = Blending::OFF;

#if defined(ENABLE_COMPUTE_SHADERS)
  FxShaderStorageBuffer* _streakcu_vertex_io_buffer    = nullptr;
  const FxShaderStorageBlock* _streakcu_storage_block  = nullptr;
  const FxComputeShader* _streakcu_shader              = nullptr;
#endif


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
  float _gradientAlphaIntensity = 1.0f;
  float _gradientColorIntensity = 1.0f;
};

using gradientmaterial_ptr_t = std::shared_ptr<GradientMaterial>;

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
} //namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////
