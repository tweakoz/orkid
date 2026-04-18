////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/lev2/gfx/shadman.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

FxUniformBuffer* VkFxInterface::createUniformBuffer(size_t length) {
  auto ub = new FxUniformBuffer;
  ub->_length         = length;
  auto uniblk_buf = ub->_impl.makeShared<VulkanBuffer>(_contextVK, length, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  return ub;
}

///////////////////////////////////////////////////////////////////////////////

fxuniformbuffermapping_ptr_t VkFxInterface::mapUniformBuffer(FxUniformBuffer* b, //
                                                             size_t base, //
                                                             size_t length) { //

  auto bufimpl = b->_impl.getShared<VulkanBuffer>();
  auto mapping = std::make_shared<FxUniformBufferMapping>();
  mapping->_buffer = b;
  mapping->_offset   = base;
  mapping->_fxi    = this;  // Fix: Set the FxInterface pointer
  if(length==0){
    mapping->_length = bufimpl->_length;
  }
  else{
    mapping->_length   = length;
  }
  mapping->_mappedaddr = bufimpl->map(base, mapping->_length,0);
  return mapping;
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::unmapUniformBuffer(FxUniformBufferMapping* mapping) {
  auto bufimpl = mapping->_buffer->_impl.getShared<VulkanBuffer>();
  bufimpl->unmap();
  mapping->_impl.make<void*>(nullptr);
  mapping->_mappedaddr = nullptr; // Clear the mapped address
}

///////////////////////////////////////////////////////////////////////////////

FxShaderStorageBuffer* VkFxInterface::createStorageBuffer(size_t length) {
  auto ssbo = new FxShaderStorageBuffer;
  ssbo->_length = length;
  auto ssbo_buf = ssbo->_impl.makeShared<VulkanBuffer>(_contextVK, length,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  return ssbo;
}

///////////////////////////////////////////////////////////////////////////////

storagebuffermappingptr_t VkFxInterface::mapStorageBuffer(FxShaderStorageBuffer* b, //
                                                          size_t base, //
                                                          size_t length, //
                                                          BufferMapAccess access) { //
  auto bufimpl = b->_impl.getShared<VulkanBuffer>();
  auto mapping = std::make_shared<FxShaderStorageBufferMapping>();
  mapping->_buffer = b;
  mapping->_fxi = this;
  mapping->_offset = base;
  mapping->_access = access;
  if(length == 0) {
    mapping->_length = bufimpl->_length;
  } else {
    mapping->_length = length;
  }
  mapping->_mappedaddr = bufimpl->map(base, mapping->_length, 0);
  return mapping;
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::unmapStorageBuffer(FxShaderStorageBufferMapping* mapping) {
  auto bufimpl = mapping->_buffer->_impl.getShared<VulkanBuffer>();
  bufimpl->unmap();
  mapping->_impl.make<void*>(nullptr);
  mapping->_mappedaddr = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::copyBufferIntoStorageBuffer(FxShaderStorageBuffer* ssbo,
                                                std::vector<uint8_t> buffer,
                                                size_t dest_offset) {
  auto bufimpl = ssbo->_impl.getShared<VulkanBuffer>();
  size_t copy_size = buffer.size();
  OrkAssert((dest_offset + copy_size) <= ssbo->_length);

  auto mapped = bufimpl->map(dest_offset, copy_size, 0);
  memcpy(mapped, buffer.data(), copy_size);
  bufimpl->unmap();
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindStorageBuffer(const FxShaderStorageBlock* block,
                                      FxShaderStorageBuffer* buffer) {

 if(0) printf("bindStorageBuffer: block<%s> buffer<%p>\n", block ? block->_name.c_str() : "null", buffer);



  if (!block || !buffer) {
    return;
  }

  // Get Vulkan-specific implementations
  auto vk_block_ptr = block->_impl.getShared<VkFxShaderStorageBlock*>();
  if (!vk_block_ptr) {
    return;
  }
  auto vk_block = *vk_block_ptr;

  auto vk_buffer = buffer->_impl.getShared<VulkanBuffer>();
  if (!vk_buffer) {
    return;
  }

  // Store binding for later use when creating descriptor sets
  vk_block->_bound_buffer = vk_buffer;
  vk_block->_bound_ssbo = buffer;

  // Mark block as dirty for descriptor set update
  if (_currentPipeline && _currentPipeline->_vk_program) {
    auto program = _currentPipeline->_vk_program;
    auto it = program->_vk_ssbo_blocks.find(block->_name);
    if (it != program->_vk_ssbo_blocks.end()) {
      // Track dirty SSBOs (similar to UBOs)
      _currentPipeline->_dirty_ssbo_blocks.insert(it->second.get());
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
