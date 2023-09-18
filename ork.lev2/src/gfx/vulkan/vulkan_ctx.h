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
#include <ork/kernel/orkpool.h>
#include <ork/file/chunkfile.inl>

///////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/shadlang.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/shadlang.h>

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
#if defined(ENABLE_COMPUTE_SHADERS)
struct VkComputeInterface;
#endif
//
struct VulkanTextureObject;
struct VulkanFxShaderObject;
struct VkFxShaderFile;
struct VkFxShaderProgram;
struct VkFxShaderPass;
struct VkFxShaderTechnique;
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
struct VkCommandBufferImpl;
struct VulkanRenderPass;
struct VulkanRenderSubPass;
struct VkSwapChainCaps;
struct VkSwapChain;
struct VkMsaaState;
struct VkRasterState;
struct VkBufferLayout;
//
using vkinstance_ptr_t   = std::shared_ptr<VulkanInstance>;
using vkdeviceinfo_ptr_t = std::shared_ptr<VulkanDeviceInfo>;
using vkdevgrp_ptr_t     = std::shared_ptr<VulkanDeviceGroup>;
using vkcontext_ptr_t    = std::shared_ptr<VkContext>;
using vkcontext_rawptr_t = VkContext*;
//
using vkdwi_ptr_t = std::shared_ptr<VkDrawingInterface>;
using vkimi_ptr_t = std::shared_ptr<VkImiInterface>;
//using vkrsi_ptr_t = std::shared_ptr<VkRasterStateInterface>;
using vkmsi_ptr_t = std::shared_ptr<VkMatrixStackInterface>;
using vkfbi_ptr_t = std::shared_ptr<VkFrameBufferInterface>;
using vkgbi_ptr_t = std::shared_ptr<VkGeometryBufferInterface>;
using vktxi_ptr_t = std::shared_ptr<VkTextureInterface>;
using vkfxi_ptr_t = std::shared_ptr<VkFxInterface>;
#if defined(ENABLE_COMPUTE_SHADERS)
using vkci_ptr_t = std::shared_ptr<VkComputeInterface>;
#endif
//
using vktexobj_ptr_t  = std::shared_ptr<VulkanTextureObject>;
using vkfxsfile_ptr_t = std::shared_ptr<VkFxShaderFile>;
using vkfxsobj_ptr_t  = std::shared_ptr<VulkanFxShaderObject>;
using vkfxsprg_ptr_t  = std::shared_ptr<VkFxShaderProgram>;
using vkfxspass_ptr_t = std::shared_ptr<VkFxShaderPass>;
using vkfxstek_ptr_t  = std::shared_ptr<VkFxShaderTechnique>;
using vkpipeline_obj_ptr_t = std::shared_ptr<VkPipelineObject>;
using vkprimclass_ptr_t = std::shared_ptr<VkPrimitiveClass>;
using vkfxsuniset_ptr_t     = std::shared_ptr<VkFxShaderUniformSet>;
using vkfxsunisetitem_ptr_t = std::shared_ptr<VkFxShaderUniformSetItem>;
using vkfxsunisetsamp_ptr_t = std::shared_ptr<VkFxShaderUniformSetSampler>;

using vkfxsuniblk_ptr_t     = std::shared_ptr<VkFxShaderUniformBlk>;
using vkfxsuniblkitem_ptr_t = std::shared_ptr<VkFxShaderUniformBlkItem>;
using vkfxpushconstantblk_ptr_t = std::shared_ptr<VkFxShaderPushConstantBlock>;
using vkbufferlayout_ptr_t = std::shared_ptr<VkBufferLayout>;

using vkrtbufimpl_ptr_t    = std::shared_ptr<VklRtBufferImpl>;
using vkrtgrpimpl_ptr_t    = std::shared_ptr<VkRtGroupImpl>;
using vktexasynctask_ptr_t = std::shared_ptr<VkTextureAsyncTask>;
using vktexloadreq_ptr_t   = std::shared_ptr<VkTexLoadReq>;
using vkfxshader_bin_t     = std::vector<uint32_t>;
using vkvtxbuf_ptr_t       = std::shared_ptr<VulkanVertexBuffer>;
using vkidxbuf_ptr_t       = std::shared_ptr<VulkanIndexBuffer>;
using vkvertexinputconfig_ptr_t = std::shared_ptr<VkVertexInputConfiguration>;
using vkloadctx_ptr_t      = std::shared_ptr<VkLoadContext>;
using vkcmdbufimpl_ptr_t   = std::shared_ptr<VkCommandBufferImpl>;
using vkrenderpass_ptr_t   = std::shared_ptr<VulkanRenderPass>;
using vksubpass_ptr_t      = std::shared_ptr<VulkanRenderSubPass>;
using vkswapchaincaps_ptr_t = std::shared_ptr<VkSwapChainCaps>;
using vkswapchain_ptr_t = std::shared_ptr<VkSwapChain>;
using vkmsaastate_ptr_t = std::shared_ptr<VkMsaaState>;
using vkrasterstate_ptr_t = std::shared_ptr<VkRasterState>;

