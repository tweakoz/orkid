////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_vksynch = logger()->configureChannel("VKSYNCH", fvec3(1, .8, .9), false);
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VulkanSemaphoreBase::VulkanSemaphoreBase(vkcontext_rawptr_t ctxVK)
    : _ctxVK(ctxVK) {
  _vksema = VK_NULL_HANDLE;
}

///////////////////////////////////////////////////

VulkanSemaphoreBase::~VulkanSemaphoreBase() {
  try {
    if(_ctxVK && _ctxVK->_vkdevice && _vksema) {
      vkDestroySemaphore(_ctxVK->_vkdevice, _vksema, nullptr);
      _vksema = VK_NULL_HANDLE;
    }
  } catch (...) {
    // Swallow — during static destruction _ctxVK may be dangling.
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

std::atomic<int> VulkanTimelineSemaphore::_semaphorecount = 0;

VulkanTimelineSemaphore::VulkanTimelineSemaphore(vkcontext_rawptr_t ctxVK)
    : VulkanSemaphoreBase(ctxVK) {

  VkSemaphoreTypeCreateInfoKHR STCI = {};
  initializeVkStruct(STCI, VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR);
  STCI.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
  STCI.initialValue  = 0;

  VkSemaphoreCreateInfo SCI = {};
  initializeVkStruct(SCI, VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
  SCI.pNext = &STCI;

  VkResult OK = vkCreateSemaphore(_ctxVK->_vkdevice, &SCI, nullptr, &_vksema);
  OrkAssert(OK == VK_SUCCESS);

  int count = _semaphorecount.fetch_add(1);
  // logchan_vksynch->log("CONSTRUCT VulkanTimelineSemaphore<%p> count<%d>", (void*) this, count);
}

///////////////////////////////////////////////////

VulkanTimelineSemaphore::~VulkanTimelineSemaphore() {
  int count = _semaphorecount.fetch_sub(1);
  // logchan_vksynch->log("DESTROY VulkanTimelineSemaphore<%p> count<%d>", (void*) this, count);
}

///////////////////////////////////////////////////

uint64_t VulkanTimelineSemaphore::hostQuery() const {
  uint64_t value;
  vkGetSemaphoreCounterValue(_ctxVK->_vkdevice, _vksema, &value);
  return value;
}

///////////////////////////////////////////////////

bool VulkanTimelineSemaphore::hostWait(uint64_t value, uint64_t timeout_ns) const {
  VkSemaphoreWaitInfo waitInfo{};
  waitInfo.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
  waitInfo.semaphoreCount = 1;
  waitInfo.pSemaphores    = &_vksema;
  waitInfo.pValues        = &value;

  VkResult result = vkWaitSemaphores(_ctxVK->_vkdevice, &waitInfo, timeout_ns);
  return result == VK_SUCCESS;
}

///////////////////////////////////////////////////

void VulkanTimelineSemaphore::hostSignal(uint64_t value) {
  VkSemaphoreSignalInfo signalInfo{};
  signalInfo.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
  signalInfo.semaphore = _vksema;
  signalInfo.value     = value;

  VkResult result = vkSignalSemaphore(_ctxVK->_vkdevice, &signalInfo);
  OrkAssert(result == VK_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VulkanCompletionSemaphore::VulkanCompletionSemaphore(vkcontext_rawptr_t ctxVK)
    : VulkanSemaphoreBase(ctxVK) {
  VkSemaphoreTypeCreateInfoKHR STCI = {};
  initializeVkStruct(STCI, VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR);
  STCI.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
  STCI.initialValue  = 0;

  VkSemaphoreCreateInfo SCI = {};
  initializeVkStruct(SCI, VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
  SCI.pNext = &STCI;

  VkResult OK = vkCreateSemaphore(_ctxVK->_vkdevice, &SCI, nullptr, &_vksema);
  if (OK != VK_SUCCESS) {
    const char* error_str = nullptr;
    switch(OK) {
      case VK_ERROR_OUT_OF_HOST_MEMORY: error_str = "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
      case VK_ERROR_OUT_OF_DEVICE_MEMORY: error_str = "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
      case VK_ERROR_TOO_MANY_OBJECTS: error_str = "VK_ERROR_TOO_MANY_OBJECTS"; break;
      case VK_ERROR_DEVICE_LOST: error_str = "VK_ERROR_DEVICE_LOST"; break;
      default: error_str = "UNKNOWN"; break;
    }
    printf("ERROR: vkCreateSemaphore (VulkanCompletionSemaphore) failed: %s (code=%d)\n", error_str, OK);
  }
  OrkAssert(OK == VK_SUCCESS);
}

///////////////////////////////////////////////////

VulkanCompletionSemaphore::~VulkanCompletionSemaphore() {
}

///////////////////////////////////////////////////

bool VulkanCompletionSemaphore::isSignalled() const {
  uint64_t value;
  VkResult result = vkGetSemaphoreCounterValue(_ctxVK->_vkdevice, _vksema, &value);
  OrkAssert(result == VK_SUCCESS);
  return value > 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VulkanBinarySemaphore::VulkanBinarySemaphore(vkcontext_rawptr_t ctxVK)
    : VulkanSemaphoreBase(ctxVK) {
  VkSemaphoreTypeCreateInfoKHR STCI = {};
  initializeVkStruct(STCI, VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR);
  STCI.semaphoreType        = VK_SEMAPHORE_TYPE_BINARY;
  STCI.initialValue         = 0;
  VkSemaphoreCreateInfo SCI = {};
  initializeVkStruct(SCI, VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
  SCI.pNext = &STCI;

  VkResult OK = vkCreateSemaphore(_ctxVK->_vkdevice, &SCI, nullptr, &_vksema);
  OrkAssert(OK == VK_SUCCESS);
}

///////////////////////////////////////////////////

VulkanBinarySemaphore::~VulkanBinarySemaphore() {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VulkanFenceObject::VulkanFenceObject(vkcontext_rawptr_t ctxVK)
    : _ctxVK(ctxVK) {
  VkFenceCreateInfo FCI = {};
  initializeVkStruct(FCI, VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
  FCI.flags   = VK_FENCE_CREATE_SIGNALED_BIT;
  VkResult OK = vkCreateFence(_ctxVK->_vkdevice, &FCI, nullptr, &_vkfence);
  OrkAssert(OK == VK_SUCCESS);
}

///////////////////////////////////////////////////

VulkanFenceObject::~VulkanFenceObject() {
  try {
    if(_ctxVK && _ctxVK->_vkdevice && _vkfence) {
      vkDestroyFence(_ctxVK->_vkdevice, _vkfence, nullptr);
    }
  } catch (...) {
    // Swallow — during static destruction _ctxVK may be dangling.
  }
}

///////////////////////////////////////////////////

void VulkanFenceObject::reset() {
  logchan_vksynch->log("FENCE: reset: fence %p", (void*)_vkfence);
  vkResetFences(_ctxVK->_vkdevice, 1, &_vkfence);
  logchan_vksynch->log("FENCE: reset: fence %p reset complete", (void*)_vkfence);
}

///////////////////////////////////////////////////

void VulkanFenceObject::wait() {
  logchan_vksynch->log("FENCE: wait: waiting for fence %p", (void*)_vkfence);
  vkWaitForFences(_ctxVK->_vkdevice, 1, &_vkfence, true, UINT64_MAX);
  logchan_vksynch->log("FENCE: wait: fence %p wait complete", (void*)_vkfence);
  for (auto item : _onReached) {
    item();
  }
  _onReached.clear();
}

///////////////////////////////////////////////////

void VulkanFenceObject::onCrossed(void_lambda_t op) {
  _onReached.push_back(op);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void VkContext::onFenceCrossed(void_lambda_t op) {
  auto swapchain   = _fbi->_swapchain;
  size_t sub_index = swapchain->subIndex();
  auto fence       = swapchain->_frameFences[sub_index];
  if (fence) {
    fence->onCrossed(op);
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

barrier_ptr_t createImageBarrier(
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkAccessFlagBits srcAccessMask,
    VkAccessFlagBits dstAccessMask) {
  barrier_ptr_t barrier = std::make_shared<VkImageMemoryBarrier>();
  initializeVkStruct(*barrier, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
  barrier->image               = image;
  barrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier->oldLayout           = oldLayout;
  barrier->newLayout           = newLayout;
  barrier->srcAccessMask       = srcAccessMask;
  barrier->dstAccessMask       = dstAccessMask;
  auto& range                  = barrier->subresourceRange;
  range.aspectMask             = VK_IMAGE_ASPECT_COLOR_BIT;
  range.baseMipLevel           = 0;
  range.levelCount             = 1;
  range.baseArrayLayer         = 0;
  range.layerCount             = 1;
  return barrier;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
