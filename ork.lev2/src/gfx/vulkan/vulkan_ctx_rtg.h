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
struct VklRtBufferImpl {
  VklRtBufferImpl(VkRtGroupImpl* par, uint64_t usage, VkFormat fmt);

  void _transitionImage(vkpricmdbufimpl_ptr_t cb, const VkTransitionParams& params);
  void _transitionToRenderTarget(vkpricmdbufimpl_ptr_t cb);
  void _transitionToTexture(vkpricmdbufimpl_ptr_t cb);
  void _transitionToHostRead(vkpricmdbufimpl_ptr_t cb);
  void _transitionToPresent(vkpricmdbufimpl_ptr_t cb);

  void setLayout(VkImageLayout layout);
  void _replaceImage(VkFormat new_fmt, VkImageView new_view, VkImage new_img);

  VkRtGroupImpl* _rtg_impl = nullptr;
  uint64_t _usage = "none"_crcu;
  VkFormat _vkfmt = VK_FORMAT_UNDEFINED;
  bool _init               = true;
  bool _is_surface         = false;
  VkImage _vkimg;
  vkimageobj_ptr_t _imgobj;
  VkImageView _vkimgview;
  VkAttachmentDescription _attachmentDesc;
  VkAttachmentReference _attachmentRef;
  VkDescriptorImageInfo _descriptorInfo;
  VkImageLayout _currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  svar64_t _teximpl;
  fvec4 _clear_color;
  float _clear_depth = 1.0f;
};
///////////////////////////////////////////////////////////////////////////////
struct VkRtGroupImpl {
  VkRtGroupImpl(vkcontext_rawptr_t ctxVK);

  rtgroup_attachments_ptr_t attachments();
  vkrenderinfo_ptr_t renderinfo();
  void _updateClearParams(rtgroup_rawptr_t _rtg);

  void _transitionToRenderTarget(vkpricmdbufimpl_ptr_t cb);
  void _transitionToTexture(vkpricmdbufimpl_ptr_t cb);
  void _transitionToHostRead(vkpricmdbufimpl_ptr_t cb);
  void _updateMainSurface(VkFrameBufferInterface* fbi);

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
  vkmsaastate_ptr_t _msaaState;

  vkrenderinfo_ptr_t _rinfo_retain;
  vkpipelinerenderinfo_ptr_t _prinfo_retain;

  secondary_commandbuffer_ptr_t _cmdbufRTG;
  VkCommandBufferBeginInfo _cmdBufCBBI_GFX;
  VkCommandBufferInheritanceInfo _cmdBufII;
  std::unordered_set<vkrenderinfo_ptr_t> _renderinfo_set;
};
///////////////////////////////////////////////////////////////////////////////
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
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
