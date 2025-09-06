////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "headers/vulkan_ctx.h"
#include <mutex>

namespace ork::lev2::vulkan {

///////////////////////////////////////////////////////////////////////////////
// Dynamic UBO System - manages a single large buffer for all UBOs
// to avoid per-draw overwrites in coherent memory
///////////////////////////////////////////////////////////////////////////////

class VkDynamicUBOSystem {
public:
  static constexpr size_t MAX_CHUNKS = 4096;  // Support many draws per frame
  
  struct Allocation {
    uint32_t dynamic_offset;  // Offset to pass to vkCmdBindDescriptorSets
    void* cpu_ptr;           // CPU pointer for writing
    size_t size;             // Size available
  };

  void init(vkcontext_rawptr_t ctx);
  void shutdown();
  
  // Called per-draw, on-demand for ANY UBO
  Allocation allocate(size_t data_size, uint32_t frame_index);
  
  // Get the global buffer for descriptor set binding
  vkbuffer_ptr_t get_buffer() const { return _global_buffer; }
  
  // Reset allocation for new frame
  void begin_frame(uint32_t frame_index);
  
  size_t get_alignment() const { return _actual_alignment; }

private:
  size_t align_up(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
  }

  vkcontext_rawptr_t _context = nullptr;
  
  // Single large buffer for ALL UBOs
  vkbuffer_ptr_t _global_buffer;
  void* _mapped_base = nullptr;
  size_t _buffer_size = 0;
  size_t _actual_alignment = 256;  // Set from device properties
  
  // Ring buffer allocation
  size_t _current_offset = 0;
  uint32_t _current_frame = 0;
  
  // Thread safety for multi-threaded command buffer recording
  std::mutex _allocation_mutex;
};

// Global system instance
extern VkDynamicUBOSystem* g_dynamic_ubo_system;

} // namespace ork::lev2::vulkan