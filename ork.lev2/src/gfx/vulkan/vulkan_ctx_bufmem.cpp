////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/util/crc64.h>
#include <ork/kernel/memcpy.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static auto logchan_vkbufmem = logger()->configureChannel("VKBUFMEM", fvec3(0.5, 0.5, 0.5), false);
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

///////////////////////////////////////////////////////////////////////////////

std::atomic<int> VulkanMemoryForImage::_imgmemcount(0);
std::atomic<size_t> VulkanMemoryForImage::_imgmembytes(0);
std::atomic<size_t> VulkanMemoryForImage::_imgmemSN(0);

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

  int SN       = _imgmemSN.fetch_add(1);
  int count    = _imgmemcount.fetch_add(1);
  size_t bytes = _imgmembytes.fetch_add(_memreq->size);
  if((SN&0xff)==0){
    if(0)logchan_vkbufmem->log("VulkanMemoryForImage<%p> SN<%d> bytes-alloced<%zu> alloc-count<%d> ", (void*)this, SN, bytes, count);
  }
}

VulkanMemoryForImage::~VulkanMemoryForImage() {
  try {
    if(_ctxVK && _ctxVK->_vkdevice && _vkmem) {
      vkFreeMemory(_ctxVK->_vkdevice, *_vkmem, nullptr);
    }
  } catch (...) {
    // Swallow — during static destruction _ctxVK may be dangling.
  }
  int count    = _imgmemcount.fetch_sub(1);
  size_t bytes = _imgmembytes.fetch_sub(_memreq->size);
  if(0)printf("~VulkanMemoryForImage<%p> bytes-freed<%zu> bytes-remaining<%zu> alloc-count<%d> \n",
         (void*)this,
         _memreq->size,
         bytes,
         count - 1);
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
  try {
    if(_ctxVK && _ctxVK->_vkdevice && _vkmem) {
      vkFreeMemory(_ctxVK->_vkdevice, *_vkmem, nullptr);
    }
  } catch (...) {
    // Swallow — during static destruction _ctxVK may be dangling.
  }
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

uint64_t hashImageCreationParams(
    int w,                             //
    int h,                             //
    int d,                             //
    EBufferFormat fmt,                 //
    int nummips,
    uint64_t usage ) {                     //
    boost::Crc64 crc;
    crc.init();
  crc.accumulateItem(w);
  crc.accumulateItem(h);
  crc.accumulateItem(d);
  crc.accumulateItem(fmt);
  crc.accumulateItem(nummips);
  crc.accumulateItem(usage);
  crc.finish();
  return crc.result();
}
///////////////////////////////////////////////////////////////////////////////

vkimagecreateinfo_ptr_t makeVKICI(
    int w,
    int h,
    int d, //
    VkFormat fmt,
    int nummips) { //
  auto VKICI = std::make_shared<VkImageCreateInfo>();
  initializeVkStruct(*VKICI, VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
  VKICI->imageType     = VK_IMAGE_TYPE_2D;
  VKICI->format        = fmt;
  VKICI->extent.width  = w;
  VKICI->extent.height = h;
  VKICI->extent.depth  = d;
  VKICI->mipLevels     = nummips;
  VKICI->arrayLayers   = 1;
  VKICI->samples       = VK_SAMPLE_COUNT_1_BIT;
  VKICI->tiling        = VK_IMAGE_TILING_OPTIMAL;
  VKICI->sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
  VKICI->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VKICI->usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  return VKICI;
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
  VKICI->usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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
  ret->anisotropyEnable        = VK_FALSE;
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
std::atomic<int> VulkanImageObject::_imgobjcount = 0;
std::atomic<size_t> VulkanImageObject::_imgobjSN = 0;
///////////////////////////////////////////////////////////////////////////////
VulkanImageObject::VulkanImageObject(vkcontext_rawptr_t ctx, vkimagecreateinfo_ptr_t cinfo, std::string name)
    : _ctx(ctx)
    , _cinfo(cinfo) {

  initializeVkStruct(_vkimage);
  initializeVkStruct(_vkimageview);
  VkResult ok = vkCreateImage(_ctx->_vkdevice, cinfo.get(), nullptr, &_vkimage);
  //OrkAssert((uint64_t)_vkimage != 0xdc00000000dcULL)
  OrkAssert(VK_SUCCESS == ok);
  _imgmem = std::make_shared<VulkanMemoryForImage>(_ctx, _vkimage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (name != "") {
    _ctx->_setObjectDebugName(_vkimage, VK_OBJECT_TYPE_IMAGE, name.c_str());
    std::string mem_name = name + "_mem";
    _ctx->_setObjectDebugName(*(_imgmem->_vkmem), VK_OBJECT_TYPE_DEVICE_MEMORY, mem_name.c_str());
  }
  _format = cinfo->format;
  _serial_number = _imgobjSN.fetch_add(1);
  int count = _imgobjcount.fetch_add(1);
  if((_serial_number&0xff)==0){
    logchan_vkbufmem->log("VulkanImageObject<%p> SN<%zu> numalive<%d>", (void*)this, _serial_number, count );
  }
}
VulkanImageObject::VulkanImageObject(vkcontext_rawptr_t ctx, VkImage img, VkImageView vkimgview, VkFormat fmt)
    : _ctx(ctx)
    , _vkimage(img)
    , _vkimageview(vkimgview)
    , _format(fmt) {
  _serial_number = _imgobjSN.fetch_add(1);
  int count = _imgobjcount.fetch_add(1);
  if((_serial_number&0xff)==0){
    logchan_vkbufmem->log("VulkanImageObject<%p> SN<%zu> numalive<%d>", (void*)this, _serial_number, count );
  }
}
///////////////////////////////////////////////////////////////////////////////
VulkanImageObject::~VulkanImageObject() {
  _imgobjcount.fetch_sub(1);
  try {
    if(!_ctx || !_ctx->_vkdevice){
      return;
    }
    if (_delete_imageview and (_vkimageview != VK_NULL_HANDLE)) {
      vkDestroyImageView(_ctx->_vkdevice, _vkimageview, nullptr);
    }
    if (_delete_image and (_vkimage != VK_NULL_HANDLE)) {
      vkDestroyImage(_ctx->_vkdevice, _vkimage, nullptr);
    }
    if (_delete_devicemem and (_vkdevicemem != VK_NULL_HANDLE)) {
      vkFreeMemory(_ctx->_vkdevice, _vkdevicemem, nullptr);
    }
    _vkimage = VK_NULL_HANDLE;
    _imgmem = nullptr;
  } catch (...) {
    // Swallow — during static destruction _ctx may be dangling.
  }
}
///////////////////////////////////////////////////////////////////////////////

std::atomic<int> VulkanBuffer::_buffercount    = 0;
std::atomic<size_t> VulkanBuffer::_bufferbytes = 0;
std::atomic<size_t> VulkanBuffer::_bufferSN = 0;

VulkanBuffer::VulkanBuffer(vkcontext_rawptr_t ctxVK, size_t length, VkBufferUsageFlags usage, std::string name)
    : _ctxVK(ctxVK)
    , _length(length)
    , _usage(usage) {
  
  // Debug logging to diagnose zero-length buffer creation
  if (length == 0) {
    logchan_vkbufmem->log("ERROR: VulkanBuffer constructor called with length=0, usage=0x%x, name='%s'", usage, name.c_str());
  }
  
  OrkAssert(_length > 0);

  VkBufferCreateInfo BUFINFO;
  initializeVkStruct(_cinfo, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
  _cinfo.size        = _length;
  _cinfo.usage       = usage;
  _cinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  initializeVkStruct(_vkbuffer);
  OrkVkAssert(vkCreateBuffer(ctxVK->_vkdevice, &_cinfo, nullptr, &_vkbuffer));

  if (name != "") {
    _ctxVK->_setObjectDebugName(_vkbuffer, VK_OBJECT_TYPE_BUFFER, name.c_str());
  }

  _memory = std::make_shared<VulkanMemoryForBuffer>(
      ctxVK, _vkbuffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  vkBindBufferMemory(ctxVK->_vkdevice, _vkbuffer, *_memory->_vkmem, 0);
  
  // Also set debug name for the memory
  if (name != "") {
    std::string mem_name = name + FormatString("_memory<%p>",(void*)*_memory->_vkmem);
    _ctxVK->_setObjectDebugName(*_memory->_vkmem, VK_OBJECT_TYPE_DEVICE_MEMORY, mem_name.c_str());
  }

  int SN = _bufferSN.fetch_add(1);
  int count = _buffercount.fetch_add(1);
  _bufferbytes.fetch_add(_length);
  if((SN&0xff)==0){
    logchan_vkbufmem->log("VulkanBuffer<%p> SN<%d> numalive<%d> bytes<%zu>", (void*)this, SN, count, size_t(_bufferbytes));
  }
}
//////////////////////////////////////
VulkanBuffer::~VulkanBuffer() {
  try {
    if(_ctxVK && _ctxVK->_vkdevice && _vkbuffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(_ctxVK->_vkdevice, _vkbuffer, nullptr);
    }
  } catch (...) {
    // Swallow — during static destruction _ctxVK may be dangling.
  }
  _buffercount.fetch_sub(1);
  _bufferbytes.fetch_sub(_length);
  _memory = nullptr;
}
//////////////////////////////////////
void VulkanBuffer::copyFromHost(const void* src, size_t length) {
  OrkAssert(length <= _length);
  void* dst = nullptr;
  vkMapMemory(_ctxVK->_vkdevice, *_memory->_vkmem, 0, _length, 0, &dst);
  static size_t _numcopied = 0;
  static size_t _prvnumcopied = 0;
  _numcopied += length;
  if((_numcopied - _prvnumcopied) > (1<<30) ) {
    //logchan_vkbufmem->log("VulkanBuffer copyFromHost copied<%zu> total<%zu>", length, _numcopied);
    _prvnumcopied = _numcopied;
  }
  std::memcpy(dst, src, length);
  vkUnmapMemory(_ctxVK->_vkdevice, *_memory->_vkmem);
}
//////////////////////////////////////
void VulkanBuffer::copyToHost(void* dst, size_t length) {
  OrkAssert(length <= _length);
  void* src = nullptr;
  vkMapMemory(_ctxVK->_vkdevice, *_memory->_vkmem, 0, length, 0, &src);
  memcpy_fast(dst, src, length);
  vkUnmapMemory(_ctxVK->_vkdevice, *_memory->_vkmem);
}
//////////////////////////////////////
void* VulkanBuffer::map(size_t offset, size_t length, VkMemoryMapFlags flags) {
  if (length < 1)
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
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
