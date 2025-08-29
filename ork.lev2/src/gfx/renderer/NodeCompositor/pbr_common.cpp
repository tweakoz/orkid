////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

//#include <format>
//#include <print> // mac also ahead on this...
#include <algorithm>
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

#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_light_processor_cpu.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_light_processor_simple.h>
#include <ork/lev2/gfx/radiancemaps_asset.h>
#include <ork/lev2/gfx/xir_format.h>

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
  _clearColor     = fvec4(0, 0, 0, 1);
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

radiancemaps_ptr_t CommonStuff::requestRadianceMaps(const AssetPath& texture_path) {
  // Load XIR file directly using the registered XIR loader
  auto load_req = std::make_shared<asset::LoadRequest>(texture_path);
  
  // Load using generic asset mechanism - the XIR extension will route to RadianceMapsLoader
  auto generic_asset = asset::AssetManager<RadianceMapsAsset>::load(load_req);
  if (generic_asset) {
    // Cast to RadianceMapsAsset
    auto radiancemaps_asset = std::dynamic_pointer_cast<RadianceMapsAsset>(generic_asset);
    if (radiancemaps_asset) {
      //_radiance_maps = radiancemaps_asset->_radiance_maps;
      printf("RRM: asset<%p> irrmaps<%p>\n", (void*) radiancemaps_asset.get(), (void*) radiancemaps_asset->_radiance_maps.get()  );
      return radiancemaps_asset->_radiance_maps;
    }
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
void CommonStuff::requestAndRefSkyboxTexture(asset::loadrequest_ptr_t load_req) {
  auto generic_asset = asset::AssetManager<RadianceMapsAsset>::load(load_req);
  if( auto as_radmaps = std::dynamic_pointer_cast<RadianceMapsAsset>(generic_asset) ){
    _radiance_maps = as_radmaps->_radiance_maps;
    printf("RARST: asset<%p> irrmaps<%p> pbrcommon<%p>\n", (void*) as_radmaps.get(), (void*) _radiance_maps.get(), (void*) this  );
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
    printf( "spin up ssao screen noise for key<%llu>\n", key);
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
lev2::texture_ptr_t CommonStuff::envSpecularTexture() const {
  return _radiance_maps->_filtenvSpecularMap;
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
  c->directProperty("ClearColor", &CommonStuff::_clearColor);
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
