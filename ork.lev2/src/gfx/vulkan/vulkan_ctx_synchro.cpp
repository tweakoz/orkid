////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ctx.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

VulkanTimelineSemaphoreObject::VulkanTimelineSemaphoreObject(vkcontext_rawptr_t ctxVK)
    : _ctxVK(ctxVK) {

  VkSemaphoreTypeCreateInfoKHR STCI = {};
  initializeVkStruct(STCI, VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR);
  STCI.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
  STCI.initialValue  = 0;

  VkSemaphoreCreateInfo SCI = {};
  initializeVkStruct(SCI, VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
  SCI.pNext = &STCI;

  VkResult OK = vkCreateSemaphore(_ctxVK->_vkdevice, &SCI, nullptr, &_vksema);
  OrkAssert(OK == VK_SUCCESS);
}

///////////////////////////////////////////////////

VulkanTimelineSemaphoreObject::~VulkanTimelineSemaphoreObject() {
  vkDestroySemaphore(_ctxVK->_vkdevice, _vksema, nullptr);
}

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
  vkDestroyFence(_ctxVK->_vkdevice, _vkfence, nullptr);
}

///////////////////////////////////////////////////

void VulkanFenceObject::reset() {
  vkResetFences(_ctxVK->_vkdevice, 1, &_vkfence);
}

///////////////////////////////////////////////////

void VulkanFenceObject::wait() {
  vkWaitForFences(_ctxVK->_vkdevice, 1, &_vkfence, true, UINT64_MAX);
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

void VkContext::onFenceCrossed(void_lambda_t op) {
  auto fence = _fbi->_swapchain->_fence;
  fence->onCrossed(op);
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
} //namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
