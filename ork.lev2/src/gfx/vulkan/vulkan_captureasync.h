#pragma once

#include <ork/lev2/gfx/targetinterfaces.h>
#include "headers/vulkan_ctx.h"
#include "headers/vk_protos.h"

namespace ork::lev2::vulkan {

struct VkCaptureAsyncImpl {
  // Metadata (from what was VulkanCaptureData)
  capturebuffer_ptr_t capture_buffer;
  texture_ptr_t capture_texture;
  file::Path path;
  int width = 0;
  int height = 0;
  EBufferFormat format = EBufferFormat::NONE;
  bool frame_submitted = false;
  
  // Vulkan async resources
  vkcontext_rawptr_t _contextVK = nullptr;
  vkbuffer_ptr_t _stagingBuffer;
  vkfence_obj_ptr_t _fence;
  
  // Capture state
  bool _copySubmitted = false;
  bool _dataRetrieved = false;
  
  // Methods
  VkCaptureAsyncImpl() = default;  // Allow default construction for metadata-only use
  VkCaptureAsyncImpl(vkcontext_rawptr_t ctx);
  ~VkCaptureAsyncImpl();
    
};

using vkcaptureasyncimpl_ptr_t = std::shared_ptr<VkCaptureAsyncImpl>;

} // namespace ork::lev2::vulkan