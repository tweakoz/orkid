#pragma once 
namespace ork::dds {
struct DDS_HEADER;
}
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
inline VkDeviceSize vkAlignUp(
    VkDeviceSize value,       //
    VkDeviceSize alignment) { //
  return (value + alignment - 1) & ~(alignment - 1);
}
///////////////////////////////////////////////////////////////////////////////
template <typename T> void initializeVkStruct(T& s, VkStructureType s_type) {
  memset(&s, 0, sizeof(T));
  s.sType = s_type;
}
///////////////////////////////////////////////////////////////////////////////
template <typename T> void initializeVkStruct(T& s) {
  memset(&s, 0, sizeof(T));
}
///////////////////////////////////////////////////////////////////////////////
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
struct VertexStreamConfigItem;
struct VertexStreamConfig;
struct VkFxShaderDescriptorSetItem;
struct VkFxShaderUniformSetsReference;
struct VkFxShaderUniformBlksReference;
struct VkFxShaderSamplerSetsReference;
//struct VkDescriptorSetBindings;
struct VulkanDescriptorSet;
struct VulkanDescriptorSetCache;
struct VkFrameBufferInterface;
struct VulkanFenceObject;
struct VulkanEventObject;
struct VulkanSamplerObject;
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
using vertex_strconfig_item_ptr_t = std::shared_ptr<VertexStreamConfigItem>;
using vertex_strconfig_ptr_t = std::shared_ptr<VertexStreamConfig>;
using vkfence_obj_ptr_t = std::shared_ptr<VulkanFenceObject>;
using vkevent_obj_ptr_t = std::shared_ptr<VulkanEventObject>;
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

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
