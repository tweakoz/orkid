#pragma once 
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
struct VulkanSemaphoreBase {
  VulkanSemaphoreBase(vkcontext_rawptr_t ctxVK);
  virtual ~VulkanSemaphoreBase();
  vkcontext_rawptr_t _ctxVK;
  VkSemaphore _vksema;
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanBinarySemaphore final : public VulkanSemaphoreBase {
  VulkanBinarySemaphore(vkcontext_rawptr_t ctxVK);
  ~VulkanBinarySemaphore();
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanTimelineSemaphore final : public VulkanSemaphoreBase {
  VulkanTimelineSemaphore(vkcontext_rawptr_t ctxVK);
  ~VulkanTimelineSemaphore();
  uint64_t hostQuery() const;
  bool hostWait(uint64_t value, uint64_t timeout_ns = UINT64_MAX) const;
  void hostSignal(uint64_t value);
  static std::atomic<int> _semaphorecount;
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanCompletionSemaphore final : public VulkanSemaphoreBase {
  VulkanCompletionSemaphore(vkcontext_rawptr_t ctxVK);
  ~VulkanCompletionSemaphore();
  bool isSignalled() const;
  void_lambda_t _onComplete = nullptr;
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanFenceObject {
  VulkanFenceObject(vkcontext_rawptr_t ctxVK);
  ~VulkanFenceObject();
  // site names the caller in the wedge abort (see vulkan_wedge.h); every
  // frame-pacing wait in the backend funnels through here.
  void wait(const char* site = "fence");
  void reset();
  void onCrossed(void_lambda_t op);
  std::vector<void_lambda_t> _onReached;
  vkcontext_rawptr_t _ctxVK;
  VkFence _vkfence;
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanEventObject {
  VulkanEventObject(vkcontext_rawptr_t ctxVK);
  ~VulkanEventObject();
  void wait();
  void reset();
  void onCrossed(void_lambda_t op);
  std::vector<void_lambda_t> _onReached;
  vkcontext_rawptr_t _ctxVK;
  VkEvent _vkevent;
};
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
