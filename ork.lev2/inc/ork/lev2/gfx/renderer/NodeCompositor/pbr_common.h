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
  texture_ptr_t _filtenvDiffuseMap;
  texture_ptr_t _brdfIntegrationMapGGX;
  texture_ptr_t _brdfIntegrationMapVelvet;
  texture_ptr_t _brdfIntegrationMapGGXRIM;
  texture_ptr_t _brdfIntegrationMapBlinn;
  texture_ptr_t _brdfIntegrationMapPhong;
  std::vector<float> _specularRoughnessValues;  // Roughness value for each array slice
  int _numRoughnessLevels = 0;  // Number of roughness levels in array
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
  lev2::texturearray_ptr_t envSpecularTexture() const;
  lev2::texture_ptr_t envDiffuseTexture() const;

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
  static radiancemaps_ptr_t requestRadianceMaps(const AssetPath& texture_path);
  static radiancemaps_ptr_t requestRadianceMapsAsync(const AssetPath& texture_path);
  // Blocking variant: returns only after the radiance maps are fully
  // GPU-resident. Must be called on the GPU thread with a live context
  // because no other thread drains GfxEnv::_deferredContextOps while
  // we block — this routine self-pumps that queue.
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
  uint64_t _brdftype = 0;
  float _dppZbias = 1.0e-3f;
  bool _enable_skybox = true;

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
private:
  std::mutex _mutex;
  std::map<std::string, radiancemaps_ptr_t> _cache;
};

using radiancemap_cache_ptr_t = std::shared_ptr<RadianceMapCache>;

/// Process-wide radiance map cache (lazy singleton).
radiancemap_cache_ptr_t getRadianceMapCache();

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::pbr {