#pragma once
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
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

// TODO Deprecated. Prefer using designated initializers.
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
struct VulkanFxShaderStage;
struct VkFxShaderFile;
struct VkFxShaderProgram;
struct VkFxShaderState;
struct VkFxShaderTechnique;
struct VkFxShaderSamplerSet;
struct VkFxShaderSamplerSetItem;
struct VkFxShaderUniformSet;
struct VkFxShaderUniformSetItem;
struct VkFxShaderUniformSampler;
struct VkFxShaderUniformBlock;
struct VkFxShaderUniformBlkItem;
struct VkFxShaderStorageBlock;
struct VkFxShaderStorageBlockState;
struct VkFxShaderPushConstantBlock;
struct VkPipelineState;
struct VkComputePipelineState;
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
struct VulkanDescriptorSetState;
struct VulkanDescriptorSetCacheState;
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
using vkdescriptorsetcache_state_ptr_t = std::shared_ptr<VulkanDescriptorSetCacheState>;
using vkdescriptorsetstate_ptr_t    = std::shared_ptr<VulkanDescriptorSetState>;
using vkdescriptorsetstate_rawptr_t = VulkanDescriptorSetState*;
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
using vktexobj_ptr_t           = std::shared_ptr<VulkanTextureObject>;
using vkfxsfile_ptr_t          = std::shared_ptr<VkFxShaderFile>;
using vkfxsstage_ptr_t         = std::shared_ptr<VulkanFxShaderStage>;
using vkfxsprg_ptr_t           = std::shared_ptr<VkFxShaderProgram>;
using vkfxsprg_rawptr_t        = VkFxShaderProgram*;
using vkfxsstate_rawptr_t      = VkFxShaderState*;
using vkfxstek_ptr_t           = std::shared_ptr<VkFxShaderTechnique>;
using vkfxstek_rawptr_t        = VkFxShaderTechnique*;
using vkpipelinestate_ptr_t   = std::shared_ptr<VkPipelineState>;
using vkpipelinestate_rawptr_t = VkPipelineState*;
using vkcompute_pipeline_ptr_t = std::shared_ptr<VkComputePipelineState>;
using vkprimclass_ptr_t     = std::shared_ptr<VkPrimitiveClass>;
using vkfxssmpset_ptr_t     = std::shared_ptr<VkFxShaderSamplerSet>;
using vkfxsuniset_ptr_t     = std::shared_ptr<VkFxShaderUniformSet>;
using vkfxsunisetitem_ptr_t = std::shared_ptr<VkFxShaderUniformSetItem>;
using vkfxsunisampler_ptr_t = std::shared_ptr<VkFxShaderUniformSampler>;

