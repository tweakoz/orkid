#pragma once 
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
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
///////////////////////////////////////////////////////////////////////////////
struct VkViewportTracker {
  int _width  = 0;
  int _height = 0;
  int _x      = 0;
  int _y      = 0;
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanRenderInfo {
  VulkanRenderInfo(VkRtGroupImpl* rtg);
  ~VulkanRenderInfo();
  VkRenderingInfo _renderinfo;
  std::vector<VkRenderingAttachmentInfo> _rainfos_color;
  VkRenderingAttachmentInfo _rainfo_depth;
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanPipelineRenderInfo {
  VulkanPipelineRenderInfo(rtgroup_rawptr_t rtg);
  ~VulkanPipelineRenderInfo();

  rtgroup_ptr_t _rtg;
  VkPipelineRenderingCreateInfo _createInfo;
  std::vector<VkFormat> _colorFormats;
  VkFormat _depthFormat = VK_FORMAT_UNDEFINED;
};
///////////////////////////////////////////////////////////////////////////////
struct VkTransitionParams {
    VkImageLayout layout;
    VkAccessFlagBits srcAccess, dstAccess;
    VkPipelineStageFlags srcStage, dstStage;
};
///////////////////////////////////////////////////////////////////////////////
struct RtGroupAttachments {
  std::vector<VkAttachmentDescription> _descriptions;
  std::vector<VkAttachmentReference> _references;
  std::vector<VkImageView> _imageviews;
  std::vector<VkDescriptorImageInfo> descimginfos;
};
///////////////////////////////////////////////////////////////////////////////
struct VkLoadContext {
  VkContext* _vkcontext     = nullptr;
  GLFWwindow* _pushedWindow = nullptr;
};
///////////////////////////////////////////////////////////////////////////////
struct VkPlatformObject {
  CtxGLFW* _ctxbase     = nullptr;
  bool _needsInit       = true;
  void_lambda_t _bindop = []() {};
};
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
