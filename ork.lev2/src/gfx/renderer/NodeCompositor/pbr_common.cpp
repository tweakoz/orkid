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

namespace {

constexpr int   kProcNumRoughnessLevels = 10;
constexpr float kProcRoughnessPower     = 0.5f;
constexpr int   kProcEnvW               = 32;
constexpr int   kProcEnvH               = 64;

fvec3 sampleGradientAt(const std::vector<std::pair<float, fvec3>>& stops, float t) {
  if (t <= stops.front().first) return stops.front().second;
  if (t >= stops.back().first)  return stops.back().second;
  for (size_t i = 0; i + 1 < stops.size(); ++i) {
    float t0 = stops[i].first, t1 = stops[i + 1].first;
    if (t >= t0 && t <= t1) {
      float frac = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0f;
      return stops[i].second * (1.0f - frac) + stops[i + 1].second * frac;
    }
  }
  return stops.back().second;
}

// Solid-angle-weighted full-sphere average; lerp target for high-roughness slices.
fvec3 sphereAverage(const std::vector<std::pair<float, fvec3>>& stops, int n = 64) {
  fvec3 acc(0); float w_sum = 0.0f;
  for (int i = 0; i < n; ++i) {
    float v = (float(i) + 0.5f) / float(n);
    float w = sinf(v * float(PI));
    acc   = acc + sampleGradientAt(stops, v) * w;
    w_sum = w_sum + w;
  }
  return (w_sum > 0.0f) ? acc * (1.0f / w_sum) : fvec3(0);
}

// Cosine-weighted hemisphere integral E(n) = (1/π) ∫ L(ω) max(0, n·ω) dω,
// for a surface normal at polar angle theta_n. Rotation-symmetric env so the
// result depends only on theta_n.
fvec3 directionalDiffuse(const std::vector<std::pair<float, fvec3>>& stops, float theta_n) {
  float nx = sinf(theta_n), ny = cosf(theta_n);
  fvec3 acc(0); float w_sum = 0.0f;
  constexpr int kT = 32, kP = 32;
  for (int it = 0; it < kT; ++it) {
    float theta_w = (float(it) + 0.5f) / float(kT) * float(PI);
    float cy = cosf(theta_w), cx = sinf(theta_w);
    fvec3 L = sampleGradientAt(stops, theta_w / float(PI));
    for (int ip = 0; ip < kP; ++ip) {
      float phi = (float(ip) + 0.5f) / float(kP) * 2.0f * float(PI);
      float dot = nx * (cx * cosf(phi)) + ny * cy;
      if (dot > 0.0f) {
        float w = dot * cx;
        acc   = acc + L * w;
        w_sum = w_sum + w;
      }
    }
  }
  return (w_sum > 0.0f) ? acc * (1.0f / w_sum) : fvec3(0);
}

// RGBA32F image filled with a per-row color via `row_color(v)`.
template<typename F>
image_ptr_t buildRowwiseImage(int w, int h, F&& row_color) {
  auto img              = std::make_shared<Image>();
  img->_format          = EBufferFormat::RGBA32F;
  img->_bytesPerChannel = 4;
  img->init(w, h, 4, 4);
  auto p = (float*)img->_data->data();
  for (int y = 0; y < h; ++y) {
    float v = (h > 1) ? float(y) / float(h - 1) : 0.0f;
    fvec3 c = row_color(v);
    for (int x = 0; x < w; ++x) {
      int idx = (y * w + x) * 4;
      p[idx + 0] = c.x; p[idx + 1] = c.y; p[idx + 2] = c.z; p[idx + 3] = 1.0f;
    }
  }
  return img;
}

// In-place upload — reuses VkImage / TextureArray slices, no descriptor churn.
void uploadRadianceMapsImages(
    radiancemaps_ptr_t maps,
    const std::vector<image_ptr_t>& spec_images,
    image_ptr_t diffuse_image,
    Context* ctx) {
  OrkAssert(int(spec_images.size()) == maps->_numRoughnessLevels);
  auto txi = ctx->TXI();
  for (size_t i = 0; i < spec_images.size(); ++i) {
    auto slice = maps->_filtenvSpecularMapArray->slice(i);
    txi->updateTextureArraySlice(slice.get(), spec_images[i]);
  }
  txi->initTextureFromImage(maps->_filtenvDiffuseMap.get(), diffuse_image, false, false);
}

} // namespace

///////////////////////////////////////////////////////////////////////////////

