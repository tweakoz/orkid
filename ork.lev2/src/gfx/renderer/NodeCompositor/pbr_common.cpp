////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <algorithm>
#include <ork/pch.h>
#include <ork/rtti/Class.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/mutex.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
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

#include <ork/profiling.inl>
#include <ork/asset/Asset.inl>

ImplementReflectionX(ork::lev2::pbr::CommonStuff, "pbr::CommonStuff");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::pbr {

constexpr size_t KNUMSSAONOISEFRAMES = 2;

static logchannel_ptr_t logchan_pbrcom = logger()->createChannel("PBRCOM", fvec3(0.8, 0.8, 0.5), true);


///////////////////////////////////////////////////////////////////////////////

static asset::vars_ptr_t _irradianceVars() {

  auto vars                                          = std::make_shared<asset::vars_t>();
  vars->makeValueForKey<Texture::proc_t>("postproc") = //
      [vars](
          texture_ptr_t tex, //
          Context* targ,     //
          datablock_constptr_t inp_datablock) -> datablock_ptr_t {
    logchan_pbrcom->log(
        "EnvironmentTexture Irradiance PreProcessor tex<%p:%s> datablocklen<%zu>",
        tex.get(),
        tex->_debugName.c_str(),
        inp_datablock->length());

    auto hasher = DataBlock::createHasher();
    hasher->accumulateString("irradiancemap-v1");
    hasher->accumulateItem<uint64_t>(inp_datablock->hash()); // data content
    hasher->finish();
    uint64_t cachekey = hasher->result();

    //////////////////////////////////////////
    // TODO cache at this level...
    //////////////////////////////////////////

    auto irrmapdblock = DataBlockCache::findDataBlock(cachekey);
    if (false) { // irrmapdblock) {
      // found in cache, nothing to do..
    } else {
      // not found in cache, generate
      irrmapdblock = std::make_shared<DataBlock>();
      ///////////////////////////

      auto load_req = tex->loadRequest();
      OrkAssert(load_req);

      auto equirectangular = load_req->_asset_vars->typedValueForKey<bool>("equirectangular").value();


      auto filtenvSpecularMap = PBRMaterial::filterSpecularEnvMap(tex, targ,equirectangular);
      auto filtenvDiffuseMap  = PBRMaterial::filterDiffuseEnvMap(tex, targ,equirectangular);
      auto brdfIntegrationMap = PBRMaterial::brdfIntegrationMap(targ);

      load_req->_asset_vars->makeValueForKey<texture_ptr_t>("irrmap_spec") = filtenvSpecularMap;
      load_req->_asset_vars->makeValueForKey<texture_ptr_t>("irrmap_diff") = filtenvDiffuseMap;
      load_req->_asset_vars->makeValueForKey<texture_ptr_t>("brdf_map") = brdfIntegrationMap;

      auto irrmaps = load_req->_asset_vars->typedValueForKey<irradiancemaps_ptr_t>("irrmaps").value();
      irrmaps->_filtenvSpecularMap = filtenvSpecularMap;
      irrmaps->_filtenvDiffuseMap  = filtenvDiffuseMap;
      irrmaps->_brdfIntegrationMap = brdfIntegrationMap;
      //_environmentMipScale = _filtenvSpecularMap->_num_mips-1;
      //////////////////////////////////////////////////////////////
      DataBlockCache::setDataBlock(cachekey, irrmapdblock);
    }

    return irrmapdblock;
  };
  return vars;
}

///////////////////////////////////////////////////////////////////////////////
CommonStuff::CommonStuff() {

  _irradianceMaps = std::make_shared<IrradianceMaps>();
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
  //_environmentTextureAsset->_varmap = *_irradianceVars();
}
///////////////////////////////////////////////////////////////////////////////

irradiancemaps_ptr_t CommonStuff::requestIrradianceMaps(const AssetPath& texture_path) {
  auto load_req            = std::make_shared<asset::LoadRequest>(texture_path);
  load_req->_asset_vars    = _irradianceVars();
  auto irrmaps = std::make_shared<IrradianceMaps>();
  load_req->_asset_vars->makeValueForKey<bool>("equirectangular") = false;
  load_req->_asset_vars->makeValueForKey<irradiancemaps_ptr_t>("irrmaps") = irrmaps;
  auto enviromentmap_asset = asset::AssetManager<lev2::TextureAsset>::load(load_req);
  OrkAssert(enviromentmap_asset->GetTexture() != nullptr);
  OrkAssert(enviromentmap_asset->_varmap.hasKey("postproc"));
  return irrmaps;
}

///////////////////////////////////////////////////////////////////////////////
void CommonStuff::requestAndRefSkyboxTexture(asset::loadrequest_ptr_t load_req) {
  auto texture_path = load_req->_asset_path;
  load_req->_asset_vars    = _irradianceVars();
  load_req->_asset_vars->makeValueForKey<bool>("equirectangular") = true;
  load_req->_asset_vars->makeValueForKey<irradiancemaps_ptr_t>("irrmaps") = _irradianceMaps;
  _irradianceMaps->_loadRequest = load_req;
  opq::mainSerialQueue()->enqueue([=]() {
    auto enviromentmap_asset = asset::AssetManager<lev2::TextureAsset>::load(load_req);
    OrkAssert(enviromentmap_asset->GetTexture() != nullptr);
    OrkAssert(enviromentmap_asset->_varmap.hasKey("postproc"));
    assignEnvTexture(enviromentmap_asset);
    if(load_req->_on_load_complete)
      load_req->_on_load_complete();
  });
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
    printf( "spin up ssao screen noise for key<%zu>\n", key);
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
  return _irradianceMaps->_filtenvSpecularMap;
}
///////////////////////////////////////////////////////////////////////////////
lev2::texture_ptr_t CommonStuff::envDiffuseTexture() const {
  return _irradianceMaps->_filtenvDiffuseMap;
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

  c->accessorProperty(
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
            return _irradianceVars();
          });
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::pbr
