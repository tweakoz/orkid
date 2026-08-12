////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>
///////////////////////////////////////////////////////////////////////////////
struct GLFWwindow;
namespace ork::lev2 { class ShmTexConsumer; }

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
#include <ork/kernel/priority_stack.inl>
#include <ork/file/chunkfile.inl>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gpumicrotask.h>
#include <ork/lev2/gfx/shadlang.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/shadlang.h>
#include <ork/lev2/gfx/shadman.h>

#define GLFW_INCLUDE_VULKAN
#if defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#endif
#include <ork/lev2/glfw/ctx_glfw.h>
#include <GLFW/glfw3native.h>
#if defined(__linux__)
#include <ork/lev2/drm/drm_types.h>
#include <ork/lev2/drm/ctx_drm.h>
#endif
///////////////////////////////////////////////////////////////////////////////
#include "vk_protos.h"
#include "vk_geom.h"
#include "vk_misc.h"
#include "vk_image.h"
#include "vk_memory.h"
#include "vk_rtgroup.h"
#include "vk_synchro.h"
#include "vk_pipeline.h"
#include "vk_merged_resources.h"
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {

using vkseccmdbufarray_t = std::vector<secondary_commandbuffer_ptr_t>;
using vkcompsema_set_t = std::unordered_set<vkcompletionsemaphore_ptr_t>;
///////////////////////////////////////////////////////////////////////////////
constexpr EBufferFormat DEPTH_FORMAT = EBufferFormat::Z24S8;
// FLIP_Y_LIKE_OPENGL=false: Y-flip is owned by the rasterizer via a negative-
// height viewport plus FrontFace=CLOCKWISE mapping of the logical CCW front-
// face. The projection matrix stays in native GL/RH form (no det-negative
// flip_y), so ALL sources of projection matrices — CameraData::computeMatrices,
// CameraMatrices::setCustomProjection (VR eyes, portals, custom renders) —
// produce consistent winding without per-caller compensation.
constexpr bool FLIP_Y_LIKE_OPENGL = false;
///////////////////////////////////////////////////////////////////////////////
using StagingBufferPool = LockedObjectPoolX<SbsPoolAdapter>;
using stagingbufferpool_ptr_t = std::shared_ptr<StagingBufferPool>;
///////////////////////////////////////////////////////////////////////////////
using SecCmdBufPool = LockedObjectPoolX<SecCmdBufPoolAdapter>;
using sseccmdbufpool_ptr_t = std::shared_ptr<SecCmdBufPool>;///////////////////////////////////////////////////////////////////////////////
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
  bool _supportsMeshShader         = false; // VK_EXT_mesh_shader ENABLED + meshShader feature chained
  uint32_t _maxMeshWkgInvocations  = 0;     // maxMeshWorkGroupInvocations; 0 until the ext is chained
  bool _supportsTaskShader         = false; // taskShader feature CHAINED (the amplification stage is legal)
  uint32_t _maxTaskWkgInvocations  = 0;     // maxTaskWorkGroupInvocations; 0 unless taskShader chained
  uint32_t _maxTaskPayloadSize     = 0;     // maxTaskPayloadSize (bytes); 0 unless taskShader chained
  bool _supportsMultiview          = false; // core VK1.1 multiview feature bit chained VK_TRUE
  uint32_t _maxMultiviewViewCount  = 0;     // maxMultiviewViewCount; 0 until multiview is chained
  bool _supportsMultiviewMeshShader = false; // mesh stage legal inside a multiview pass
  uint32_t _maxMeshMultiviewViewCount = 0;  // maxMeshMultiviewViewCount; 0 until the mesh ext is chained

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
  vkdeviceinfo_ptr_t findPresentableDevice(); // no surface needed
  std::vector<const char*> _instance_extensions;
  uint32_t _numgpus   = 0;
  uint32_t _numgroups = 0;
  shadlang::slpcache_ptr_t _slp_cache;
  MpMcBoundedQueue<load_token_t> _loadTokens;
  bool _debugEnabled = false;
  vkdeviceinfo_ptr_t _preferred;

  std::vector<VkContext*> _contexts;

  PFN_vkCreateDebugUtilsMessengerEXT _vkCreateDebugUtilsMessengerEXT = nullptr;
  PFN_vkSetDebugUtilsObjectNameEXT   _vkSetDebugUtilsObjectName = nullptr;

  //////////////////////////////////////////////
  template <typename T> bool _fetchInstanceProcAddr(T& object, const char* name) {
    object = reinterpret_cast<T>(vkGetInstanceProcAddr(_instance, name));
    return (object != nullptr);
  }

};
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

  void DrawInstancedIndexedPrimitiveEML(
      const VertexBufferBase& VBuf,
      const IndexBufferBase& IdxBuf,
      PrimitiveType eType,
      size_t instance_count,
      size_t first_instance) final;

  // GPU-driven indirect draws (count from a compute-written storage buffer) — see gbi.h.
  void DrawInstancedIndexedPrimitiveIndirectEML(
      const VertexBufferBase& VBuf,
      const IndexBufferBase& IdxBuf,
      PrimitiveType eType,
      const FxShaderStorageBuffer* indirect_args,
      size_t args_offset = 0) final;

  void DrawIndirectEML(
      PrimitiveType eType,
      const FxShaderStorageBuffer* indirect_args,
      size_t args_offset = 0) final;

  void DrawIndexedIndirectEML(
      const FxShaderStorageBuffer* index_buffer,
      PrimitiveType eType,
      const FxShaderStorageBuffer* indirect_args,
      size_t args_offset = 0,
      int index_size = 4) final;

  //////////////////////////////////////////////
  // taskless EXT mesh shaders
  //////////////////////////////////////////////

  void DrawMeshTasksEML(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) final;

  void DrawMeshTasksIndirectEML(const FxShaderStorageBuffer* indirect_args, size_t args_offset = 0) final;

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
struct VkRtgStackItemImpl {
  bool _did_begin_rendering = false;  // Whether this push actually called vkCmdBeginRenderingKHR
  bool _was_redundant = false;        // Whether this push was a no-op (same rtgroup already active)
  RtGroup* _previous_rtgroup = nullptr; // The RTGroup that was active before this push
};

////////////////////////////////////////////////////////////////////////////////
// Vulkan Framebuffer Output (owned by VkFrameBufferInterface::_output):
//   VkFramebufferOutput     — abstract base: beginFrame / endFrame / submit / currentFrameFence
//     VkOffscreen           — headless; submits with fence, no presentation
//     VkSwapChain           — GLFW/surface swapchain; acquires image, presents via KHR
//     VkSwapChainDRM        — Linux DRM direct-rendering (vk_swapchain_drm.h); exports via dmabuf
//     VkDisplayClientOutput — Output to Orkid Display Client
////////////////////////////////////////////////////////////////////////////////

struct VkFramebufferOutput {
  virtual ~VkFramebufferOutput() = default;

  // Acquire the output image and inject it into the main RTG color buffer so rendering
  // goes directly into the output surface. Called from _pushRtGroup for the main RTG.
  virtual void beginFrame(vkcontext_rawptr_t ctxVK) {}

  // Transition main RTG color buffer to its required end-of-frame layout (e.g. PRESENT_SRC_KHR).
  // Called inside the primary CB before submit.
  virtual void endFrame(vkcontext_rawptr_t ctxVK) = 0;

  // Submit the primary command buffer and present (or, for offscreen, just submit and wait).
  virtual void submit(vkcontext_rawptr_t ctxVK) = 0;

  // Returns the timing estimator owned by this output, or nullptr for non-swapchain outputs.
  virtual time_predictor_ptr_t getScanoutPredictor() const { return nullptr; }