using vkfxsuniblk_ptr_t         = std::shared_ptr<VkFxShaderUniformBlock>;
using vkfxsuniblk_wkptr_t       = std::weak_ptr<VkFxShaderUniformBlock>;
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
// Vulkan Inline Designated Intializer Struct Creation
//   Allows nesting info struct pointers and arrays inside vk method call itself.
//
//   Removes need for all tertiary CreateInfo struct names. 'SCSCI', 'info' etc.
//   Keeps info struct in context of parent struct to easily see where it's used.
//   Uses raw structs from vulkan.h directly to stay familiar with Vulkan API.
//   Optimally compield out: https://godbolt.org/z/Wq5b73ETj
//
//   Small Exmaple:
//
//     vkWaitSemaphores(device,
//         pConst(VkSemaphoreWaitInfo{
//           VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
//           .semaphoreCount = 1,
//           .pSemaphores    = &_timeline,
//           .pValues        = &_timeline_value,
//         }),
//         UINT64_MAX));
//
//   Big Example:
//
//     vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
//       pConst(VkGraphicsPipelineCreateInfo{
//         VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
//         .stageCount = 2,
//         .pStages    = pArr<VkPipelineShaderStageCreateInfo>({
//           {
//             VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
//             .stage  = VK_SHADER_STAGE_VERTEX_BIT,
//             .module = vertShader,
//             .pName  = "main",
//           },
//           {
//             VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
//             .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
//             .module = fragShader,
//             .pName  = "main",
//           },
//         }),
//         .pVertexInputState = pConst(VkPipelineVertexInputStateCreateInfo{
//           VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
//           .vertexBindingDescriptionCount = 1,
//           .pVertexBindingDescriptions    = pArr<VkVertexInputBindingDescription>({
//             { .binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX },
//           }),
//           .vertexAttributeDescriptionCount = 3,
//           .pVertexAttributeDescriptions    = pArr<VkVertexInputAttributeDescription>({
//             { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, pos)    },
//             { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, normal) },
//             { .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,    .offset = offsetof(Vertex, uv)     },
//           }),
//         }),
//         .pInputAssemblyState = pConst(VkPipelineInputAssemblyStateCreateInfo{
//           VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
//           .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
//           .primitiveRestartEnable = VK_FALSE,
//         }),
//         .pViewportState = pConst(VkPipelineViewportStateCreateInfo{
//           VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
//           .viewportCount = 1,
//           .scissorCount  = 1,
//         }),
//         .pRasterizationState = pConst(VkPipelineRasterizationStateCreateInfo{
//           VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
//           pNext(VkPipelineRasterizationConservativeStateCreateInfoEXT{
//             VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,
//             .conservativeRasterizationMode    = VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT,
//             .extraPrimitiveOverestimationSize = 0.0f,
//           }),
//           .depthClampEnable        = VK_FALSE,
//           .rasterizerDiscardEnable = VK_FALSE,
//           .polygonMode             = VK_POLYGON_MODE_FILL,
//           .cullMode                = VK_CULL_MODE_BACK_BIT,
//           .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
//           .depthBiasEnable         = VK_FALSE,
//           .lineWidth               = 1.0f,
//         }),
//         .pMultisampleState = pConst(VkPipelineMultisampleStateCreateInfo{
//           VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
//           .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
//           .sampleShadingEnable  = VK_FALSE,
//         }),
//         .pDepthStencilState = pConst(VkPipelineDepthStencilStateCreateInfo{
//           VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
//           .depthTestEnable       = VK_TRUE,
//           .depthWriteEnable      = VK_TRUE,
//           .depthCompareOp        = VK_COMPARE_OP_LESS,
//           .depthBoundsTestEnable = VK_FALSE,
//           .stencilTestEnable     = VK_FALSE,
//         }),
//         .pColorBlendState = pConst(VkPipelineColorBlendStateCreateInfo{
//           VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
//           pNext(VkPipelineColorWriteCreateInfoEXT{
//             VK_STRUCTURE_TYPE_PIPELINE_COLOR_WRITE_CREATE_INFO_EXT,
//             .attachmentCount  = 1,
//             .pColorWriteEnables = pArr<VkBool32>({ VK_TRUE }),
//           }),
//           .logicOpEnable   = VK_FALSE,
//           .attachmentCount = 1,
//           .pAttachments    = pConst(VkPipelineColorBlendAttachmentState{
//             .blendEnable    = VK_FALSE,
//             .colorWriteMask = VK_COLOR_COMPONENT_R_BIT
//                             | VK_COLOR_COMPONENT_G_BIT
//                             | VK_COLOR_COMPONENT_B_BIT
//                             | VK_COLOR_COMPONENT_A_BIT,
//           }),
//         }),
//         .pDynamicState = pConst(VkPipelineDynamicStateCreateInfo{
//           VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
//           .dynamicStateCount = 2,
//           .pDynamicStates    = pArr<VkDynamicState>({
//             VK_DYNAMIC_STATE_VIEWPORT,
//             VK_DYNAMIC_STATE_SCISSOR,
//           }),
//         }),
//         .layout     = pipelineLayout,
//         .renderPass = renderPass,
//         .subpass    = 0,
//       }),
//       ORK_VK_ALLOC, &pipeline);
// 
// pConst and pNext are the same. Naming is solely a visual aid for being read.
// The extra attributes ensure it is only used inline within a function call. 
// Pointers to temporaries are valid in C++ within the expresion (before ;).
//
////////////////////////////////////////////////////////////////////////////////

template<typename T>
__attribute__((warn_unused_result, returns_nonnull))
constexpr const T* pConst(T&& val [[clang::lifetimebound]]) { return &val; }

