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

uint32_t VkContext::_findMemoryType(    //
    uint32_t typeFilter,                //
    VkMemoryPropertyFlags properties) { //
  VkPhysicalDeviceMemoryProperties memProperties;
  initializeVkStruct(memProperties);
  vkGetPhysicalDeviceMemoryProperties(_vkphysicaldevice, &memProperties);
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }
  OrkAssert(false);
  return 0;
}

///////////////////////////////////////////////////////////////////////////////

VulkanMemoryForImage::VulkanMemoryForImage(vkcontext_rawptr_t ctxVK, VkImage image, VkMemoryPropertyFlags memprops)
    : _ctxVK(ctxVK)
    , _vkimage(image) {

  _memreq    = std::make_shared<VkMemoryRequirements>();
  _allocinfo = std::make_shared<VkMemoryAllocateInfo>();
  _vkmem     = std::make_shared<VkDeviceMemory>();

  initializeVkStruct(*_memreq);
  initializeVkStruct(*_allocinfo, VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
  initializeVkStruct(*_vkmem);

  vkGetImageMemoryRequirements(_ctxVK->_vkdevice, _vkimage, _memreq.get());
  _allocinfo->allocationSize  = _memreq->size;
  _allocinfo->memoryTypeIndex = _ctxVK->_findMemoryType(_memreq->memoryTypeBits, memprops);

  VkResult OK = vkAllocateMemory(_ctxVK->_vkdevice, _allocinfo.get(), nullptr, _vkmem.get());
  OrkAssert(OK == VK_SUCCESS);

  OK = vkBindImageMemory(_ctxVK->_vkdevice, _vkimage, *_vkmem, 0);
  OrkAssert(OK == VK_SUCCESS);
}

VulkanMemoryForImage::~VulkanMemoryForImage() {
  vkFreeMemory(_ctxVK->_vkdevice, *_vkmem, nullptr);
}

///////////////////////////////////////////////////////////////////////////////

VulkanMemoryForBuffer::VulkanMemoryForBuffer(vkcontext_rawptr_t ctxVK, VkBuffer buffer, VkMemoryPropertyFlags memprops)
    : _ctxVK(ctxVK)
    , _vkbuffer(buffer) {

  _memreq    = std::make_shared<VkMemoryRequirements>();
  _allocinfo = std::make_shared<VkMemoryAllocateInfo>();
  _vkmem     = std::make_shared<VkDeviceMemory>();

  initializeVkStruct(*_memreq);
  initializeVkStruct(*_allocinfo, VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
  initializeVkStruct(*_vkmem);

  vkGetBufferMemoryRequirements(_ctxVK->_vkdevice, _vkbuffer, _memreq.get());
  _allocinfo->allocationSize  = _memreq->size;
  _allocinfo->memoryTypeIndex = _ctxVK->_findMemoryType(_memreq->memoryTypeBits, memprops);

  VkResult OK = vkAllocateMemory(_ctxVK->_vkdevice, _allocinfo.get(), nullptr, _vkmem.get());
  OrkAssert(OK == VK_SUCCESS);
}

VulkanMemoryForBuffer::~VulkanMemoryForBuffer() {
  vkFreeMemory(_ctxVK->_vkdevice, *_vkmem, nullptr);
}

///////////////////////////////////////////////////////////////////////////////

vkivci_ptr_t createImageViewInfo2D(
    VkImage image,                      //
    VkFormat format,                    //
    VkImageAspectFlagBits aspectMask) { //
  auto IVCI = std::make_shared<VkImageViewCreateInfo>();
  initializeVkStruct(*IVCI, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
  IVCI->image                           = image;
  IVCI->viewType                        = VK_IMAGE_VIEW_TYPE_2D;
  IVCI->format                          = format;
  IVCI->subresourceRange.aspectMask     = aspectMask;
  IVCI->subresourceRange.baseMipLevel   = 0;
  IVCI->subresourceRange.levelCount     = 1;
  IVCI->subresourceRange.baseArrayLayer = 0;
  IVCI->subresourceRange.layerCount     = 1;
  IVCI->components.r                    = VK_COMPONENT_SWIZZLE_R;
  IVCI->components.g                    = VK_COMPONENT_SWIZZLE_G;
  IVCI->components.b                    = VK_COMPONENT_SWIZZLE_B;
  IVCI->components.a                    = VK_COMPONENT_SWIZZLE_A;
  return IVCI;
}

///////////////////////////////////////////////////////////////////////////////

vkimagecreateinfo_ptr_t makeVKICI(
    int w,
    int h,
    int d, //
    EBufferFormat fmt,
    int nummips) { //
  auto VKICI = std::make_shared<VkImageCreateInfo>();
  initializeVkStruct(*VKICI, VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
  VKICI->imageType     = VK_IMAGE_TYPE_2D;
  VKICI->format        = VkFormatConverter::convertBufferFormat(fmt);
  VKICI->extent.width  = w;
  VKICI->extent.height = h;
  VKICI->extent.depth  = d;
  VKICI->mipLevels     = nummips;
  VKICI->arrayLayers   = 1;
  VKICI->samples       = VK_SAMPLE_COUNT_1_BIT;
  VKICI->tiling        = VK_IMAGE_TILING_OPTIMAL;
  VKICI->sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
  VKICI->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  // VKICI->usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  return VKICI;
}

///////////////////////////////////////////////////////////////////////////////

vksamplercreateinfo_ptr_t makeVKSCI() { //
  auto ret = std::make_shared<VkSamplerCreateInfo>();
  initializeVkStruct(*ret, VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
  ret->magFilter               = VK_FILTER_LINEAR;
  ret->minFilter               = VK_FILTER_LINEAR;
  ret->addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  ret->addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  ret->addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  ret->anisotropyEnable        = VK_TRUE;
  ret->maxAnisotropy           = 16;
  ret->borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  ret->unnormalizedCoordinates = VK_FALSE;
  ret->compareEnable           = VK_FALSE;
  ret->compareOp               = VK_COMPARE_OP_ALWAYS;
  ret->mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  ret->mipLodBias              = 0.0f;
  ret->minLod                  = 0.0f;
  ret->maxLod                  = 1.0f;
  return ret;
}

///////////////////////////////////////////////////////////////////////////////

VulkanImageObject::VulkanImageObject(vkcontext_rawptr_t ctx, vkimagecreateinfo_ptr_t cinfo)
    : _ctx(ctx)
    , _cinfo(cinfo) {

  initializeVkStruct(_vkimage);
  VkResult ok = vkCreateImage(_ctx->_vkdevice, cinfo.get(), nullptr, &_vkimage);
  OrkAssert(VK_SUCCESS == ok);
  _imgmem = std::make_shared<VulkanMemoryForImage>(_ctx, _vkimage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}
VulkanImageObject::~VulkanImageObject() {
}

///////////////////////////////////////////////////////////////////////////////
VulkanBuffer::VulkanBuffer(vkcontext_rawptr_t ctxVK, size_t length, VkBufferUsageFlags usage)
    : _ctxVK(ctxVK)
    , _length(length)
    , _usage(usage) {
  OrkAssert(_length > 0);
  if(_length<1){
    _length=1;
  }

  VkBufferCreateInfo BUFINFO;
  initializeVkStruct(_cinfo, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
  _cinfo.size        = _length;
  _cinfo.usage       = usage;
  _cinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  initializeVkStruct(_vkbuffer);
  VkResult ok = vkCreateBuffer(ctxVK->_vkdevice, &_cinfo, nullptr, &_vkbuffer);
  OrkAssert(VK_SUCCESS == ok);

  _memory = std::make_shared<VulkanMemoryForBuffer>(
      ctxVK, _vkbuffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  vkBindBufferMemory(ctxVK->_vkdevice, _vkbuffer, *_memory->_vkmem, 0);
}
//////////////////////////////////////
VulkanBuffer::~VulkanBuffer() {
  vkDestroyBuffer(_ctxVK->_vkdevice, _vkbuffer, nullptr);
  _memory = nullptr;
}
//////////////////////////////////////
void VulkanBuffer::copyFromHost(const void* src, size_t length) {
  OrkAssert(length <= _length);
  void* dst = nullptr;
  vkMapMemory(_ctxVK->_vkdevice, *_memory->_vkmem, 0, _length, 0, &dst);
  memcpy(dst, src, _length);
  vkUnmapMemory(_ctxVK->_vkdevice, *_memory->_vkmem);
}
//////////////////////////////////////
void* VulkanBuffer::map(size_t offset, size_t length, VkMemoryMapFlags flags) {
  if(length<1)
    length = 1;
  void* dst = nullptr;
  vkMapMemory(_ctxVK->_vkdevice, *_memory->_vkmem, offset, length, flags, &dst);
  return dst;
}
//////////////////////////////////////
void VulkanBuffer::unmap() {
  vkUnmapMemory(_ctxVK->_vkdevice, *_memory->_vkmem);
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
