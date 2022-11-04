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

struct CommonStuff {

  texture_ptr_t _filtenvSpecularMap;
  texture_ptr_t _filtenvDiffuseMap;
  asset::vars_ptr_t _texAssetVarMap;

	CommonStuff(){

  _texAssetVarMap = std::make_shared<asset::vars_t>();

  _texAssetVarMap->makeValueForKey<Texture::proc_t>("postproc") = //
      [this](texture_ptr_t tex, //
      	     Context* targ, //
      	     datablock_constptr_t inp_datablock) -> datablock_ptr_t {

    /*printf(
        "EnvironmentTexture Irradiance PreProcessor tex<%p:%s> datablocklen<%zu>...\n",
        tex.get(),
        tex->_debugName.c_str(),
        inp_datablock->length());*/

    auto hasher = DataBlock::createHasher();
    hasher->accumulateString("irradiancemap-v0");
    hasher->accumulateItem<uint64_t>(inp_datablock->hash()); // data content
    hasher->finish();
    uint64_t cachekey = hasher->result();
    auto irrmapdblock = DataBlockCache::findDataBlock(cachekey);
    if (false) { // irrmapdblock) {
      // found in cache, nothing to do..
      OrkAssert(false);
    } else {
      // not found in cache, generate
      irrmapdblock = std::make_shared<DataBlock>();
      ///////////////////////////
      _filtenvSpecularMap = PBRMaterial::filterSpecularEnvMap(tex, targ);
      _filtenvDiffuseMap  = PBRMaterial::filterDiffuseEnvMap(tex, targ);
      //////////////////////////////////////////////////////////////
      DataBlockCache::setDataBlock(cachekey, irrmapdblock);
    }

    return irrmapdblock;
  };

}

};

using commonstuff_ptr_t = std::shared_ptr<CommonStuff>;


} // namespace ork::lev2::pbr {