radiancemaps_ptr_t CommonStuff::makeProceduralRadianceMaps(Context* ctx) {
  auto maps = std::make_shared<RadianceMaps>();
  maps->_numRoughnessLevels = kProcNumRoughnessLevels;
  maps->_specularRoughnessValues.resize(kProcNumRoughnessLevels);
  for (int i = 0; i < kProcNumRoughnessLevels; ++i) {
    maps->_specularRoughnessValues[i] =
        powf(float(i) / float(kProcNumRoughnessLevels - 1), kProcRoughnessPower);
  }

  auto black = buildRowwiseImage(kProcEnvW, kProcEnvH, [](float) { return fvec3(0); });

  auto txi          = ctx->TXI();
  auto specular_arr = std::make_shared<TextureArray>();
  specular_arr->_tex->_debugName = "procRadSpec";
  TextureArrayInitData TID;
  TID._slices.resize(kProcNumRoughnessLevels);
  for (int i = 0; i < kProcNumRoughnessLevels; ++i) {
    uint32_t usage_id = CrcString(FormatString("roughness_%d", i).c_str()).hashed();
    TID._slices[i]    = TextureArrayInitSubItem{usage_id, black};
  }
  txi->initTextureArray2DFromData(specular_arr.get(), TID);

  auto diffuse_tex        = std::make_shared<Texture>();
  diffuse_tex->_debugName = "procRadDiff";
  txi->initTextureFromImage(diffuse_tex.get(), black, false, false);

  // Equirectangular: U wraps, V clamps (no pole bleed at V=0/V=1).
  auto setEquirectangularSampling = [&](Texture* tex) {
    tex->TexSamplingMode()._texAddrModeS = TextureAddressMode::WRAP;
    tex->TexSamplingMode()._texAddrModeT = TextureAddressMode::CLAMP;
    tex->TexSamplingMode()._texAddrModeR = TextureAddressMode::CLAMP;
    txi->ApplySamplingMode(tex);
  };
  setEquirectangularSampling(specular_arr->_tex.get());
  setEquirectangularSampling(diffuse_tex.get());

  maps->_filtenvSpecularMapArray = specular_arr;
  maps->_filtenvDiffuseMap       = diffuse_tex;

  maps->_brdfIntegrationMapGGX    = PBRMaterial::brdfIntegrationMap(ctx, "GGX");
  maps->_brdfIntegrationMapVelvet = PBRMaterial::brdfIntegrationMap(ctx, "GGXVELVET");
  maps->_brdfIntegrationMapGGXRIM = PBRMaterial::brdfIntegrationMap(ctx, "GGXRIM");
  maps->_brdfIntegrationMapBlinn  = PBRMaterial::brdfIntegrationMap(ctx, "BLINN");
  maps->_brdfIntegrationMapPhong  = PBRMaterial::brdfIntegrationMap(ctx, "PHONG");

  return maps;
}

void CommonStuff::updateRadianceMapsSolidColor(
    radiancemaps_ptr_t maps, fvec3 color, Context* ctx) {
  OrkAssert(maps && maps->_filtenvSpecularMapArray);
  int W = int(maps->_filtenvSpecularMapArray->_width);
  int H = int(maps->_filtenvSpecularMapArray->_height);
  auto img = buildRowwiseImage(W, H, [color](float) { return color; });
  std::vector<image_ptr_t> spec(maps->_numRoughnessLevels, img);
  uploadRadianceMapsImages(maps, spec, img, ctx);
}

void CommonStuff::updateRadianceMapsGradient(
    radiancemaps_ptr_t maps,
    const std::vector<std::pair<float, fvec3>>& stops,
    Context* ctx) {
  OrkAssert(maps && maps->_filtenvSpecularMapArray);
  OrkAssert(!stops.empty());
  int W = int(maps->_filtenvSpecularMapArray->_width);
  int H = int(maps->_filtenvSpecularMapArray->_height);
  fvec3 avg = sphereAverage(stops);

  // Specular slices: gradient at V, lerped toward sphere-average by the
  // slice's roughness. Diffuse: per-row directional irradiance, V-flipped to
  // match the disk diffuse baker convention (V=0 → down, V=1 → up).
  std::vector<image_ptr_t> spec;
  spec.reserve(maps->_specularRoughnessValues.size());
  for (float r : maps->_specularRoughnessValues) {
    spec.push_back(buildRowwiseImage(W, H, [&](float v) {
      fvec3 c = sampleGradientAt(stops, v);
      return c * (1.0f - r) + avg * r;
    }));
  }
  auto diff = buildRowwiseImage(W, H, [&](float v) {
    return directionalDiffuse(stops, (1.0f - v) * float(PI));
  });
  uploadRadianceMapsImages(maps, spec, diff, ctx);
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
