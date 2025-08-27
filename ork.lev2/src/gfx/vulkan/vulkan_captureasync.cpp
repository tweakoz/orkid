////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_captureasync.h"
#include "headers/vulkan_ctx.h"
#include <ork/lev2/gfx/image.h>

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

bool VkCaptureAsyncImpl::isDataReady() const {
  if (_dataRetrieved) {
    return true; // Already retrieved
  }
  
  if (!_copySubmitted || !_fence) {
    return false; // No fence to check
  }
  
  // Check fence status without blocking
  VkResult result = vkGetFenceStatus(_contextVK->_vkdevice, _fence->_vkfence);
  return (result == VK_SUCCESS);
}

////////////////////////////////////////////////////////////////

void VkCaptureAsyncImpl::waitForData() {
  OrkAssert(_fence); // Must have a fence to wait on
  OrkAssert(_copySubmitted); // Copy must have been submitted
  
  // Wait for the fence forever
  vkWaitForFences(_contextVK->_vkdevice, 1, &_fence->_vkfence, VK_TRUE, UINT64_MAX);
}

////////////////////////////////////////////////////////////////

} // namespace ork::lev2::vulkan