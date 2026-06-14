#pragma once 
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

struct VulkanMemoryForImage {
  VulkanMemoryForImage(vkcontext_rawptr_t ctxVK, VkImage image, VkMemoryPropertyFlags memprops);
  ~VulkanMemoryForImage();

  vkcontext_rawptr_t _ctxVK;
  VkImage _vkimage = VK_NULL_HANDLE;
  vkmemreq_ptr_t _memreq;
  vkmemallocinfo_ptr_t _allocinfo;
  vkmem_ptr_t _vkmem;

  static std::atomic<int> _imgmemcount;
  static std::atomic<size_t> _imgmembytes;
  static std::atomic<size_t> _imgmemSN;
};

///////////////////////////////////////////////////////////////////////////////

struct VulkanMemoryForBuffer {
  VulkanMemoryForBuffer(
      vkcontext_rawptr_t ctxVK,
      VkBuffer buffer,
      VkMemoryPropertyFlags memprops = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  ~VulkanMemoryForBuffer();

  vkcontext_rawptr_t _ctxVK;
  VkBuffer _vkbuffer = VK_NULL_HANDLE;
  vkmemreq_ptr_t _memreq;
  vkmemallocinfo_ptr_t _allocinfo;
  vkmem_ptr_t _vkmem;
};

///////////////////////////////////////////////////////////////////////////////

struct VulkanBuffer {
  // memprops default = the historical HOST_VISIBLE|HOST_COHERENT (directly mappable). Pass
  // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT for a GPU-resident buffer — then map/copy route through the
  // context's _syncTransfer staging (host can't map device-local memory).
  VulkanBuffer(vkcontext_rawptr_t ctxVK, size_t length, VkBufferUsageFlags usage, std::string name = "",
               VkMemoryPropertyFlags memprops = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  ~VulkanBuffer();

  void copyFromHost(const void* src, size_t length, size_t dstOffset = 0);  // host -> this (staged if device-local)
  void copyToHost(void* dst, size_t length, size_t srcOffset = 0);          // this -> host (staged if device-local)
  void* map(size_t offset, size_t length, VkMemoryMapFlags flags);          // host-visible only
  void unmap();

  vkcontext_rawptr_t _ctxVK;
  size_t _length;
  VkBufferUsageFlags _usage;
  VkBufferCreateInfo _cinfo;
  VkBuffer _vkbuffer = VK_NULL_HANDLE;
  vkmemforbuf_ptr_t _memory;
  bool _hostVisible = true;   // false = device-local (map/copy go through staging)

  static std::atomic<int> _buffercount;
  static std::atomic<size_t> _bufferbytes;
  static std::atomic<size_t> _bufferSN;
};
///////////////////////////////////////////////////////////////////////////////
struct SbsPoolAdapter {
  using item_t = vkbuffer_ptr_t;
  static constexpr size_t _num_alloc_per_batch = 2;
  /////////////////////
  SbsPoolAdapter(vkcontext_rawptr_t ctxVK, size_t size, uint64_t usage);
  item_t allocFresh();
  /////////////////////
  vkcontext_rawptr_t _contextVK = nullptr;
  const size_t _size;
  const uint64_t _usage;
};
///////////////////////////////////////////////////////////////////////////////
struct SecCmdBufPoolAdapter {
  using item_t = secondary_commandbuffer_ptr_t;
  static constexpr size_t _num_alloc_per_batch = 2;
  /////////////////////
  SecCmdBufPoolAdapter(vkcontext_rawptr_t ctxVK);
  item_t allocFresh();
  /////////////////////
  vkcontext_rawptr_t _contextVK = nullptr;
};
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
