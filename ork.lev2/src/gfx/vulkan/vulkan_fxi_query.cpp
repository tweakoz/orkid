////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ctx.h"
#include <ork/lev2/gfx/shadman.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

const FxShaderTechnique* VkFxInterface::technique(FxShader* pshader, const std::string& name) {
  auto vkshfile = pshader->_internalHandle.get<vkfxsfile_ptr_t>();
  auto it_tek = vkshfile->_vk_techniques.find(name);
  if(it_tek!=vkshfile->_vk_techniques.end()){
    vkfxstek_ptr_t tek = it_tek->second;
    return tek->_orktechnique.get();
  }
  else{
    auto shader_name = vkshfile->_shader_name;
    auto tu = vkshfile->_trans_unit;
    printf( "VkFxInterface tu<%p> shader<%s> technique<%s> not found\n", //
            (void*) tu.get(), //
            shader_name.c_str(), //
            name.c_str() );
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

const FxShaderParam* VkFxInterface::parameter(FxShader* pshader, const std::string& name) {
  const FxShaderParam* rval = nullptr;
  auto vkshfile = pshader->_internalHandle.get<vkfxsfile_ptr_t>();
  auto shader_name = vkshfile->_shader_name;
  size_t num_unisets = vkshfile->_vk_uniformsets.size();
  auto tu = vkshfile->_trans_unit;
  for( auto item : vkshfile->_vk_uniformsets ) {
    auto uniset_name = item.first;
    printf( "search uniset<%s>\n", uniset_name.c_str() );
    auto uniset = item.second;
    auto it_item = uniset->_items_by_name.find(name);
    if( it_item != uniset->_items_by_name.end() ) {
      auto item = it_item->second;
      rval = item->_orkparam.get();
      printf( "VkFxInterface tu<%p> shader<%s> parameter<%s> found>\n", //
            (void*) tu.get(), //
            shader_name.c_str(), //
            name.c_str() ); //
      break;
    }
    auto it_samp = uniset->_samplers_by_name.find(name);
    if( it_samp != uniset->_samplers_by_name.end() ) {
      auto samp = it_samp->second;
      rval = samp->_orkparam.get();
      printf( "VkFxInterface tu<%p> shader<%s> sampler<%s> found>\n", //
            (void*) tu.get(), //
            shader_name.c_str(), //
            name.c_str()); //
      break;
    }
  }
  if(rval==nullptr){
    printf( "VkFxInterface tu<%p> shader<%s> parameter<%s> not found numunisets<%zu>\n", //
            (void*) tu.get(), //
            shader_name.c_str(), //
            name.c_str(), //
            num_unisets );
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

const FxShaderParamBlock* VkFxInterface::parameterBlock(FxShader* pshader, const std::string& name) {
  const FxShaderParamBlock* rval = nullptr;
  auto vkshfile = pshader->_internalHandle.get<vkfxsfile_ptr_t>();
  auto tu = vkshfile->_trans_unit;
  auto it = vkshfile->_vk_uniformblks.find(name);
  if( it != vkshfile->_vk_uniformblks.end() ) {
    printf( "found uniblk<%s>\n", name.c_str() );
    auto vk_uniblk = it->second;
    auto ork_uniblk = vk_uniblk->_orkparamblock;
    rval = ork_uniblk.get();
  }
  else{
    auto vkshfile = pshader->_internalHandle.get<vkfxsfile_ptr_t>();
    auto shader_name = vkshfile->_shader_name;
    size_t num_uniblks = vkshfile->_vk_uniformblks.size();
    printf( "VkFxInterface tu<%p> shader<%s> uniblock<%s> not found numuniblks<%zu>\n", //
            (void*) tu.get(), //
            shader_name.c_str(), //
            name.c_str(), //
            num_uniblks );
    OrkAssert(false);
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
#if defined(ENABLE_COMPUTE_SHADERS)
///////////////////////////////////////////////////////////////////////////////
const FxComputeShader* VkFxInterface::computeShader(FxShader* pshader, const std::string& name) {
  auto vkshfile = pshader->_internalHandle.get<vkfxsfile_ptr_t>();
  OrkAssert(false);
  return nullptr;
}
const FxShaderStorageBlock* VkFxInterface::storageBlock(FxShader* pshader, const std::string& name) {
  auto vkshfile = pshader->_internalHandle.get<vkfxsfile_ptr_t>();
  OrkAssert(false);
  return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
#endif
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
