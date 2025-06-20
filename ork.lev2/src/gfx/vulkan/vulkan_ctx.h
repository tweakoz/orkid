////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <memory>

///////////////////////////////////////////////////////////////////////////////

struct GLFWwindow;

#if defined(__APPLE__)
#define VK_USE_PLATFORM_MACOS_MVK
#endif

#include <vulkan/vulkan.hpp>

///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/svariant.h>
#include <ork/kernel/concurrent_queue.h>
#include <ork/kernel/datablock.h>
#include <ork/kernel/datacache.h>
#include <ork/kernel/orkpool.inl>
#include <ork/file/chunkfile.inl>

///////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/shadlang.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/shadlang.h>
#include <ork/lev2/gfx/shadman.h>

#define GLFW_INCLUDE_VULKAN
#import <ork/lev2/glfw/ctx_glfw.h>
#include <GLFW/glfw3native.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::dds {
struct DDS_HEADER;
}

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

constexpr EBufferFormat DEPTH_FORMAT = EBufferFormat::Z24S8;

inline VkDeviceSize vkAlignUp(
    VkDeviceSize value,       //
    VkDeviceSize alignment) { //
  return (value + alignment - 1) & ~(alignment - 1);
}

template <typename T> void initializeVkStruct(T& s, VkStructureType s_type) {
  memset(&s, 0, sizeof(T));
  s.sType = s_type;
}
template <typename T> void initializeVkStruct(T& s) {
  memset(&s, 0, sizeof(T));
}
struct VulkanInstance;
struct VulkanDeviceInfo;
struct VulkanDeviceGroup;
//
struct VkContext;
struct VkDrawingInterface;
struct VkImiInterface;
struct VkRasterStateInterface;
struct VkMatrixStackInterface;
struct VkFrameBufferInterface;
struct VkGeometryBufferInterface;
struct VkTextureInterface;
struct VkFxInterface;
struct VkComputeInterface;
//
struct VulkanRenderInfo;
struct VulkanPipelineRenderInfo;
//
struct VulkanTextureObject;
struct VulkanFxShaderObject;
struct VkFxShaderFile;
struct VkFxShaderProgram;
struct VkFxShaderPass;
struct VkFxShaderTechnique;
struct VkFxShaderSamplerSet;
struct VkFxShaderSamplerSetItem;
struct VkFxShaderUniformSet;
struct VkFxShaderUniformSetItem;
struct VkFxShaderUniformSetSampler;
struct VkFxShaderUniformBlk;
struct VkFxShaderUniformBlkItem;
struct VkFxShaderPushConstantBlock;
struct VkPipelineObject;
struct VkPrimitiveClass;
struct VklRtBufferImpl;
struct VkRtGroupImpl;
struct VkTextureAsyncTask;
struct VkTexLoadReq;
struct VulkanVertexBuffer;
struct VkVertexInputConfiguration;
struct VulkanIndexBuffer;
struct VkLoadContext;
struct VkPrimaryCommandBufferImpl;
struct VkSecondaryCommandBufferImpl;
struct VkSwapChainCaps;
struct VkSwapChain;
struct VkMsaaState;
struct VkRasterState;
struct VkBufferLayout;
struct VulkanSemaphoreBase;
struct VulkanBinarySemaphore;
struct VulkanTimelineSemaphore;
struct VulkanCompletionSemaphore;

//
using vkinstance_ptr_t   = std::shared_ptr<VulkanInstance>;
using vkdeviceinfo_ptr_t = std::shared_ptr<VulkanDeviceInfo>;
using vkdevgrp_ptr_t     = std::shared_ptr<VulkanDeviceGroup>;
using vkcontext_ptr_t    = std::shared_ptr<VkContext>;
using vkcontext_rawptr_t = VkContext*;

using vkrenderinfo_ptr_t          = std::shared_ptr<VulkanRenderInfo>;
using vkpipelinerenderinfo_ptr_t  = std::shared_ptr<VulkanPipelineRenderInfo>;
using vksemaphorebase_ptr_t       = std::shared_ptr<VulkanSemaphoreBase>;
using vkbinarysemaphore_ptr_t     = std::shared_ptr<VulkanBinarySemaphore>;
using vktimelinesemaphore_ptr_t   = std::shared_ptr<VulkanTimelineSemaphore>;
using vkcompletionsemaphore_ptr_t = std::shared_ptr<VulkanCompletionSemaphore>;
//
using vkdwi_ptr_t = std::shared_ptr<VkDrawingInterface>;
using vkimi_ptr_t = std::shared_ptr<VkImiInterface>;
// using vkrsi_ptr_t = std::shared_ptr<VkRasterStateInterface>;
using vkmsi_ptr_t    = std::shared_ptr<VkMatrixStackInterface>;
using vkfbi_ptr_t    = std::shared_ptr<VkFrameBufferInterface>;
using vkgbi_ptr_t    = std::shared_ptr<VkGeometryBufferInterface>;
using vktxi_ptr_t    = std::shared_ptr<VkTextureInterface>;
using vktxi_rawptr_t = VkTextureInterface*;

using vkfxi_ptr_t = std::shared_ptr<VkFxInterface>;
using vkci_ptr_t  = std::shared_ptr<VkComputeInterface>;
//
using vktexobj_ptr_t        = std::shared_ptr<VulkanTextureObject>;
using vkfxsfile_ptr_t       = std::shared_ptr<VkFxShaderFile>;
using vkfxsobj_ptr_t        = std::shared_ptr<VulkanFxShaderObject>;
using vkfxsprg_ptr_t        = std::shared_ptr<VkFxShaderProgram>;
using vkfxspass_ptr_t       = std::shared_ptr<VkFxShaderPass>;
using vkfxstek_ptr_t        = std::shared_ptr<VkFxShaderTechnique>;
using vkpipeline_obj_ptr_t  = std::shared_ptr<VkPipelineObject>;
using vkprimclass_ptr_t     = std::shared_ptr<VkPrimitiveClass>;
using vkfxssmpset_ptr_t     = std::shared_ptr<VkFxShaderSamplerSet>;
using vkfxsuniset_ptr_t     = std::shared_ptr<VkFxShaderUniformSet>;
using vkfxsunisetitem_ptr_t = std::shared_ptr<VkFxShaderUniformSetItem>;
using vkfxsunisetsamp_ptr_t = std::shared_ptr<VkFxShaderUniformSetSampler>;

using vkfxsuniblk_ptr_t         = std::shared_ptr<VkFxShaderUniformBlk>;
using vkfxsuniblkitem_ptr_t     = std::shared_ptr<VkFxShaderUniformBlkItem>;
using vkfxpushconstantblk_ptr_t = std::shared_ptr<VkFxShaderPushConstantBlock>;
using vkbufferlayout_ptr_t      = std::shared_ptr<VkBufferLayout>;

using vkrtbufimpl_ptr_t         = std::shared_ptr<VklRtBufferImpl>;
using vkrtgrpimpl_ptr_t         = std::shared_ptr<VkRtGroupImpl>;
using vktexasynctask_ptr_t      = std::shared_ptr<VkTextureAsyncTask>;
using vktexloadreq_ptr_t        = std::shared_ptr<VkTexLoadReq>;
using vkfxshader_bin_t          = std::vector<uint32_t>;
using vkvtxbuf_ptr_t            = std::shared_ptr<VulkanVertexBuffer>;
using vkidxbuf_ptr_t            = std::shared_ptr<VulkanIndexBuffer>;
using vkvertexinputconfig_ptr_t = std::shared_ptr<VkVertexInputConfiguration>;
using vkloadctx_ptr_t           = std::shared_ptr<VkLoadContext>;
using vkpricmdbufimpl_ptr_t     = std::shared_ptr<VkPrimaryCommandBufferImpl>;
using vkseccmdbufimpl_ptr_t     = std::shared_ptr<VkSecondaryCommandBufferImpl>;
using vkswapchaincaps_ptr_t     = std::shared_ptr<VkSwapChainCaps>;
using vkswapchain_ptr_t         = std::shared_ptr<VkSwapChain>;
using vkmsaastate_ptr_t         = std::shared_ptr<VkMsaaState>;
using vkrasterstate_ptr_t       = std::shared_ptr<VkRasterState>;

