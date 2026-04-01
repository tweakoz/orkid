#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
namespace ork::dds {
struct DDS_HEADER;
}
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

// User for all pAlloc parameters.
// Currentrly nullptr but provides single location to potentialy change it.
#define ORK_VK_ALLOC nullptr

////////////////////////////////////////////////////////////////////////////////
// OrkVkAssert
//   Assert that a VkResult is VK_SUCCESS, logging the error string if not.
////////////////////////////////////////////////////////////////////////////////

#define OrkVkAssert(result)                                                \
  {                                                                        \
    VkResult _vk_res = (result);                                           \
    if (_vk_res != VK_SUCCESS) [[unlikely]] {                              \
      fprintf(stderr, "VkResult error %d (%s) at %s:%d\n",                 \
              (int)_vk_res, string_VkResult(_vk_res), __FILE__, __LINE__); \
      OrkAssert(false);                                                    \
    }                                                                      \
  }

////////////////////////////////////////////////////////////////////////////////

inline VkDeviceSize vkAlignUp(
    VkDeviceSize value,       //
    VkDeviceSize alignment) { //
  return (value + alignment - 1) & ~(alignment - 1);
}

////////////////////////////////////////////////////////////////////////////////

// TODO Deprecated. Prefer using designated initializers. Or pConst and pNext.
template <typename T> void initializeVkStruct(T& s, VkStructureType s_type) {
  memset(&s, 0, sizeof(T));
  s.sType = s_type;
}

template <typename T> void initializeVkStruct(T& s) {
  memset(&s, 0, sizeof(T));
}

////////////////////////////////////////////////////////////////////////////////

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
struct VkFxShaderUniformSampler;
struct VkFxShaderUniformBlk;
struct VkFxShaderUniformBlkItem;
struct VkFxShaderStorageBlock;
struct VkFxShaderPushConstantBlock;
struct VkPipelineObject;
struct VkComputePipelineObject;
struct VkPrimitiveClass;
struct VklRtBufferImpl;
struct VkRtGroupImpl;
struct VkTexLoadReq;
struct VulkanVertexBuffer;
struct VkVertexInputConfiguration;
struct VulkanIndexBuffer;
struct VkLoadContext;
struct VkPrimaryCommandBufferImpl;
struct VkSecondaryCommandBufferImpl;
struct VkSwapChainCaps;
struct VkSwapChain;
#if defined(__linux__)
struct VkSwapChainDRM;
#endif
struct VkMsaaState;
struct VkRasterState;
struct VkBufferLayout;
struct VulkanSemaphoreBase;
struct VulkanBinarySemaphore;
struct VulkanTimelineSemaphore;
struct VulkanCompletionSemaphore;
struct AlignedRange;
struct DirtyRange;
using alignedrange_ptr_t = std::shared_ptr<AlignedRange>;
using dirtyrange_ptr_t = std::shared_ptr<DirtyRange>;

