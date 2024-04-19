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

struct IrradianceMaps {

  texture_ptr_t _filtenvSpecularMap;
  texture_ptr_t _filtenvDiffuseMap;
  texture_ptr_t _brdfIntegrationMap;
  asset::loadrequest_ptr_t _loadRequest;

};

///////////////////////////////////////////////////////////////////////////////

struct CommonStuff : public ork::Object {
  DeclareConcreteX(CommonStuff, ork::Object);

  CommonStuff();

  void _readEnvTexture(asset::asset_ptr_t& tex) const;
  void _writeEnvTexture(asset::asset_ptr_t const& tex);
  void setEnvTexturePath(file::Path path);

  void assignEnvTexture(asset::asset_ptr_t texasset);
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

  asset::loadrequest_ptr_t requestAndRefSkyboxTexture(const AssetPath& texture_path);
  static irradiancemaps_ptr_t requestIrradianceMaps(const AssetPath& texture_path);


  irradiancemaps_ptr_t _irradianceMaps;
  lev2::texture_ptr_t _brdfIntegrationMap = nullptr;

  asset::asset_ptr_t _environmentTextureAsset;
  float _environmentIntensity = 1.0f;
  float _environmentMipBias   = 0.0f;
  float _environmentMipScale  = 1.0f;
  float _diffuseLevel         = 1.0f;
  float _specularLevel        = 1.0f;
  float _specularMipBias      = 0.0f;
  float _skyboxLevel          = 1.0f;
  float _depthFogDistance     = 1000.0f;
  float _depthFogPower        = 1.0f;
  fvec3 _ambientLevel;
  fvec4 _clearColor;

  bool _useDepthPrepass = true;

};


} // namespace ork::lev2::pbr {