using smpset_map_t      = std::map<std::string, vkfxssmpset_ptr_t>;
using uniset_map_t      = std::map<std::string, vkfxsuniset_ptr_t>;
using uniblk_map_t      = std::map<std::string, vkfxsuniblk_ptr_t>;
using uniset_item_map_t = std::map<std::string, vkfxsunisetitem_ptr_t>;

extern vkinstance_ptr_t _GVI;

using vkmemreq_ptr_t       = std::shared_ptr<VkMemoryRequirements>;
using vkmemallocinfo_ptr_t = std::shared_ptr<VkMemoryAllocateInfo>;
using vkmem_ptr_t          = std::shared_ptr<VkDeviceMemory>;
struct VulkanMemoryForImage;
struct VulkanMemoryForBuffer;
struct VulkanBuffer;
struct VulkanImageObject;
using vkmemforimg_ptr_t       = std::shared_ptr<VulkanMemoryForImage>;
using vkmemforbuf_ptr_t       = std::shared_ptr<VulkanMemoryForBuffer>;
using vkbuffer_ptr_t          = std::shared_ptr<VulkanBuffer>;
using vkimagecreateinfo_ptr_t = std::shared_ptr<VkImageCreateInfo>;
using vkimageobj_ptr_t        = std::shared_ptr<VulkanImageObject>;

using vkivci_ptr_t              = std::shared_ptr<VkImageViewCreateInfo>;
using vksamplercreateinfo_ptr_t = std::shared_ptr<VkSamplerCreateInfo>;

vkivci_ptr_t createImageViewInfo2D(VkImage image, VkFormat format, VkImageAspectFlagBits aspectMask);

struct VkFormatConverter {

  static const VkFormatConverter _instance;
  VkFormatConverter();
  static VkFormat convertBufferFormat(EBufferFormat fmt_in);
  static EBufferFormat convertBufferFormat(VkFormat fmt_in);
  static VkImageLayout layoutForUsage(uint64_t usage);
  static VkImageAspectFlagBits aspectForUsage(uint64_t usage);
  std::unordered_map<EBufferFormat, VkFormat> _fmtmap;
  std::unordered_map<VkFormat, EBufferFormat> _inv_fmtmap;
  std::unordered_map<uint64_t, VkImageLayout> _layoutmap;
  std::unordered_map<uint64_t, VkImageAspectFlagBits> _aspectmap;
};

vkimagecreateinfo_ptr_t makeVKICI(
    int w,
    int h,
    int d, //
    EBufferFormat fmt,
    int nummips);

vksamplercreateinfo_ptr_t makeVKSCI();

struct VulkanVertexInterface;
struct VulkanVertexInterfaceInput;
using vkvertexinterfaceinput_ptr_t = std::shared_ptr<VulkanVertexInterfaceInput>;
using vkvertexinterface_ptr_t      = std::shared_ptr<VulkanVertexInterface>;

struct VulkanGeometryInterface;
struct VulkanGeometryInterfaceInput;
using vkgeometryinterfaceinput_ptr_t = std::shared_ptr<VulkanGeometryInterfaceInput>;
using vkgeometryinterface_ptr_t      = std::shared_ptr<VulkanGeometryInterface>;

uint64_t hashImageCreationParams(
    int w,                             //
    int h,                             //
    int d,                             //
    EBufferFormat fmt,                 //
    int nummips,
    uint64_t usage );
    
      ///////////////////////////////////////////////////////////////////////////////

struct VkViewportTracker {

  int _width  = 0;
  int _height = 0;
  int _x      = 0;
  int _y      = 0;
};
using vkviewporttracker_ptr_t = std::shared_ptr<VkViewportTracker>;

///////////////////////////////////////////////////////////////////////////////

struct VkSwapChainCaps {

  bool supportsPresentationMode(VkPresentModeKHR mode) const;

  VkSurfaceCapabilitiesKHR _capabilities;
  std::vector<VkSurfaceFormatKHR> _formats;
  std::set<VkPresentModeKHR> _presentModes;
};

///////////////////////////////////////////////////////////////////////////////

struct VulkanDeviceInfo {

  VkPhysicalDevice _phydev;
  VkPhysicalDeviceProperties _devprops;
  VkPhysicalDeviceFeatures _devfeatures;
  VkPhysicalDeviceFeatures2 _devfeatures2;
  VkPhysicalDeviceMemoryProperties _devmemprops;
  std::vector<VkExtensionProperties> _extensions;
  std::vector<VkMemoryHeap> _heaps;
  std::vector<VkQueueFamilyProperties> _queueprops;
  std::set<std::string> _extension_set;

  bool _is_discrete                = false;
  bool _supportsDynamicRendering   = false;
  bool _supportsVulkan13           = false;
  bool _supportsTimelineSemaphores = false;
  bool _supportsSynchronization2   = false;

  size_t _maxWkgCountX = 0;
  size_t _maxWkgCountY = 0;
  size_t _maxWkgCountZ = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct VulkanDeviceGroup {
  size_t _deviceCount = 0;
  std::vector<vkdeviceinfo_ptr_t> _device_infos;
};

///////////////////////////////////////////////////////////////////////////////

struct VulkanInstance {

  VulkanInstance();
  ~VulkanInstance();
  void _setupDebugMessenger();

  VkApplicationInfo _appdata;
  VkInstanceCreateInfo _instancedata;
  VkInstance _instance;
  std::vector<VkPhysicalDeviceGroupProperties> _phygroups;
  std::vector<vkdevgrp_ptr_t> _devgroups;
  std::vector<vkdeviceinfo_ptr_t> _device_infos;
  vkdeviceinfo_ptr_t findDeviceForSurface(VkSurfaceKHR surface);
  std::vector<const char*> _instance_extensions;
  uint32_t _numgpus   = 0;
  uint32_t _numgroups = 0;
  shadlang::slpcache_ptr_t _slp_cache;
  MpMcBoundedQueue<load_token_t> _loadTokens;
  bool _debugEnabled = false;
  vkdeviceinfo_ptr_t _preferred;

  std::set<VkContext*> _contexts;
};

///////////////////////////////////////////////////////////////////////////////

struct VkMsaaState {
  VkMsaaState();
  VkPipelineMultisampleStateCreateInfo _VKSTATE;
  int _pipeline_bits = -1;
};

struct VkRasterState {
  VkRasterState(rasterstate_ptr_t rstate);
  VkPipelineRasterizationStateCreateInfo _VKRSCI;
  VkPipelineDepthStencilStateCreateInfo _VKDSSCI;
  VkPipelineColorBlendStateCreateInfo _VKCBSI;
  VkPipelineColorBlendAttachmentState _VKCBATT;
  int _pipeline_bits = -1;

  using rsmap_t = std::unordered_map<uint64_t, int>;