  // True when this output's present path itself paces the render loop to a frame cadence
  // (a blocking/display-locked present). Base default false (offscreen / non-blocking).
  virtual bool providesFramePacing() const { return false; }

  // Return the fence for the current frame (before _incrementFrame advances _sub_index).
  vkfence_obj_ptr_t currentFrameFence() const { return _frame_fences[_sub_index]; }

  void _incrementFrame() {
    _current_frame++;
    _sub_index = _current_frame % MAX_FRAMES_IN_FLIGHT;
  }

  vkfence_obj_ptr_t _frame_fences[MAX_FRAMES_IN_FLIGHT] = {nullptr};

  u64      _current_frame = 0;
  u32      _sub_index     = 0;     // _current_frame % MAX_FRAMES_IN_FLIGHT, updated by _incrementFrame()
  bool     _acquired      = false; // true between beginFrame and submit
  int      _width         = 0;
  int      _height        = 0;
};

////////////////////////////////////////////////////////////////////////////////
// Offscreen / headless output path (no presentation target)
////////////////////////////////////////////////////////////////////////////////

struct VkOffscreen : public VkFramebufferOutput {
  VkOffscreen(vkcontext_rawptr_t ctxVK);
  void endFrame(vkcontext_rawptr_t ctxVK) override final;
  void submit(vkcontext_rawptr_t ctxVK) override final;
};

////////////////////////////////////////////////////////////////////////////////
// Display Client Output path: renders main scene into VkDisplayClient-owned images.
////////////////////////////////////////////////////////////////////////////////

struct VkDisplayClientLocalData {
  VkSemaphore    server_timeline;
  VkSemaphore    client_timeline;

  VkImage        images[MAX_FRAMES_IN_FLIGHT];
  VkImageView    views[MAX_FRAMES_IN_FLIGHT];
  VkDeviceMemory mems[MAX_FRAMES_IN_FLIGHT];

  VkImage        depth_images[MAX_FRAMES_IN_FLIGHT];
  VkImageView    depth_views[MAX_FRAMES_IN_FLIGHT];
  VkDeviceMemory depth_mems[MAX_FRAMES_IN_FLIGHT];
};

struct VkDisplayClient : OrkDisplayClient {
  virtual bool initialize(VkDevice device) = 0;
  
  u8   acquireImage(VkDevice device);
  void releaseImage(VkDevice device, u8 id);

  u64 _server_wait_timeline_value = 0;
  VkDisplayClientLocalData _local = {};
};

using vkdisplayclient_ptr_t = std::shared_ptr<VkDisplayClient>;

struct VkDisplayClientOutput : public VkFramebufferOutput {

  VkDisplayClientOutput(vkcontext_rawptr_t ctxVK, int width, int height, vkdisplayclient_ptr_t client);
  ~VkDisplayClientOutput();

  void beginFrame(vkcontext_rawptr_t ctxVK) override final;
  void endFrame(vkcontext_rawptr_t ctxVK)   override final;
  void submit(vkcontext_rawptr_t ctxVK)     override final;

  vkcontext_rawptr_t    _gfx_ctx        = nullptr;
  vkdisplayclient_ptr_t _display_client = nullptr;
  u8 _acquired_index = UINT8_MAX; 

  std::shared_ptr<VulkanImageObject> _imgobjs[MAX_FRAMES_IN_FLIGHT];
};

////////////////////////////////////////////////////////////////////////////////
// GLFW / Vulkan-surface swapchain output path
////////////////////////////////////////////////////////////////////////////////

struct VkSwapChainCaps {
  bool supportsPresentationMode(VkPresentModeKHR mode) const;

  VkSurfaceCapabilitiesKHR _capabilities;
  std::vector<VkSurfaceFormatKHR> _formats;
  std::set<VkPresentModeKHR> _presentModes;
};

struct VkSwapChain : public VkFramebufferOutput {

  VkSwapChain(vkcontext_rawptr_t ctxVK);
  ~VkSwapChain();

  void beginFrame(vkcontext_rawptr_t ctxVK) override final;
  void endFrame(vkcontext_rawptr_t ctxVK)   override final;
  void submit(vkcontext_rawptr_t ctxVK)     override final;

  // FIFO/FIFO_RELAXED block at vblank → they pace the loop. MAILBOX/IMMEDIATE do not.
  bool providesFramePacing() const override final {
    return (_presentMode == VK_PRESENT_MODE_FIFO_KHR) or (_presentMode == VK_PRESENT_MODE_FIFO_RELAXED_KHR);
  }

  void _reinit();
  void _buildup();
  void _teardown();
  void _enqueuePresentFrame(vkcontext_rawptr_t ctxVK);
  void _acquireImage(vkcontext_rawptr_t ctxVK);
  void _waitFrame();
  void _enqueueFrame(vkcontext_rawptr_t ctxVK);

  vkcontext_rawptr_t _contextVK       = nullptr;
  VkSwapchainKHR     _vkSwapChain     = VK_NULL_HANDLE;
  VkSemaphore        _semaOkToPresent = VK_NULL_HANDLE;

  vkbinarysemaphore_ptr_t _imageAcquiredSemaphores[MAX_FRAMES_IN_FLIGHT]  = {nullptr};
  vkbinarysemaphore_ptr_t _renderCompleteSemaphores[MAX_FRAMES_IN_FLIGHT] = {nullptr};

  std::vector<vkimageobj_ptr_t>     _swapChainImages;
  std::vector<VkSemaphore>          _allSignalSemaphores;
  std::vector<VkSemaphore>          _allWaitSemaphores;
  std::vector<uint64_t>             _allSignalValues;
  std::vector<uint64_t>             _allWaitValues;
  std::vector<VkPipelineStageFlags> _allWaitStages;

  // index of the swapchain image currently acquired for rendering; 0xffffffff = none
  u32 _curSwapWriteImage = 0xffffffff;

  // present mode selected in _buildup — drives providesFramePacing() (FIFO=vsync-paced).
  VkPresentModeKHR _presentMode = VK_PRESENT_MODE_FIFO_KHR;

  // set by resize callback; drained at beginFrame before _acquireImage
  bool _pendingReinit = false;
};

////////////////////////////////////////////////////////////////////////////////
// VkSwapchainMetal
////////////////////////////////////////////////////////////////////////////////

#if defined(__APPLE__)
struct VkSwapchainMetal : public VkFramebufferOutput {

  VkSwapchainMetal(vkcontext_rawptr_t ctxVK);
  ~VkSwapchainMetal();

  void beginFrame(vkcontext_rawptr_t ctxVK) override final;
  void endFrame(vkcontext_rawptr_t ctxVK)   override final;
  void submit(vkcontext_rawptr_t ctxVK)     override final;

  // beginFrame sleeps until the predicted scanout (race-the-beam) → display-locked pacing.
  bool providesFramePacing() const override final { return true; }

  // Called from CVDisplayLink callback — timing estimator only, no blit.
  void _onVsync(const void* outputTime); // const CVTimeStamp*

  // Blit offscreen texture to a CAMetalDrawable and present.
  void _blitThenPresent(u32 sub);

  // Check the window's current NSScreen and retarget the CVDisplayLink if it
  // has moved to a different display (e.g. HMD plugged in after startup).
  void _checkDisplay();

  void _buildup();
  void _teardown();

  vkcontext_rawptr_t _contextVK = nullptr;

  // Offscreen render targets — double-buffered by _sub_index.
  vkimageobj_ptr_t _offscreen_imgobjs[MAX_FRAMES_IN_FLIGHT];
  // Corresponding MTLTexture pointers (non-owning — MoltenVK retains via VkImage).
  void* _offscreen_mtltextures[MAX_FRAMES_IN_FLIGHT] = {nullptr};