using uniset_map_t = std::map<std::string,vkfxsuniset_ptr_t>;
using uniset_item_map_t = std::map<std::string, vkfxsunisetitem_ptr_t>;

extern vkinstance_ptr_t _GVI;

struct VkFormatConverter{

    static const VkFormatConverter _instance;
    VkFormatConverter();
    VkFormat convertBufferFormat(EBufferFormat fmt_in) const;
    VkImageLayout layoutForUsage(uint64_t usage) const;
    VkImageAspectFlagBits aspectForUsage(uint64_t usage) const;
    std::unordered_map<EBufferFormat,VkFormat> _fmtmap;
    std::unordered_map<uint64_t,VkImageLayout> _layoutmap;
    std::unordered_map<uint64_t,VkImageAspectFlagBits> _aspectmap;
};

///////////////////////////////////////////////////////////////////////////////

struct VkViewportTracker{

  int _width = 0;
  int _height = 0;
  int _x = 0;
  int _y = 0;
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
  VkPhysicalDeviceMemoryProperties _devmemprops;
  std::vector<VkExtensionProperties> _extensions;
  std::vector<VkMemoryHeap> _heaps;
  std::vector<VkQueueFamilyProperties> _queueprops;
  std::set<std::string> _extension_set;

  bool _is_discrete    = false;
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

struct VkMsaaState{
  VkMsaaState();
  VkPipelineMultisampleStateCreateInfo _VKSTATE;
  int _pipeline_bits = -1;
};

struct VkRasterState{
  VkRasterState(rasterstate_ptr_t rstate);
  VkPipelineRasterizationStateCreateInfo _VKRSCI;
  VkPipelineDepthStencilStateCreateInfo _VKDSSCI;
  VkPipelineColorBlendStateCreateInfo _VKCBSI;
  VkPipelineColorBlendAttachmentState _VKCBATT;
  int _pipeline_bits = -1;
};

///////////////////////////////////////////////////////////////////////////////

struct VkCommandBufferImpl{
  VkCommandBuffer _vkcmdbuf;
};

struct VulkanRenderPass{
  VulkanRenderPass(RenderPass* rpass);
  std::vector<RenderSubPass*> _toposorted_subpasses;
  VkRenderPass _vkrp;
};
struct VulkanRenderSubPass{

};

///////////////////////////////////////////////////////////////////////////////

struct VklRtBufferImpl {
  VklRtBufferImpl(VkRtGroupImpl* par, RtBuffer* rtb);
  VkRtGroupImpl* _parent = nullptr;
  RtBuffer* _rtb         = nullptr;
  bool _init             = true;
  VkImage _vkimg;
  VkMemoryPropertyFlags _vkmemflags;
  VkDeviceMemory _vkmem;
  VkFormat _vkfmt;
  VkImageView _vkimgview;
  VkAttachmentDescription _attachmentDesc;
  svar64_t _teximpl;
};

///////////////////////////////////////////////////////////////////////////////

struct VkRtGroupImpl {
  VkRtGroupImpl(RtGroup* _rtg);
  RtGroup* _rtg = nullptr;
  vkrtbufimpl_ptr_t _standard;
  vkrtbufimpl_ptr_t _depthonly;
  std::vector<VkImageView> _vkattachments;
  int _width  = 0;
  int _height = 0;
  VkFramebuffer _vkfb;
  int _pipeline_bits = -1;
  vkmsaastate_ptr_t _msaaState;
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

struct VulkanTextureObject {

  VulkanTextureObject(vktxi_ptr_t txi);
  ~VulkanTextureObject();

  // GLuint mObject;
  // GLuint mFbo;
  // GLuint mDbo;
  // GLenum mTarget;

