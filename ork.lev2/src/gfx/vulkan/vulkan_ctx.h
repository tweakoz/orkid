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
#include "vulkan_ctx_protos.h"
#include "vulkan_ctx_geom.h"
#include "vulkan_ctx_misc.h"
#include "vulkan_ctx_image.h"
#include "vulkan_ctx_memory.h"
#include "vulkan_ctx_synch.h"
#include "vulkan_ctx_pipeline.h"
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
constexpr EBufferFormat DEPTH_FORMAT = EBufferFormat::Z24S8;
///////////////////////////////////////////////////////////////////////////////
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
///////////////////////////////////////////////////////////////////////////////
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
///////////////////////////////////////////////////////////////////////////////
struct VulkanVertexInterface {
  using input_t = VulkanVertexInterfaceInput;
  std::string _name;
  vkvertexinterface_ptr_t _parent;
  std::vector<vkvertexinterfaceinput_ptr_t> _inputs;
  int _pipeline_bits = -1;
  uint64_t _hash     = 0;
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanGeometryInterfaceInput {
  std::string _datatype;
  std::string _identifier;
  std::string _semantic;
  size_t _datasize = 0;
};
///////////////////////////////////////////////////////////////////////////////
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
///////////////////////////////////////////////////////////////////////////////
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

  void __setRtGroup(rtgroup_rawptr_t Base);
  vkrtgrpimpl_ptr_t _buildRtgImplFromTextureArraySlice(rtgroup_rawptr_t rtg);
  vkrtgrpimpl_ptr_t _buildRtgImplFromScratch(rtgroup_rawptr_t rtg);
  vkrtgrpimpl_ptr_t _buildRtgImplForMainSurface(rtgroup_rawptr_t rtg);

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
  //////////////////////////////////////////////

  vkswapchain_ptr_t _swapchain;
  std::unordered_set<vkswapchain_ptr_t> _old_swapchains;
};

///////////////////////////////////////////////////////////////////////////////
using StagingBufferPool = ObjectPoolX<SbsPoolAdapter>;
using stagingbufferpool_ptr_t = std::shared_ptr<StagingBufferPool>;
///////////////////////////////////////////////////////////////////////////////
using SecCmdBufPool = BoundedConcurrentObjectPoolX<SecCmdBufPoolAdapter,256>;
using sseccmdbufpool_ptr_t = std::shared_ptr<SecCmdBufPool>;///////////////////////////////////////////////////////////////////////////////
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

  void _bindPipeline(VkCommandBuffer cmdbuf, vkpipeline_obj_ptr_t pipe);
  void _bindGfxDescriptorSetOnSlot(VkCommandBuffer cmdbuf, vkdescriptorset_ptr_t desc_set, size_t slot);
  void _bindVertexBufferOnSlot(VkCommandBuffer cmdbuf, vkvtxbuf_ptr_t vb, size_t slot);

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
  VkCommandBuffer _vkcmdbuffer_current;
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
  Timer _present_timer;
  float _prev_time = 0.0f;
  float _total_wait_time = 0.0f;
  float _total_frame_time = 0.0f;
  float _present_wait_time = 0.0f;
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
extern vkinstance_ptr_t _GVI;
} // namespace ork::lev2::vulkan

