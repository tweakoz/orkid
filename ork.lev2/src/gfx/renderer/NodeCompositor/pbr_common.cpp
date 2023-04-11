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

#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_light_processor_cpu.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_light_processor_simple.h>

#include <ork/profiling.inl>
#include <ork/asset/Asset.inl>

ImplementReflectionX(ork::lev2::pbr::CommonStuff, "pbr::CommonStuff");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::pbr {
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
          [](ork::object_ptr_t obj) -> asset::vars_t {
            auto _this = std::dynamic_pointer_cast<CommonStuff>(obj);
            OrkAssert(_this);
            OrkAssert(false);
            return _this->_texAssetVarMap;
          });

}
///////////////////////////////////////////////////////////////////////////////
CommonStuff::CommonStuff() {
  _clearColor = fvec4(0,0,0,1);

  _texAssetVarMap.makeValueForKey<Texture::proc_t>("postproc") = //
      [this](texture_ptr_t tex, //
             Context* targ, //
             datablock_constptr_t inp_datablock) -> datablock_ptr_t {

    printf(
        "EnvironmentTexture Irradiance PreProcessor tex<%p:%s> datablocklen<%zu>...\n",
        tex.get(),
        tex->_debugName.c_str(),
        inp_datablock->length());

    auto hasher = DataBlock::createHasher();
    hasher->accumulateString("irradiancemap-v1");
    hasher->accumulateItem<uint64_t>(inp_datablock->hash()); // data content
    hasher->finish();
    uint64_t cachekey = hasher->result();
    auto irrmapdblock = DataBlockCache::findDataBlock(cachekey);
    if (false){ //irrmapdblock) {
      // found in cache, nothing to do..
    } else {
      // not found in cache, generate
      irrmapdblock = std::make_shared<DataBlock>();
      ///////////////////////////
      printf( "HELLO1\n");
      _filtenvSpecularMap = PBRMaterial::filterSpecularEnvMap(tex, targ);
      printf( "HELLO2\n");
      _filtenvDiffuseMap  = PBRMaterial::filterDiffuseEnvMap(tex, targ);
      _brdfIntegrationMap = PBRMaterial::brdfIntegrationMap(targ);
      //_environmentMipScale = _filtenvSpecularMap->_num_mips-1;
      //////////////////////////////////////////////////////////////
      DataBlockCache::setDataBlock(cachekey, irrmapdblock);
    }

    return irrmapdblock;
  };

}
///////////////////////////////////////////////////////////////////////////////
void CommonStuff::_writeEnvTexture(asset::asset_ptr_t const& tex) {
  assignEnvTexture(tex);
}
///////////////////////////////////////////////////////////////////////////////
void CommonStuff::assignEnvTexture(asset::asset_ptr_t texasset){
  asset::vars_t old_varmap;
  if(_environmentTextureAsset){
    old_varmap = _environmentTextureAsset->_varmap;
    printf("OLD <%p:%s>\n\n", _environmentTextureAsset.get(),_environmentTextureAsset->name().c_str());
  }
  printf("NEW <%p:%s>\n\n", texasset.get(),texasset->name().c_str());

  _environmentTextureAsset = texasset;
  if (nullptr == _environmentTextureAsset)
    return;
  _environmentTextureAsset->_varmap = _texAssetVarMap;
}
///////////////////////////////////////////////////////////////////////////////
  asset::loadrequest_ptr_t CommonStuff::requestSkyboxTexture(const AssetPath& texture_path) {
    auto load_req = std::make_shared<asset::LoadRequest>(texture_path);
    load_req->_asset_vars = _texAssetVarMap;
    auto enviromentmap_asset = asset::AssetManager<lev2::TextureAsset>::load(load_req);
    OrkAssert(enviromentmap_asset->GetTexture() != nullptr);
    OrkAssert(enviromentmap_asset->_varmap.hasKey("postproc"));
    assignEnvTexture(enviromentmap_asset);
    return load_req;
  }

///////////////////////////////////////////////////////////////////////////////
lev2::texture_ptr_t CommonStuff::envSpecularTexture() const{
  return _filtenvSpecularMap;
}
///////////////////////////////////////////////////////////////////////////////
lev2::texture_ptr_t CommonStuff::envDiffuseTexture() const{
  return _filtenvDiffuseMap;
}
///////////////////////////////////////////////////////////////////////////////
void CommonStuff::_readEnvTexture(asset::asset_ptr_t& tex) const {
  tex = _environmentTextureAsset;
}
///////////////////////////////////////////////////////////////////////////////
void CommonStuff::setEnvTexturePath(file::Path path) {
  auto mtl_load_req = std::make_shared<asset::LoadRequest>(path);
  auto envl_asset = asset::AssetManager<TextureAsset>::load(mtl_load_req);
  OrkAssert(false);
  // TODO - inject asset postload ops ()
}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::pbr {