  int _maxmip = 0;
  vktexasynctask_ptr_t _async;
  vktxi_ptr_t _txi;

  static std::atomic<size_t> _vkto_count;
};

///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderUniformSetItem {
  std::string _datatype;
  std::string _identifier;
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
  static size_t descriptor_set_counter;
  size_t _descriptor_set_id = 0;
  std::unordered_map<std::string, vkfxsunisetsamp_ptr_t> _samplers_by_name;
  std::unordered_map<std::string, vkfxsunisetitem_ptr_t> _items_by_name;
  std::vector<vkfxsunisetitem_ptr_t> _items_by_order;
};
struct VkFxShaderUniformSetsReference {
  static size_t descriptor_set_counter;
  uniset_map_t _unisets;
};
using vkfxsunisetsref_ptr_t = std::shared_ptr<VkFxShaderUniformSetsReference>;
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderPushConstantBlock {
  uniset_map_t _vtx_unisets;
  uniset_map_t _frg_unisets;

  uniset_item_map_t _vtx_items_by_name;
  uniset_item_map_t _frg_items_by_name;

  vkbufferlayout_ptr_t _vtx_layout;
  vkbufferlayout_ptr_t _frg_layout;

  std::vector<VkPushConstantRange> _ranges;
  size_t _blockSize = 0;
};

using descriptor_bindings_vect_t = std::vector<VkDescriptorSetLayoutBinding>;
//using descriptor_samplerinfos_vect_t = std::vector<VkSamplerCreateInfo>;
struct VkDescriptorSetBindings{

  descriptor_bindings_vect_t _vkbindings;
  std::vector<VkSampler> _vksamplers;
  VkDescriptorSetLayout _dsetlayout;
};

using vkdescriptors_ptr_t = std::shared_ptr<VkDescriptorSetBindings>;

///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderUniformBlkItem {
  std::string _datatype;
  std::string _identifier;
  std::shared_ptr<FxShaderParam> _orkparam;
};
struct VkFxShaderUniformBlk {
  static size_t descriptor_set_counter;
  size_t _descriptor_set_id = 0;
  std::shared_ptr<FxShaderParamBlock> _orkparamblock;
  std::unordered_map<std::string, vkfxsuniblkitem_ptr_t> _items_by_name;
  std::vector<vkfxsuniblkitem_ptr_t> _items_by_order;
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderFile {
  std::string _shader_name;
  //shadlang::SHAST::translationunit_ptr_t _trans_unit;
  std::unordered_map<std::string, vkfxsobj_ptr_t> _vk_shaderobjects;
  std::unordered_map<std::string, vkfxstek_ptr_t> _vk_techniques;
  std::unordered_map<std::string, vkfxsuniset_ptr_t> _vk_uniformsets;
  std::unordered_map<std::string, vkfxsuniblk_ptr_t> _vk_uniformblks;
};

struct VulkanFxShaderObject {

  VulkanFxShaderObject(vkcontext_rawptr_t ctx, vkfxshader_bin_t bin);
  ~VulkanFxShaderObject();

  vkcontext_rawptr_t _contextVK;
  vkfxshader_bin_t _spirv_binary;
  VkShaderModuleCreateInfo _vk_shadermoduleinfo;
  VkShaderModule _vk_shadermodule;
  VkPipelineShaderStageCreateInfo _shaderstageinfo;
  //shadlang::SHAST::astnode_ptr_t _astnode; // debug only
  vkfxsunisetsref_ptr_t _uniset_refs;
  std::unordered_map<std::string, vkfxsuniblk_ptr_t> _vk_uniformblks;
  uint64_t _STAGE = 0;
  VkPushConstantRange _vkpc_range;
};

struct VkParamSetItem{
  VkFxShaderUniformSetItem* _vk_param = nullptr;
  svar64_t _value;
};

struct VkFxShaderProgram {

  VkFxShaderProgram();

  vkfxsobj_ptr_t _vtxshader;
  vkfxsobj_ptr_t _geoshader;
  vkfxsobj_ptr_t _tctshader;
  vkfxsobj_ptr_t _tevshader;
  vkfxsobj_ptr_t _frgshader;
  vkfxsobj_ptr_t _comshader;
  vkfxpushconstantblk_ptr_t _pushConstantBlock;