  // Metal present infrastructure (all ObjC objects stored as void*).
  void* _metalLayer          = nullptr; // CAMetalLayer* (non-owning, NSView retains)
  void* _presentCommandQueue = nullptr; // id<MTLCommandQueue> (owned, +1 from newCommandQueue)
  void* _displayLink         = nullptr; // CVDisplayLinkRef (owned, +1 from Create)

  // Stores MTLSharedEvent. Metal equivalent of VkTimelineSemaphore.
  // Used for frame waiting and pacing.
  void* _timeline = nullptr;

  u64 _last_frame_delta_ns = 0; // previous frame duration: _frame_start_tick → fence done
  u64 _frame_start_tick    = 0; // tick recorded after the JIT sleep
  AdaptiveWait _begin_wait{ AdaptiveWait::Mode::Precise };

  u32 _current_display_id = 0; // CGDirectDisplayID currently targeted by CVDisplayLink

  // CVDisplayLink-fed predictor for next scanout time; also exposed for VR pose prediction.
  time_predictor_ptr_t _scan_out_predictor = std::make_shared<TimePredictor>();
  time_predictor_ptr_t getScanoutPredictor() const override { return _scan_out_predictor; }
};
#endif // __APPLE__ && !ORK_NO_METAL_SWAPCHAIN

////////////////////////////////////////////////////////////////////////////////
// VkFrameBufferInterface - Vulkan framebuffer/rendertarget management.
//  Owns the output target (_output), manages RTG push/pop, captures, and
////////////////////////////////////////////////////////////////////////////////

struct VkFrameBufferInterface final : public FrameBufferInterface {

  VkFrameBufferInterface(vkcontext_rawptr_t ctx);
  ~VkFrameBufferInterface();

  ///////////////////////////////////////////////////////

  captureasync_ptr_t capture(const RtBuffer* inpbuf, const file::Path& pth, void_lambda_t on_capture_complete = nullptr) final;
  captureasync_ptr_t captureToTexture(const RtBuffer* inpbuf, Texture& tex, void_lambda_t on_capture_complete = nullptr) final;
  captureasync_ptr_t captureAsFormat(const RtBuffer* inpbuf, capturebuffer_ptr_t buffer, EBufferFormat destfmt, void_lambda_t on_capture_complete = nullptr) final;
  captureasync_ptr_t capturePixelAsync(pixelfetchctx_ptr_t pfc, int x, int y, void_lambda_t on_capture_complete = nullptr) final;
  void GetPixel(const fvec4& rAt, PixelFetchContext& ctx) final;

  ///////////////////////////////////////////////////////

  void rtGroupClear(rtgroup_rawptr_t rtg) final;
  void rtGroupMipGen(rtgroup_rawptr_t rtg) final;
  void rtGroupTransitionToTexture(rtgroup_rawptr_t rtg) final;
  void msaaBlit(rtgroup_ptr_t src, rtgroup_ptr_t dst) final;
  void blit(rtgroup_ptr_t src, rtgroup_ptr_t dst) final;
  void downsample2x2(rtgroup_ptr_t src, rtgroup_ptr_t dst) final;
  void transitionDepthForSampling(rtgroup_ptr_t rtg) final;
  void transitionDepthForWriting(rtgroup_ptr_t rtg) final;

  //////////////////////////////////////////////

  void _initializeContext(DisplayBuffer* pBuf);
  void _setViewport(int iX, int iY, int iW, int iH) final;
  void _setScissor(int iX, int iY, int iW, int iH) final;
  void _doBeginFrame(void) final;
  void _doEndFrame(void) final;
  void _pushRtGroup(rtgroup_rawptr_t Base) final;
  void _popRtGroup() final;
  // emitted at every pass end: fills the single-sample depth copy for a multiview
  // MSAA target, whose in-pass resolve can only ever reach one view.
  void _endedDepthWritePass(vkrtgrpimpl_ptr_t impl);

  //////////////////////////////////////////////

  void __setRtGroup(rtgroup_rawptr_t Base);
  vkrtgrpimpl_ptr_t _buildRtgImplFromTextureArraySlice(rtgroup_rawptr_t rtg);
  vkrtgrpimpl_ptr_t _buildRtgImplFromScratch(rtgroup_rawptr_t rtg);
  vkrtgrpimpl_ptr_t _buildRtgImplForMainSurface(rtgroup_rawptr_t rtg);

  //////////////////////////////////////////////

  // Ensure a depth buffer exists on rtg at the given size, creating or resizing as needed.
  void _ensureDepth(rtgroup_ptr_t rtg, int w, int h, const VkRtbCreateOption& depth_opt);

  freestyle_mtl_ptr_t utilshader();
  vkrtgrpimpl_ptr_t _createRtGroupImpl(const VkRtgCreateOptions& options);
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

  // Output target: VkSwapChain, VkSwapChainDRM, or VkOffscreen.
  vkfboutput_ptr_t _output;

};

///////////////////////////////////////////////////////////////////////////////
struct VkTextureInterface final : public TextureInterface {

  VkTextureInterface(vkcontext_rawptr_t ctx);

  void _beginFrame();
  //
  bool destroyTexture(texture_ptr_t ptex) final;
  void ApplySamplingMode(Texture* ptex) final;
  void initTextureFromData(Texture* ptex, TextureInitData tid) final;
  void initTextureFromGpuExternalSurface(Texture* ptex) final;
  bool initFromShm(texture_ptr_t tex, std::shared_ptr<ShmTexConsumer> consumer) final;
  bool externalTextureChanged(const Texture* ptex);  // Check if external backing changed
  void generateMipMaps(Texture* ptex) final;
  Texture* createFromMipChain(MipChain* from_chain) final;

  void _createFromLoadReq(texloadreq_ptr_t tlr) final;
  void _initTextureFromRtBuffer(RtBuffer* rtb);
  void initTextureArray2DFromData(TextureArray* array, TextureArrayInitData tid) final;

  // chunked-upload API (see ork/lev2/gfx/txi.h)
  void reserveTexture(Texture* tex, int w, int h, int num_mips, EBufferFormat fmt) final;
  void reserveTextureArray(TextureArray* tarr, int w, int h, int num_slices, int num_mips, EBufferFormat fmt) final;
  void uploadTextureRegion(Texture* tex, const TextureRegionUpload& upload, ::ork::void_lambda_t on_complete) final;
  void finalizeUpload(Texture* tex, ::ork::void_lambda_t on_complete) final;

  /////////////////////////////
  // init a blank texture array
  /////////////////////////////

  void initTextureArray2D(TextureArray* ptex) final; 
  void initTextureArray2DAsync(TextureArray* ptex) final;
  void _enqueueInitTextureArray2DOnCB(TextureArray* ptex,VkCommandBuffer extcmdbuf);

  /////////////////////////////

  void updateTextureArraySlice(TextureArraySliceRef* slice, image_ptr_t img) final;
  void _updateTextureArraySlice(TextureArraySliceRef* slice, compressedmipchain_ptr_t mipc);

  void updateTextureArray(TextureArray* array) final; // sync


  // Helper function to convert 24-bit formats to 32-bit on macOS
  static EBufferFormat convertFormatForPlatform(EBufferFormat format);
  
  vkcontext_rawptr_t _contextVK;
  using sbpoolmap_t = std::unordered_map<size_t, stagingbufferpool_ptr_t>;
  stagingbufferpool_ptr_t stagingBufferPoolForSrcOfSize(size_t size);
  LockedResource<sbpoolmap_t> _stagingSrcBuffers;
  std::unordered_set<vktexobj_ptr_t> _texobjs_pending_for_deletion;
  std::unordered_set<vkimageobj_ptr_t> _imgobjs_pending_for_deletion;  // For swapping VkImages in external textures
  LockedResource<sseccmdbufpool_ptr_t> _seccmdbufpool_xfer;
  size_t _current_frame = 0;  // Frame counter for deferred resource deletion
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
  fxbuffer_member_constptr_t findStorageMember(fxparamstorageblock_constptr_t block, const std::string& member_name) final;

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

