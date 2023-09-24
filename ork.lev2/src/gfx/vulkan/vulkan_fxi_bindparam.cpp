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

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamBool(const FxShaderParam* hpar, const bool bval) {
  auto vk_param = hpar->_impl.get<VkFxShaderUniformSetItem*>();
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamInt(const FxShaderParam* hpar, const int ival) {
  auto vk_param = hpar->_impl.get<VkFxShaderUniformSetItem*>();
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamVect2(const FxShaderParam* hpar, const fvec2& Vec) {
  auto vk_param = hpar->_impl.get<VkFxShaderUniformSetItem*>();
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamVect3(const FxShaderParam* hpar, const fvec3& Vec) {
  auto vk_param = hpar->_impl.get<VkFxShaderUniformSetItem*>();
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamVect4(const FxShaderParam* hpar, const fvec4& Vec) {
  auto& param_set = _currentVKPASS->_vk_program->_pending_params.emplace_back();
  param_set._vk_param = hpar->_impl.get<VkFxShaderUniformSetItem*>();
  param_set._ork_param = param_set._vk_param->_orkparam.get();
  param_set._value.set<fvec4>(Vec);
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamVect2Array(const FxShaderParam* hpar, const fvec2* Vec, const int icount) {
  auto vk_param = hpar->_impl.get<VkFxShaderUniformSetItem*>();
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamVect3Array(const FxShaderParam* hpar, const fvec3* Vec, const int icount) {
  auto vk_param = hpar->_impl.get<VkFxShaderUniformSetItem*>();
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamVect4Array(const FxShaderParam* hpar, const fvec4* Vec, const int icount) {
  auto vk_param = hpar->_impl.get<VkFxShaderUniformSetItem*>();
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamFloatArray(const FxShaderParam* hpar, const float* pfA, const int icnt) {
  auto vk_param = hpar->_impl.get<VkFxShaderUniformSetItem*>();
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamFloat(const FxShaderParam* hpar, float fA) {
  auto& param_set = _currentVKPASS->_vk_program->_pending_params.emplace_back();
  param_set._vk_param = hpar->_impl.get<VkFxShaderUniformSetItem*>();
  param_set._ork_param = param_set._vk_param->_orkparam.get();
  param_set._value.set<float>(fA);
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamMatrix(const FxShaderParam* hpar, const fmtx4& Mat) {
  auto& param_set = _currentVKPASS->_vk_program->_pending_params.emplace_back();
  param_set._vk_param = hpar->_impl.get<VkFxShaderUniformSetItem*>();
  param_set._ork_param = param_set._vk_param->_orkparam.get();
  param_set._value.set<fmtx4>(Mat);
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamMatrix(const FxShaderParam* hpar, const fmtx3& Mat) {
  auto vk_param = hpar->_impl.get<VkFxShaderUniformSetItem*>();
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamMatrixArray(const FxShaderParam* hpar, const fmtx4* MatArray, int iCount) {
  auto vk_param = hpar->_impl.get<VkFxShaderUniformSetItem*>();
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamU32(const FxShaderParam* hpar, uint32_t uval) {
  auto vk_param = hpar->_impl.get<VkFxShaderUniformSetItem*>();
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamU64(const FxShaderParam* hpar, uint64_t uval) {
  auto vk_param = hpar->_impl.get<VkFxShaderUniformSetItem*>();
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamBlockBuffer(const FxShaderParamBlock* block, FxShaderParamBuffer* buffer) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamCTex(const FxShaderParam* hpar, const Texture* pTex) {
  auto vk_shprog = _currentVKPASS->_vk_program;
  OrkAssert(vk_shprog->_descriptors);
  //auto vk_param = hpar->_impl.get<VkFxShaderUniformSetSampler*>();
  auto vk_tex = pTex->_impl.getShared<VulkanTextureObject>();
  const VkDescriptorImageInfo& DII = vk_tex->_vkdescriptor_info;
  //const VkSampler& sampler = vk_tex->_vksampler->_vksampler;
  //const VkImageView& imgview = vk_tex->_imgobj->_vkimageview;
  auto it = vk_shprog->_samplers_by_orkparam.find(hpar);
  OrkAssert(it != vk_shprog->_samplers_by_orkparam.end());
  size_t binding_index = it->second;
  auto descriptors = vk_shprog->_descriptors;
  size_t sampler_count = descriptors->_sampler_count;
  printf( "binding_index<%zu> sampler_count<%zu>\n", binding_index, sampler_count );
  VkWriteDescriptorSet DWRITE = {};
  initializeVkStruct(DWRITE,VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
  DWRITE.dstSet = _currentPipeline->_vkDescriptorSet;
  DWRITE.dstBinding = binding_index; // The binding point in the shader
  DWRITE.descriptorCount = 1;
  DWRITE.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  DWRITE.pImageInfo = &DII;

  vkUpdateDescriptorSets( _contextVK->_vkdevice, // device
                          1, &DWRITE, // descriptor write
                          0, nullptr // descriptor copy
                          );
  
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