  std::vector<VkParamSetItem> _pending_params;
  std::vector<uint8_t> _pushdatabuffer;
  vkdescriptors_ptr_t _descriptors;

  int _pipeline_bits = -1;

  std::unordered_map<std::string, vkfxsuniset_ptr_t> _vk_uniformsets;

};
struct VkPipelineObject{

  void applyPendingParams(vkcmdbufimpl_ptr_t cmdbuf);

  vkfxsprg_ptr_t _vk_program;
  VkGraphicsPipelineCreateInfo _VKGFXPCI;
  VkPipeline _pipeline;
  VkPipelineLayout _pipelineLayout;
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

struct VkVertexInputConfiguration{
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
  VkBufferCreateInfo _vkbufinfo;
  VkBuffer _vkbuf;
  VkMemoryPropertyFlags _vkmemflags;
  VkDeviceMemory _vkmem;
  vkcontext_rawptr_t _ctx;
  vkvertexinputconfig_ptr_t _vertexConfig;
};
struct VulkanIndexBuffer {
  VulkanIndexBuffer(vkcontext_rawptr_t ctx, size_t length);
  ~VulkanIndexBuffer();
  VkBufferCreateInfo _vkbufinfo;
  VkBuffer _vkbuf;
  VkMemoryPropertyFlags _vkmemflags;
  VkDeviceMemory _vkmem;
  vkcontext_rawptr_t _ctx;
};

///////////////////////////////////////////////////////////////////////////////

struct VkLoadContext {
  VkContext* _vkcontext     = nullptr;
  GLFWwindow* _pushedWindow = nullptr;
};

struct VkSwapChain{
  VkSwapchainKHR _vkSwapChain;
  std::vector<VkImage> _vkSwapChainImages;
  std::vector<VkImageView> _vkSwapChainImageViews;
  std::vector<VkFramebuffer> _vkFrameBuffers;
  std::vector<rtgroup_ptr_t> _rtgs;
  std::vector<rtbuffer_ptr_t> _color_rtbs;
  std::vector<rtbuffer_ptr_t> _depth_rtbs;
  //std::vector<VkImage>       _vkDepthImages;
  //std::vector<VkImageView>   _vkDepthImageViews;

  uint32_t _curSwapWriteImage = 0xffffffff;

  VkImage image();
  VkImageView imageView();
  VkFramebuffer framebuffer();

  int _width = 0;
  int _height = 0;
  VkExtent2D _extent;
  vkrenderpass_ptr_t _mainRenderPass;
};

void _vkCreateImageForBuffer(vkcontext_rawptr_t ctxVK, //
                             vkrtbufimpl_ptr_t bufferimpl,
                             uint64_t usage );

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

/*
struct VkRasterStateInterface final : public RasterStateInterface {

  VkRasterStateInterface(vkcontext_rawptr_t ctx);
  void BindRasterState(const SRasterState& rState, bool bForce) final;

  void SetZWriteMask(bool bv) final;
  void SetRGBAWriteMask(bool rgb, bool a) final;
  RGBAMask SetRGBAWriteMask(const RGBAMask& newmask) final;
  void SetBlending(Blending eVal) final;
  void SetDepthTest(EDepthTest eVal) final;
  void SetCullTest(ECullTest eVal) final;
  void setScissorTest(EScissorTest eVal) final;

  vkcontext_rawptr_t _contextVK;
};
*/
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

  vkvertexinputconfig_ptr_t _instantiateVertexConfig(EVtxStreamFormat format);

  void DrawPrimitiveEML(
      const VertexBufferBase& VBuf, //
      PrimitiveType eType,
      int ivbase,
      int ivcount) final;

#if defined(ENABLE_COMPUTE_SHADERS)

  void DrawPrimitiveEML(
      const FxShaderStorageBuffer* SSBO, //
      PrimitiveType eType = PrimitiveType::NONE,
      int ivbase          = 0,
      int ivcount         = 0) final;

#endif

  void
  DrawIndexedPrimitiveEML(const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, PrimitiveType eType, int ivbase, int ivcount)
      final;

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
  std::unordered_map<EVtxStreamFormat, vkvertexinputconfig_ptr_t> _vertexInputConfigs;
  std::unordered_map<uint64_t,vkprimclass_ptr_t> _primclasses;
};

///////////////////////////////////////////////////////////////////////////////

struct VkFrameBufferInterface final : public FrameBufferInterface {

