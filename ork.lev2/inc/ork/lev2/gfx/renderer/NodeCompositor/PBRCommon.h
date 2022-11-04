#pragma once 

#include <ork/dataflow/dataflow.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/renderer/frametek.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/compositormaterial.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/kernel/varmap.inl>
#include <ork/kernel/datacache.h>

namespace ork::lev2::pbr {

struct CommonStuff : public ork::Object {
  DeclareConcreteX(CommonStuff, ork::Object);

  CommonStuff();

  void _readEnvTexture(asset::asset_ptr_t& tex) const;
  void _writeEnvTexture(asset::asset_ptr_t const& tex);
  void setEnvTexturePath(file::Path path);

  lev2::texture_ptr_t envSpecularTexture() const;
  lev2::texture_ptr_t envDiffuseTexture() const;

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

  texture_ptr_t _filtenvSpecularMap;
  texture_ptr_t _filtenvDiffuseMap;
  asset::asset_ptr_t _environmentTextureAsset;
  asset::vars_ptr_t _texAssetVarMap;
  float _environmentIntensity = 1.0f;
  float _environmentMipBias   = 0.0f;
  float _environmentMipScale  = 0.0f;
  float _diffuseLevel         = 1.0f;
  float _specularLevel        = 1.0f;
  float _skyboxLevel          = 1.0f;
  float _depthFogDistance     = 1000.0f;
  float _depthFogPower        = 1.0f;
  fvec3 _ambientLevel;
  fvec4 _clearColor;

};

using commonstuff_ptr_t = std::shared_ptr<CommonStuff>;


} // namespace ork::lev2::pbr {