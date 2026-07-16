////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ubo_dynamic.h"
#include "headers/vulkan_ctx.h"

namespace ork::lev2::vulkan {

// Global instance
VkDynamicUBOSystem* g_dynamic_ubo_system = nullptr;

void VkDynamicUBOSystem::init(vkcontext_rawptr_t ctx) {
  _context = ctx;
  
  // Query actual alignment from device
  VkPhysicalDeviceProperties props;
  vkGetPhysicalDeviceProperties(ctx->_vkphysicaldevice, &props);
  _actual_alignment = props.limits.minUniformBufferOffsetAlignment;
  
  // Calculate total buffer size
  // Use larger chunks to accommodate biggest UBOs (ublk_std_matrices is 1136 bytes)
  size_t chunk_size = align_up(2048, _actual_alignment);
  _buffer_size = MAX_CHUNKS * chunk_size;
  
  if(0)printf("VkDynamicUBOSystem: Creating global buffer of %zu MB (alignment=%zu, chunk_size=%zu)\n",
         _buffer_size / (1024*1024), _actual_alignment, chunk_size);
  
  // Use VulkanBuffer constructor - it handles all the Vulkan setup.
  // Request DEVICE_LOCAL|HOST_VISIBLE|HOST_COHERENT = the BAR/ReBAR window: CPU writes
  // through the persistent map land in VRAM-visible memory, so the GPU's per-draw UBO
  // reads stay local instead of fetching over PCIe every draw. Degrades to plain
  // HOST_VISIBLE sysram when no BAR type exists or the BAR heap is full
  // (_findMemoryType / VulkanMemoryForBuffer fallback). BAR memory is write-combined:
  // this mapping must stay WRITE-ONLY from the CPU (the only writer is the shadow-buffer
  // memcpy in applyPendingUboUpdates — keep it that way).
  _global_buffer = std::make_shared<VulkanBuffer>(
    ctx,
    _buffer_size,
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    "DynamicUBO",
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
  );
  
  // Map the entire buffer persistently for CPU writes
  _mapped_base = _global_buffer->map(0, _buffer_size, 0);

  if (_mapped_base == nullptr) {
    printf("VkDynamicUBOSystem: FATAL - buffer mapping failed!\n");
    OrkAssert(false);
  }
  if(0)printf("VkDynamicUBOSystem: buffer mapped at %p, size=%zu MB, alignment=%zu vkbuffer=%p\n",
         _mapped_base, _buffer_size / (1024*1024), _actual_alignment, (void*)_global_buffer->_vkbuffer);
}

void VkDynamicUBOSystem::shutdown() {
  if (_global_buffer) {
    if (_mapped_base) {
      _global_buffer->unmap();
      _mapped_base = nullptr;
    }
    
    // VulkanBuffer destructor handles cleanup
    _global_buffer = nullptr;
  }
}

VkDynamicUBOSystem::Allocation VkDynamicUBOSystem::allocate(size_t data_size, uint32_t frame_index) {
  std::lock_guard<std::mutex> lock(_allocation_mutex);

  // First, align the CURRENT OFFSET to ensure dynamic offset is properly aligned
  // This is critical for Vulkan which requires aligned dynamic offsets
  _current_offset = align_up(_current_offset, _actual_alignment);

  // Align size to device requirements
  size_t aligned_size = align_up(data_size, _actual_alignment);
  
  // Wrap around if needed
  if (_current_offset + aligned_size > _buffer_size) {
    _current_offset = 0;
    // TODO: In production, should track frame to avoid overwriting in-flight data
    //printf("VkDynamicUBOSystem: WARNING - buffer wrapped around, may overwrite in-flight data\n");
  }
  
  Allocation alloc;
  alloc.dynamic_offset = _current_offset;
  alloc.cpu_ptr = static_cast<uint8_t*>(_mapped_base) + _current_offset;
  alloc.size = aligned_size;
  
  _current_offset += aligned_size;
  
  return alloc;
}

void VkDynamicUBOSystem::begin_frame(uint32_t frame_index) {
  _current_frame = frame_index;
  // Could implement more sophisticated frame-based ring buffering here
  // For now, simple sequential allocation works
}

} // namespace ork::lev2::vulkan