  VkFrameBufferInterface(vkcontext_rawptr_t ctx);
  ~VkFrameBufferInterface();

  ///////////////////////////////////////////////////////

  void _clearColorAndDepth(const fcolor4& rCol, float fdepth) final;
  void _clearDepth(float fdepth) final;

  void capture(const RtBuffer* inpbuf, const file::Path& pth) final;
  bool captureToTexture(const CaptureBuffer& capbuf, Texture& tex) final;
  bool captureAsFormat(const RtBuffer* inpbuf, CaptureBuffer* buffer, EBufferFormat destfmt) final;

  void GetPixel(const fvec4& rAt, PixelFetchContext& ctx) final;

  void rtGroupClear(RtGroup* rtg) final;
  void rtGroupMipGen(RtGroup* rtg) final;
  void msaaBlit(rtgroup_ptr_t src, rtgroup_ptr_t dst) final;
  void blit(rtgroup_ptr_t src, rtgroup_ptr_t dst) final;
  void downsample2x2(rtgroup_ptr_t src, rtgroup_ptr_t dst) final;

  //////////////////////////////////////////////

  void _initializeContext(DisplayBuffer* pBuf);
  void _setViewport(int iX, int iY, int iW, int iH) final;
  void _setScissor(int iX, int iY, int iW, int iH) final;
  void _doBeginFrame(void) final;
  void _doEndFrame(void) final;
  void _pushRtGroup(RtGroup* Base) final;
  RtGroup* _popRtGroup() final;
  void _postPushRtGroup(RtGroup* Base);
  void _present();
  //////////////////////////////////////////////

  freestyle_mtl_ptr_t utilshader();

  vkrtgrpimpl_ptr_t _createRtGroupImpl(RtGroup* rtg);
  //void _doPushMainSurface() final;
  //void _doPopMainSurface() final;

  rtgroup_ptr_t _main_rtg;
  rtbuffer_ptr_t _main_rtb_color;
  rtbuffer_ptr_t _main_rtb_depth;

  RtGroup* _active_rtgroup = nullptr;
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
  void _acquireSwapChainForFrame();
  void _transitionSwapChainForClear();
  void _enq_transitionSwapChainForPresent();
  void _clearSwapChainBuffer();
  void _bindSwapChainToRenderPass(vkrenderpass_ptr_t rpass);

  //////////////////////////////////////////////

  vkswapchain_ptr_t _swapchain;
  std::unordered_set<vkswapchain_ptr_t> _old_swapchains;
  VkSemaphore _swapChainImageAcquiredSemaphore;
};

///////////////////////////////////////////////////////////////////////////////

struct VkTextureInterface final : public TextureInterface {

  VkTextureInterface(vkcontext_rawptr_t ctx);

  void TexManInit(void) final;

  bool _loadImageTexture(texture_ptr_t ptex, datablock_ptr_t inpdata);
  bool _loadXTXTexture(texture_ptr_t ptex, datablock_ptr_t inpdata);
  bool _loadDDSTexture(const AssetPath& fname, texture_ptr_t ptex);
  bool _loadDDSTexture(texture_ptr_t ptex, datablock_ptr_t inpdata);
  bool _loadVDSTexture(const AssetPath& fname, texture_ptr_t ptex);
  //
  void _loadXTXTextureMainThreadPart(vktexloadreq_ptr_t req);
  void _loadDDSTextureMainThreadPart(vktexloadreq_ptr_t req);
  //
  bool LoadTexture(texture_ptr_t ptex, datablock_ptr_t inpdata) final;
  bool destroyTexture(texture_ptr_t ptex) final;
  bool LoadTexture(const AssetPath& fname, texture_ptr_t ptex) final;
  void SaveTexture(const ork::AssetPath& fname, Texture* ptex) final;
  void ApplySamplingMode(Texture* ptex) final;
  void UpdateAnimatedTexture(Texture* ptex, TextureAnimationInst* tai) final;
  void initTextureFromData(Texture* ptex, TextureInitData tid) final;
  void generateMipMaps(Texture* ptex) final;
  Texture* createFromMipChain(MipChain* from_chain) final;

  // std::map<size_t, pbosetptr_t> _pbosets;
  vkcontext_rawptr_t _contextVK;
};

///////////////////////////////////////////////////////////////////////////////

struct VkFxInterface final : public FxInterface {

  VkFxInterface(vkcontext_rawptr_t ctx);
  ~VkFxInterface();

