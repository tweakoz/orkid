////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
/////////////////////////////////////////

struct MaterialBase : public ork::Object {
  DeclareAbstractX(MaterialBase, ork::Object);
public:
  virtual void gpuInit(const RenderContextInstData& RCID) = 0;
  virtual void update(const RenderContextInstData& RCID){}
  MaterialBase()
      : _material(nullptr) {
  }
  freestyle_mtl_ptr_t _material;
  fxpipeline_ptr_t _pipeline;
};

using basematerial_ptr_t = std::shared_ptr<MaterialBase>;

/////////////////////////////////////////

struct FlatMaterial : public MaterialBase {
  DeclareAbstractX(FlatMaterial, MaterialBase);
public:
  static std::shared_ptr<FlatMaterial> createShared();
  FlatMaterial();
  void update(const RenderContextInstData& RCID) final;
  void gpuInit(const RenderContextInstData& RCID) final;
  fvec4 _color;
  fxparam_constptr_t _paramflatcolor;
};

using flatmaterial_ptr_t = std::shared_ptr<FlatMaterial>;

/////////////////////////////////////////

struct GradientMaterial : public MaterialBase {
  DeclareAbstractX(GradientMaterial, MaterialBase);
public:
  static std::shared_ptr<GradientMaterial> createShared();
  GradientMaterial();
  void update(const RenderContextInstData& RCID) final;
  void gpuInit(const RenderContextInstData& RCID) final;
  fvec4 _color;
  fxparam_constptr_t _paramflatcolor;
};

using gradientmaterial_ptr_t = std::shared_ptr<GradientMaterial>;

/////////////////////////////////////////

struct TextureMaterial : public MaterialBase {
  DeclareAbstractX(TextureMaterial, MaterialBase);

public:
  static std::shared_ptr<TextureMaterial> createShared();
  TextureMaterial();
  void update(const RenderContextInstData& RCID) final;
  void gpuInit(const RenderContextInstData& RCID) final;
  texture_ptr_t _texture;
};

using texturematerial_ptr_t = std::shared_ptr<TextureMaterial>;

/////////////////////////////////////////

struct TexGridMaterial : public MaterialBase {
  DeclareAbstractX(TexGridMaterial, MaterialBase);

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
  DeclareAbstractX(VolTexMaterial, MaterialBase);

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
  dflow::dgmoduleinst_ptr_t createInstance() const final;

  basematerial_ptr_t _material;
};

using spritemodule_ptr_t = std::shared_ptr<SpriteRendererData>;


/////////////////////////////////////////
} //namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////