  static LockedResource<rsmap_t> _global_rasterstate_map;
};

///////////////////////////////////////////////////////////////////////////

struct VulkanVertexInterfaceInput {
  std::string _datatype;
  std::string _identifier;
  std::string _semantic;
  size_t _datasize = 0;
};
struct VulkanVertexInterface {
  using input_t = VulkanVertexInterfaceInput;
  std::string _name;
  vkvertexinterface_ptr_t _parent;
  std::vector<vkvertexinterfaceinput_ptr_t> _inputs;
  int _pipeline_bits = -1;
  uint64_t _hash     = 0;
};

struct VulkanGeometryInterfaceInput {
  std::string _datatype;
  std::string _identifier;
  std::string _semantic;
  size_t _datasize = 0;
};
struct VulkanGeometryInterface {
  using input_t = VulkanGeometryInterfaceInput;
  std::string _name;
  vkgeometryinterface_ptr_t _parent;
  std::vector<vkgeometryinterfaceinput_ptr_t> _inputs;
  int _pipeline_bits = -1;
  uint64_t _hash     = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct VkPrimaryCommandBufferImpl {

  VkPrimaryCommandBufferImpl(vkcontext_rawptr_t ctxVK);
  ~VkPrimaryCommandBufferImpl();

  VkCommandBuffer _vkcmdbuf = VK_NULL_HANDLE;
  bool _recorded            = false;
  vkcontext_rawptr_t _contextVK;
  PrimaryCommandBuffer* _orkCB = nullptr;

  std::vector<secondary_commandbuffer_ptr_t> _secondary_cmdbuffers;
  static std::atomic<int> _cmdbufcount;
};
struct VkSecondaryCommandBufferImpl {

  VkSecondaryCommandBufferImpl(vkcontext_rawptr_t ctxVK);
  ~VkSecondaryCommandBufferImpl();

  VkCommandBuffer _vkcmdbuf      = VK_NULL_HANDLE;
  SecondaryCommandBuffer* _orkCB = nullptr;
  bool _recorded                 = false;
  vkcontext_rawptr_t _contextVK;
  static std::atomic<int> _cmdbufcount;
  // Optional timeline semaphore to signal when this command buffer completes
  vkcompletionsemaphore_ptr_t _completionSemaphore;
};

struct VulkanRenderInfo {
  VulkanRenderInfo(rtgroup_rawptr_t rtg);
  ~VulkanRenderInfo();

  rtgroup_rawptr_t _rtg;
  VkRenderingInfo _renderinfo;
  std::vector<VkRenderingAttachmentInfo> _rainfos_color;
  VkRenderingAttachmentInfo _rainfo_depth;
};
struct VulkanPipelineRenderInfo {
  VulkanPipelineRenderInfo(rtgroup_rawptr_t rtg);
  ~VulkanPipelineRenderInfo();

  rtgroup_ptr_t _rtg;
  VkPipelineRenderingCreateInfo _createInfo;
};

///////////////////////////////////////////////////////////////////////////////

struct VkTransitionParams {
    VkImageLayout layout;
    VkAccessFlagBits srcAccess, dstAccess;
    VkPipelineStageFlags srcStage, dstStage;
    bool colorOnly = false;
};

///////////////////////////////////////////////////////////////////////////////

struct VklRtBufferImpl {
  VklRtBufferImpl(VkRtGroupImpl* par, RtBuffer* rtb);

  void _transitionImage(vkpricmdbufimpl_ptr_t cb, const VkTransitionParams& params);
  void _transitionToRenderTarget(vkpricmdbufimpl_ptr_t cb);
  void _transitionToTexture(vkpricmdbufimpl_ptr_t cb);
  void _transitionToHostRead(vkpricmdbufimpl_ptr_t cb);

  void setLayout(VkImageLayout layout);
  void _replaceImage(VkFormat new_fmt, VkImageView new_view, VkImage new_img);

  VkRtGroupImpl* _rtg_impl = nullptr;
  RtBuffer* _rtb           = nullptr;
  bool _init               = true;
  bool _is_surface         = false;
  VkImage _vkimg;
  vkimageobj_ptr_t _imgobj;
  VkFormat _vkfmt;
  VkImageView _vkimgview;
  VkAttachmentDescription _attachmentDesc;
  VkAttachmentReference _attachmentRef;
  VkDescriptorImageInfo _descriptorInfo;
  VkImageLayout _currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  svar64_t _teximpl;
};

struct RtGroupAttachments {
  std::vector<VkAttachmentDescription> _descriptions;
  std::vector<VkAttachmentReference> _references;
  std::vector<VkImageView> _imageviews;
  std::vector<VkDescriptorImageInfo> descimginfos;
};

using rtgroup_attachments_ptr_t = std::shared_ptr<RtGroupAttachments>;

///////////////////////////////////////////////////////////////////////////////

struct VkRtGroupImpl {
  VkRtGroupImpl(vkcontext_rawptr_t ctxVK, rtgroup_rawptr_t _rtg);

  rtgroup_attachments_ptr_t attachments();
  vkrenderinfo_ptr_t renderinfo();

  void _transitionToRenderTarget(vkpricmdbufimpl_ptr_t cb);
  void _transitionToTexture(vkpricmdbufimpl_ptr_t cb);
  void _transitionToHostRead(vkpricmdbufimpl_ptr_t cb);

  rtgroup_rawptr_t _rtg = nullptr;
  vkrtbufimpl_ptr_t _standard;
  vkrtbufimpl_ptr_t _depthonly;
  rtgroup_attachments_ptr_t __attachments;
  vkcontext_rawptr_t _contextVK = nullptr;
  
  int _width         = 0;
  int _height        = 0;
  int _pipeline_bits = -1;
  vkmsaastate_ptr_t _msaaState;

  vkrenderinfo_ptr_t _rinfo_retain;
  vkpipelinerenderinfo_ptr_t _prinfo_retain;

  secondary_commandbuffer_ptr_t _cmdbufRTG;
  VkCommandBufferBeginInfo _cmdBufCBBI_GFX;
  VkCommandBufferInheritanceInfo _cmdBufII;
  std::unordered_set<vkrenderinfo_ptr_t> _renderinfo_set;
};

///////////////////////////////////////////////////////////////////////////////

struct VkTextureAsyncTask {
  VkTextureAsyncTask();
  std::atomic<int> _lock;
  std::queue<void_lambda_t> _onFinished;
};

///////////////////////////////////////////////////////////////////////////////

struct VkTexLoadReq {
  texture_ptr_t ptex;
  const dds::DDS_HEADER* _ddsheader = nullptr;
  vktexobj_ptr_t pTEXOBJ;
  std::string _texname;
  DataBlockInputStream _inpstream;
  std::shared_ptr<CompressedImageMipChain> _cmipchain;
};

///////////////////////////////////////////////////////////////////////////////

struct VulkanMemoryForImage {
  VulkanMemoryForImage(vkcontext_rawptr_t ctxVK, VkImage image, VkMemoryPropertyFlags memprops);
  ~VulkanMemoryForImage();

  vkcontext_rawptr_t _ctxVK;
  VkImage _vkimage;
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
  VkBuffer _vkbuffer;
  vkmemreq_ptr_t _memreq;
  vkmemallocinfo_ptr_t _allocinfo;
  vkmem_ptr_t _vkmem;
};

///////////////////////////////////////////////////////////////////////////////

struct VulkanBuffer {
  VulkanBuffer(vkcontext_rawptr_t ctxVK, size_t length, VkBufferUsageFlags usage, std::string name = "");
  ~VulkanBuffer();

  void copyFromHost(const void* src, size_t length);
  void copyToHost(void* dst, size_t length);
  void* map(size_t offset, size_t length, VkMemoryMapFlags flags);
  void unmap();

  vkcontext_rawptr_t _ctxVK;
  size_t _length;
  VkBufferUsageFlags _usage;
  VkBufferCreateInfo _cinfo;
  VkBuffer _vkbuffer;
  vkmemforbuf_ptr_t _memory;