struct VulkanVertexInterface;
struct VulkanVertexInterfaceInput;
struct VulkanGeometryInterface;
struct VulkanGeometryInterfaceInput;
struct VkViewportTracker;
struct VkPlatformObject;
#if defined(__linux__)
struct VkPlatformObjectDRM;
#endif
struct VertexStreamConfigItem;
struct VertexStreamConfig;
struct VkFxShaderDescriptorSetItem;
struct VkFxShaderUniformSetsReference;
struct VkFxShaderUniformBlksReference;
struct VkFxShaderStorageBlocksReference;
struct VkFxShaderSamplerSetsReference;
//struct VkDescriptorSetBindings;
struct VulkanDescriptorSet;
struct VulkanDescriptorSetCache;
struct VkFrameBufferInterface;
struct VulkanFenceObject;
struct VulkanEventObject;
struct VulkanSamplerObject;
struct VkFramebufferOutput;
struct VkOffscreen;
struct InFlightTextureTransfer;
struct RtGroupAttachments;
struct VkRtbCreateOption;
struct VkRtgCreateOptions;
///////////////////////////////////////////////////////////////////////////////
using vksampler_obj_ptr_t = std::shared_ptr<VulkanSamplerObject>;
using inflighttextrans_ptr_t = std::shared_ptr<InFlightTextureTransfer>;
using rtgroup_attachments_ptr_t = std::shared_ptr<RtGroupAttachments>;
///////////////////////////////////////////////////////////////////////////////
using vkvertexinterfaceinput_ptr_t = std::shared_ptr<VulkanVertexInterfaceInput>;
using vkvertexinterface_ptr_t      = std::shared_ptr<VulkanVertexInterface>;
using vkgeometryinterfaceinput_ptr_t = std::shared_ptr<VulkanGeometryInterfaceInput>;
using vkgeometryinterface_ptr_t      = std::shared_ptr<VulkanGeometryInterface>;
using vkviewporttracker_ptr_t = std::shared_ptr<VkViewportTracker>;
using vkplatformobject_ptr_t = std::shared_ptr<VkPlatformObject>;
#if defined(__linux__)
using vkplatformobject_drm_ptr_t = std::shared_ptr<VkPlatformObjectDRM>;
#endif
using vertex_strconfig_item_ptr_t = std::shared_ptr<VertexStreamConfigItem>;
using vertex_strconfig_ptr_t = std::shared_ptr<VertexStreamConfig>;
using vkfence_obj_ptr_t    = std::shared_ptr<VulkanFenceObject>;
using vkevent_obj_ptr_t    = std::shared_ptr<VulkanEventObject>;
///////////////////////////////////////////////////////////////////////////////
using vkfxdescsetitem_ptr_t = std::shared_ptr<VkFxShaderDescriptorSetItem>;
using vkfxsunisetsref_ptr_t = std::shared_ptr<VkFxShaderUniformSetsReference>;
using vkfxsuniblksref_ptr_t = std::shared_ptr<VkFxShaderUniformBlksReference>;
using vkfxssmpsetsref_ptr_t = std::shared_ptr<VkFxShaderSamplerSetsReference>;
//using vkdescriptorbindings_ptr_t = std::shared_ptr<VkDescriptorSetBindings>;
using descriptor_bindings_vect_t = std::vector<VkDescriptorSetLayoutBinding>;
using vkdescriptorsetcache_ptr_t = std::shared_ptr<VulkanDescriptorSetCache>;
using vkdescriptorset_ptr_t = std::shared_ptr<VulkanDescriptorSet>;
///////////////////////////////////////////////////////////////////////////////
using barrier_ptr_t = std::shared_ptr<VkImageMemoryBarrier>;

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
using vkcompute_pipeline_ptr_t = std::shared_ptr<VkComputePipelineObject>;
using vkprimclass_ptr_t     = std::shared_ptr<VkPrimitiveClass>;
using vkfxssmpset_ptr_t     = std::shared_ptr<VkFxShaderSamplerSet>;
using vkfxsuniset_ptr_t     = std::shared_ptr<VkFxShaderUniformSet>;
using vkfxsunisetitem_ptr_t = std::shared_ptr<VkFxShaderUniformSetItem>;
using vkfxsunisampler_ptr_t = std::shared_ptr<VkFxShaderUniformSampler>;

using vkfxsuniblk_ptr_t         = std::shared_ptr<VkFxShaderUniformBlk>;
using vkfxsuniblk_wkptr_t       = std::weak_ptr<VkFxShaderUniformBlk>;
using vkfxsuniblkitem_ptr_t     = std::shared_ptr<VkFxShaderUniformBlkItem>;
using vkfxssbo_ptr_t             = std::shared_ptr<VkFxShaderStorageBlock>;
using vkfxssbo_wkptr_t          = std::weak_ptr<VkFxShaderStorageBlock>;
using vkfxpushconstantblk_ptr_t = std::shared_ptr<VkFxShaderPushConstantBlock>;
using vkbufferlayout_ptr_t      = std::shared_ptr<VkBufferLayout>;

using vkrtbufimpl_ptr_t         = std::shared_ptr<VklRtBufferImpl>;
using vkrtgrpimpl_ptr_t         = std::shared_ptr<VkRtGroupImpl>;
using vkrtbufimpl_wkptr_t       = std::weak_ptr<VklRtBufferImpl>;
using vkrtgrpimpl_wkptr_t       = std::weak_ptr<VkRtGroupImpl>;
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
#if defined(__linux__)
using vkswapchaindrm_ptr_t      = std::shared_ptr<VkSwapChainDRM>;
using vkswapchaindrm_rawptr_t   = VkSwapChainDRM*;
#endif
using vkmsaastate_ptr_t         = std::shared_ptr<VkMsaaState>;
using vkrasterstate_ptr_t       = std::shared_ptr<VkRasterState>;
using vkfboutput_ptr_t          = std::shared_ptr<VkFramebufferOutput>;

using smpset_map_t      = std::map<std::string, vkfxssmpset_ptr_t>;
using uniset_map_t      = std::map<std::string, vkfxsuniset_ptr_t>;
using uniblk_map_t      = std::map<std::string, vkfxsuniblk_ptr_t>;
using ssbo_map_t        = std::map<std::string, vkfxssbo_ptr_t>;
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
///////////////////////////////////////////////////////////////////////////////
vkimagecreateinfo_ptr_t makeVKICI(
    int w,
    int h,
    int d, //
    EBufferFormat fmt,
    int nummips);

vksamplercreateinfo_ptr_t makeVKSCI();