  void _doBeginFrame() final;
  void _doEndFrame() final;

  int BeginBlock(fxtechnique_constptr_t tek, const RenderContextInstData& data) final;
  bool BindPass(int ipass) final;
  void EndPass() final;
  void EndBlock() final;
  void CommitParams(void) final;
  void reset() final;

  const FxShaderTechnique* technique(FxShader* hfx, const std::string& name) final;
  const FxShaderParam* parameter(FxShader* hfx, const std::string& name) final;
  const FxShaderParamBlock* parameterBlock(FxShader* hfx, const std::string& name) final;

#if defined(ENABLE_COMPUTE_SHADERS)
  const FxComputeShader* computeShader(FxShader* hfx, const std::string& name) final;
  const FxShaderStorageBlock* storageBlock(FxShader* hfx, const std::string& name) final;
#endif

  void BindParamBool(const FxShaderParam* hpar, const bool bval) final;
  void BindParamInt(const FxShaderParam* hpar, const int ival) final;
  void BindParamVect2(const FxShaderParam* hpar, const fvec2& Vec) final;
  void BindParamVect3(const FxShaderParam* hpar, const fvec3& Vec) final;
  void BindParamVect4(const FxShaderParam* hpar, const fvec4& Vec) final;
  void BindParamVect2Array(const FxShaderParam* hpar, const fvec2* Vec, const int icount) final;
  void BindParamVect3Array(const FxShaderParam* hpar, const fvec3* Vec, const int icount) final;
  void BindParamVect4Array(const FxShaderParam* hpar, const fvec4* Vec, const int icount) final;
  void BindParamFloatArray(const FxShaderParam* hpar, const float* pfA, const int icnt) final;
  void BindParamFloat(const FxShaderParam* hpar, float fA) final;
  void BindParamMatrix(const FxShaderParam* hpar, const fmtx4& Mat) final;
  void BindParamMatrix(const FxShaderParam* hpar, const fmtx3& Mat) final;
  void BindParamMatrixArray(const FxShaderParam* hpar, const fmtx4* MatArray, int iCount) final;
  void BindParamU32(const FxShaderParam* hpar, uint32_t uval) final;
  void BindParamCTex(const FxShaderParam* hpar, const Texture* pTex) final;
  void BindParamU64(const FxShaderParam* hpar, uint64_t uval) final;

  bool LoadFxShader(const AssetPath& pth, FxShader* ptex) final;
  FxShader* shaderFromShaderText(const std::string& name, const std::string& shadertext) final;

  datablock_ptr_t _writeIntermediateToDataBlock(shadlang::SHAST::transunit_ptr_t tunit);
  vkfxsfile_ptr_t _readFromDataBlock(datablock_ptr_t inpdata,FxShader* shader);
  vkfxsfile_ptr_t _loadShaderFromShaderText(
      FxShader* shader,               //
      const std::string& parser_name, //
      const std::string& shadertext);

  vkpipeline_obj_ptr_t _fetchPipeline(vkvtxbuf_ptr_t vb, vkprimclass_ptr_t primclas);

  // ubo
  FxShaderParamBuffer* createParamBuffer(size_t length) final;
  parambuffermappingptr_t mapParamBuffer(FxShaderParamBuffer* b, size_t base, size_t length) final;
  void unmapParamBuffer(FxShaderParamBufferMapping* mapping) final;
  void bindParamBlockBuffer(const FxShaderParamBlock* block, FxShaderParamBuffer* buffer) final;

  void _doPushRasterState(rasterstate_ptr_t rs) final;
  rasterstate_ptr_t _doPopRasterState() final;

  void _bindPipeline(vkpipeline_obj_ptr_t pipe );

  fxtechnique_constptr_t _currentORKTEK = nullptr;
  VkFxShaderTechnique* _currentVKTEK;
  vkfxspass_ptr_t _currentVKPASS;
  vkcontext_rawptr_t _contextVK;
  std::map<AssetPath, vkfxsfile_ptr_t> _fxshaderfiles;
  std::unordered_map<uint64_t,vkpipeline_obj_ptr_t> _pipelines;
  shadlang::slpcache_ptr_t _slp_cache;
  std::stack<rasterstate_ptr_t> _rasterstate_stack;
  rasterstate_ptr_t _current_rasterstate;
  lev2::rasterstate_ptr_t _default_rasterstate;
  vkpipeline_obj_ptr_t _currentPipeline;

};

///////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_COMPUTE_SHADERS)

struct VkComputeInterface : public ComputeInterface {

