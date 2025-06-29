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

bool VkFxInterface::_tryBindMergedResource(const FxShaderParam* hpar,
                                          VkMergedResourceBinding::Type expected_type,
                                          void* resource_data) {
  if (!_currentVKPASS) {
    printf("_tryBindMergedResource: _currentVKPASS is null for param<%s>\n", hpar->_name.c_str());
    return false;
  }
  
  auto vk_shprog = _currentVKPASS->_vk_program;
  if (!vk_shprog) {
    printf("_tryBindMergedResource: _vk_program is null for param<%s>\n", hpar->_name.c_str());
    return false;
  }
  
  if (!_currentVKPASS->_merged_resources) {
    return false;
  }
  
  // Find binding info in merged resources
  auto [set_id, binding_info] = findBindingInMergedResources(_currentVKPASS->_merged_resources, hpar->_name);
  
  if (!binding_info) {
    printf("_tryBindMergedResource: param<%s> not found in merged resources\n", hpar->_name.c_str());
    return false;
  }
  
  if (binding_info->type != expected_type) {
    printf("_tryBindMergedResource: param<%s> type mismatch - expected<%d> actual<%d>\n", 
           hpar->_name.c_str(), 
           static_cast<int>(expected_type), 
           static_cast<int>(binding_info->type));
    return false;
  }
  
  // Store the binding info
  vk_shprog->_merged_resource_bindings[hpar] = std::make_pair(set_id, binding_info->binding_id);
  
  // Store the resource data based on type
  switch (expected_type) {
    case VkMergedResourceBinding::Type::Sampler:
      if (resource_data) {
        vk_shprog->_textures_by_orkparam[hpar] = *static_cast<vktexobj_ptr_t*>(resource_data);
      }
      break;
    case VkMergedResourceBinding::Type::UniformBlock:
      if (resource_data) {
        vk_shprog->_uniformbuffers_by_orkparam[hpar] = *static_cast<vkbuffer_ptr_t*>(resource_data);
      }
      break;
    case VkMergedResourceBinding::Type::StorageBuffer:
      // TODO: Add storage for storage buffers when the data structure is added
      break;
  }
  
  if(0)printf("_tryBindMergedResource: param<%s> -> merged resource set<%d> binding<%d>\n", 
         hpar->_name.c_str(), set_id, binding_info->binding_id);
  
  return true;
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
  if (!block || !buffer) {
    return;
  }
  
  if (!_currentVKPASS) {
    printf("bindUniformBuffer: _currentVKPASS is null for block<%s>\n", block->_name.c_str());
    return;
  }
  
  auto vk_shprog = _currentVKPASS->_vk_program;
  if (!vk_shprog) {
    printf("bindUniformBuffer: _vk_program is null for block<%s>\n", block->_name.c_str());
    return;
  }
  
  // Get the Vulkan buffer implementation
  auto vk_buffer = buffer->_impl.tryAsShared<VulkanBuffer>();
  if (!vk_buffer) {
    printf("bindUniformBuffer: buffer has no Vulkan implementation\n");
    return;
  }
  
  // Get the Vulkan uniform block from the block's implementation
  auto vk_block = block->_impl.tryAs<VkFxShaderUniformBlk*>();
  if (!vk_block) {
    printf("bindUniformBuffer: block<%s> has no Vulkan implementation\n", block->_name.c_str());
    return;
  }
  
  // Find the associated parameter for this block
  // First check if the block has an associated orkparam
  auto vk_blk_impl = vk_block.value();
  if (!vk_blk_impl || !vk_blk_impl->_orkparamblock) {
    printf("bindUniformBuffer: block<%s> has no associated parameter\n", block->_name.c_str());
    return;
  }
  
  // The FxUniformBlock should have a pseudo-parameter that represents the block binding
  // Try to bind via merged resources using the block name as the parameter name
  auto dummy_param = std::make_shared<FxShaderParam>();
  dummy_param->_name = block->_name;
  
  // Store the buffer in the program's uniform buffer map
  if (_tryBindMergedResource(dummy_param.get(), VkMergedResourceBinding::Type::UniformBlock, &vk_buffer)) {
    // Success
  } else {
    printf("bindUniformBuffer: failed to bind block<%s> via merged resources\n", block->_name.c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamTexture(const FxShaderParam* hpar, const Texture* pTex) {
  if (!pTex) {
    return;
  }
  
  vktexobj_ptr_t vk_tex;
  if (auto as_to = pTex->_impl.tryAsShared<VulkanTextureObject>()) {
    vk_tex = as_to.value();
  } else {
    // Use default texture based on texture type
    switch (pTex->_texType) {
      case ETEXTYPE_2D:
        vk_tex = _contextVK->_defaultTexImpl2D;
        break;
      case ETEXTYPE_CUBE:
        vk_tex = _contextVK->_defaultTexImplCube;
        break;
      case ETEXTYPE_2D_ARRAY:
        vk_tex = _contextVK->_defaultTexImpl2DArray;
        break;
      case ETEXTYPE_3D:
        vk_tex = _contextVK->_defaultTexImpl3D;
        break;
      default:
        // For unsupported types, use 2D as fallback
        vk_tex = _contextVK->_defaultTexImpl2D;
        break;
    }
    //printf("Using default texture for tex<%p:%s> type<%d>\n", pTex, pTex->_debugName.c_str(), pTex->_texType);
  }
  
  // Try to bind via merged resources
  _tryBindMergedResource(hpar, VkMergedResourceBinding::Type::Sampler, &vk_tex);
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