template<typename T>
__attribute__((warn_unused_result, returns_nonnull))
constexpr const T* pNext(T&& val [[clang::lifetimebound]]) { return &val; }

template<typename T>
__attribute__((warn_unused_result, returns_nonnull))
constexpr const T* pArr(std::initializer_list<T> vals [[clang::lifetimebound]]) { return vals.begin(); }

////////////////////////////////////////////////////////////////////////////////
// Shorthand Inline Cmd Functions
//   Wrappers to vk called which which can inlined and optimized out.
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

///////////////////////////////////////////////////////
// VK_SET_DEBUG_NAME
//   Automatically fill in objectType on vkSetDebugUtilsObjectName call.
//   Implemented as define, not template, to properly output line and file in OrkVkAssert.
///////////////////////////////////////////////////////
#define VK_OBJECT_TYPE_OF(obj) _Generic((obj),                             \
  VkInstance:                 VK_OBJECT_TYPE_INSTANCE,                     \
  VkPhysicalDevice:           VK_OBJECT_TYPE_PHYSICAL_DEVICE,              \
  VkDevice:                   VK_OBJECT_TYPE_DEVICE,                       \
  VkQueue:                    VK_OBJECT_TYPE_QUEUE,                        \
  VkCommandPool:              VK_OBJECT_TYPE_COMMAND_POOL,                 \
  VkCommandBuffer:            VK_OBJECT_TYPE_COMMAND_BUFFER,               \
  VkFence:                    VK_OBJECT_TYPE_FENCE,                        \
  VkSemaphore:                VK_OBJECT_TYPE_SEMAPHORE,                    \
  VkEvent:                    VK_OBJECT_TYPE_EVENT,                        \
  VkDeviceMemory:             VK_OBJECT_TYPE_DEVICE_MEMORY,                \
  VkBuffer:                   VK_OBJECT_TYPE_BUFFER,                       \
  VkBufferView:               VK_OBJECT_TYPE_BUFFER_VIEW,                  \
  VkImage:                    VK_OBJECT_TYPE_IMAGE,                        \
  VkImageView:                VK_OBJECT_TYPE_IMAGE_VIEW,                   \
  VkSampler:                  VK_OBJECT_TYPE_SAMPLER,                      \
  VkShaderModule:             VK_OBJECT_TYPE_SHADER_MODULE,                \
  VkPipelineCache:            VK_OBJECT_TYPE_PIPELINE_CACHE,               \
  VkPipelineLayout:           VK_OBJECT_TYPE_PIPELINE_LAYOUT,              \
  VkPipeline:                 VK_OBJECT_TYPE_PIPELINE,                     \
  VkRenderPass:               VK_OBJECT_TYPE_RENDER_PASS,                  \
  VkFramebuffer:              VK_OBJECT_TYPE_FRAMEBUFFER,                  \
  VkDescriptorSetLayout:      VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,        \
  VkDescriptorPool:           VK_OBJECT_TYPE_DESCRIPTOR_POOL,              \
  VkDescriptorSet:            VK_OBJECT_TYPE_DESCRIPTOR_SET,               \
  VkQueryPool:                VK_OBJECT_TYPE_QUERY_POOL,                   \
  VkSwapchainKHR:             VK_OBJECT_TYPE_SWAPCHAIN_KHR,                \
  VkSurfaceKHR:               VK_OBJECT_TYPE_SURFACE_KHR,                  \
  VkAccelerationStructureKHR: VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR,  \
  default:                    VK_OBJECT_TYPE_UNKNOWN                       \
)

#define VK_SET_DEBUG_NAME(device, object, name)                             \
  OrkVkAssert(ork::lev2::vulkan::_GVI->_vkSetDebugUtilsObjectName((device), \
    pConst(VkDebugUtilsObjectNameInfoEXT{                                   \
      VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,                   \
      .objectType   = VK_OBJECT_TYPE_OF(object),                            \
      .objectHandle = reinterpret_cast<uint64_t>(object),                   \
      .pObjectName  = (name),                                               \
    })))

////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan
////////////////////////////////////////////////////////////////////////////////