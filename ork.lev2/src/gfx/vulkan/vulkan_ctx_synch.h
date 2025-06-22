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
struct VulkanBinarySemaphore : public VulkanSemaphoreBase {
  VulkanBinarySemaphore(vkcontext_rawptr_t ctxVK);
  ~VulkanBinarySemaphore() final;
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanTimelineSemaphore : public VulkanSemaphoreBase {
  VulkanTimelineSemaphore(vkcontext_rawptr_t ctxVK);
  ~VulkanTimelineSemaphore() final;
  uint64_t hostQuery() const;
  bool hostWait(uint64_t value, uint64_t timeout_ns = UINT64_MAX) const;
  void hostSignal(uint64_t value);
  static std::atomic<int> _semaphorecount;
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanCompletionSemaphore : public VulkanSemaphoreBase {
  VulkanCompletionSemaphore(vkcontext_rawptr_t ctxVK);
  ~VulkanCompletionSemaphore() final;
  bool isSignalled() const;
  void_lambda_t _onComplete = nullptr;
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanFenceObject {
  VulkanFenceObject(vkcontext_rawptr_t ctxVK);
  ~VulkanFenceObject();
  void wait();
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