  static std::atomic<int> _buffercount;
  static std::atomic<size_t> _bufferbytes;
  static std::atomic<size_t> _bufferSN;
};

using barrier_ptr_t = std::shared_ptr<VkImageMemoryBarrier>;
barrier_ptr_t createImageBarrier(
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkAccessFlagBits srcAccessMask,
    VkAccessFlagBits dstAccessMask);

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
using vkfence_obj_ptr_t = std::shared_ptr<VulkanFenceObject>;

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
using vkevent_obj_ptr_t = std::shared_ptr<VulkanEventObject>;

///////////////////////////////////////////////////////////////////////////////

struct VulkanImageObject {
  VulkanImageObject(vkcontext_rawptr_t ctx, vkimagecreateinfo_ptr_t cinfo, std::string name = "");
  ~VulkanImageObject();
  vkcontext_rawptr_t _ctx = nullptr;
  vkimagecreateinfo_ptr_t _cinfo;
  VkImage _vkimage;
  VkImageView _vkimageview;
  vkmemforimg_ptr_t _imgmem;
  static std::atomic<int> _imgobjcount;
  static std::atomic<size_t> _imgobjSN;
};

struct VulkanSamplerObject {
  VulkanSamplerObject(vkcontext_rawptr_t ctx, vksamplercreateinfo_ptr_t cinfo);
  vksamplercreateinfo_ptr_t _cinfo;
  VkSampler _vksampler;
};
using vksampler_obj_ptr_t = std::shared_ptr<VulkanSamplerObject>;

struct InFlightTextureTransfer {
  InFlightTextureTransfer(vkcontext_rawptr_t ctx, 
                          vkbuffer_ptr_t stg_buffer,
                          secondary_commandbuffer_ptr_t cmd_buffer);
  ~InFlightTextureTransfer();
  vkbuffer_ptr_t _staging_buffer;
  secondary_commandbuffer_ptr_t _command_buffer;
  static std::atomic<int> _xfercount;
  static std::atomic<size_t> _xferSN;
};
using inflighttextrans_ptr_t = std::shared_ptr<InFlightTextureTransfer>;
struct VulkanTextureObject {

  VulkanTextureObject(vktxi_rawptr_t txi);
  ~VulkanTextureObject();

  std::unordered_set<vkbuffer_ptr_t> _staging_buffers;
  vkimageobj_ptr_t _imgobj;
  int _maxmip = 0;
  vktexasynctask_ptr_t _async;
  vktxi_rawptr_t _txi;
  vksampler_obj_ptr_t _vksampler;
  VkDescriptorImageInfo _vkdescriptor_info;
  secondary_commandbuffer_ptr_t _loadCB;
  uint64_t _image_params_hash = 0;

  std::unordered_set<inflighttextrans_ptr_t> _inflight_transfers;