  //////////////////////////////////////////

  datablock_ptr_t _writeIntermediateToDataBlock(shadlang::SHAST::transunit_ptr_t tunit);
  vkfxsfile_ptr_t _readFromDataBlock(datablock_ptr_t inpdata, FxShader* shader);
  vkfxsfile_ptr_t _loadShaderFromShaderText(
      FxShader* shader,               //
      const std::string& parser_name, //
      const std::string& shadertext,  //
      shadlang::slpcache_ptr_t slp_cache);

  // interplay between the current rasterstate stack top and the pass stateblock
  //  rasterstate (if any) - higher priority wins. Every _fetchPipeline* variant
  //  bakes from this, and _bindPipeline reads it back for dynamic state.
  rasterstate_ptr_t _effectiveRasterState();

  vkpipelinestate_rawptr_t _fetchPipeline( vkvtxbuf_ptr_t vb, vkprimclass_ptr_t primclas);
  vkpipelinestate_ptr_t _createPipeline( vkvtxbuf_ptr_t vb,             //
                                        vkprimclass_ptr_t primclas,    //
                                        vkrasterstate_ptr_t rstate );  //
  // SSBO-only pipelines (no vertex buffer, vertex shader reads from SSBO via gl_VertexID)
  vkpipelinestate_rawptr_t _fetchPipelineSSBO(vkprimclass_ptr_t primclas);
  vkpipelinestate_ptr_t _createPipelineSSBO(vkprimclass_ptr_t primclas, vkrasterstate_ptr_t rstate);
  // taskless mesh pipelines (MESH+FRAGMENT stages; no vertex input, no input assembly,
  //  hence no vertex buffer and no primclass — topology comes from the mesh stage itself)
  vkpipelinestate_rawptr_t _fetchPipelineMesh();
  vkpipelinestate_ptr_t _createPipelineMesh(vkrasterstate_ptr_t rstate);
  void _createPipelineReport(vkpipelinestate_ptr_t pipeline);           //
  VkPipelineLayoutCreateInfo _createPipelineLayoutData(vkpipelinestate_ptr_t pipeline);
  // ubo
  FxUniformBuffer* createUniformBuffer(size_t length) final;
  fxuniformbuffermapping_ptr_t mapUniformBuffer(FxUniformBuffer* b, size_t base, size_t length) final;
  void unmapUniformBuffer(FxUniformBufferMapping* mapping) final;
  void bindUniformBuffer(const FxUniformBlock* block, FxUniformBuffer* buffer) final;

  // ssbo
  FxShaderStorageBuffer* createStorageBuffer(
      size_t length,
      StorageBufferUsage usage   = StorageBufferUsage::DEFAULT,
      BufferResidency    residency = BufferResidency::HOST) final;
  void destroyStorageBuffer(FxShaderStorageBuffer* buffer) final; // immediate (GPU-idle contract)
  storagebuffermappingptr_t mapStorageBuffer(
      FxShaderStorageBuffer* b,
      size_t base,
      size_t length,
      BufferMapAccess access) final;
  void unmapStorageBuffer(FxShaderStorageBufferMapping* mapping) final;
  void readStorageBuffer(FxShaderStorageBuffer* b, size_t base, size_t length, void* dst) final;
  void writeStorageBuffer(FxShaderStorageBuffer* b, size_t base, size_t length, const void* src) final;
  void bindStorageBuffer(const FxShaderStorageBlock* block, FxShaderStorageBuffer* buffer, size_t byte_offset = 0) final;
  void copyBufferIntoStorageBuffer(FxShaderStorageBuffer* ssbo, std::vector<uint8_t> buffer, size_t dest_offset) final;

  void _doPushRasterState(rasterstate_ptr_t rs) final;
  rasterstate_ptr_t _doPopRasterState() final;
  void applyRasterState(const RasterState& rstate) final;

  void _bindPipeline(VkCommandBuffer cmdbuf, vkpipelinestate_rawptr_t pipe);
  void _uploadPipelineData(VkCommandBuffer cmdbuf, vkpipelinestate_rawptr_t pipe);
  void _bindGfxDescriptorSetOnSlot(VkCommandBuffer cmdbuf, vkdescriptorsetstate_ptr_t desc_set, size_t slot);
  void _bindVertexBufferOnSlot(VkCommandBuffer cmdbuf, vkvtxbuf_ptr_t vb, size_t slot);

  int _pipelineBitsForShader(vkfxshaderpass_rawptr_t shprog);

  VkFxShaderUniformBlockState* uniformStateForBlock(VkFxShaderUniformBlock* block);
  VkFxShaderStorageBlockState* storageStateForBlock(VkFxShaderStorageBlock* block);

  void _ensureBlockStates(vkfxshaderpass_rawptr_t prog);
  void _logMissingBindState(const std::string& name);

  std::vector<uint32_t> _dynamic_offsets;

  // Dedicated backing buffers for the per-frame-constant uniform blocks
  // (isNonDynamicUniformBlock). Recorded by bindUniformBuffer, read by the
  // descriptor-write path. Keyed by block name: one buffer serves every
  // program declaring that block, so this is process-wide state, not per-pass.
  std::map<std::string, vkbuffer_ptr_t> _nondynamic_ubo_buffers;

  // Zero-filled stand-in for programs that DECLARE a non-dynamic block but
  // never bind it — they read zeros under the dynamic path too, and must keep
  // reading zeros. Shared per size; see the descriptor-write site.
  vkbuffer_ptr_t _zeroUniformBuffer(size_t length);
  std::map<size_t, vkbuffer_ptr_t> _zero_ubo_buffers;
  
  vkfxshaderpassstate_rawptr_t _current_shader_pass_state = nullptr;
  vkpipelinestate_rawptr_t     _currentPipeline = nullptr;
  fxtechnique_constptr_t       _currentORKTEK = nullptr;
  vkfxstek_rawptr_t            _currentVKTEK  = nullptr;
  vkfxshaderpass_rawptr_t      _currentVKPASS = nullptr;
  vkcontext_rawptr_t           _contextVK     = nullptr;
  
  std::unordered_map<VkFxShaderUniformBlock*, VkFxShaderUniformBlockState> _uniform_block_states;
  std::unordered_map<VkFxShaderStorageBlock*, VkFxShaderStorageBlockState> _storage_block_states;
  
  std::unordered_map<vkfxshaderpass_rawptr_t, VkFxShaderPassState> _shader_pass_states;
  std::unordered_map<uint64_t, vkpipelinestate_ptr_t> _pipelines;
  
  std::map<AssetPath, vkfxsfile_ptr_t> _fxshaderfiles;
  std::mutex _fxshaderfiles_mutex; // guards _fxshaderfiles (map only, not compiles)
  shadlang::slpcache_ptr_t _slp_cache;
  priority_stack<rasterstate_ptr_t> _rasterstate_stack;
  rasterstate_ptr_t _rasterstate_top;
  lev2::rasterstate_ptr_t _default_rasterstate;
  std::unordered_map<uint64_t, int> _vk_vtxinterface_cache;
  std::unordered_map<uint64_t, int> _vk_geointerface_cache;
  std::array<vkdescriptorsetstate_ptr_t, 4> _active_gfx_descriptorSets;
  std::array<vkvtxbuf_ptr_t, 4> _active_vbs;
  std::set<std::string> _logged_missing_bind_states;
  bool _enable_pipeline_debug = false;
  
  bool _tryBindMergedResource(const FxShaderParam* hpar,
                              VkMergedResourceBinding::Type expected_type,
                              svar64_t resource_data);

