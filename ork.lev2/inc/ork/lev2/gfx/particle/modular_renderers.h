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
};

using spritemodule_ptr_t = std::shared_ptr<SpriteRendererData>;

/////////////////////////////////////////

struct MaterialBase : public ork::Object {
  DeclareAbstractX(MaterialBase, ork::Object);
public:
  virtual test_mtl_ptr_t bind(Context* pT) = 0;
  virtual void update(float ftexframe)               = 0;
  MaterialBase()
      : _material(nullptr) {
  }
  test_mtl_ptr_t _material;
};

/////////////////////////////////////////

struct TextureMaterial : public MaterialBase {
  DeclareAbstractX(TextureMaterial, MaterialBase);

public:
  TextureMaterial();
  void update(float ftexframe) final;
  texture_ptr_t _texture;
  test_mtl_ptr_t bind(Context* pT) final;
};

/////////////////////////////////////////

/////////////////////////////////////////
} //namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////