uint64_t hashImageCreationParams(
    int w,                             //
    int h,                             //
    int d,                             //
    EBufferFormat fmt,                 //
    int nummips,
    uint64_t usage );

void _vkReplaceImageForBuffer(
    vkcontext_rawptr_t ctxVK, //
    vkrtbufimpl_ptr_t bufferimpl,
    VkFormat new_fmt,
    VkImageView new_view,
    VkImage new_img);

void _vkCreateImageForBuffer(
    vkcontext_rawptr_t ctxVK, //
    vkrtbufimpl_ptr_t bufferimpl,
    EBufferFormat ork_fmt,
    uint64_t usage);
void _vkCreateImageForBuffer(
    vkcontext_rawptr_t ctxVK, //
    vkrtbufimpl_ptr_t bufferimpl,
    VkRtbCreateOption options);

// TODO lets not wrap vulkan struct initializes in a shared_ptr. Replace with one of the patterns below.
barrier_ptr_t createImageBarrier(
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkAccessFlagBits srcAccessMask,
    VkAccessFlagBits dstAccessMask);
vkimagecreateinfo_ptr_t makeVKICI(
    int w,
    int h,
    int d, //
    VkFormat fmt,
    int nummips);

////////////////////////////////////////////////////////////////////////////////
// Default Initialized Plain Value Structs
////////////////////////////////////////////////////////////////////////////////

// TODO refactor to using VkColorSubresourceRange style instead to still enable named designated intiializers
inline constexpr VkImageSubresourceRange vkColorSubresource(
    uint32_t baseMip = 0, uint32_t mipCount = 1,
    uint32_t baseLayer = 0, uint32_t layerCount = 1) {
  return {VK_IMAGE_ASPECT_COLOR_BIT, baseMip, mipCount, baseLayer, layerCount};
}

struct VkColorSubresourceRange {
  VkImageAspectFlags aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  uint32_t           baseMipLevel   = 0;
  uint32_t           levelCount     = 1;
  uint32_t           baseArrayLayer = 0;
  uint32_t           layerCount     = 1;
  constexpr operator VkImageSubresourceRange() const {
    return {aspectMask, baseMipLevel, levelCount, baseArrayLayer, layerCount};
  }
};

struct VkColorSubresourceLayers {
  VkImageAspectFlags aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  uint32_t           mipLevel       = 0;
  uint32_t           baseArrayLayer = 0;
  uint32_t           layerCount     = 1;
  constexpr operator VkImageSubresourceLayers() const {
    return {aspectMask, mipLevel, baseArrayLayer, layerCount};
  }
};

////////////////////////////////////////////////////////////////////////////////
// Vk Inline Initialization Pointers
//   Always calling Vulkan functions with all temporary variables.
//
//   vkQueueSubmit(ctxVK->_vkqueue_graphics, 1, 
//     pConst(VkSubmitInfo{
//       VK_STRUCTURE_TYPE_SUBMIT_INFO,
//       pNext(VkTimelineSemaphoreSubmitInfo{
//         VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
//         .signalSemaphoreValueCount = (uint32_t)ctxVK->_oneShotSignalValues.size(),
//         .pSignalSemaphoreValues    = ctxVK->_oneShotSignalValues.data(),
//       }),
//       .commandBufferCount   = 1,
//       .pCommandBuffers      = &ctxVK->_cmdbufcurpri_gfx->_vkcmdbuf,
//       .signalSemaphoreCount = (uint32_t)ctxVK->_oneShotSignalSemaphores.size(),
//       .pSignalSemaphores    = ctxVK->_oneShotSignalSemaphores.data(),
//     }), 
//     fence->_vkfence);
//
// Optimal for compiler optimization:
//   https://godbolt.org/z/Wq5b73ETj
//
////////////////////////////////////////////////////////////////////////////////

// pConst and pNext are the same. Naming is solely a visual aid when being used.
// The extra attributes ensure it is only used inline within a function call.
template<typename T>
__attribute__((warn_unused_result, returns_nonnull))
const T* pConst(T&& val [[clang::lifetimebound]]) { return &val; }

template<typename T>
__attribute__((warn_unused_result, returns_nonnull))
const T* pNext(T&& val [[clang::lifetimebound]]) { return &val; }


////////////////////////////////////////////////////////////////////////////////
// Shorthand Inline Cmd Functions
//   Wrappers that do not contain logic themselves.
//   Only shorthand for functions in vulkan header.
////////////////////////////////////////////////////////////////////////////////

inline void vkCmdImageBarrier(
    VkCommandBuffer cmd,
    VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
    std::initializer_list<VkImageMemoryBarrier>&& barriers) {
  vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, (u32)barriers.size(), barriers.begin());
}

