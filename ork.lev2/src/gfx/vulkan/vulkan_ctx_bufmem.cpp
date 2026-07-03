////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/util/crc64.h>
#include <ork/kernel/memcpy.inl>
#include <map>
#include <mutex>
#include <unistd.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static auto logchan_vkbufmem = logger()->configureChannel("VKBUFMEM", fvec3(0.5, 0.5, 0.5), false);

///////////////////////////////////////////////////////////////////////////////
// H-MEM1 instrumentation (~/LOADX.md): ORKID_VK_MEMTRACE=<path|1> logs every
// buffer/image vkAllocateMemory + free to a file (default
// /tmp/orkid_memtrace_<pid>.log): requested vs GRANTED memory-type flags, owning
// heap, per-heap live totals. Allocation FAILURES dump all heap totals before the
// assert fires, so an OOM death leaves its evidence on disk. Not covered:
// swapchain / external-image allocations (fixed, few).
///////////////////////////////////////////////////////////////////////////////

static thread_local std::string g_vkmemtrace_tag; // resource owners set this around the Memory ctor

struct VkMemTrace {
  static bool enabled() {
    static bool e = (getenv("ORKID_VK_MEMTRACE") != nullptr);
    return e;
  }
  static VkMemTrace& instance() {
    static VkMemTrace t;
    return t;
  }
  VkMemTrace() {
    const char* v    = getenv("ORKID_VK_MEMTRACE");
    std::string path = (v and strlen(v) > 1) ? v : FormatString("/tmp/orkid_memtrace_%d.log", int(getpid()));
    _file = fopen(path.c_str(), "w");
    printf("[VKMEMTRACE] writing to <%s>\n", path.c_str());
    if (_file) {
      fprintf(_file, "# orkid VK memory trace (H-MEM1) pid<%d>\n", int(getpid()));
      fflush(_file);
    }
  }
  static std::string _flags(VkMemoryPropertyFlags f) {
    std::string s;
    auto add = [&](const char* t) { s += s.empty() ? t : (std::string("|") + t); };
    if (f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) add("DL");
    if (f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) add("HV");
    if (f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) add("HC");
    if (f & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) add("HCACHE");
    if (f & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) add("LAZY");
    return s.empty() ? "none" : s;
  }
  static const char* _result(VkResult r) {
    switch (r) {
      case VK_SUCCESS: return "OK";
      case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "OUT_OF_DEVICE_MEMORY";
      case VK_ERROR_OUT_OF_HOST_MEMORY: return "OUT_OF_HOST_MEMORY";
      default: {
        static thread_local char buf[32];
        snprintf(buf, sizeof(buf), "VkResult(%d)", int(r));
        return buf;
      }
    }
  }
  struct DevInfo {
    VkPhysicalDeviceMemoryProperties _props;
    int64_t _heapLive[VK_MAX_MEMORY_HEAPS] = {0};
  };
  DevInfo& _dev(vkcontext_rawptr_t ctx) { // caller holds _mtx
    auto phy = ctx->_vkphysicaldevice;
    auto it  = _devs.find(phy);
    if (it != _devs.end())
      return it->second;
    auto& dev = _devs[phy];
    vkGetPhysicalDeviceMemoryProperties(phy, &dev._props);
    VkPhysicalDeviceProperties pdp;
    vkGetPhysicalDeviceProperties(phy, &pdp);
    if (_file) {
      fprintf(_file, "[DEVICE] name<%s> heaps<%u> types<%u>\n", pdp.deviceName, dev._props.memoryHeapCount, dev._props.memoryTypeCount);
      for (uint32_t h = 0; h < dev._props.memoryHeapCount; h++)
        fprintf(
            _file, "[HEAP %u] size<%.0fMB> device_local<%d>\n", h,
            double(dev._props.memoryHeaps[h].size) / 1048576.0,
            int((dev._props.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0));
      for (uint32_t t = 0; t < dev._props.memoryTypeCount; t++)
        fprintf(
            _file, "[TYPE %u] heap<%u> flags<%s>\n", t, dev._props.memoryTypes[t].heapIndex,
            _flags(dev._props.memoryTypes[t].propertyFlags).c_str());
      fflush(_file);
    }
    return dev;
  }
  void onAlloc(const char* kind, vkcontext_rawptr_t ctx, size_t size, VkMemoryPropertyFlags req, uint32_t typeIdx, VkResult res) {
    std::lock_guard<std::mutex> lk(_mtx);
    if (nullptr == _file)
      return;
    auto& dev     = _dev(ctx);
    uint32_t heap = dev._props.memoryTypes[typeIdx].heapIndex;
    if (res == VK_SUCCESS)
      dev._heapLive[heap] += int64_t(size);
    fprintf(
        _file, "[%s %s seq<%zu>] size<%zu> req<%s> type<%u> typeflags<%s> heap<%u> heaplive<%.1fMB/%.0fMB> result<%s> tag<%s>\n",
        (res == VK_SUCCESS) ? "ALLOC" : "FAIL", kind, _seq++, size,
        _flags(req).c_str(), typeIdx, _flags(dev._props.memoryTypes[typeIdx].propertyFlags).c_str(), heap,
        double(dev._heapLive[heap]) / 1048576.0, double(dev._props.memoryHeaps[heap].size) / 1048576.0,
        _result(res), g_vkmemtrace_tag.c_str());
    if (res != VK_SUCCESS)
      for (uint32_t h = 0; h < dev._props.memoryHeapCount; h++)
        fprintf(
            _file, "[FAILDUMP] heap<%u> live<%.1fMB> size<%.0fMB>\n", h,
            double(dev._heapLive[h]) / 1048576.0, double(dev._props.memoryHeaps[h].size) / 1048576.0);
    fflush(_file);
  }
  void onFree(const char* kind, vkcontext_rawptr_t ctx, size_t size, uint32_t typeIdx) {
    std::lock_guard<std::mutex> lk(_mtx);
    if (nullptr == _file)
      return;
    auto& dev     = _dev(ctx);
    uint32_t heap = dev._props.memoryTypes[typeIdx].heapIndex;
    dev._heapLive[heap] -= int64_t(size);
    fprintf(_file, "[FREE %s seq<%zu>] size<%zu> heap<%u> heaplive<%.1fMB>\n", kind, _seq++, size, heap, double(dev._heapLive[heap]) / 1048576.0);
    fflush(_file);
  }
  std::mutex _mtx;
  FILE* _file = nullptr;
  size_t _seq = 0;
  std::map<VkPhysicalDevice, DevInfo> _devs;
};
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
  // Preference-ordered degrade: a DEVICE_LOCAL|HOST_VISIBLE request is asking for a
  // BAR/ReBAR window — on parts without one (no-ReBAR discrete, some drivers) fall back
  // to plain host-visible sysram rather than asserting. The caller's map()/write path is
  // identical either way (_hostVisible keys off the REQUESTED HV bit). A pure
  // DEVICE_LOCAL request (no HV) still asserts below — degrading that would silently
  // change GPU-read perf class and mapability expectations.
  if ((properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) and (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
    VkMemoryPropertyFlags degraded = properties & ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
      if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & degraded) == degraded) {
        return i;
      }
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
  if (VkMemTrace::enabled())
    VkMemTrace::instance().onAlloc("IMG", _ctxVK, size_t(_memreq->size), memprops, _allocinfo->memoryTypeIndex, OK);
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
    if(_ctxVK && _vkmem) {
      if (VkMemTrace::enabled() && _memreq && _allocinfo)
        VkMemTrace::instance().onFree("IMG", _ctxVK, size_t(_memreq->size), _allocinfo->memoryTypeIndex);
      _ctxVK->destroyImageMemory(*_vkmem); // no-op post-shutdown
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
  if (VkMemTrace::enabled())
    VkMemTrace::instance().onAlloc("BUF", _ctxVK, size_t(_memreq->size), memprops, _allocinfo->memoryTypeIndex, OK);
  // A DEVICE_LOCAL|HOST_VISIBLE request targets the BAR window, which on non-ReBAR-sized
  // parts is tiny (often 256MB) — heap exhaustion there is an expected steady state, not
  // a bug. Degrade to plain host-visible sysram and retry once (memtrace logs both
  // attempts, so the degrade is visible in the trace).
  if (OK != VK_SUCCESS                                            //
      and (memprops & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)        //
      and (memprops & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {     //
    VkMemoryPropertyFlags degraded = memprops & ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    _allocinfo->memoryTypeIndex    = _ctxVK->_findMemoryType(_memreq->memoryTypeBits, degraded);
    OK = vkAllocateMemory(_ctxVK->_vkdevice, _allocinfo.get(), nullptr, _vkmem.get());
    if (VkMemTrace::enabled())
      VkMemTrace::instance().onAlloc("BUF", _ctxVK, size_t(_memreq->size), degraded, _allocinfo->memoryTypeIndex, OK);
  }
  OrkAssert(OK == VK_SUCCESS);
}

VulkanMemoryForBuffer::~VulkanMemoryForBuffer() {
  try {
    if(_ctxVK && _ctxVK->_vkdevice && _vkmem) {
      if (VkMemTrace::enabled() && _memreq && _allocinfo)
        VkMemTrace::instance().onFree("BUF", _ctxVK, size_t(_memreq->size), _allocinfo->memoryTypeIndex);
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
  if (VkMemTrace::enabled())
    g_vkmemtrace_tag = FormatString(
        "%s|fmt<%d>|%ux%ux%u|mips<%u>", name.c_str(), int(cinfo->format), cinfo->extent.width, cinfo->extent.height,
        cinfo->extent.depth, cinfo->mipLevels);
  _imgmem = std::make_shared<VulkanMemoryForImage>(_ctx, _vkimage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (VkMemTrace::enabled())
    g_vkmemtrace_tag.clear();
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
  if (getenv("ORKID_VK_IMAGE_TRACE")) {
    printf(
        "[VKIMGTRACE] vkimage<%p> name<%s> fmt<%d> extent<%ux%ux%u> usage<0x%x>\n",
        (void*)_vkimage,
        name.c_str(),
        int(cinfo->format),
        cinfo->extent.width,
        cinfo->extent.height,
        cinfo->extent.depth,
        unsigned(cinfo->usage));
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
    if (_ctx) {
      // pass only the handles this object owns; the funnel no-ops past shutdown.
      _ctx->destroyImageObject(
          _delete_imageview   ? _vkimageview : VK_NULL_HANDLE,
          _delete_image       ? _vkimage     : VK_NULL_HANDLE,
          _delete_devicemem   ? _vkdevicemem : VK_NULL_HANDLE);
    }
    _vkimage = VK_NULL_HANDLE;
    _imgmem  = nullptr;
  } catch (...) {
    // Swallow — during static destruction _ctx may be dangling.
  }
}
///////////////////////////////////////////////////////////////////////////////

std::atomic<int> VulkanBuffer::_buffercount    = 0;
std::atomic<size_t> VulkanBuffer::_bufferbytes = 0;
std::atomic<size_t> VulkanBuffer::_bufferSN = 0;

VulkanBuffer::VulkanBuffer(vkcontext_rawptr_t ctxVK, size_t length, VkBufferUsageFlags usage, std::string name,
                           VkMemoryPropertyFlags memprops)
    : _ctxVK(ctxVK)
    , _length(length)
    , _usage(usage)
    , _hostVisible((memprops & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
  
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

  if (VkMemTrace::enabled())
    g_vkmemtrace_tag = FormatString("%s|usage<0x%x>", name.c_str(), unsigned(usage));
  _memory = std::make_shared<VulkanMemoryForBuffer>(ctxVK, _vkbuffer, memprops);
  if (VkMemTrace::enabled())
    g_vkmemtrace_tag.clear();
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
    if (_ctxVK) {
      if (_deferredDestroy) {
        // last ref may drop on ANY thread while a recorded CB still references the
        // buffer — hand the VK objects to the context's delayed-destroy queue (>=
        // frames in flight). The lambda holds _memory alive; vkFreeMemory (and the
        // memtrace FREE accounting in ~VulkanMemoryForBuffer) runs at drain time.
        // VkContexts persist to process end (teardown-funnel convention), so the
        // captured raw ctx outlives the queue; post-shutdown the funnel no-ops.
        auto ctx   = _ctxVK;
        auto vkbuf = _vkbuffer;
        auto mem   = _memory;
        _ctxVK->enqueueDelayedDestroy([ctx, vkbuf, mem]() {
          ctx->destroyBuffer(vkbuf); // no-op post-shutdown
        }, 3);
      } else {
        // GPU-idle contract (bake arena / FXI destroyStorageBuffer): reclaim NOW.
        _ctxVK->destroyBuffer(_vkbuffer); // no-op post-shutdown
      }
    }
  } catch (...) {
    // Swallow — during static destruction _ctxVK may be dangling.
  }
  _buffercount.fetch_sub(1);
  _bufferbytes.fetch_sub(_length);
  _memory = nullptr;
}
//////////////////////////////////////
void VulkanBuffer::copyFromHost(const void* src, size_t length, size_t dstOffset) {
  OrkAssert((dstOffset + length) <= _length);
  if (not _hostVisible) {                                  // device-local: stage host -> staging -> this
    auto& st = _ctxVK->_syncTransfer;
    std::lock_guard<std::mutex> lock(st.mutex);
    _ctxVK->ensureSyncStagingSize(length);
    void* sp = st.staging_buffer->map(0, length, 0);
    std::memcpy(sp, src, length);
    st.staging_buffer->unmap();
    _ctxVK->beginSyncTransferCB();
    VkBufferCopy region{}; region.srcOffset = 0; region.dstOffset = dstOffset; region.size = length;
    vkCmdCopyBuffer(st.command_buffer_impl->_vkcmdbuf, st.staging_buffer->_vkbuffer, _vkbuffer, 1, &region);
    _ctxVK->endAndSubmitSyncTransferCB();
    return;
  }
  void* dst = this->map(dstOffset, length, 0);
  std::memcpy(dst, src, length);
  this->unmap();
}
//////////////////////////////////////
void VulkanBuffer::copyToHost(void* dst, size_t length, size_t srcOffset) {
  OrkAssert((srcOffset + length) <= _length);
  if (not _hostVisible) {                                  // device-local: stage this -> staging -> host
    auto& st = _ctxVK->_syncTransfer;
    std::lock_guard<std::mutex> lock(st.mutex);
    // readback staging is HOST_CACHED (the CPU reads it below) and TRANSFER_DST —
    // CPU reads from the write-combined upload staging run ~150MB/s.
    _ctxVK->ensureSyncReadbackStagingSize(length);
    _ctxVK->beginSyncTransferCB();
    VkBufferCopy region{}; region.srcOffset = srcOffset; region.dstOffset = 0; region.size = length;
    vkCmdCopyBuffer(st.command_buffer_impl->_vkcmdbuf, _vkbuffer, st.readback_buffer->_vkbuffer, 1, &region);
    _ctxVK->endAndSubmitSyncTransferCB();
    void* sp = st.readback_buffer->map(0, length, 0);
    memcpy_fast(dst, sp, length);
    st.readback_buffer->unmap();
    return;
  }
  void* src = this->map(srcOffset, length, 0);
  memcpy_fast(dst, src, length);
  this->unmap();
}
//////////////////////////////////////
void* VulkanBuffer::map(size_t offset, size_t length, VkMemoryMapFlags flags) {
  OrkAssert(_hostVisible && "VulkanBuffer::map on a device-local buffer — use copyFromHost/copyToHost (or the FXI staging map path)");
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
