#pragma once

#include <ork/lev2/gfx/targetinterfaces.h>
#include "headers/vulkan_ctx.h"
#include "headers/vk_protos.h"

namespace ork::lev2::vulkan {

struct VkCaptureAsyncImpl {
  VkCaptureAsyncImpl(vkcontext_rawptr_t ctx);
  ~VkCaptureAsyncImpl();

  // Vulkan resources
  vkcontext_rawptr_t _contextVK = nullptr;
  vkbuffer_ptr_t _stagingBuffer;
  vkfence_obj_ptr_t _fence;
  
  // Capture state
  bool _copySubmitted = false;
  bool _dataRetrieved = false;
  
  // Wait for fence and copy data
  bool retrieveData(CaptureBuffer* out_buffer);
};

using vkcaptureasyncimpl_ptr_t = std::shared_ptr<VkCaptureAsyncImpl>;

} // namespace ork::lev2::vulkan