  void _ensureUBORegistered(VkFxShaderUniformBlock* block);
};
///////////////////////////////////////////////////////////////////////////////
struct VkComputeInterface : public ComputeInterface {

  VkComputeInterface(vkcontext_rawptr_t ctx);

  void beginDispatchPhase(const char* label = nullptr) final;
  void endDispatchPhase() final;
  void storageBarrier() final;
  void copyBufferRegion(
      FxShaderStorageBuffer* src, size_t src_offset,
      FxShaderStorageBuffer* dst, size_t dst_offset,
      size_t size) final;
  void copySSBOToVertexBuffer(
      FxShaderStorageBuffer* src, size_t src_offset,
      VertexBufferBase* dst_vb, size_t dst_offset,
      size_t size) final;

  void dispatchCompute(const FxComputeShader* shader, uint32_t numgroups_x, uint32_t numgroups_y, uint32_t numgroups_z) final;

  void dispatchComputeIndirect(const FxComputeShader* shader, FxShaderStorageBuffer* args, size_t args_offset = 0) final;

  void dispatchComputeInline(const FxComputeShader* shader, uint32_t numgroups_x, uint32_t numgroups_y, uint32_t numgroups_z) final;


  void bindStorageBuffer(const FxComputeShader* shader, uint32_t binding_index, FxShaderStorageBuffer* buffer) final;
  void bindStorageBufferOnBlock(const FxComputeShader* shader, FxShaderStorageBuffer* buffer, const FxShaderStorageBlock* block) final;

  void bindImage(const FxComputeShader* shader, uint32_t binding_index, Texture* tex, ImageBindAccess access) final;
  void bindSampler(const FxComputeShader* shader, uint32_t binding_index, Texture* tex) final;

  // PipelineCompute* createComputePipe(ComputeShader* csh);
  // void bindComputeShader(ComputeShader* csh);

  // PipelineCompute* _currentComputePipeline = nullptr;
  vkcontext_rawptr_t _contextVK;
  vkfxi_ptr_t _fxi;
  bool _inDispatchPhase = false;
  int  _phaseDepth = 0; // reentrancy: begin/endDispatchPhase nest so a per-view fan-out can batch
                        // EVERY drawable's cull into ONE submit (only the outermost end submits)
  bool _didSuspendRenderPass = false;

  // Dedicated compute command buffer
  VkCommandBuffer _computeCmdBuf = VK_NULL_HANDLE;
  uint32_t _dispatchCount = 0;
  // transfer (buffer-copy) commands recorded this phase. A copy-ONLY phase (no compute
  // dispatch) still holds GPU work that must be submitted — the outermost endDispatchPhase
  // submits when EITHER count is non-zero (dispatchCount alone drives the descriptor-set ring).
  uint32_t _transferCount = 0;
  // bumped each beginDispatchPhase; pipelines recycle their per-dispatch descriptor
  // set ring when the generation changes (prior-phase command buffer has completed).
  uint64_t _dispatchGeneration = 0;
  // open GPU slice for the OUTERMOST labeled phase (-1 = this phase is untimed)
  int _phaseSliceHandle = -1;
  // the OUTERMOST labeled phase's name, held for the profiler range + debug label it
  // opened (null = unlabeled phase, which emits neither). Callers pass string literals.
  const char* _phaseLabel = nullptr;

  // ---- C.5 P3b: NON-BLOCKING submit (flag: ORK_HM_NB_SUBMIT=1; blocking = the soak-default).
  // Depth-1 overlap: endDispatchPhase submits with the persistent fence and RETURNS; the wait
  // happens at the next hazard point — the next beginDispatchPhase (cmdbuf reset + descriptor-ring
  // recycling + COW-pool reuse all become safe there) or ANY host storage-buffer map (param
  // rewrites, readbacks). GPU->GPU ordering vs the render needs nothing extra: same VkQueue,
  // submission order + the end-phase barrier. Waits for the pending phase's fence (no-op if none).
  void syncPendingDispatch();
  VkFence _phaseFence  = VK_NULL_HANDLE; // persistent (created on first submit; never destroyed
                                         // post-shutdown per the teardown-funnel lesson)
  bool _phasePending   = false;          // a submitted-but-unwaited phase is in flight
  bool _nonblocking    = false;          // ORK_HM_NB_SUBMIT=1 (read once in the ctor)
};

///////////////////////////////////////////////////////////////////////////////

// MT0/MT1 (JUL05_GPUMICROTASK §2.4): the engine's ONLY GPU timing primitive —
// ALWAYS ON, never gated by ORK_PROFILER_ENABLE (T3). One whole-frame begin/end
// pair (MT0) PLUS N named per-pass slices (MT1) in the same query pool.
//
// SLICES come from command buffers this class does not own and cannot see: the
// frame's primary CB (render-pass instances, vulkan_fbi_rtgroup), the compute
// interface's own CB (VkComputeInterface dispatch phases) and the one-shot XR
// blit CBs (vulkan_vr_composite). So the slice API takes the CB per call and the
// per-frame slot cursor is ATOMIC — two CBs recording in the same frame must
// never claim the same query pair.
//
// MULTIVIEW: a timestamp written INSIDE a multiview render-pass instance consumes
// one query per view, silently shifting every later index. Every slice write in
// this engine is therefore placed OUTSIDE any render-pass instance (before
// vkCmdBeginRenderingKHR / after vkCmdEndRenderingKHR); the compute and blit CBs
// have no render pass at all.
///////////////////////////////////////////////////////////////////////////////

struct VkGpuSliceTimer final : public GpuSliceTimer {
  // Per-frame-slot SLICE capacity. Counted in device-side SEGMENTS, not in names:
  // a suspended+resumed render pass, or an rtgroup pushed twice, spends a pair per
  // bracket and the readback sums them by name. Overflow is counted and published
  // (GpuPassSnapshot::_dropped) rather than dropped silently.
  static constexpr uint32_t kMaxSlices = 128;
  static constexpr uint32_t kSliceNameMax = 40; // copied, not referenced: the readback is
                                               // lag-2 and an rtgroup can die in between
  // Query-pair RING with lag-2 NON-WAITING reads. The first Linux/RADV gate run
  // proved why: a WAIT_BIT read right after submit wedged the amdgpu GPU in
  // kernel dma_fence_wait (that submit does NOT fully resolve the frame there,
  // unlike MoltenVK's blocking path). This timer must NEVER wait: frame N reads
  // frame N-2's pair with AVAILABILITY, and "not ready" is simply gpu_ms=-1
  // (GpuFrameTiming falls back to present-idle for that frame).
  static constexpr uint32_t kRingDepth = 4; // pairs; > MAX_FRAMES_IN_FLIGHT(2)

  // POOL LAYOUT: [frame pairs: 2 * kRingDepth][slice pairs: 2 * kMaxSlices * kRingDepth]
  static constexpr uint32_t kFrameQueryBase = 0;
  static constexpr uint32_t kSliceQueryBase = 2 * kRingDepth;
  static constexpr uint32_t kNumQueries     = kSliceQueryBase + 2 * kMaxSlices * kRingDepth;

  VkDevice        _device       = VK_NULL_HANDLE;
  VkQueryPool     _query_pool   = VK_NULL_HANDLE;
  VkCommandBuffer _cmdbuf       = VK_NULL_HANDLE; // this frame's primary CB (not owned)
  double          _tickToMs     = 1.0;
  // ATOMIC: slices are opened from the compute interface's CB and the XR blit CBs,
  // which are recorded off the frame's own call stack — those readers need a frame
  // index that is never torn.
  std::atomic<uint64_t> _frameCounter{0}; // advanced once per frame (in beginFrame)

