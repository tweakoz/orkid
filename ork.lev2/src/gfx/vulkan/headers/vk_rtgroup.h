#pragma once 
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
struct VkMsaaState {
  VkMsaaState();
  VkPipelineMultisampleStateCreateInfo _VKSTATE;
  int _pipeline_bits = -1;
};
///////////////////////////////////////////////////////////////////////////////
struct VkRtbCreateOption {
  VkFormat _format = VK_FORMAT_UNDEFINED; 
  uint64_t _usage = 0;                    
  bool _with_texture = false;                
};
///////////////////////////////////////////////////////////////////////////////
struct VkRtgCreateOptions {
  rtgroup_rawptr_t _rtgroup = nullptr;
  int _width = 0;
  int _height = 0;
  uint64_t _usage = 0;
  MsaaSamples _msaaSamples = MsaaSamples::MSAA_1X;
  std::vector<VkRtbCreateOption> _colorOptions;
  VkRtbCreateOption _depthOptions;
};
///////////////////////////////////////////////////////////////////////////////
struct VklRtBufferImpl {
  VklRtBufferImpl(vkcontext_rawptr_t ctxVK, VkRtGroupImpl* par, uint64_t usage, VkFormat fmt);
  ~VklRtBufferImpl();

  void _transitionImage(vkpricmdbufimpl_ptr_t cb, const VkTransitionParams& params);
  void _transitionMsaaImage(vkpricmdbufimpl_ptr_t cb, const VkTransitionParams& params); // _msaa_imgobj only
  void _transitionToRenderTarget(vkpricmdbufimpl_ptr_t cb);
  void _transitionToTexture(vkpricmdbufimpl_ptr_t cb);
  void _transitionToHostRead(vkpricmdbufimpl_ptr_t cb);
  void _transitionToPresent(vkpricmdbufimpl_ptr_t cb);

  void setLayout(VkImageLayout layout);
  void _replaceImage(vkimageobj_ptr_t imgobj);

  vkcontext_rawptr_t _contextVK = nullptr;
  VkRtGroupImpl* _rtg_impl = nullptr;
  uint64_t _usage = "none"_crcu;
  VkFormat _vkfmt = VK_FORMAT_UNDEFINED;
  bool _init               = true;
  bool _is_surface         = false;
  VkAttachmentDescription _attachmentDesc;
  VkAttachmentReference _attachmentRef;
  VkDescriptorImageInfo _descriptorInfo;
  VkImageLayout _currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  vkimageobj_ptr_t _imgobj;
  // MSAA: when the parent RtGroup is multisampled, _imgobj stays the SINGLE-sample resolve target
  // (everything samples it, unchanged) and _msaa_imgobj is the multisample image the render pass
  // actually renders into, resolved DOWN to _imgobj at endRendering. null when MSAA is off.
  vkimageobj_ptr_t _msaa_imgobj;
  svar64_t _teximpl;
  fvec4 _clear_color;
  float _clear_depth = 1.0f;

  // Per-face image views for cubemap rendering (6 views, one per face)
  std::array<VkImageView, 6> _cubeFaceViews = {VK_NULL_HANDLE};
  bool _hasCubeFaceViews = false;
};
///////////////////////////////////////////////////////////////////////////////
struct VkRtGroupImpl {
  VkRtGroupImpl(vkcontext_rawptr_t ctxVK, rtgroup_rawptr_t rtgroup);
  ~VkRtGroupImpl();

  rtgroup_attachments_ptr_t attachments();
  vkrenderinfo_ptr_t renderinfo();
  vkrenderinfo_ptr_t renderinfoForResume();
  void _updateClearParams(rtgroup_rawptr_t _rtg);

  void _transitionToRenderTarget(vkpricmdbufimpl_ptr_t cb);
  void _transitionToTexture(vkpricmdbufimpl_ptr_t cb);
  void _transitionToHostRead(vkpricmdbufimpl_ptr_t cb);
  void _updateMainSurface(VkFrameBufferInterface* fbi);
  void _invalidateAttachments();
  void _setupCubeFaceRendering(int face_index);

  static void assignToRtGroup(vkrtgrpimpl_ptr_t rtgimpl, rtgroup_rawptr_t rtgroup);

  rtgroup_rawptr_t _rtgroup = nullptr;
  vkrtbufimpl_ptr_t _standard;
  vkrtbufimpl_ptr_t _depthonly;
  rtgroup_attachments_ptr_t __attachments;
  vkcontext_rawptr_t _contextVK = nullptr;
  std::vector<vkrtbufimpl_ptr_t> _color_buffer_impls;
  vkrtbufimpl_ptr_t _depth_buffer_impl;
  int _width         = 0;
  int _height        = 0;
  int _pipeline_bits = -1;
  bool _autoclear = true;
  // One-shot flag set by FBI::transitionDepthForSampling().
  // When true, the next _transitionToRenderTarget() for the depth buffer
  // targets VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL instead of
  // VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, and the VulkanRenderInfo's
  // depth attachment imageLayout picks up the same value. The flag resets
  // at the end of the matching _popRtGroup.
  bool _depthReadOnlyMode = false;
  vkmsaastate_ptr_t _msaaState;

  vkrenderinfo_ptr_t _rinfo_retain;
  vkrenderinfo_ptr_t _rinfo_resume_retain;
  vkpipelinerenderinfo_ptr_t _prinfo_retain;

  secondary_commandbuffer_ptr_t _cmdbufRTG;
  VkCommandBufferBeginInfo _cmdBufCBBI_GFX;
  VkCommandBufferInheritanceInfo _cmdBufII;
  std::unordered_set<vkrenderinfo_ptr_t> _renderinfo_set;
};
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