  static std::atomic<size_t> _vkto_count;
};

///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderUniformSetItem {
  std::string _datatype;
  std::string _identifier;
  size_t _offset = 0;
  std::shared_ptr<FxShaderParam> _orkparam;
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderUniformSetSampler {
  size_t _binding_id = -1;
  std::string _datatype;
  std::string _identifier;
  std::shared_ptr<FxShaderParam> _orkparam;
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderUniformSet {
  std::unordered_map<std::string, vkfxsunisetitem_ptr_t> _items_by_name;
  std::vector<vkfxsunisetitem_ptr_t> _items_by_order;
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderDescriptorSet {
  size_t _descriptor_set_id = 0;
};
using vkfxdescset_ptr_t = std::shared_ptr<VkFxShaderDescriptorSet>;
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderSamplerSet : public VkFxShaderDescriptorSet {
  std::unordered_map<std::string, vkfxsunisetsamp_ptr_t> _samplers_by_name;
  std::vector<vkfxsunisetsamp_ptr_t> _samplers_by_order;
  svar64_t _impl;
};
struct VkFxShaderUniformBlkItem {
  std::string _datatype;
  std::string _identifier;
  size_t _offset = 0;
  std::shared_ptr<FxShaderParam> _orkparam;
};
struct VkFxShaderUniformBlk : public VkFxShaderDescriptorSet {
  std::shared_ptr<FxUniformBlock> _orkparamblock;
  std::unordered_map<std::string, vkfxsuniblkitem_ptr_t> _items_by_name;
  std::vector<vkfxsuniblkitem_ptr_t> _items_by_order;
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderUniformSetsReference {
  uniset_map_t _unisets;
};
struct VkFxShaderUniformBlksReference {
  uniblk_map_t _uniblks;
};
struct VkFxShaderSamplerSetsReference {
  static size_t descriptor_set_counter;
  smpset_map_t _smpsets;
};
using vkfxsunisetsref_ptr_t = std::shared_ptr<VkFxShaderUniformSetsReference>;
using vkfxsuniblksref_ptr_t = std::shared_ptr<VkFxShaderUniformBlksReference>;
using vkfxssmpsetsref_ptr_t = std::shared_ptr<VkFxShaderSamplerSetsReference>;
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderPushConstantBlock {
  uniset_map_t _vtx_unisets;
  uniset_map_t _frg_unisets;

  uniset_item_map_t _vtx_items_by_name;
  uniset_item_map_t _frg_items_by_name;

  vkbufferlayout_ptr_t _data_layout;

  std::vector<VkPushConstantRange> _ranges;
  size_t _blockSize = 0;
};

using descriptor_bindings_vect_t = std::vector<VkDescriptorSetLayoutBinding>;
// using descriptor_samplerinfos_vect_t = std::vector<VkSamplerCreateInfo>;
struct VkDescriptorSetBindings {

  std::map<size_t, vkfxdescset_ptr_t> _descriptorsets;

  descriptor_bindings_vect_t _vkbindings;
  size_t _sampler_count = 0;
  VkDescriptorSetLayout _dsetlayout;
};

using vkdescriptorbindings_ptr_t = std::shared_ptr<VkDescriptorSetBindings>;

///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderFile {
  std::string _shader_name;
  // shadlang::SHAST::translationunit_ptr_t _trans_unit;
  std::unordered_map<std::string, vkfxsobj_ptr_t> _vk_shaderobjects;
  std::unordered_map<std::string, vkfxstek_ptr_t> _vk_techniques;
  std::unordered_map<std::string, vkfxssmpset_ptr_t> _vk_samplersets;
  std::unordered_map<std::string, vkfxsuniset_ptr_t> _vk_uniformsets;
  std::unordered_map<std::string, vkfxsuniblk_ptr_t> _vk_uniformblks;
  std::unordered_map<std::string, vkvertexinterface_ptr_t> _vk_vtxinterfaces;
  std::unordered_map<std::string, vkgeometryinterface_ptr_t> _vk_geointerfaces;
};

struct VulkanFxShaderObject {

  VulkanFxShaderObject(vkcontext_rawptr_t ctx, vkfxshader_bin_t bin);
  ~VulkanFxShaderObject();

  vkcontext_rawptr_t _contextVK;
  vkfxshader_bin_t _spirv_binary;
  VkShaderModuleCreateInfo _vk_shadermoduleinfo;
  VkShaderModule _vk_shadermodule;
  VkPipelineShaderStageCreateInfo _shaderstageinfo;
  // shadlang::SHAST::astnode_ptr_t _astnode; // debug only
  vkfxsunisetsref_ptr_t _uniset_refs;
  vkfxsuniblksref_ptr_t _uniblk_refs;
  vkfxssmpsetsref_ptr_t _smpset_refs;
  std::unordered_map<std::string, vkfxsuniblk_ptr_t> _vk_uniformblks;
  std::vector<std::string> _vk_interfaces;

  uint64_t _STAGE = 0;
  VkPushConstantRange _vkpc_range;
  std::string _name;
};

struct VkParamSetItem {
  VkFxShaderUniformSetItem* _vk_param = nullptr;
  fxparam_constptr_t _ork_param       = nullptr;
  svar64_t _value;
};

struct VkFxShaderProgram {

  VkFxShaderProgram(VkFxShaderFile* file);

  void bindDescriptorTexture(fxparam_constptr_t param, const Texture* pTex);

  vkfxsobj_ptr_t _vtxshader;
  vkfxsobj_ptr_t _geoshader;
  vkfxsobj_ptr_t _tctshader;
  vkfxsobj_ptr_t _tevshader;
  vkfxsobj_ptr_t _frgshader;
  vkfxsobj_ptr_t _comshader;

  vkvertexinterface_ptr_t _vertexinterface;
  vkgeometryinterface_ptr_t _geometryinterface;

  vkfxpushconstantblk_ptr_t _pushConstantBlock;

  std::vector<VkParamSetItem> _pending_params;
  std::vector<void_lambda_t> _pending_param_ops;
  std::vector<uint8_t> _pushdatabuffer;
  vkdescriptorbindings_ptr_t _descriptors;
  std::unordered_map<fxparam_constptr_t, size_t> _samplers_by_orkparam;
  std::unordered_map<fxparam_constptr_t, vktexobj_ptr_t> _textures_by_orkparam;
  std::unordered_map<size_t, vktexobj_ptr_t> _textures_by_binding;
  int _pipeline_bits_prg       = -1;
  int _pipeline_bits_composite = -1;

  std::unordered_map<std::string, vkfxssmpset_ptr_t> _vk_samplersets;
  std::unordered_map<std::string, vkfxsuniset_ptr_t> _vk_uniformsets;
  std::unordered_map<std::string, vkfxsuniblk_ptr_t> _vk_uniformblks;
  VkFxShaderFile* _shader_file = nullptr;
};

struct VulkanDescriptorSet {
  VkDescriptorSet _vkdescset;
};
using vkdescriptorset_ptr_t = std::shared_ptr<VulkanDescriptorSet>;

struct VulkanDescriptorSetCache {

  VulkanDescriptorSetCache(vkcontext_rawptr_t ctx);

  vkdescriptorset_ptr_t fetchDescriptorSetForProgram(vkfxsprg_ptr_t program);

  std::unordered_map<uint64_t, vkdescriptorset_ptr_t> _vkDescriptorSetByHash;
  vkcontext_rawptr_t _ctxVK;
};
using vkdescriptorsetcache_ptr_t = std::shared_ptr<VulkanDescriptorSetCache>;

struct VkPipelineObject {

  VkPipelineObject(vkcontext_rawptr_t ctx);

  void applyPendingPushConstants(vkpricmdbufimpl_ptr_t cmdbuf);

  vkfxsprg_ptr_t _vk_program;
  VkGraphicsPipelineCreateInfo _VKGFXPCI;
  VkPipeline _pipeline;
  VkPipelineLayout _pipelineLayout;
  vkdescriptorsetcache_ptr_t _descriptorSetCache;

  vkviewporttracker_ptr_t _viewport;
  vkviewporttracker_ptr_t _scissor;
};

struct VkFxShaderPass {
  vkfxsprg_ptr_t _vk_program;
};
struct VkFxShaderTechnique {
  VkFxShaderTechnique();
  ~VkFxShaderTechnique();
  std::vector<vkfxspass_ptr_t> _vk_passes;
  std::shared_ptr<FxShaderTechnique> _orktechnique;
};

///////////////////////////////////////////////////////////////////////////////

struct VkVertexInputConfiguration {
  VkVertexInputBindingDescription _binding_description;
  std::vector<VkVertexInputAttributeDescription> _attribute_descriptions;
  VkPipelineVertexInputStateCreateInfo _vertex_input_state;
  int _pipeline_bits = -1;
};

struct VkPrimitiveClass {
  VkPipelineInputAssemblyStateCreateInfo _input_assembly_state;
  PrimitiveType _primtype;
  int _pipeline_bits = -1;
};

struct VulkanVertexBuffer {
  VulkanVertexBuffer(vkcontext_rawptr_t ctx, VertexBufferBase& vbuf);
  ~VulkanVertexBuffer();
  vkbuffer_ptr_t _vkbuffer = VK_NULL_HANDLE;
  vkcontext_rawptr_t _ctx  = nullptr;
  VertexBufferBase& _ork_vtxbuf;
  std::unordered_map<uint64_t, vkvertexinputconfig_ptr_t> _vif_to_layout;

  // vkvertexinputconfig_ptr_t _vertexConfig;
};
struct VulkanIndexBuffer {
  VulkanIndexBuffer(vkcontext_rawptr_t ctx, size_t length);
  ~VulkanIndexBuffer();
  vkbuffer_ptr_t _vkbuffer = VK_NULL_HANDLE;
  vkcontext_rawptr_t _ctx  = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct VkLoadContext {
  VkContext* _vkcontext     = nullptr;
  GLFWwindow* _pushedWindow = nullptr;
};

struct VkSwapChain {

  VkSwapChain();

  rtgroup_ptr_t currentRTG();

  void acquireImage(vkcontext_rawptr_t ctxVK);
  void enqueueFrame(vkcontext_rawptr_t ctxVK);
  void enqueuePresentFrame(vkcontext_rawptr_t ctxVK);
  void waitPresentFrame(vkcontext_rawptr_t ctxVK);
  size_t subIndex() const; // Which frame-in-flight we're on (0 or 1 if MAX=2)

  void _submitFrameWithSemaphores(vkcontext_rawptr_t ctxVK);

  VkSwapchainKHR _vkSwapChain;
  std::vector<rtgroup_ptr_t> _rtgs;
  static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;               // CPU can be ahead by 2 frames
  std::vector<vkbinarysemaphore_ptr_t> _imageAcquiredSemaphores;  // One per frame-in-flight
  std::vector<vkbinarysemaphore_ptr_t> _renderCompleteSemaphores; // One per frame-in-flight
  std::vector<vkfence_obj_ptr_t> _frameFences;                    // One per frame-in-flight
  std::vector<VkSemaphore> _semasOkToRender;
  std::vector<VkSemaphore> _semasOkToPresent;
  std::vector<VkPipelineStageFlags> _waitOnPipelineStages;


  std::vector<VkSemaphore> _allSignalSemaphores;
  std::vector<VkSemaphore> _allWaitSemaphores;
  std::vector<uint64_t> _allSignalValues;
  std::vector<uint64_t> _allWaitValues;
  std::vector<VkPipelineStageFlags> _allWaitStages;

  size_t _currentFrame = 0; // Which frame-in-flight we're on (0 or 1 if MAX=2)

  uint32_t _curSwapWriteImage = 0xffffffff;
};

void _vkCreateImageForBuffer(
    vkcontext_rawptr_t ctxVK, //
    vkrtbufimpl_ptr_t bufferimpl,
    EBufferFormat ork_fmt,
    uint64_t usage);

///////////////////////////////////////////////////////////////////////////////

struct VkDrawingInterface final : public DrawingInterface {
  VkDrawingInterface(vkcontext_rawptr_t ctx);
  vkcontext_rawptr_t _contextVK;
};

///////////////////////////////////////////////////////////////////////////////

struct VkImiInterface final : public ImmInterface {
  VkImiInterface(vkcontext_rawptr_t ctx);
  void _doBeginFrame() final;
  void _doEndFrame() final;
  vkcontext_rawptr_t _contextVK;
};

///////////////////////////////////////////////////////////////////////////////

struct VkMatrixStackInterface final : public MatrixStackInterface {

  VkMatrixStackInterface(vkcontext_rawptr_t ctx);

  fmtx4 Ortho(float left, float right, float top, float bottom, float fnear, float ffar); // virtual
  fmtx4 Frustum(float left, float right, float top, float bottom, float zn, float zf);    // virtual

  vkcontext_rawptr_t _contextVK;
};

///////////////////////////////////////////////////////////////////////////////

struct VertexStreamConfigItem {
  std::string _vbuf_datatype;
  std::string _semantic;
  size_t _datasize   = 0;
  size_t _dataoffset = 0;
  VkFormat _vkformat = VK_FORMAT_UNDEFINED;
};

using vertex_strconfig_item_ptr_t = std::shared_ptr<VertexStreamConfigItem>;

struct VertexStreamConfig {

  void addItem(std::string sem, std::string vb_dt, size_t ds, size_t offset, VkFormat fmt);
  std::unordered_map<std::string, vertex_strconfig_item_ptr_t> _item_by_semantic;
  size_t _stride = 0;
};

using vertex_strconfig_ptr_t = std::shared_ptr<VertexStreamConfig>;

///////////////////////////////////////////////////////////////////////////////

struct VkGeometryBufferInterface final : public GeometryBufferInterface {

  VkGeometryBufferInterface(vkcontext_rawptr_t ctx);

  void _doBeginFrame() final;

  ///////////////////////////////////////////////////////////////////////
  // VtxBuf Interface
  ///////////////////////////////////////////////////////////////////////

  void* LockVB(VertexBufferBase& VBuf, int ivbase, int icount) final;
  void UnLockVB(VertexBufferBase& VBuf) final;

  const void* LockVB(const VertexBufferBase& VBuf, int ivbase = 0, int icount = 0) final;
  void UnLockVB(const VertexBufferBase& VBuf) final;

  void ReleaseVB(VertexBufferBase& VBuf) final;

  //

  void* LockIB(IndexBufferBase& VBuf, int ivbase, int icount) final;
  void UnLockIB(IndexBufferBase& VBuf) final;

  const void* LockIB(const IndexBufferBase& VBuf, int ibase = 0, int icount = 0) final;
  void UnLockIB(const IndexBufferBase& VBuf) final;

  void ReleaseIB(IndexBufferBase& VBuf) final;

  //

  vertex_strconfig_ptr_t _instantiateVertexStreamConfig(EVtxStreamFormat format);

  vkvertexinputconfig_ptr_t vertexInputState(vkvtxbuf_ptr_t vbuf, vkvertexinterface_ptr_t vif);

  void DrawPrimitiveEML(
      const VertexBufferBase& VBuf, //
      PrimitiveType eType,
      int ivbase,
      int ivcount) final;

  void DrawPrimitiveEML(
      const FxShaderStorageBuffer* SSBO, //
      PrimitiveType eType,
      int ivbase  = 0,
      int ivcount = 0) final;

  void DrawIndexedPrimitiveEML(const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, PrimitiveType eType) final;

  void DrawInstancedIndexedPrimitiveEML(
      const VertexBufferBase& VBuf,
      const IndexBufferBase& IdxBuf,
      PrimitiveType eType,
      size_t instance_count) final;

  //////////////////////////////////////////////
  // nvidia mesh shaders
  //////////////////////////////////////////////

#if defined(ENABLE_NVMESH_SHADERS)
  void DrawMeshTasksNV(uint32_t first, uint32_t count) final;
  void DrawMeshTasksIndirectNV(int32_t* indirect) final;
  void MultiDrawMeshTasksIndirectNV(int32_t* indirect, uint32_t drawcount, uint32_t stride) final;
  void MultiDrawMeshTasksIndirectCountNV(int32_t* indirect, int32_t* drawcount, uint32_t maxdrawcount, uint32_t stride) final;
#endif

  //////////////////////////////////////////////

  vkcontext_rawptr_t _contextVK;
  uint32_t _lastComponentMask = 0xFFFFFFFF;
  std::unordered_map<uint64_t, vkprimclass_ptr_t> _primclasses;
  std::unordered_map<EVtxStreamFormat, vertex_strconfig_ptr_t> _vertexStreamConfigs;
};

///////////////////////////////////////////////////////////////////////////////

struct VkFrameBufferInterface final : public FrameBufferInterface {

  VkFrameBufferInterface(vkcontext_rawptr_t ctx);
  ~VkFrameBufferInterface();

  ///////////////////////////////////////////////////////

  void capture(const RtBuffer* inpbuf, const file::Path& pth) final;
  bool captureToTexture(const CaptureBuffer& capbuf, Texture& tex) final;
  bool captureAsFormat(const RtBuffer* inpbuf, CaptureBuffer* buffer, EBufferFormat destfmt) final;
  void GetPixel(const fvec4& rAt, PixelFetchContext& ctx) final;

  ///////////////////////////////////////////////////////

  void rtGroupClear(rtgroup_rawptr_t rtg) final;
  void rtGroupMipGen(rtgroup_rawptr_t rtg) final;
  void msaaBlit(rtgroup_ptr_t src, rtgroup_ptr_t dst) final;
  void blit(rtgroup_ptr_t src, rtgroup_ptr_t dst) final;
  void downsample2x2(rtgroup_ptr_t src, rtgroup_ptr_t dst) final;

  //////////////////////////////////////////////

  void _initializeContext(DisplayBuffer* pBuf);
  void _setViewport(int iX, int iY, int iW, int iH) final;
  void _setScissor(int iX, int iY, int iW, int iH) final;
  void _doBeginFrame(void) final;
  void _doEndFrame(void) final;
  void _pushRtGroup(rtgroup_rawptr_t Base) final;
  void _popRtGroup(bool continue_render) final;

  //////////////////////////////////////////////

  freestyle_mtl_ptr_t utilshader();
  vkrtgrpimpl_ptr_t _createRtGroupImpl(rtgroup_rawptr_t rtg);

  //////////////////////////////////////////////

  freestyle_mtl_ptr_t _freestyle_mtl;
  const FxShaderTechnique* _tek_downsample2x2 = nullptr;
  const FxShaderTechnique* _tek_blit          = nullptr;
  const FxShaderParam* _fxpMVP                = nullptr;
  const FxShaderParam* _fxpColorMap           = nullptr;

  vkviewporttracker_ptr_t _viewportTracker;
  vkviewporttracker_ptr_t _scissorTracker;
  vkcontext_rawptr_t _contextVK;

  //////////////////////////////////////////////
  void _initSwapChain();
  void _enq_transitionMainRtgToPresent();

  //////////////////////////////////////////////

  vkswapchain_ptr_t _swapchain;
  std::unordered_set<vkswapchain_ptr_t> _old_swapchains;
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
using StagingBufferPool = ObjectPoolX<SbsPoolAdapter>;
using stagingbufferpool_ptr_t = std::shared_ptr<StagingBufferPool>;
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
using SecCmdBufPool = BoundedConcurrentObjectPoolX<SecCmdBufPoolAdapter,256>;
using sseccmdbufpool_ptr_t = std::shared_ptr<SecCmdBufPool>;
///////////////////////////////////////////////////////////////////////////////

struct VkTextureInterface final : public TextureInterface {

  VkTextureInterface(vkcontext_rawptr_t ctx);

  void TexManInit() final;

  //
  bool destroyTexture(texture_ptr_t ptex) final;
  void ApplySamplingMode(Texture* ptex) final;
  void initTextureFromData(Texture* ptex, TextureInitData tid) final;
  void generateMipMaps(Texture* ptex) final;
  Texture* createFromMipChain(MipChain* from_chain) final;

  void _createFromLoadReq(texloadreq_ptr_t tlr) final;
  void _initTextureFromRtBuffer(RtBuffer* rtb);

  vkcontext_rawptr_t _contextVK;

  stagingbufferpool_ptr_t stagingBufferPoolForSrcOfSize(size_t size);
  std::unordered_map<size_t, stagingbufferpool_ptr_t> _stagingSrcBuffers;

  sseccmdbufpool_ptr_t _seccmdbufpool_xfer;
};

///////////////////////////////////////////////////////////////////////////////

struct VkFxInterface final : public FxInterface {

  VkFxInterface(vkcontext_rawptr_t ctx);
  ~VkFxInterface();

  void _doBeginFrame() final;
  void _doEndFrame() final;

  int BeginBlock(fxtechnique_constptr_t tek, const RenderContextInstData& data) final;
  void EndBlock() final;
  void CommitParams(void) final;
  void reset() final;

  fxtechnique_constptr_t technique(FxShader* hfx, const std::string& name) final;
  fxparam_constptr_t parameter(FxShader* hfx, const std::string& name) final;
  fxuniformblock_constptr_t uniformBlock(FxShader* hfx, const std::string& name) final;
  fxsamplerset_constptr_t samplerSet(FxShader* hfx, const std::string& name) final;

  const FxComputeShader* computeShader(FxShader* hfx, const std::string& name) final;
  const FxShaderStorageBlock* storageBlock(FxShader* hfx, const std::string& name) final;

  void bindParamBool(const FxShaderParam* hpar, const bool bval) final;
  void bindParamInt(const FxShaderParam* hpar, const int ival) final;
  void bindParamVect2(const FxShaderParam* hpar, const fvec2& Vec) final;
  void bindParamVect3(const FxShaderParam* hpar, const fvec3& Vec) final;
  void bindParamVect4(const FxShaderParam* hpar, const fvec4& Vec) final;
  void bindParamVect2Array(const FxShaderParam* hpar, const fvec2* Vec, const int icount) final;
  void bindParamVect3Array(const FxShaderParam* hpar, const fvec3* Vec, const int icount) final;
  void bindParamVect4Array(const FxShaderParam* hpar, const fvec4* Vec, const int icount) final;
  void bindParamFloatArray(const FxShaderParam* hpar, const float* pfA, const int icnt) final;
  void bindParamFloat(const FxShaderParam* hpar, float fA) final;
  void bindParamMatrix(const FxShaderParam* hpar, const fmtx4& Mat) final;
  void bindParamMatrix(const FxShaderParam* hpar, const fmtx3& Mat) final;
  void bindParamMatrixArray(const FxShaderParam* hpar, const fmtx4* MatArray, int iCount) final;
  void bindParamU32(const FxShaderParam* hpar, uint32_t uval) final;
  void bindParamTexture(const FxShaderParam* hpar, const Texture* pTex) final;
  void bindParamTextureArray(const FxShaderParam* hpar, const TextureArray* tex_array) final;
  void bindParamU64(const FxShaderParam* hpar, uint64_t uval) final;

  bool LoadFxShader(const AssetPath& pth, FxShader* ptex) final;
  FxShader* shaderFromShaderText(const std::string& name, const std::string& shadertext) final;

  //////////////////////////////////////////
  // new descriptorset api
  //////////////////////////////////////////

  size_t numDescriptorSetBindPoints(fxtechnique_constptr_t tek);

  fxdescriptorsetbindpoint_constptr_t descriptorSetBindPoint(fxtechnique_constptr_t tek, int slot_index);

  void bindDescriptorSet(fxdescriptorsetbindpoint_constptr_t bindingpoint, fxdescriptorset_constptr_t the_set);

  //////////////////////////////////////////

  datablock_ptr_t _writeIntermediateToDataBlock(shadlang::SHAST::transunit_ptr_t tunit);
  vkfxsfile_ptr_t _readFromDataBlock(datablock_ptr_t inpdata, FxShader* shader);
  vkfxsfile_ptr_t _loadShaderFromShaderText(
      FxShader* shader,               //
      const std::string& parser_name, //
      const std::string& shadertext);

  vkpipeline_obj_ptr_t _fetchPipeline(vkvtxbuf_ptr_t vb, vkprimclass_ptr_t primclas);

  // ubo
  FxUniformBuffer* createUniformBuffer(size_t length) final;
  fxuniformbuffermapping_ptr_t mapUniformBuffer(FxUniformBuffer* b, size_t base, size_t length) final;
  void unmapUniformBuffer(FxUniformBufferMapping* mapping) final;
  void bindUniformBuffer(const FxUniformBlock* block, FxUniformBuffer* buffer) final;

  void _doPushRasterState(rasterstate_ptr_t rs) final;
  rasterstate_ptr_t _doPopRasterState() final;

  void _bindPipeline(vkpipeline_obj_ptr_t pipe);
  void _bindGfxDescriptorSetOnSlot(vkdescriptorset_ptr_t desc_set, size_t slot);
  void _bindVertexBufferOnSlot(vkvtxbuf_ptr_t vb, size_t slot);

  void _flushRenderPassScopedState();
  int _pipelineBitsForShader(vkfxsprg_ptr_t shprog);

  fxtechnique_constptr_t _currentORKTEK = nullptr;
  VkFxShaderTechnique* _currentVKTEK;
  vkfxspass_ptr_t _currentVKPASS;
  vkcontext_rawptr_t _contextVK;
  std::map<AssetPath, vkfxsfile_ptr_t> _fxshaderfiles;
  std::unordered_map<uint64_t, vkpipeline_obj_ptr_t> _pipelines;
  shadlang::slpcache_ptr_t _slp_cache;
  std::stack<rasterstate_ptr_t> _rasterstate_stack;
  rasterstate_ptr_t _current_rasterstate;
  lev2::rasterstate_ptr_t _default_rasterstate;
  vkpipeline_obj_ptr_t _currentPipeline;
  std::unordered_map<uint64_t, int> _vk_vtxinterface_cache;
  std::unordered_map<uint64_t, int> _vk_geointerface_cache;
  std::array<vkdescriptorset_ptr_t, 4> _active_gfx_descriptorSets;
  std::array<vkvtxbuf_ptr_t, 4> _active_vbs;
};

///////////////////////////////////////////////////////////////////////////////

struct VkComputeInterface : public ComputeInterface {

  VkComputeInterface(vkcontext_rawptr_t ctx);

  void dispatchCompute(const FxComputeShader* shader, uint32_t numgroups_x, uint32_t numgroups_y, uint32_t numgroups_z) final;

  void dispatchComputeIndirect(const FxComputeShader* shader, int32_t* indirect) final;

#if defined(ENABLE_SSBO)

  void copyBufferIntoStorageBuffer(FxShaderStorageBuffer* ssbo, std::vector<uint8_t>, size_t dest_offset) final;
  FxShaderStorageBuffer* createStorageBuffer(size_t length) final;
  storagebuffermappingptr_t mapStorageBuffer(FxShaderStorageBuffer* b, size_t base = 0, size_t length = 0) final;
  void unmapStorageBuffer(FxShaderStorageBufferMapping* mapping) final;
  void bindStorageBuffer(const FxComputeShader* shader, uint32_t binding_index, FxShaderStorageBuffer* buffer) final;

#if defined(ENABLE_PYTORCH)
  FxShaderStorageBuffer* storageBufferFromTensor(torchtensor_ptr_t tensor) final;
  void copyTensorIntoStorageBuffer(FxShaderStorageBuffer* ssbo, torchtensor_ptr_t tensor, size_t dest_offset) final;
#endif

#endif

  void bindImage(const FxComputeShader* shader, uint32_t binding_index, Texture* tex, ImageBindAccess access) final;

  // PipelineCompute* createComputePipe(ComputeShader* csh);
  // void bindComputeShader(ComputeShader* csh);

  // PipelineCompute* _currentComputePipeline = nullptr;
  vkcontext_rawptr_t _contextVK;
  vkfxi_ptr_t _fxi;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

struct VkContext : public Context {

  DeclareConcreteX(VkContext, Context);

  VkContext();

public:
  // static vkcontext_ptr_t makeShared();
  static bool HaveExtension(const std::string& extname);
  static const CClass* gpClass;
  // static orkvector<std::string> gVKExtensions;
  // static orkset<std::string> gVKExtensionSet;

  ///////////////////////////////////////////////////////////////////////

  ~VkContext();

  void FxInit();

  ///////////////////////////////////////////////////////////////////////

  void _doResizeMainSurface(int iw, int ih) final;
  void _doPreBeginFrame() final;
  void _doBeginFrame() final;
  void _doEndFrame() final;
  ctx_platform_handle_t _doClonePlatformHandle() const final;

  //////////////////////////////////////////////

  secondary_commandbuffer_ptr_t _beginRecordCommandBuffer(std::string name, rtgroup_rawptr_t rtg) final;
  void _endRecordCommandBuffer(secondary_commandbuffer_ptr_t cmdbuf) final;

  void _beginRecordCommandBuffer(secondary_commandbuffer_ptr_t cbuf);

  //////////////////////////////////////////////
  // Interfaces

  FxInterface* FXI() final;
  ImmInterface* IMI() final;
  MatrixStackInterface* MTXI() final;
  GeometryBufferInterface* GBI() final;
  FrameBufferInterface* FBI() final;
  TextureInterface* TXI() final;
  ComputeInterface* CI() final;
  DrawingInterface* DWI() final;

  ///////////////////////////////////////////////////////////////////////

  void makeCurrentContext(void) final;

  void initializeWindowContext(Window* pWin, CTXBASE* pctxbase) final; // make a window
  void initializeOffscreenContext(DisplayBuffer* pBuf) final;          // make a pbuffer
  void initializeLoaderContext() final;

  void debugPushGroup(const std::string str, const fvec4& color) final;
  void debugPopGroup() final;
  void debugPushGroup(secondary_commandbuffer_ptr_t cb, const std::string str, const fvec4& color) final;
  void debugPopGroup(secondary_commandbuffer_ptr_t cb) final;

  void debugMarker(const std::string str, const fvec4& color) final;

  void TakeThreadOwnership() final;
  bool SetDisplayMode(DisplayMode* mode) final;
  load_token_t _doBeginLoad() final;
  void _doEndLoad(load_token_t ploadtok) final; // virtual

  //////////////////////////////////////////////
  void _doEnqueueSecondaryCommandBuffer(secondary_commandbuffer_ptr_t cmdbuf) final;

  //////////////////////////////////////////////

  vkswapchaincaps_ptr_t _swapChainCapsForSurface(VkSurfaceKHR surface);

  uint32_t _findMemoryType( //
      uint32_t typeFilter,  //
      VkMemoryPropertyFlags properties);

  //////////////////////////////////////////////
  void _initVulkanForDevInfo(vkdeviceinfo_ptr_t devinfo);
  void _initVulkanForWindow(VkSurfaceKHR surface);
  void _initVulkanForOffscreen(DisplayBuffer* pBuf);
  void _initVulkanCommon();
  //////////////////////////////////////////////
  template <typename T> void _setObjectDebugName(T& object, VkObjectType objectType, const char* name) {
    if (_vkSetDebugUtilsObjectName) {
      VkDebugUtilsObjectNameInfoEXT nameInfo = {};
      initializeVkStruct(nameInfo, VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT);
      nameInfo.objectType   = objectType;
      nameInfo.objectHandle = reinterpret_cast<uint64_t>(object);
      nameInfo.pObjectName  = name;
      _vkSetDebugUtilsObjectName(_vkdevice, &nameInfo);
    }
  }
  //////////////////////////////////////////////
  template <typename T> bool _fetchDeviceProcAddr(T& object, const char* name) {
    object = reinterpret_cast<T>(vkGetDeviceProcAddr(_vkdevice, name));
    return (object != nullptr);
  }
  //////////////////////////////////////////////
  VkDevice _vkdevice;
  VkPhysicalDevice _vkphysicaldevice;
  vkdeviceinfo_ptr_t _vkdeviceinfo;
  VkSurfaceKHR _vkpresentationsurface;
  vkswapchaincaps_ptr_t _vkpresentation_caps;
  std::vector<const char*> _device_extensions;
  size_t _num_queue_types = 0;

  //////////////////////////////////////////////

  std::vector<float> _queuePriorities;
  std::vector<VkDeviceQueueCreateInfo> _DQCIs;
  static constexpr uint32_t NO_QUEUE = 0xffffffff;
  uint32_t _vkqfid_graphics          = NO_QUEUE;
  uint32_t _vkqfid_compute           = NO_QUEUE;
  uint32_t _vkqfid_transfer          = NO_QUEUE;
  VkQueue _vkqueue_graphics;
  VkCommandPool _vkcmdpool_graphics;

  primary_commandbuffer_ptr_t _defaultCommandBuffer;
  vkpricmdbufimpl_ptr_t _defaultCommandBufferImpl;
  vkpricmdbufimpl_ptr_t _cmdbufcurpri_gfx;
  vkpricmdbufimpl_ptr_t primary_cb();

  vksampler_obj_ptr_t _sampler_base;
  std::vector<vksampler_obj_ptr_t> _sampler_per_maxlod;
  VkDescriptorPool _vkDescriptorPool;
  //////////////////////////////////////////////
  PFN_vkSetDebugUtilsObjectNameEXT _vkSetDebugUtilsObjectName = nullptr;
  PFN_vkCmdDebugMarkerBeginEXT _vkCmdDebugMarkerBeginEXT      = nullptr;
  PFN_vkCmdDebugMarkerEndEXT _vkCmdDebugMarkerEndEXT          = nullptr;
  PFN_vkCmdDebugMarkerInsertEXT _vkCmdDebugMarkerInsertEXT    = nullptr;
  PFN_vkCmdBeginRendering _vkCmdBeginRenderingKHR             = nullptr;
  PFN_vkCmdEndRendering _vkCmdEndRenderingKHR                 = nullptr;
  //////////////////////////////////////////////
  void* mhHWND;
  vkcontext_ptr_t _parentTarget;
  std::stack<void*> mDCStack;
  std::stack<void*> mGLRCStack;
  EDepthTest meCurDepthTest;
  bool mTargetDrawableSizeDirty;
  bool _first_frame = true;
  shared_pool::fixed_pool<PrimaryCommandBuffer, 4> _pri_cmdbuf_pool;

  //////////////////////////////////////////////
  vkpricmdbufimpl_ptr_t _createPrimaryVkCommandBuffer(PrimaryCommandBuffer* par);
  vkseccmdbufimpl_ptr_t _createSecondaryVkCommandBuffer(SecondaryCommandBuffer* par);
  void enqueueDeferredOneShotCommand(secondary_commandbuffer_ptr_t cmdbuf);
  std::vector<secondary_commandbuffer_ptr_t> _pendingOneShotCommands;
  std::unordered_set<vkcompletionsemaphore_ptr_t> _pendingOneShotSemas;
  void onFenceCrossed(void_lambda_t op);
  //////////////////////////////////////////////

  vkdwi_ptr_t _dwi;
  vkimi_ptr_t _imi;
  vkmsi_ptr_t _msi;
  vkfbi_ptr_t _fbi;
  vkgbi_ptr_t _gbi;
  vktxi_ptr_t _txi;
  vkfxi_ptr_t _fxi;
  vkci_ptr_t _ci;
};

///////////////////////////////////////////////////////////////////////////

void _vkReplaceImageForBuffer(
    vkcontext_rawptr_t ctxVK, //
    vkrtbufimpl_ptr_t bufferimpl,
    VkFormat new_fmt,
    VkImageView new_view,
    VkImage new_img);

///////////////////////////////////////////////////////////////////////////

struct VkPlatformObject {
  CtxGLFW* _ctxbase     = nullptr;
  bool _needsInit       = true;
  void_lambda_t _bindop = []() {};
};
using vkplatformobject_ptr_t = std::shared_ptr<VkPlatformObject>;

///////////////////////////////////////////////////////////////////////////

extern vkinstance_ptr_t _GVI;

} // namespace ork::lev2::vulkan