  // vkResetQueryPool (core 1.2 hostQueryReset). REQUIRED for the slice region: its
  // writes come from several CBs, one of which (compute) is submitted BEFORE the
  // frame's primary CB, so a vkCmdResetQueryPool on the primary CB would wipe
  // timestamps the device had already written. The host resets the slot at readback
  // instead, two frames after its last write.
  PFN_vkResetQueryPool _vkResetQueryPool = nullptr;
  // VK_EXT_calibrated_timestamps, when the device has it: publishes the
  // gpu-domain -> CLOCK_MONOTONIC offset alongside the numbers so a GPU span can be
  // placed on the same timeline as a CPU one. Absent = the HUD says "uncal".
  PFN_vkGetCalibratedTimestampsEXT _vkGetCalibratedTimestamps = nullptr;

  // Slices are published by the app's RENDER context only. The LOADING context runs
  // its own near-empty frames at thousands per second — its 3us spans and its own
  // rtgroup name would overwrite the render context's numbers in the process-wide
  // GpuPassStats sink several times per real frame (first observed as a GPU page with
  // one 0.00ms row). Set per frame from meTargetType, so no init-order can get it wrong.
  bool _sliceEnable = true;

  struct SliceSlot {
    char _name[kSliceNameMax] = {0};
  };
  // per-ring-slot slice bookkeeping
  std::atomic<uint32_t> _sliceCursor[kRingDepth];  // claimed pairs this frame slot
  std::atomic<uint32_t> _sliceDropped[kRingDepth]; // claims past kMaxSlices
  SliceSlot             _sliceSlots[kRingDepth][kMaxSlices];

  VkGpuSliceTimer(VkDevice device, float timestamp_period_ns);
  ~VkGpuSliceTimer() override;

  // The primary CB pool-recycles a new VkCommandBuffer handle every frame —
  // caller must repoint this before beginFrame().
  void setCmdBuf(VkCommandBuffer cb) { _cmdbuf = cb; }

  void  beginFrame() override;
  void  endFrame() override;
  float readbackFrameMs() override;

  // MT1 per-pass slices. sliceBegin claims a pair in THIS frame's ring slot and
  // writes the opening timestamp into `cb`; the returned handle is passed back to
  // sliceEnd (same cb, same frame). -1 = the frame's slot budget is spent (counted,
  // published, never fatal) — sliceEnd(-1) is a no-op.
  // BOTH calls must be outside any render-pass instance (multiview rule above).
  int  sliceBegin(VkCommandBuffer cb, const char* name);
  void sliceEnd(VkCommandBuffer cb, int handle);

  // Read the lag-2 frame slot's slices, publish them into GpuPassStats (summed by
  // name) together with the whole-frame span, and host-reset the slot. Called once
  // per frame from the frame-end readback block; never waits.
  void readbackSlices(float gpu_frame_ms);
};
using vkgpuslicetimer_ptr_t = std::shared_ptr<VkGpuSliceTimer>;

////////////////////////////////////////////////////////////////////////////////
// VkThreadedQueue
//   Wraps a single VkQueue with a mutex so multiple threads can safely submit
//   without assuming more than one queue per family is available.
////////////////////////////////////////////////////////////////////////////////

struct VkThreadedQueue {
  VkQueue _vkqueue = VK_NULL_HANDLE;
  u32     _qfid    = 0xffffffff;

  // Using Mutex for now. Only used in debug scenarios.
  // If using in production should switch to MPMC queue.
  // However would need to reworked the swap reinit logc to no need to wait.
  //
  // Recursive because an external client bound to this queue (e.g. an XR runtime
  // that submits inside its own frame-submission calls) is serialized by holding
  // this across the whole external-submit region — and the engine's own composite
  // blit nested inside that region re-enters queueSubmit on the same thread.
  std::recursive_mutex _submit_mutex;

  VkResult queueSubmit(const VkSubmitInfo* pSubmits, VkFence fence);
  VkResult queuePresent(const VkPresentInfoKHR* pPresentInfo);

  // When submitting from multipled threads you need to use this wait
  // not vkDeviceWaitIdle as that can technically make calls to queues
  // across threads and cause validation errors. 
  VkResult queueWaitIdle();
};

using vkthreadedqueue_ptr_t = std::shared_ptr<VkThreadedQueue>;

// monotonic count of every vkQueueSubmit the engine has issued (all queues, all threads).
//  VkContext turns it into the per-frame Context::submitCount() at the frame boundary.
std::atomic<uint64_t>& vkGlobalSubmitCounter();

// ERROR-severity validation messages seen by the debug messenger, and whether the
//  validation layer is loaded at all (instance-global, like the messenger itself).
int vkValidationErrorCount();
bool vkValidationArmed();

////////////////////////////////////////////////////////////////////////////////

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

  // Pre-destruction Vulkan teardown. Releases the presentation
  // surface (when owned) and drops the device reference while the
  // owning shared_ptrs are still live. Called from
  // Context::shutdown(); the eventual ~VkContext is then a no-op
  // path that skips the surface destroy since the field is null.
  void _doShutdown() final;

  // Vertex/index buffer teardown funneled through the context so it can be gated on shutdown:
  // BEFORE _doShutdown() it queues the VkBuffer for deferred cleanup (the old destructor body);
  // AFTER shutdown (device + command buffers gone) it is a NO-OP, avoiding the post-shutdown
  // destructor abort. Called from ~VulkanVertexBuffer / ~VulkanIndexBuffer.
  void destroyVertexBuffer(vkbuffer_ptr_t vkbuffer);
  void destroyIndexBuffer(vkbuffer_ptr_t vkbuffer);
  // raw-Vulkan-handle teardown, funneled through the context so it is a NO-OP after _doShutdown()
  // (device gone). Called from VulkanBuffer / VulkanImageObject / VulkanMemoryForImage /
  // VkComputePipelineState destructors. Bookkeeping (ref counts etc.) stays in those dtors.
  void destroyBuffer(VkBuffer buffer);
  void destroyImageMemory(VkDeviceMemory mem);
  void destroyImageObject(VkImageView view, VkImage image, VkDeviceMemory mem);
  void destroyComputePipelineState(
      VkPipeline pipeline, VkPipelineLayout layout,
      VkDescriptorSetLayout dsl, const std::vector<VkDescriptorPool>& pools);
  bool _isShutdown = false;

  void FxInit();

  ///////////////////////////////////////////////////////////////////////

  void _doResizeMainSurface(int iw, int ih) final;
  void _doPreBeginFrame() final;
  void _doBeginFrame() final;
  void _doEndFrame() final;
  ctx_platform_handle_t _doClonePlatformHandle() const final;

  void _beginAssetProcessing();
  void _endAssetProcessing();

  void _doBeginPrimaryCommandBuffer() final;
  void _doEndPrimaryCommandBuffer() final;
  void _doSubmitPrimaryCommandBuffer() final;
  void _doExecuteInlineGpuJob(const void_lambda_t& record) final;
  void _onGpuPreInit() final;
  void _onGpuPostInit() final;
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
  int msaaMaxSamples() final;   // from VkPhysicalDeviceLimits framebufferColor+DepthSampleCounts
  bool supportsVolumeRenderTarget(EBufferFormat fmt) final; // vkGetPhysicalDeviceImageFormatProperties probe
  bool supportsMeshShader() const final; // VK_EXT_mesh_shader enabled on this device
  bool supportsTaskShader() const final; // + the taskShader (amplification) feature chained
  uint32_t maxTaskPayloadSize() const final;
  int taskShaderDrawCount() const final;
  bool supportsMeshShaderIndirect() const final; // + vkCmdDrawMeshTasksIndirectEXT loadable
  bool supportsMultiview() const final;            // core VK1.1 multiview feature chained
  int maxMultiviewViewCount() const final;         // device's maxMultiviewViewCount (0 if unsupported)
  bool supportsMultiviewMeshShader() const final;  // mesh stage legal inside a multiview pass
  uint32_t maxMeshMultiviewViewCount() const final;
  int validationErrorCount() const final;
  bool validationArmed() const final;

  time_predictor_ptr_t getScanoutPredictor() const final {
    return (_fbi && _fbi->_output) ? _fbi->_output->getScanoutPredictor() : nullptr;
  }

  bool displayProvidesFramePacing() const final {
    return (_fbi && _fbi->_output) ? _fbi->_output->providesFramePacing() : false;
  }

  // The graphics queue may be handed to an external client (e.g. an XR runtime)
  // that submits to it inside its own frame-submission calls; expose that queue's
  // submit mutex so the caller can serialize the whole external-submit region.
  std::recursive_mutex* externalSubmitMutex() const final {
    return _gfxqueue ? &_gfxqueue->_submit_mutex : nullptr;
  }

  ///////////////////////////////////////////////////////////////////////

  void makeCurrentContext(void) final;

  void initializeWindowContext(Window* pWin, CTXBASE* pctxbase) final; // make a window
  void initializeOffscreenContext(DisplayBuffer* pBuf) final;          // make a pbuffer
  void initializeDisplayClientContext(vkdisplayclient_ptr_t client);  // client output via exchange
  void initializeLoaderContext() final;