///////////////////////////////////////////////////////
// vkCmdBlitColorImage
//   Handles the full pre/post barrier pair for a color image blit in one call.
///////////////////////////////////////////////////////
inline void vkCmdBlitColorImage(
    VkCommandBuffer cmd,
    VkImage src_img, VkPipelineStageFlags src_stage,
    VkImage dst_img, VkPipelineStageFlags dst_stage,
    VkExtent2D src_extent, VkExtent2D dst_extent,
    VkFilter filter = VK_FILTER_LINEAR) {
  vkCmdImageBarrier(cmd,
      src_stage, VK_PIPELINE_STAGE_TRANSFER_BIT, {
        { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .srcAccessMask       = VK_ACCESS_SHADER_READ_BIT,
          .dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT,
          .oldLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image               = src_img,
          .subresourceRange    = VkColorSubresourceRange{}},
        { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .srcAccessMask       = 0,
          .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
          .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
          .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image               = dst_img,
          .subresourceRange    = VkColorSubresourceRange{}},
      });
  vkCmdBlitImage(cmd,
      src_img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      dst_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      pConst(VkImageBlit{
        .srcSubresource = VkColorSubresourceLayers{},
        .srcOffsets     = {{0,0,0}, {(s32)src_extent.width,(s32)src_extent.height,1}},
        .dstSubresource = VkColorSubresourceLayers{},
        .dstOffsets     = {{0,0,0}, {(s32)dst_extent.width,(s32)dst_extent.height,1}},
      }), filter);
  vkCmdImageBarrier(cmd,
      VK_PIPELINE_STAGE_TRANSFER_BIT, dst_stage, {
        { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .srcAccessMask       = VK_ACCESS_TRANSFER_READ_BIT,
          .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
          .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image               = src_img,
          .subresourceRange    = VkColorSubresourceRange{}},
        { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
          .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
          .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image               = dst_img,
          .subresourceRange    = VkColorSubresourceRange{}},
      });
}

template <typename T> struct VkObjectTypeFor;
template <> struct VkObjectTypeFor<VkQueue>          { static constexpr VkObjectType value = VK_OBJECT_TYPE_QUEUE; };
template <> struct VkObjectTypeFor<VkImage>          { static constexpr VkObjectType value = VK_OBJECT_TYPE_IMAGE; };
template <> struct VkObjectTypeFor<VkImageView>      { static constexpr VkObjectType value = VK_OBJECT_TYPE_IMAGE_VIEW; };
template <> struct VkObjectTypeFor<VkBuffer>         { static constexpr VkObjectType value = VK_OBJECT_TYPE_BUFFER; };
template <> struct VkObjectTypeFor<VkDeviceMemory>   { static constexpr VkObjectType value = VK_OBJECT_TYPE_DEVICE_MEMORY; };
template <> struct VkObjectTypeFor<VkCommandBuffer>  { static constexpr VkObjectType value = VK_OBJECT_TYPE_COMMAND_BUFFER; };
template <> struct VkObjectTypeFor<VkSemaphore>      { static constexpr VkObjectType value = VK_OBJECT_TYPE_SEMAPHORE; };
template <> struct VkObjectTypeFor<VkFence>          { static constexpr VkObjectType value = VK_OBJECT_TYPE_FENCE; };
template <> struct VkObjectTypeFor<VkPipeline>       { static constexpr VkObjectType value = VK_OBJECT_TYPE_PIPELINE; };
template <> struct VkObjectTypeFor<VkRenderPass>     { static constexpr VkObjectType value = VK_OBJECT_TYPE_RENDER_PASS; };
template <> struct VkObjectTypeFor<VkFramebuffer>    { static constexpr VkObjectType value = VK_OBJECT_TYPE_FRAMEBUFFER; };
template <> struct VkObjectTypeFor<VkDescriptorSet>  { static constexpr VkObjectType value = VK_OBJECT_TYPE_DESCRIPTOR_SET; };
template <> struct VkObjectTypeFor<VkShaderModule>   { static constexpr VkObjectType value = VK_OBJECT_TYPE_SHADER_MODULE; };

// Implemented as define, not template, to properly output line and file in OrkVkAssert.
#define VkSetDebugName(device, object, name)                                             \
  OrkVkAssert(ork::lev2::vulkan::_GVI->_vkSetDebugUtilsObjectName((device),              \
    pConst(VkDebugUtilsObjectNameInfoEXT{                                                \
      VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,                                \
      .objectType   = VkObjectTypeFor<std::remove_reference_t<decltype(object)>>::value, \
      .objectHandle = reinterpret_cast<uint64_t>(object),                                \
      .pObjectName  = (name),                                                            \
    })))   

////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan
////////////////////////////////////////////////////////////////////////////////