  VkComputeInterface(vkcontext_rawptr_t ctx);

  void dispatchCompute(const FxComputeShader* shader, uint32_t numgroups_x, uint32_t numgroups_y, uint32_t numgroups_z) final;

  void dispatchComputeIndirect(const FxComputeShader* shader, int32_t* indirect) final;

  FxShaderStorageBuffer* createStorageBuffer(size_t length) final;
  storagebuffermappingptr_t mapStorageBuffer(FxShaderStorageBuffer* b, size_t base = 0, size_t length = 0) final;
  void unmapStorageBuffer(FxShaderStorageBufferMapping* mapping) final;
  void bindStorageBuffer(const FxComputeShader* shader, uint32_t binding_index, FxShaderStorageBuffer* buffer) final;
  void bindImage(const FxComputeShader* shader, uint32_t binding_index, Texture* tex, ImageBindAccess access) final;

  // PipelineCompute* createComputePipe(ComputeShader* csh);
  // void bindComputeShader(ComputeShader* csh);

  // PipelineCompute* _currentComputePipeline = nullptr;
  vkcontext_rawptr_t _contextVK;
  vkfxi_ptr_t _fxi;
};

#endif
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

struct VkContext : public Context {

  DeclareAbstractX(VkContext, Context);

private:
  VkContext();

public:
  static vkcontext_ptr_t makeShared();
  static bool HaveExtension(const std::string& extname);
  static const CClass* gpClass;
  // static orkvector<std::string> gVKExtensions;
  // static orkset<std::string> gVKExtensionSet;

  ///////////////////////////////////////////////////////////////////////

  ~VkContext();

  void FxInit();

  ///////////////////////////////////////////////////////////////////////

  void _doResizeMainSurface(int iw, int ih) final;
  void _doBeginFrame() final;
  void _doEndFrame() final;
  ctx_platform_handle_t _doClonePlatformHandle() const final;


  //////////////////////////////////////////////

  commandbuffer_ptr_t _beginRecordCommandBuffer() final;
  void _endRecordCommandBuffer(commandbuffer_ptr_t cmdbuf) final;
  void _beginRenderPass(renderpass_ptr_t) final;
  void _endRenderPass(renderpass_ptr_t) final;
  void _beginSubPass(rendersubpass_ptr_t) final;
  void _endSubPass(rendersubpass_ptr_t) final;

  void _beginExecuteSubPass(rendersubpass_ptr_t);
  void _endExecuteSubPass(rendersubpass_ptr_t);

  //////////////////////////////////////////////
  // Interfaces

  FxInterface* FXI() final;
  ImmInterface* IMI() final;
  //RasterStateInterface* RSI() final;
  MatrixStackInterface* MTXI() final;
  GeometryBufferInterface* GBI() final;
  FrameBufferInterface* FBI() final;
  TextureInterface* TXI() final;
#if defined(ENABLE_COMPUTE_SHADERS)
  ComputeInterface* CI() final;
#endif
  DrawingInterface* DWI() final;

  ///////////////////////////////////////////////////////////////////////

  void makeCurrentContext(void) final;
  void present(CTXBASE* ctxbase) final;

  void initializeWindowContext(Window* pWin, CTXBASE* pctxbase) final; // make a window
  void initializeOffscreenContext(DisplayBuffer* pBuf) final;          // make a pbuffer
  void initializeLoaderContext() final;

  void debugPushGroup(const std::string str) final;
  void debugPopGroup() final;
  void debugMarker(const std::string str) final;

  void TakeThreadOwnership() final;
  bool SetDisplayMode(DisplayMode* mode) final;
  load_token_t _doBeginLoad() final;
  void _doEndLoad(load_token_t ploadtok) final; // virtual

  vkswapchaincaps_ptr_t _swapChainCapsForSurface(VkSurfaceKHR surface);

  //////////////////////////////////////////////

  uint32_t _findMemoryType( //
      uint32_t typeFilter,  //
      VkMemoryPropertyFlags properties);

