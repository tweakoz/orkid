////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

//#include <format>
//#include <print> // mac also ahead on this...
#include <algorithm>
#include <chrono>
#include <thread>
#include <ork/pch.h>
#include <ork/rtti/Class.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/mutex.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/application/application.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/kernel/datacache.h>
#include <ork/gfx/brdf.inl>
#include <ork/gfx/dds.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/util/logger.h>
#include <ork/math/misc_math.h>

#include <ork/lev2/gfx/radiancemaps_asset.h>
#include <ork/lev2/gfx/xir_format.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/image.h>

#include <ork/profiling.inl>
#include <ork/asset/Asset.inl>

ImplementReflectionX(ork::lev2::pbr::CommonStuff, "pbr::CommonStuff");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::pbr {

constexpr size_t KNUMSSAONOISEFRAMES = 60;

static logchannel_ptr_t logchan_pbrcom = logger()->configureChannel("PBRCOM", fvec3(0.8, 0.8, 0.5), false);

///////////////////////////////////////////////////////////////////////////////
CommonStuff::CommonStuff() {

  _radiance_maps = std::make_shared<RadianceMaps>();
  _clearcolor     = fvec4(0, 0, 0, 1);
}
///////////////////////////////////////////////////////////////////////////////
void CommonStuff::assignEnvTexture(asset::asset_ptr_t texasset) {
  asset::vars_ptr_t old_varmap;
  if (_environmentTextureAsset) {
    old_varmap = _environmentTextureAsset->_varmap.clone();
    // printf("OLD <%p:%s>\n\n", _environmentTextureAsset.get(),_environmentTextureAsset->name().c_str());
  }
  // printf("NEW <%p:%s>\n\n", texasset.get(),texasset->name().c_str());

  _environmentTextureAsset = texasset;
  if (nullptr == _environmentTextureAsset)
    return;
  //_environmentTextureAsset->_varmap = *_RadianceVars();
}
///////////////////////////////////////////////////////////////////////////////

// Path-string expansion helper: resolves <assetcache>/, <stage>/, etc. to
// real filesystem paths. The scenegraph code that consumes the initial
// SkyboxTexPathStr (scenegraph.cpp ~L266) does this before constructing a
// LoadRequest; the request* functions below historically did NOT, so any
// caller passing a placeholder-bearing path (e.g. "<assetcache>/envmaps2/
// foo.xir") got a silent nullptr from AssetManager::load → null
// _radiance_maps → crash in envSpecularTexture(). Centralizing the
// expansion here keeps callers symmetric with the scenegraph path.
static AssetPath _expandIfNeeded(const AssetPath& p) {
  auto s = p.toStdString();
  if (s.find("<") != std::string::npos) {
    return AssetPath(file::Path::expandPathString(s));
  }
  return p;
}

radiancemaps_ptr_t CommonStuff::requestRadianceMaps(const AssetPath& texture_path) {
  auto resolved = _expandIfNeeded(texture_path);
  // Load XIR file directly using the registered XIR loader
  auto load_req = std::make_shared<asset::LoadRequest>(resolved);

  // Load using generic asset mechanism - the XIR extension will route to RadianceMapsLoader
  auto generic_asset = asset::AssetManager<RadianceMapsAsset>::load(load_req);
  if (generic_asset) {
    // Cast to RadianceMapsAsset
    auto radiancemaps_asset = std::dynamic_pointer_cast<RadianceMapsAsset>(generic_asset);
    if (radiancemaps_asset) {
      //_radiance_maps = radiancemaps_asset->_radiance_maps;
      if(0)printf("RRM: asset<%p> irrmaps<%p>\n", (void*) radiancemaps_asset.get(), (void*) radiancemaps_asset->_radiance_maps.get()  );
      return radiancemaps_asset->_radiance_maps;
    }
  }
  return nullptr;
}
radiancemaps_ptr_t CommonStuff::requestRadianceMapsAsync(const AssetPath& texture_path) {
  auto resolved = _expandIfNeeded(texture_path);
  // Load XIR file directly using the registered XIR loader
  auto load_req = std::make_shared<asset::LoadRequest>(resolved);

  // Load using generic asset mechanism - the XIR extension will route to RadianceMapsLoader
  auto generic_asset = asset::AssetManager<RadianceMapsAsset>::load(load_req);
  if (generic_asset) {
    // Cast to RadianceMapsAsset
    auto radiancemaps_asset = std::dynamic_pointer_cast<RadianceMapsAsset>(generic_asset);
    if (radiancemaps_asset) {
      //_radiance_maps = radiancemaps_asset->_radiance_maps;
      if(0)printf("RRM: asset<%p> irrmaps<%p>\n", (void*) radiancemaps_asset.get(), (void*) radiancemaps_asset->_radiance_maps.get()  );
      return radiancemaps_asset->_radiance_maps;
    }
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

radiancemaps_ptr_t CommonStuff::requestRadianceMapsSync(const AssetPath& texture_path, Context* ctx) {
  auto resolved = _expandIfNeeded(texture_path);
  // Kick off the load. RadianceMapsLoader wraps its async decode + three
  // deferred GPU-upload ops with a terminal deferred op that decrements
  // the LoadRequest's partial-load counter, so the counter hitting zero
  // means every layer has finished and the RadianceMaps are GPU-resident.
  auto load_req = std::make_shared<asset::LoadRequest>(resolved);
  auto generic_asset = asset::AssetManager<RadianceMapsAsset>::load(load_req);
  auto rm_asset = std::dynamic_pointer_cast<RadianceMapsAsset>(generic_asset);
  if (!rm_asset) return nullptr;

  // Self-pump the deferred context queue while we wait. Caller is on the
  // GPU thread; no other thread is draining _deferredContextOps during
  // this blocking period (frame-begin would, but we may be called before
  // the render loop starts). The concurrent queue runs `op` on its own
  // worker threads; it enqueues the deferred ops we drain here.
  auto& env = GfxEnv::GetRef();
  while (load_req->_partial_load_counter.load() > 0) {
    env.processDeferredContextOps(ctx);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return rm_asset->_radiance_maps;
}

///////////////////////////////////////////////////////////////////////////////

radiancemaps_ptr_t CommonStuff::makeRadianceMapsSolidColor(fvec3 color, Context* ctx) {
  // Match the standard 10-level / pow(i/9, 0.5) layout produced by xir bakes
  // so shaders and material parameters see a familiar roughness distribution.
  constexpr int kNumRoughnessLevels = 10;
  constexpr float kRoughnessPower   = 0.5f;
  constexpr int kEnvW               = 16;
  constexpr int kEnvH               = 8;

  auto irrmaps                  = std::make_shared<RadianceMaps>();
  irrmaps->_numRoughnessLevels  = kNumRoughnessLevels;
  irrmaps->_specularRoughnessValues.resize(kNumRoughnessLevels);
  for (int i = 0; i < kNumRoughnessLevels; ++i) {
    irrmaps->_specularRoughnessValues[i] =
        powf(float(i) / float(kNumRoughnessLevels - 1), kRoughnessPower);
  }

  auto txi = ctx->TXI();

  irrmaps->_filtenvSpecularMapArray =
      txi->createColorTextureV3Array(color, kEnvW, kEnvH, kNumRoughnessLevels);
  irrmaps->_filtenvSpecularMapArray->_tex->_debugName = "procSolidSpec";

  // NOTE: createColorTextureV3 declares BGR8 but writes RGB-ordered data,
  // which swaps R and B on the GPU. Build the diffuse map via the
  // RGB8-correct Image path instead.
  auto diffuse_image = std::make_shared<Image>();
  diffuse_image->initRGB8WithColor(kEnvW, kEnvH, color);
  auto diffuse_tex         = std::make_shared<Texture>();
  diffuse_tex->_debugName  = "procSolidDiff";
  txi->initTextureFromImage(diffuse_tex.get(), diffuse_image, false, false);
  irrmaps->_filtenvDiffuseMap = diffuse_tex;

  irrmaps->_brdfIntegrationMapGGX    = PBRMaterial::brdfIntegrationMap(ctx, "GGX");
  irrmaps->_brdfIntegrationMapVelvet = PBRMaterial::brdfIntegrationMap(ctx, "GGXVELVET");
  irrmaps->_brdfIntegrationMapGGXRIM = PBRMaterial::brdfIntegrationMap(ctx, "GGXRIM");
  irrmaps->_brdfIntegrationMapBlinn  = PBRMaterial::brdfIntegrationMap(ctx, "BLINN");
  irrmaps->_brdfIntegrationMapPhong  = PBRMaterial::brdfIntegrationMap(ctx, "PHONG");

  return irrmaps;
}

///////////////////////////////////////////////////////////////////////////////

namespace {

// Linear interpolation between (sorted) gradient stops at parameter t in [0,1].
fvec3 sampleGradientAt(const std::vector<std::pair<float, fvec3>>& stops, float t) {
  if (t <= stops.front().first)  return stops.front().second;
  if (t >= stops.back().first)   return stops.back().second;
  for (size_t i = 0; i + 1 < stops.size(); ++i) {
    float t0 = stops[i].first;
    float t1 = stops[i + 1].first;
    if (t >= t0 && t <= t1) {
      float frac = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0f;
      return stops[i].second * (1.0f - frac) + stops[i + 1].second * frac;
    }
  }
  return stops.back().second;
}

// Solid-angle-weighted full-sphere average (used as the lerp target for
// high-roughness specular slices, which converge toward an isotropic blur).
fvec3 sphereAverage(const std::vector<std::pair<float, fvec3>>& stops, int n_samples = 64) {
  fvec3 acc(0, 0, 0);
  float w_sum = 0.0f;
  for (int i = 0; i < n_samples; ++i) {
    float v = (float(i) + 0.5f) / float(n_samples);
    float w = sinf(v * float(PI));
    acc   = acc + sampleGradientAt(stops, v) * w;
    w_sum = w_sum + w;
  }
  return (w_sum > 0.0f) ? acc * (1.0f / w_sum) : fvec3(0, 0, 0);
}

// Directional diffuse irradiance: cosine-weighted hemisphere integral of L(ω)
// for a surface normal at polar angle theta_n from the up-axis. Because the
// gradient is rotation-symmetric about the up-axis, the result depends only
// on theta_n (no longitude variation). This is the standard Lambertian IBL
// integral: E(n) = (1/π) ∫_hemi L(ω) max(0, n·ω) dω.
fvec3 directionalDiffuse(
    const std::vector<std::pair<float, fvec3>>& stops,
    float theta_n) {
  // n in the y-x plane, WLOG.
  float nx = sinf(theta_n);
  float ny = cosf(theta_n);

  constexpr int kThetaSamples = 32;
  constexpr int kPhiSamples   = 32;

  fvec3 acc(0, 0, 0);
  float w_sum = 0.0f;
  for (int it = 0; it < kThetaSamples; ++it) {
    float theta_w = (float(it) + 0.5f) / float(kThetaSamples) * float(PI);
    float cy      = cosf(theta_w);
    float cx      = sinf(theta_w); // also the solid-angle jacobian
    fvec3 L       = sampleGradientAt(stops, theta_w / float(PI));
    for (int ip = 0; ip < kPhiSamples; ++ip) {
      float phi = (float(ip) + 0.5f) / float(kPhiSamples) * 2.0f * float(PI);
      float ox  = cx * cosf(phi);
      float dot = nx * ox + ny * cy;
      if (dot > 0.0f) {
        float w = dot * cx;
        acc     = acc + L * w;
        w_sum   = w_sum + w;
      }
    }
  }
  return (w_sum > 0.0f) ? acc * (1.0f / w_sum) : fvec3(0, 0, 0);
}

// Build an Image where each row's color is the gradient at that V, optionally
// lerped toward `avg` to simulate roughness-induced blur.
image_ptr_t buildGradientImage(
    const std::vector<std::pair<float, fvec3>>& stops,
    fvec3 avg,
    float roughness_blend, // 0 = sharp gradient, 1 = uniform avg
    int w,
    int h) {
  auto img             = std::make_shared<Image>();
  img->_format         = EBufferFormat::RGB8;
  img->_bytesPerChannel = 1;
  img->init(w, h, 3, 1);
  auto outptr = (uint8_t*)img->_data->data();
  for (int y = 0; y < h; ++y) {
    float v       = (h > 1) ? float(y) / float(h - 1) : 0.0f;
    fvec3 c       = sampleGradientAt(stops, v);
    fvec3 blended = c * (1.0f - roughness_blend) + avg * roughness_blend;
    uint8_t r     = uint8_t(std::clamp(blended.x, 0.0f, 1.0f) * 255.0f);
    uint8_t g     = uint8_t(std::clamp(blended.y, 0.0f, 1.0f) * 255.0f);
    uint8_t b     = uint8_t(std::clamp(blended.z, 0.0f, 1.0f) * 255.0f);
    for (int x = 0; x < w; ++x) {
      int idx          = (y * w + x) * 3;
      outptr[idx + 0]  = r;
      outptr[idx + 1]  = g;
      outptr[idx + 2]  = b;
    }
  }
  return img;
}

} // namespace

radiancemaps_ptr_t CommonStuff::makeRadianceMapsGradient(
    const std::vector<std::pair<float, fvec3>>& stops, Context* ctx) {
  OrkAssert(!stops.empty());

  constexpr int kNumRoughnessLevels = 10;
  constexpr float kRoughnessPower   = 0.5f;
  constexpr int kEnvW               = 32;
  constexpr int kEnvH               = 64;

  auto irrmaps                  = std::make_shared<RadianceMaps>();
  irrmaps->_numRoughnessLevels  = kNumRoughnessLevels;
  irrmaps->_specularRoughnessValues.resize(kNumRoughnessLevels);
  for (int i = 0; i < kNumRoughnessLevels; ++i) {
    irrmaps->_specularRoughnessValues[i] =
        powf(float(i) / float(kNumRoughnessLevels - 1), kRoughnessPower);
  }

  fvec3 avg = sphereAverage(stops);

  std::vector<image_ptr_t> spec_images;
  spec_images.reserve(kNumRoughnessLevels);
  for (int i = 0; i < kNumRoughnessLevels; ++i) {
    float r = irrmaps->_specularRoughnessValues[i];
    spec_images.push_back(buildGradientImage(stops, avg, r, kEnvW, kEnvH));
  }

  auto txi          = ctx->TXI();
  auto specular_arr = std::make_shared<TextureArray>();
  specular_arr->_tex->_debugName = "procGradSpec";
  TextureArrayInitData TID;
  TID._slices.resize(kNumRoughnessLevels);
  for (int i = 0; i < kNumRoughnessLevels; ++i) {
    uint32_t usage_id = CrcString(FormatString("roughness_%d", i).c_str()).hashed();
    TID._slices[i]    = TextureArrayInitSubItem{usage_id, spec_images[i]};
  }
  txi->initTextureArray2DFromData(specular_arr.get(), TID);
  // Equirectangular: U wraps (longitude seam), V clamps (no pole bleed
  // across the wraparound when bilinear filtering at V=0 / V=1). Pairs with
  // the envtools.i2 sampling using `1.0 - uv.y` instead of `-uv.y`.
  specular_arr->_tex->TexSamplingMode()._texAddrModeS = TextureAddressMode::WRAP;
  specular_arr->_tex->TexSamplingMode()._texAddrModeT = TextureAddressMode::CLAMP;
  specular_arr->_tex->TexSamplingMode()._texAddrModeR = TextureAddressMode::CLAMP;
  txi->ApplySamplingMode(specular_arr->_tex.get());
  irrmaps->_filtenvSpecularMapArray = specular_arr;

  // Diffuse: per-row directional irradiance. The disk diffuse baker stores
  // E for normal-DOWN at texel V=0 and normal-UP at V=1; we match that
  // convention.
  auto diffuse_image                = std::make_shared<Image>();
  diffuse_image->_format            = EBufferFormat::RGB8;
  diffuse_image->_bytesPerChannel   = 1;
  diffuse_image->init(kEnvW, kEnvH, 3, 1);
  auto diff_ptr = (uint8_t*)diffuse_image->_data->data();
  for (int y = 0; y < kEnvH; ++y) {
    float v       = (kEnvH > 1) ? float(y) / float(kEnvH - 1) : 0.0f;
    float theta_n = (1.0f - v) * float(PI);  // V=0 → down, V=1 → up
    fvec3 c       = directionalDiffuse(stops, theta_n);
    uint8_t r     = uint8_t(std::clamp(c.x, 0.0f, 1.0f) * 255.0f);
    uint8_t g     = uint8_t(std::clamp(c.y, 0.0f, 1.0f) * 255.0f);
    uint8_t b     = uint8_t(std::clamp(c.z, 0.0f, 1.0f) * 255.0f);
    for (int x = 0; x < kEnvW; ++x) {
      int idx           = (y * kEnvW + x) * 3;
      diff_ptr[idx + 0] = r;
      diff_ptr[idx + 1] = g;
      diff_ptr[idx + 2] = b;
    }
  }
  auto diffuse_tex        = std::make_shared<Texture>();
  diffuse_tex->_debugName = "procGradDiff";
  txi->initTextureFromImage(diffuse_tex.get(), diffuse_image, false, false);
  diffuse_tex->TexSamplingMode()._texAddrModeS = TextureAddressMode::WRAP;
  diffuse_tex->TexSamplingMode()._texAddrModeT = TextureAddressMode::CLAMP;
  diffuse_tex->TexSamplingMode()._texAddrModeR = TextureAddressMode::CLAMP;
  txi->ApplySamplingMode(diffuse_tex.get());
  irrmaps->_filtenvDiffuseMap = diffuse_tex;

  irrmaps->_brdfIntegrationMapGGX    = PBRMaterial::brdfIntegrationMap(ctx, "GGX");
  irrmaps->_brdfIntegrationMapVelvet = PBRMaterial::brdfIntegrationMap(ctx, "GGXVELVET");
  irrmaps->_brdfIntegrationMapGGXRIM = PBRMaterial::brdfIntegrationMap(ctx, "GGXRIM");
  irrmaps->_brdfIntegrationMapBlinn  = PBRMaterial::brdfIntegrationMap(ctx, "BLINN");
  irrmaps->_brdfIntegrationMapPhong  = PBRMaterial::brdfIntegrationMap(ctx, "PHONG");

  return irrmaps;
}

///////////////////////////////////////////////////////////////////////////////

radiancemap_cache_ptr_t getRadianceMapCache() {
  static radiancemap_cache_ptr_t _instance;
  if (!_instance) {
    _instance = std::make_shared<RadianceMapCache>();
  }
  return _instance;
}

radiancemaps_ptr_t RadianceMapCache::get(const AssetPath& path) {
  std::lock_guard<std::mutex> lock(_mutex);
  auto key = path.toStdString();
  auto it = _cache.find(key);
  if (it != _cache.end()) return it->second;
  auto maps = CommonStuff::requestRadianceMaps(path);
  _cache[key] = maps;
  return maps;
}

void RadianceMapCache::clear() {
  std::lock_guard<std::mutex> lock(_mutex);
  _cache.clear();
}

///////////////////////////////////////////////////////////////////////////////
void CommonStuff::requestAndRefSkyboxTexture(asset::loadrequest_ptr_t load_req) {
  auto generic_asset = asset::AssetManager<RadianceMapsAsset>::load(load_req);
  if( auto as_radmaps = std::dynamic_pointer_cast<RadianceMapsAsset>(generic_asset) ){
    _radiance_maps = as_radmaps->_radiance_maps;
    if(0)printf("RARST: asset<%p> irrmaps<%p> pbrcommon<%p>\n", (void*) as_radmaps.get(), (void*) _radiance_maps.get(), (void*) this  );
  }
}

///////////////////////////////////////////////////////////////////////////////
lev2::texture_ptr_t CommonStuff::ssaoKernel(lev2::Context* ctx,int noise_seed){

  int seed = 0;//noise_seed%97;
  int num_samples = (_ssaoNumSamples<8) ? 8 : _ssaoNumSamples;
  uint64_t key = uint64_t(seed)<<32 | uint64_t(num_samples);

  auto it = _ssaoKernels.find(key);
  if( it == _ssaoKernels.end() ){
    // make new kernel for size and cache
    std::vector<fvec3> ssaoNoise;
    math::FRANDOMGEN R(seed);
    for (unsigned int i = 0; i < num_samples; i++) {
      glm::vec3 noise(
        R.rangedf(-1,1), 
        R.rangedf(-1,1), 
        0.0f); 
      ssaoNoise.push_back(noise);
    }     
    auto texture = std::make_shared<lev2::Texture>();
    auto txi = ctx->TXI();
    TextureInitData tid;
    tid._w = num_samples;
    tid._h = 1;
    tid._d = 1;
    tid._src_format = EBufferFormat::RGB32F;
    tid._dst_format = EBufferFormat::RGB32F;
    tid._data = ssaoNoise.data();
    tid._truncation_length = ssaoNoise.size() * sizeof(fvec3);
    txi->initTextureFromData(texture.get(), tid);
    _ssaoKernels[key] = texture;
    return texture;
  }
  return it->second;

}
///////////////////////////////////////////////////////////////////////////////
struct NoiseData {
    std::vector<fvec3> _ssaoNoise;
    texture_ptr_t _texture;
};
struct NoiseDataSet{
    std::map<uint64_t, NoiseData> _ssaoKernels;
};
using noisedataset_ptr_t = std::shared_ptr<NoiseDataSet>;
///////////////////////////////////////////////////////////////////////////////
lev2::texture_ptr_t CommonStuff::ssaoScrNoise(lev2::Context* ctx, int noise_seed, int w, int h){

  int seed = noise_seed%KNUMSSAONOISEFRAMES;

  uint64_t key = uint64_t(seed)<<32 | uint64_t(w)<<16 | uint64_t(h);

  auto it = _ssaoKernels.find(key);
  if( it == _ssaoKernels.end() ){
    printf( "spin up ssao screen noise for key<%llu>\n", (ull)key);
    // make new kernel for size and cache
    std::vector<fvec3> ssaoNoise;
    int numsamples = w*h;
    math::FRANDOMGEN R(seed);
    for (unsigned int i = 0; i < numsamples; i++) {
      int x = i % w;
      int y = i / w;
      
      glm::vec3 noise(
        R.rangedf(-1,1), 
        R.rangedf(-1,1), 
        R.rangedf(-1,1));
        
      ssaoNoise.push_back(noise);
    }     
    auto texture = std::make_shared<lev2::Texture>();
    auto txi = ctx->TXI();
    TextureInitData tid;
    tid._w = w;
    tid._h = h;
    tid._d = 1;
    tid._src_format = EBufferFormat::RGB32F;
    tid._dst_format = EBufferFormat::RGB32F;
    tid._data = ssaoNoise.data();
    tid._truncation_length = ssaoNoise.size() * sizeof(fvec3);
    txi->initTextureFromData(texture.get(), tid);
    _ssaoKernels[key] = texture;
    return texture;
  }
  return it->second;

}
///////////////////////////////////////////////////////////////////////////////
void CommonStuff::_writeEnvTexture(asset::asset_ptr_t const& tex) {
  assignEnvTexture(tex);
}
///////////////////////////////////////////////////////////////////////////////
lev2::texturearray_ptr_t CommonStuff::envSpecularTexture() const {
  return _radiance_maps->_filtenvSpecularMapArray;
}
///////////////////////////////////////////////////////////////////////////////
lev2::texture_ptr_t CommonStuff::envDiffuseTexture() const {
  return _radiance_maps->_filtenvDiffuseMap;
}
///////////////////////////////////////////////////////////////////////////////
void CommonStuff::_readEnvTexture(asset::asset_ptr_t& tex) const {
  tex = _environmentTextureAsset;
}
///////////////////////////////////////////////////////////////////////////////
void CommonStuff::setEnvTexturePath(file::Path path) {
  auto mtl_load_req = std::make_shared<asset::LoadRequest>(path);
  auto envl_asset   = asset::AssetManager<TextureAsset>::load(mtl_load_req);
  OrkAssert(false);
  // TODO - inject asset postload ops ()
}
///////////////////////////////////////////////////////////////////////////////
void CommonStuff::describeX(class_t* c) {
  using namespace asset;
  c->directProperty("ClearColor", &CommonStuff::_clearcolor);
  c->directProperty("AmbientLevel", &CommonStuff::_ambientLevel);
  c->floatProperty("EnvironmentIntensity", float_range{0, 100}, &CommonStuff::_environmentIntensity);
  c->floatProperty("EnvironmentMipBias", float_range{0, 12}, &CommonStuff::_environmentMipBias);
  c->floatProperty("EnvironmentMipScale", float_range{0, 100}, &CommonStuff::_environmentMipScale);
  c->floatProperty("DiffuseLevel", float_range{0, 10}, &CommonStuff::_diffuseLevel);
  c->floatProperty("SpecularLevel", float_range{0, 10}, &CommonStuff::_specularLevel);
  c->floatProperty("SkyboxLevel", float_range{0, 10}, &CommonStuff::_skyboxLevel);
  c->floatProperty("DepthFogDistance", float_range{0.1, 5000}, &CommonStuff::_depthFogDistance);
  c->floatProperty("DepthFogPower", float_range{0.01, 100.0}, &CommonStuff::_depthFogPower);

  /*c->accessorProperty(
       "EnvironmentTexture", //
       &CommonStuff::_readEnvTexture,
       &CommonStuff::_writeEnvTexture)
      ->annotate<ConstString>("editor.class", "ged.factory.assetlist")
      ->annotate<ConstString>("editor.assettype", "lev2tex")
      ->annotate<ConstString>("editor.assetclass", "lev2tex")
      ->annotate<asset::vars_gen_t>(
          "asset.deserialize.vargen", //
          [](ork::object_ptr_t obj) -> asset::vars_ptr_t {
            auto _this = std::dynamic_pointer_cast<CommonStuff>(obj);
            OrkAssert(_this);
            OrkAssert(false);
            return _RadianceVars();
          });*/
}
void CommonStuff::onGpuInit(Context* ctx) {
  if(not _needsGpuInit){
    return;
  }
  _needsGpuInit = false;
  auto TXI = ctx->TXI();
  _texBlack = TXI->createColorTextureV3(fvec3(0, 0, 0), 64, 64);
  _texWhite = TXI->createColorTextureV3(fvec3(1, 1, 1), 64, 64);
  _texCubeBlack = TXI->createColorCubeTexture(fvec4(0, 0, 0, 1), 64,64);
  _texCubeWhite = TXI->createColorCubeTexture(fvec4(1, 1, 1, 1), 64,64);
  _texBlackArray = TXI->createColorTextureV3Array(fvec3(0, 0, 0), 64, 64, 32);
  _texWhiteLightMapArray = TXI->createColorTextureV3Array(fvec3(1, 1, 1), 64, 64, 32);
  _texBlackLightMapArray = TXI->createColorTextureV3Array(fvec3(0,0,0), 64, 64, 32);
  _texBlack->_debugName = "black";
  _texWhite->_debugName = "white";
  _texCubeBlack->_debugName = "black_cube";
  _texCubeWhite->_debugName = "white_cube";
  _texBlackArray->_debugName = "black_array";
  _texWhiteLightMapArray->_debugName = "white_lightmap_array";
  _texBlackLightMapArray->_debugName = "white_lightmap_array";
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::pbr
