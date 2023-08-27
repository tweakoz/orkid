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
  OrkAssert(it_tek!=vkshfile->_vk_techniques.end());
  vkfxstek_ptr_t tek = it_tek->second;
  return tek->_orktechnique.get();
}

///////////////////////////////////////////////////////////////////////////////

const FxShaderParam* VkFxInterface::parameter(FxShader* pshader, const std::string& name) {
  auto vkshfile = pshader->_internalHandle.get<vkfxsfile_ptr_t>();
  OrkAssert(false);
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

const FxShaderParamBlock* VkFxInterface::parameterBlock(FxShader* pshader, const std::string& name) {
  auto vkshfile = pshader->_internalHandle.get<vkfxsfile_ptr_t>();
  OrkAssert(false);
  return nullptr;
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