#if defined(__linux__)
  void initializeDRMContext(Window* pWin, CTXBASE* pctxbase) final;   // DRM direct-to-display window
#endif

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
  void _initDefaultTextures();
  //////////////////////////////////////////////
  // TODO obsolete prefer using VK_SET_DEBUG_NAME in vk_protos.h
  template <typename T> void _setObjectDebugName(T& object, VkObjectType objectType, const char* name) {
    if (_vkSetDebugUtilsObjectName) {
      VkDebugUtilsObjectNameInfoEXT nameInfo = {};
      initializeVkStruct(nameInfo, VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT);
      nameInfo.objectType   = objectType;
      nameInfo.objectHandle = reinterpret_cast<uint64_t>(object);
      nameInfo.pObjectName  = name;
      if(0)printf("Setting debug name for object type %d: %s\n", objectType, name);
      _vkSetDebugUtilsObjectName(_vkdevice, &nameInfo);
    }
  }
  //////////////////////////////////////////////
  template <typename T> bool _fetchDeviceProcAddr(T& object, const char* name) {
    object = reinterpret_cast<T>(vkGetDeviceProcAddr(_vkdevice, name));
    bool loaded = (object != nullptr);
    OrkAssertI(loaded, name);
    return loaded;
  }
  //////////////////////////////////////////////
  VkDevice _vkdevice;
  VkPhysicalDevice _vkphysicaldevice;
  // WS3: PERSISTED pipeline cache (<staging>/vkpipelinecache/<pipelineCacheUUID>.bin).
  // Owner = the context that CREATED the device (vkCreateDevice path); shared-device
  // contexts (loader/offscreen) borrow the handle. Saved+destroyed in _doShutdown.
  // vkCreate*Pipelines use of the cache is internally synchronized per the vk spec.
  VkPipelineCache _vkPipelineCache = VK_NULL_HANDLE;
  bool _ownsPipelineCache          = false;
  void _initPipelineCache();
  void _savePipelineCache();
  vkdeviceinfo_ptr_t _vkdeviceinfo;
  // Default-initialized so the loader (offscreen) path, which never
  // creates a presentation surface, doesn't trip _doShutdown's
  // vkDestroySurfaceKHR with a poison-pattern uninitialized handle.
  VkSurfaceKHR _vkpresentationsurface = VK_NULL_HANDLE;
  vkswapchaincaps_ptr_t _vkpresentation_caps;
  std::vector<const char*> _device_extensions;
  size_t _num_queue_types = 0;

  //////////////////////////////////////////////

  std::vector<float> _queuePriorities;
  std::vector<VkDeviceQueueCreateInfo> _DQCIs;
  static constexpr uint32_t NO_QUEUE = 0xffffffff;
  vkthreadedqueue_ptr_t _gfxqueue;
  uint32_t _vkqfid_compute           = NO_QUEUE;
  uint32_t _vkqfid_transfer          = NO_QUEUE;
  VkCommandPool _vkcmdpool_graphics  = VK_NULL_HANDLE;
  primary_commandbuffer_ptr_t _defaultCommandBuffer;
  vkpricmdbufimpl_ptr_t _defaultCommandBufferImpl;
  vkpricmdbufimpl_ptr_t _cmdbufcurpri_gfx;
  // tracks whether the current primary CB is in the RECORDING state. Init-time
  // code (e.g. hypermesh materialize) may cycle whole frames inside an outer
  // begin/endPrimaryCommandBuffer pair — the outer end must then no-op instead
  // of calling vkEndCommandBuffer on a non-recording CB.
  bool _pricb_recording = false;
  vkpricmdbufimpl_ptr_t primary_cb();

  // Synchronous transfer resources (for out-of-frame texture uploads)
  struct SyncTransferResources {
    primary_commandbuffer_ptr_t command_buffer;
    vkpricmdbufimpl_ptr_t command_buffer_impl;
    vkbuffer_ptr_t staging_buffer;  // upload direction: WC host memory (fast CPU writes)
    size_t staging_size = 0;
    // readback direction gets its OWN staging in HOST_CACHED memory: CPU reads from
    // write-combined memory run ~150MB/s (measured — made the terrain cook's 34GB of
    // blob readbacks take 220s); cached memory reads at full memcpy speed.
    vkbuffer_ptr_t readback_buffer;
    size_t readback_size = 0;
    std::mutex mutex;
  };
  SyncTransferResources _syncTransfer;

  void initSyncTransfer();
  void ensureSyncStagingSize(size_t needed);
  void ensureSyncReadbackStagingSize(size_t needed);
  size_t deviceLocalHeapBytes() const final; // largest DEVICE_LOCAL heap (residency budget scaling)
  void beginSyncTransferCB();
  void endAndSubmitSyncTransferCB();

  vksampler_obj_ptr_t _sampler_base;
  std::vector<vksampler_obj_ptr_t> _sampler_per_maxlod;
  VkDescriptorPool _vkDescriptorPool;
  
  // Sampler cache for texture sampling modes
  struct SamplerCacheKey {
    uint64_t _hash = 0;
    
    bool operator==(const SamplerCacheKey& other) const {
      return _hash == other._hash;
    }
  };
  
  struct SamplerCacheKeyHasher {
    size_t operator()(const SamplerCacheKey& key) const {
      return key._hash;
    }
  };
  
  std::unordered_map<SamplerCacheKey, vksampler_obj_ptr_t, SamplerCacheKeyHasher> _sampler_cache;
  std::mutex _sampler_cache_mutex;
  
  vksampler_obj_ptr_t _getOrCreateSampler(const TextureSamplingModeData& sampling_mode);
  
  // Default texture implementations for unloaded textures
  vktexobj_ptr_t _defaultTexImpl2D;
  vktexobj_ptr_t _defaultTexImplCube;
  vktexobj_ptr_t _defaultTexImpl2DArray;
  vktexobj_ptr_t _defaultTexImpl3D;
  


  PFN_vkSetDebugUtilsObjectNameEXT _vkSetDebugUtilsObjectName = nullptr;
  PFN_vkCmdDebugMarkerBeginEXT _vkCmdDebugMarkerBeginEXT      = nullptr;
  PFN_vkCmdDebugMarkerEndEXT _vkCmdDebugMarkerEndEXT          = nullptr;
  PFN_vkCmdDebugMarkerInsertEXT _vkCmdDebugMarkerInsertEXT    = nullptr;
  PFN_vkCmdBeginRendering _vkCmdBeginRenderingKHR             = nullptr;
  PFN_vkCmdEndRendering _vkCmdEndRenderingKHR                 = nullptr;
  PFN_vkCmdInsertDebugUtilsLabelEXT _vkCmdInsertDebugUtilsLabelEXT = nullptr;
  // resolved UNCONDITIONALLY (not behind the validation-layer gate): the instance
  // extension is always enabled, and an external capture tool is the consumer — it is
  // attached without the engine's debug mode being on.
  PFN_vkCmdBeginDebugUtilsLabelEXT _vkCmdBeginDebugUtilsLabelEXT = nullptr;
  PFN_vkCmdEndDebugUtilsLabelEXT _vkCmdEndDebugUtilsLabelEXT     = nullptr;
  PFN_vkCmdSetCullModeEXT _vkCmdSetCullModeEXT                = nullptr;
  PFN_vkCmdSetDepthWriteEnableEXT _vkCmdSetDepthWriteEnableEXT = nullptr;
  // bumped at each mesh draw when the bound pass carries a task stage — the only
  //  honest answer to "did the amplification stage run" (a taskless fallback still draws).
  void _countMeshDraw();
  std::atomic<int> _task_shader_draws{0};
  PFN_vkCmdDrawMeshTasksEXT _vkCmdDrawMeshTasksEXT            = nullptr;
  // INDIRECT mesh draw (count-variant deliberately absent: MoltenVK implements the direct and
  // indirect entries only, so vkCmdDrawMeshTasksIndirectCountEXT is never loaded or called).
  PFN_vkCmdDrawMeshTasksIndirectEXT _vkCmdDrawMeshTasksIndirectEXT = nullptr;
  //////////////////////////////////////////////
  // Buffers pending cleanup - accumulated when no primary CB is active
  // Moved to primary CB's cleanup list when a new primary CB begins
  std::vector<vkbuffer_ptr_t> _vkbuffers_pending_cleanup;
  std::mutex _vkbuffers_pending_cleanup_mutex;
  //////////////////////////////////////////////
  void* mhHWND;
  vkcontext_ptr_t _parentTarget;
  std::stack<void*> mDCStack;
  std::stack<void*> mGLRCStack;
  EDepthTest meCurDepthTest;
  bool mTargetDrawableSizeDirty;
  bool _first_frame = true;
  shared_pool::fixed_pool<PrimaryCommandBuffer, 16> _pri_cmdbuf_pool;
  //////////////////////////////////////////////
  vkpricmdbufimpl_ptr_t _createPrimaryVkCommandBuffer(PrimaryCommandBuffer* par);
  vkseccmdbufimpl_ptr_t _createSecondaryVkCommandBuffer(SecondaryCommandBuffer* par);
  void enqueueDeferredOneShotCommand(secondary_commandbuffer_ptr_t cmdbuf);
  LockedResource<vkseccmdbufarray_t> _pendingOneShotCommands;
  LockedResource<vkcompsema_set_t> _pendingOneShotSemas;
  void onFenceCrossed(void_lambda_t op);
  //////////////////////////////////////////////
  // One-shot semaphore submission (amortized storage, shared across all output paths)
  //////////////////////////////////////////////
  std::vector<VkSemaphore> _oneShotSignalSemaphores;
  std::vector<uint64_t> _oneShotSignalValues;
  // completion semaphores of the one-shot CBs recorded into THIS frame's primary CB
  // (coupled in _doPreBeginFrame's drain; consumed by _doSubmitPrimaryCommandBuffer).
  // A CB enqueued mid-frame (loading-phase ops) executes NEXT frame — its semaphore
  // must signal on THAT frame's submit, not this one's.
  std::vector<vkcompletionsemaphore_ptr_t> _thisFrameOneShotSemas;
  //////////////////////////////////////////////

  vkdwi_ptr_t _dwi;
  vkimi_ptr_t _imi;
  vkmsi_ptr_t _msi;
  vkfbi_ptr_t _fbi;
  vkgbi_ptr_t _gbi;
  vktxi_ptr_t _txi;
  vkfxi_ptr_t _fxi;
  vkci_ptr_t _ci;
  
  // Output target is now owned by VkFrameBufferInterface as _output.

  std::vector<captureasync_ptr_t> _pending_captures;
    void _processPendingCaptures();
  void _processPixelFetch(captureasync_ptr_t capture);
  //////////////////////////////////////////////
  // Render pass suspension/resumption support
  //////////////////////////////////////////////
  
  bool _renderPassActive = false;
  vkrtgrpimpl_ptr_t _activeRenderPassRTG = nullptr;

  uint64_t _submitCounterMark = 0; // vkGlobalSubmitCounter() value at the last frame boundary

  void suspendRenderPass();
  void resumeRenderPass();

  //////////////////////////////////////////////
  // MT0 (JUL05_GPUMICROTASK §2.4, SHADOW MODE): always-on GPU frame timing.
  // Measures + logs only — nothing here is enforced (no scheduler yet, MT1).
  // _gpuTimestampsSupported is decided ONCE at context init (the T2 caps
  // guard: timestampComputeAndGraphics + queue-family timestampValidBits,
  // escape-hatched by ORKID_MT_NO_TIMESTAMPS=1) in _initVulkanForDevInfo.
  // _mtSliceTimer stays null when unsupported — that null IS the fallback
  // path (gpu_ms=-1 into _mtFrameTiming, which then uses present-idle).
  //////////////////////////////////////////////
  // A context that REUSES another's VkDevice never runs _initVulkanForDevInfo, so it
  // would carry no timer at all — which is how the app's WINDOW/OFFSCREEN context ended
  // up with no GPU timing while the loading context (the device's creator) had it all.
  // Every device-sharing path calls this, like it copies the other device-derived state.
  void _inheritDeviceTimestampState(vkcontext_rawptr_t src);
  bool                  _gpuTimestampsSupported = false;
  vkgpuslicetimer_ptr_t _mtSliceTimer;
  GpuFrameTiming        _mtFrameTiming;
  Timer                 _mtFrameWallTimer; // cpu-frame-wall input to _mtFrameTiming

  //////////////////////////////////////////////
  // MT1: per-RENDER-PASS-INSTANCE GPU slices on the primary CB. ONE segment is
  // open at a time by construction — the primary CB has at most one live
  // dynamic-rendering instance — so the open handle is a single field rather than
  // a stack, and a suspend/resume pair closes one segment and opens another under
  // the same name (GpuPassStats sums them).
  //////////////////////////////////////////////
  void _gpuSliceOpenPass(const std::string& name);
  void _gpuSliceClosePass();
  std::string _gpuSlicePassName;      // name to reopen with after a suspend
  int         _gpuSlicePassHandle = -1; // open segment, -1 = none
  bool        _gpuSlicePassOpen   = false; // a segment bracket is live (labels included)

  //////////////////////////////////////////////
  // VK_EXT_debug_utils command-buffer LABELS, emitted at the same seams (and with the
  // same names) as the GPU slices above, so a capture tool's pass list reads in engine
  // vocabulary. Purely a tooling aid: null entry points make both a no-op, and no
  // engine behavior may depend on them.
  //////////////////////////////////////////////
  void _debugLabelBegin(VkCommandBuffer cb, const char* name);
  void _debugLabelEnd(VkCommandBuffer cb);

};
///////////////////////////////////////////////////////////////////////////
  struct VkCaptureBufferImpl {
    vkbuffer_ptr_t staging_buffer;
    VkFormat _actual_format = VK_FORMAT_UNDEFINED;
    EBufferFormat _desired_format = EBufferFormat::NONE;
  };
///////////////////////////////////////////////////////////////////////////
extern vkinstance_ptr_t _GVI;
} // namespace ork::lev2::vulkan