  //////////////////////////////////////////////
  void _initVulkanForDevInfo(vkdeviceinfo_ptr_t devinfo );
  void _initVulkanForWindow(VkSurfaceKHR surface);
  void _initVulkanForOffscreen(DisplayBuffer* pBuf);
  void _initVulkanCommon();
  //////////////////////////////////////////////
  void _doPushCommandBuffer(commandbuffer_ptr_t cmdbuf, rtgroup_ptr_t rtg ) final;
  void _doPopCommandBuffer() final;
  void _doEnqueueSecondaryCommandBuffer(commandbuffer_ptr_t cmdbuf) final;
  //////////////////////////////////////////////
  template <typename T>
  void _setObjectDebugName(T& object, VkObjectType objectType, const char* name) {
    if(_vkSetDebugUtilsObjectName){
      VkDebugUtilsObjectNameInfoEXT nameInfo = {};
      initializeVkStruct(nameInfo,VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT);
      nameInfo.objectType = objectType;
      nameInfo.objectHandle = reinterpret_cast<uint64_t>(object);
      nameInfo.pObjectName = name;
      _vkSetDebugUtilsObjectName(_vkdevice, &nameInfo);
    }
  }
  //////////////////////////////////////////////
  template <typename T> bool _fetchDeviceProcAddr(T& object, const char* name) {
    object = reinterpret_cast<T>(vkGetDeviceProcAddr(_vkdevice, name));
    return (object!=nullptr);
  }
  //////////////////////////////////////////////
  VkDevice _vkdevice;
  VkPhysicalDevice _vkphysicaldevice;
  vkdeviceinfo_ptr_t _vkdeviceinfo;
  VkSurfaceKHR _vkpresentationsurface;
  vkswapchaincaps_ptr_t _vkpresentation_caps;
  std::vector<const char*> _device_extensions;
  VkSemaphore _renderingCompleteSemaphore;
  VkFence _mainGfxSubmitFence;
  size_t _num_queue_types = 0;
  int _renderpass_index;
  int _subpass_index = 0;
  //////////////////////////////////////////////

  std::vector<float> _queuePriorities;
  std::vector<VkDeviceQueueCreateInfo> _DQCIs;
  static constexpr uint32_t NO_QUEUE = 0xffffffff;
  uint32_t _vkqfid_graphics             = NO_QUEUE;
  uint32_t _vkqfid_compute              = NO_QUEUE;
  uint32_t _vkqfid_transfer             = NO_QUEUE;
  VkQueue  _vkqueue_graphics;
  VkCommandPool _vkcmdpool_graphics;

  vkcmdbufimpl_ptr_t _cmdbufcurframe_gfx_pri;
  vkcmdbufimpl_ptr_t _cmdbufcur_gfx;
  vkcmdbufimpl_ptr_t _cmdbufprv_gfx;

  vkcmdbufimpl_ptr_t primary_cb();

  //////////////////////////////////////////////
  PFN_vkSetDebugUtilsObjectNameEXT _vkSetDebugUtilsObjectName;
  PFN_vkCmdDebugMarkerBeginEXT _vkCmdDebugMarkerBeginEXT;
  PFN_vkCmdDebugMarkerEndEXT _vkCmdDebugMarkerEndEXT;
  PFN_vkCmdDebugMarkerInsertEXT _vkCmdDebugMarkerInsertEXT;
  //////////////////////////////////////////////
  void* mhHWND;
  vkcontext_ptr_t _parentTarget;
  std::stack<void*> mDCStack;
  std::stack<void*> mGLRCStack;
  EDepthTest meCurDepthTest;
  bool mTargetDrawableSizeDirty;
  bool _first_frame = true;
  //////////////////////////////////////////////

  vkdwi_ptr_t _dwi;
  vkimi_ptr_t _imi;
  //vkrsi_ptr_t _rsi;
  vkmsi_ptr_t _msi;
  vkfbi_ptr_t _fbi;
  vkgbi_ptr_t _gbi;
  vktxi_ptr_t _txi;
  vkfxi_ptr_t _fxi;

#if defined(ENABLE_COMPUTE_SHADERS)
  vkci_ptr_t _ci;
#endif
};

///////////////////////////////////////////////////////////////////////////

 void _imageBarrier(VkCommandBuffer cmdbuf, //
                    VkImage image, //
                    VkAccessFlags srcAccessMask, //
                    VkAccessFlags dstAccessMask, //
                    VkPipelineStageFlags srcStageMask, //
                    VkPipelineStageFlags dstStageMask, //
                    VkImageLayout oldLayout, //
                    VkImageLayout newLayout);

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
