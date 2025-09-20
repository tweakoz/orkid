////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/lev2/gfx/shadman.h>
#include <ork/util/logger.h>
#include <glm/glm.hpp>

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
                                           svar64_t resource_data) {
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
  vk_shprog->_merged_resource_bindings[hpar] = DescBinding{set_id, binding_info->binding_id};
  
  // Store the resource data based on type
  switch (expected_type) {
    case VkMergedResourceBinding::Type::Sampler: {
      auto par_sampler_impl = hpar->_impl.get<VkFxShaderUniformSetSampler*>();
      auto as_vktex = resource_data.getShared<VulkanTextureObject>();
      vk_shprog->_textures_by_orkparam[hpar] = as_vktex;
      break;
    }
    case VkMergedResourceBinding::Type::UniformBlock: {
      auto as_buffer = resource_data.getShared<VulkanBuffer>();
      vk_shprog->_uniformbuffers_by_orkparam[hpar] = as_buffer;
      break;
    }
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
  if (auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>()) {
    // Push constant path
    uint32_t uval = bval ? 1 : 0;
    auto& param_set = _currentVKPASS->_vk_program->_pending_params.emplace_back();
    param_set._vk_param = as_uniset_item.value();
    param_set._ork_param = param_set._vk_param->_orkparam.get();
    param_set._value.set<uint32_t>(uval);
  }
  else if (auto as_uniblk_item = hpar->_impl.tryAs<VkFxShaderUniformBlkItem*>()) {
    // UBO path
    auto block = as_uniblk_item.value()->_parent_block;
    size_t offset = as_uniblk_item.value()->_offset;
    uint32_t uval = bval ? 1 : 0;
    memcpy(block->_shadow_buffer.data() + offset, &uval, 4);
    block->addDirtyRange(offset, 4);
    _currentVKPASS->_dirty_uniform_blocks.insert(block);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamInt(const FxShaderParam* hpar, const int ival) {
  if (auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>()) {
    auto& param_set = _currentVKPASS->_vk_program->_pending_params.emplace_back();
    param_set._vk_param = as_uniset_item.value();
    param_set._ork_param = param_set._vk_param->_orkparam.get();
    param_set._value.set<int32_t>(ival);
  }
  else if (auto as_uniblk_item = hpar->_impl.tryAs<VkFxShaderUniformBlkItem*>()) {
    auto block = as_uniblk_item.value()->_parent_block;
    size_t offset = as_uniblk_item.value()->_offset;
    memcpy(block->_shadow_buffer.data() + offset, &ival, 4);
    block->addDirtyRange(offset, 4);
    _currentVKPASS->_dirty_uniform_blocks.insert(block);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamVect2(const FxShaderParam* hpar, const fvec2& Vec) {
  if (auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>()) {
    // vec2 is fine as-is for std140 layout (8-byte alignment)
    auto& param_set = _currentVKPASS->_vk_program->_pending_params.emplace_back();
    param_set._vk_param = as_uniset_item.value();
    param_set._ork_param = param_set._vk_param->_orkparam.get();
    param_set._value.set<fvec2>(Vec);
  }
  else if (auto as_uniblk_item = hpar->_impl.tryAs<VkFxShaderUniformBlkItem*>()) {
    auto block = as_uniblk_item.value()->_parent_block;
    size_t offset = as_uniblk_item.value()->_offset;
    memcpy(block->_shadow_buffer.data() + offset, Vec.asArray(), 8);
    block->addDirtyRange(offset, 8);
    _currentVKPASS->_dirty_uniform_blocks.insert(block);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamVect3(const FxShaderParam* hpar, const fvec3& Vec) {
  if (auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>()) {
    // Convert vec3 to vec4 for Vulkan alignment (vec3 requires vec4 alignment in std140)
    fvec4 aligned_vec(Vec.x, Vec.y, Vec.z, 0.0f);
    
    auto& param_set = _currentVKPASS->_vk_program->_pending_params.emplace_back();
    param_set._vk_param = as_uniset_item.value();
    param_set._ork_param = param_set._vk_param->_orkparam.get();
    param_set._value.set<fvec4>(aligned_vec);
  } 
  else if (auto as_uniblk_item = hpar->_impl.tryAs<VkFxShaderUniformBlkItem*>()) {

    // UBO path
    auto block = as_uniblk_item.value()->_parent_block;
    size_t offset = as_uniblk_item.value()->_offset;
    
    // Vec3 needs vec4 alignment in std140
    alignas(16) float data[4] = {Vec.x, Vec.y, Vec.z, 0.0f};
    memcpy(block->_shadow_buffer.data() + offset, data, 16);
    
    // Track dirty range
    block->addDirtyRange(offset, 16);
    
    // Add block to flush list if not already there
    _currentVKPASS->_dirty_uniform_blocks.insert(block);
    
    // Debug logging for EyePostion tracking
    //if (hpar->_name == "EyePostion") {
    //  printf("UBO_UPDATE: param<EyePostion> value<%.3f %.3f %.3f> block<%s> offset<%zu> dset<%zu>\n", 
    //         Vec.x, Vec.y, Vec.z, 
    //         block->_orkparamblock ? block->_orkparamblock->_name.c_str() : "unknown", 
    //         offset,
    //         block->_descriptor_set_id);
    //}
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamVect4(const FxShaderParam* hpar, const fvec4& Vec) {
  if (auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>()) {
    auto& param_set = _currentVKPASS->_vk_program->_pending_params.emplace_back();
    param_set._vk_param = as_uniset_item.value();
    param_set._ork_param = param_set._vk_param->_orkparam.get();
    param_set._value.set<fvec4>(Vec);
  }
  else if (auto as_uniblk_item = hpar->_impl.tryAs<VkFxShaderUniformBlkItem*>()) {
    auto block = as_uniblk_item.value()->_parent_block;
    size_t offset = as_uniblk_item.value()->_offset;
    memcpy(block->_shadow_buffer.data() + offset, Vec.asArray(), 16);
    block->addDirtyRange(offset, 16);
    _currentVKPASS->_dirty_uniform_blocks.insert(block);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamVect2Array(const FxShaderParam* hpar, const fvec2* Vec, const int icount) {
  if (auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>()) {
    // Push constants - can pack as vec2s
    // TODO: Need to implement setArray in svar64_t
  }
  else if (auto as_uniblk_item = hpar->_impl.tryAs<VkFxShaderUniformBlkItem*>()) {
    auto block = as_uniblk_item.value()->_parent_block;
    size_t offset = as_uniblk_item.value()->_offset;
    
    // Each vec2 in array takes 16 bytes in std140!
    for (int i = 0; i < icount; i++) {
      memcpy(block->_shadow_buffer.data() + offset + (i * 16), Vec[i].asArray(), 8);
      // 8 bytes padding after each vec2
    }
    
    block->addDirtyRange(offset, icount * 16);
    _currentVKPASS->_dirty_uniform_blocks.insert(block);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamVect4Array(const FxShaderParam* hpar, const fvec4* Vec, const int icount) {
  if (auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>()) {
    // Push constants
    // TODO: Need to implement setArray in svar64_t
  }
  else if (auto as_uniblk_item = hpar->_impl.tryAs<VkFxShaderUniformBlkItem*>()) {
    auto block = as_uniblk_item.value()->_parent_block;
    size_t offset = as_uniblk_item.value()->_offset;
    
    memcpy(block->_shadow_buffer.data() + offset, Vec, icount * 16);
    
    block->addDirtyRange(offset, icount * 16);
    _currentVKPASS->_dirty_uniform_blocks.insert(block);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamFloatArray(const FxShaderParam* hpar, const float* pfA, const int icnt) {
  if (auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>()) {
    // Push constants - can pack tightly
    // TODO: Need to implement setArray in svar64_t
    // For now, we'll need to handle this differently
  }
  else if (auto as_uniblk_item = hpar->_impl.tryAs<VkFxShaderUniformBlkItem*>()) {
    // UBO - MUST use vec4 stride for arrays!
    auto block = as_uniblk_item.value()->_parent_block;
    size_t offset = as_uniblk_item.value()->_offset;
    
    // Each float in array takes 16 bytes in std140!
    for (int i = 0; i < icnt; i++) {
      memcpy(block->_shadow_buffer.data() + offset + (i * 16), &pfA[i], 4);
      // Padding bytes are already zero in shadow buffer
    }
    
    block->addDirtyRange(offset, icnt * 16);
    _currentVKPASS->_dirty_uniform_blocks.insert(block);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamVect3Array(const FxShaderParam* hpar, const fvec3* Vec, const int icount) {
  if (auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>()) {
    // Push constants - need to pad each vec3 to vec4
    // TODO: Need to implement setArray in svar64_t
  }
  else if (auto as_uniblk_item = hpar->_impl.tryAs<VkFxShaderUniformBlkItem*>()) {
    auto block = as_uniblk_item.value()->_parent_block;
    size_t offset = as_uniblk_item.value()->_offset;
    
    for (int i = 0; i < icount; i++) {
      float data[4] = {Vec[i].x, Vec[i].y, Vec[i].z, 0.0f};
      memcpy(block->_shadow_buffer.data() + offset + (i * 16), data, 16);
    }
        
    block->addDirtyRange(offset, icount * 16);
    _currentVKPASS->_dirty_uniform_blocks.insert(block);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamFloat(const FxShaderParam* hpar, float fA) {
  if (auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>()) {
    auto& param_set = _currentVKPASS->_vk_program->_pending_params.emplace_back();
    param_set._vk_param = as_uniset_item.value();
    param_set._ork_param = param_set._vk_param->_orkparam.get();
    param_set._value.set<float>(fA);
  }
  else if (auto as_uniblk_item = hpar->_impl.tryAs<VkFxShaderUniformBlkItem*>()) {
    auto block = as_uniblk_item.value()->_parent_block;
    size_t offset = as_uniblk_item.value()->_offset;
    memcpy(block->_shadow_buffer.data() + offset, &fA, 4);
    block->addDirtyRange(offset, 4);
    if(0)printf("VK: bindParamFloat: param<%s> value<%f> block<%p:%s> offset<0x%zx> dset<%zu>\n", 
           hpar->_name.c_str(), fA, 
           block,
           block->_orkparamblock ? block->_orkparamblock->_name.c_str() : "unknown", 
           offset,
           block->_descriptor_set_id);
    _currentVKPASS->_dirty_uniform_blocks.insert(block);
  }
  else{
    OrkAssert(false);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamMatrix(const FxShaderParam* hpar, const fmtx4& Mat) {
  if (auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>()) {
    auto& param_set = _currentVKPASS->_vk_program->_pending_params.emplace_back();
    param_set._vk_param = as_uniset_item.value();
    param_set._ork_param = param_set._vk_param->_orkparam.get();
    param_set._value.set<fmtx4>(Mat);
  }
  else if (auto as_uniblk_item = hpar->_impl.tryAs<VkFxShaderUniformBlkItem*>()) {
    auto block = as_uniblk_item.value()->_parent_block;
    size_t offset = as_uniblk_item.value()->_offset;
    // fmtx4 should already be 64 bytes, column-major
    memcpy(block->_shadow_buffer.data() + offset, &Mat, 64);
    block->addDirtyRange(offset, 64);
    _currentVKPASS->_dirty_uniform_blocks.insert(block);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamMatrix(const FxShaderParam* hpar, const fmtx3& Mat) {
  if (auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>()) {
    // Convert fmtx3 (3x3) to Vulkan-aligned mat3 layout (3x4)
    // Vulkan requires mat3 to have each column aligned to vec4 (16 bytes)
    glm::mat3 src = Mat.asGlmMat3();
    glm::mat3x4 vk_mat3; // 3 columns, 4 rows (for alignment)
    
    // Copy each column from mat3 to mat3x4
    // GLM stores matrices in column-major order
    for(int col = 0; col < 3; col++) {
      vk_mat3[col][0] = src[col][0];
      vk_mat3[col][1] = src[col][1];
      vk_mat3[col][2] = src[col][2];
      vk_mat3[col][3] = 0.0f; // padding
    }
    
    auto& param_set = _currentVKPASS->_vk_program->_pending_params.emplace_back();
    param_set._vk_param = as_uniset_item.value();
    param_set._ork_param = param_set._vk_param->_orkparam.get();
    param_set._value.set<glm::mat3x4>(vk_mat3);
  }
  else if (auto as_uniblk_item = hpar->_impl.tryAs<VkFxShaderUniformBlkItem*>()) {
    // UBO path - mat3 is 3 columns of vec4 in std140
    auto block = as_uniblk_item.value()->_parent_block;
    size_t offset = as_uniblk_item.value()->_offset;
    
    glm::mat3 src = Mat.asGlmMat3();
    for(int col = 0; col < 3; col++) {
      alignas(16) float column[4] = {
        src[col][0], src[col][1], src[col][2], 0.0f
      };
      memcpy(block->_shadow_buffer.data() + offset + (col * 16), column, 16);
    }
    
    // Track dirty range (48 bytes for mat3)
    block->addDirtyRange(offset, 48);
    _currentVKPASS->_dirty_uniform_blocks.insert(block);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamMatrixArray(const FxShaderParam* hpar, const fmtx4* MatArray, int iCount) {
  if (auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>()) {
    // Push constants
    // TODO: Need to implement setArray in svar64_t
  }
  else if (auto as_uniblk_item = hpar->_impl.tryAs<VkFxShaderUniformBlkItem*>()) {
    auto block = as_uniblk_item.value()->_parent_block;
    size_t offset = as_uniblk_item.value()->_offset;
    
    // fmtx4 should already be 64 bytes, column-major
    memcpy(block->_shadow_buffer.data() + offset, MatArray, iCount * 64);
    
    block->addDirtyRange(offset, iCount * 64);
    _currentVKPASS->_dirty_uniform_blocks.insert(block);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamU32(const FxShaderParam* hpar, uint32_t uval) {
  if (auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>()) {
    auto& param_set = _currentVKPASS->_vk_program->_pending_params.emplace_back();
    param_set._vk_param = as_uniset_item.value();
    param_set._ork_param = param_set._vk_param->_orkparam.get();
    param_set._value.set<uint32_t>(uval);
  }
  else if (auto as_uniblk_item = hpar->_impl.tryAs<VkFxShaderUniformBlkItem*>()) {
    auto block = as_uniblk_item.value()->_parent_block;
    size_t offset = as_uniblk_item.value()->_offset;
    memcpy(block->_shadow_buffer.data() + offset, &uval, 4);
    block->addDirtyRange(offset, 4);
    _currentVKPASS->_dirty_uniform_blocks.insert(block);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamU64(const FxShaderParam* hpar, uint64_t uval) {
  if (auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>()) {
    auto& param_set = _currentVKPASS->_vk_program->_pending_params.emplace_back();
    param_set._vk_param = as_uniset_item.value();
    param_set._ork_param = param_set._vk_param->_orkparam.get();
    param_set._value.set<uint64_t>(uval);
  }
  else if (auto as_uniblk_item = hpar->_impl.tryAs<VkFxShaderUniformBlkItem*>()) {
    auto block = as_uniblk_item.value()->_parent_block;
    size_t offset = as_uniblk_item.value()->_offset;
    memcpy(block->_shadow_buffer.data() + offset, &uval, 8);
    block->addDirtyRange(offset, 8);
    _currentVKPASS->_dirty_uniform_blocks.insert(block);
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
  if (_tryBindMergedResource(dummy_param.get(), VkMergedResourceBinding::Type::UniformBlock, block->_impl)) {
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
  _tryBindMergedResource(hpar, VkMergedResourceBinding::Type::Sampler, vk_tex);
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamTextureArray(const FxShaderParam* hpar, const TextureArray* tex_array) {
    if(tex_array==nullptr){
        return;
    }
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
