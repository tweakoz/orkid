////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/lev2/gfx/shadman.h>
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Helper function to find a binding in merged resources by parameter name
// Returns a pair of (set_id, binding*) or (UINT32_MAX, nullptr) if not found
///////////////////////////////////////////////////////////////////////////////

std::pair<uint32_t, VkMergedResourceBinding*> findBindingInMergedResources(
    const vk_merged_resources_ptr_t& merged_resources, 
    const std::string& param_name) {
  if (!merged_resources) {
    return {UINT32_MAX, nullptr};
  }
  for (const auto& [set_id, sources] : merged_resources->descriptor_sets) {
    for (const auto& source : sources) {
      for (const auto& binding : source->bindings) {
        if (binding->name == param_name) {
          return {uint32_t(set_id), binding.get()};
        }
      }
    }
  }
  return {UINT32_MAX, nullptr};
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamBool(const FxShaderParam* hpar, const bool bval) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamInt(const FxShaderParam* hpar, const int ival) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamVect2(const FxShaderParam* hpar, const fvec2& Vec) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamVect3(const FxShaderParam* hpar, const fvec3& Vec) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamVect4(const FxShaderParam* hpar, const fvec4& Vec) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
    auto& param_set      = _currentVKPASS->_vk_program->_pending_params.emplace_back();
    param_set._vk_param  = as_uniset_item.value();
    param_set._ork_param = param_set._vk_param->_orkparam.get();
    param_set._value.set<fvec4>(Vec);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamVect2Array(const FxShaderParam* hpar, const fvec2* Vec, const int icount) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamVect3Array(const FxShaderParam* hpar, const fvec3* Vec, const int icount) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamVect4Array(const FxShaderParam* hpar, const fvec4* Vec, const int icount) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamFloatArray(const FxShaderParam* hpar, const float* pfA, const int icnt) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamFloat(const FxShaderParam* hpar, float fA) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ) {
    auto& param_set      = _currentVKPASS->_vk_program->_pending_params.emplace_back();
    param_set._vk_param  = as_uniset_item.value();
    param_set._ork_param = param_set._vk_param->_orkparam.get();
    param_set._value.set<float>(fA);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamMatrix(const FxShaderParam* hpar, const fmtx4& Mat) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ) {
    auto& param_set      = _currentVKPASS->_vk_program->_pending_params.emplace_back();
    param_set._vk_param  = as_uniset_item.value();
    param_set._ork_param = param_set._vk_param->_orkparam.get();
    param_set._value.set<fmtx4>(Mat);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamMatrix(const FxShaderParam* hpar, const fmtx3& Mat) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamMatrixArray(const FxShaderParam* hpar, const fmtx4* MatArray, int iCount) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamU32(const FxShaderParam* hpar, uint32_t uval) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamU64(const FxShaderParam* hpar, uint64_t uval) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindUniformBuffer(const FxUniformBlock* block, FxUniformBuffer* buffer) {
  /*if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }*/
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamTexture(const FxShaderParam* hpar, const Texture* pTex) {
  auto vk_shprog = _currentVKPASS->_vk_program;
  if (!pTex) {
    return;
  }
  vktexobj_ptr_t vk_tex;
  if (auto as_to = pTex->_impl.tryAsShared<VulkanTextureObject>()) {
    vk_tex = as_to.value();
  } else {
    printf("No Texture impl tex<%p:%s>\n", pTex, pTex->_debugName.c_str());
    return;
  }
  // Find binding info in merged resources
  if (_currentVKPASS && _currentVKPASS->_merged_resources) {
    auto [set_id, binding_info] = findBindingInMergedResources(_currentVKPASS->_merged_resources, hpar->_name);
    if (binding_info && binding_info->type == VkMergedResourceBinding::Type::Sampler) {
      if(0)printf("bindParamTexture: param<%s> -> merged resource set<%d> binding<%d>\n", 
             hpar->_name.c_str(), set_id, binding_info->binding_id);
      vk_shprog->_merged_resource_bindings[hpar] = std::make_pair(set_id, binding_info->binding_id);
      vk_shprog->_textures_by_orkparam[hpar] = vk_tex;
      return;
    }
  }
  printf("bindParamTexture: param<%s> not found in merged resources\n", hpar->_name.c_str());
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamTextureArray(const FxShaderParam* hpar, const TextureArray* tex_array) {
  OrkAssert(tex_array);
  OrkAssert(tex_array->_tex);
  
  auto vk_shprog = _currentVKPASS->_vk_program;
    
  if (tex_array && tex_array->_tex) {
    // For texture arrays, use the same logic as regular textures
    // The difference is in the shader (sampler2DArray vs sampler2D)
    bindParamTexture(hpar, tex_array->_tex.get());
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
