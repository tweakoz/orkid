////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_captureasync.h"
#include "headers/vulkan_ctx.h"

namespace ork::lev2::vulkan {

////////////////////////////////////////////////////////////////

VkCaptureAsyncImpl::VkCaptureAsyncImpl(vkcontext_rawptr_t ctx) 
    : _contextVK(ctx) {
}

////////////////////////////////////////////////////////////////

VkCaptureAsyncImpl::~VkCaptureAsyncImpl() {
  // Ensure we've waited for any pending operations
  if (_fence && !_dataRetrieved) {
    vkWaitForFences(_contextVK->_vkdevice, 1, &_fence->_vkfence, VK_TRUE, UINT64_MAX);
  }
}

////////////////////////////////////////////////////////////////

bool VkCaptureAsyncImpl::retrieveData(CaptureBuffer* out_buffer) {
  if (_dataRetrieved) {
    return true; // Already retrieved
  }
  
  if (!_copySubmitted || !_fence || !_stagingBuffer) {
    return false; // Not ready
  }
  
  // Wait for the fence
  VkResult result = vkWaitForFences(_contextVK->_vkdevice, 1, &_fence->_vkfence, VK_TRUE, UINT64_MAX);
  if (result != VK_SUCCESS) {
    return false;
  }
  
  // Copy data from staging buffer to output
  size_t bufsize = out_buffer->length();
  _stagingBuffer->copyToHost(out_buffer->_data, bufsize);
  
  _dataRetrieved = true;
  return true;
}

////////////////////////////////////////////////////////////////

} // namespace ork::lev2::vulkan