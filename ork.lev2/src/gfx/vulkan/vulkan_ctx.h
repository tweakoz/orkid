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

#if defined(__APPLE__)
#define VK_USE_PLATFORM_MACOS_MVK
#endif

#include <vulkan/vulkan.hpp>

///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/svariant.h>
#include <ork/kernel/concurrent_queue.h>
#include <ork/kernel/datablock.h>
#include <ork/kernel/datacache.h>
#include <ork/file/chunkfile.inl>

///////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/shadlang.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/shadlang.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::dds {
  struct DDS_HEADER;
}

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
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
#if defined(ENABLE_COMPUTE_SHADERS)
struct VkComputeInterface;
#endif 
//
struct VkTextureObject;
struct VkFxShaderObject;
struct VkFxShaderFile;
struct VkFxShaderProgram;
struct VkFxShaderPass;
struct VkFxShaderTechnique;
struct VkFxShaderUniformSet;
struct VkFxShaderUniformSetItem;
struct VkFxShaderUniformSetSampler;
struct VkFxShaderUniformBlk;
struct VkFxShaderUniformBlkItem;
struct VkFboObject;
struct VklRtBufferImpl;
struct VkRtGroupImpl;
struct VkTextureAsyncTask;
struct VkTexLoadReq;
//
using vkinstance_ptr_t = std::shared_ptr<VulkanInstance>;
using vkdeviceinfo_ptr_t = std::shared_ptr<VulkanDeviceInfo>;
using vkdevgrp_ptr_t = std::shared_ptr<VulkanDeviceGroup>;
using vkcontext_ptr_t = std::shared_ptr<VkContext>;
using vkcontext_rawptr_t = VkContext*;
//
using vkdwi_ptr_t = std::shared_ptr<VkDrawingInterface>;
using vkimi_ptr_t = std::shared_ptr<VkImiInterface>;
using vkrsi_ptr_t = std::shared_ptr<VkRasterStateInterface>;
using vkmsi_ptr_t = std::shared_ptr<VkMatrixStackInterface>;
using vkfbi_ptr_t = std::shared_ptr<VkFrameBufferInterface>;
using vkgbi_ptr_t = std::shared_ptr<VkGeometryBufferInterface>;
using vktxi_ptr_t = std::shared_ptr<VkTextureInterface>;
using vkfxi_ptr_t = std::shared_ptr<VkFxInterface>;
#if defined(ENABLE_COMPUTE_SHADERS)
using vkci_ptr_t = std::shared_ptr<VkComputeInterface>;
#endif 
//
using vktexobj_ptr_t = std::shared_ptr<VkTextureObject>;
using vkfxsfile_ptr_t = std::shared_ptr<VkFxShaderFile>;
using vkfxsobj_ptr_t = std::shared_ptr<VkFxShaderObject>;
using vkfxsprg_ptr_t = std::shared_ptr<VkFxShaderProgram>;
using vkfxspass_ptr_t = std::shared_ptr<VkFxShaderPass>;
using vkfxstek_ptr_t = std::shared_ptr<VkFxShaderTechnique>;

using vkfxsuniset_ptr_t = std::shared_ptr<VkFxShaderUniformSet>;
using vkfxsunisetitem_ptr_t = std::shared_ptr<VkFxShaderUniformSetItem>;
using vkfxsunisetsamp_ptr_t = std::shared_ptr<VkFxShaderUniformSetSampler>;

using vkfxsuniblk_ptr_t = std::shared_ptr<VkFxShaderUniformBlk>;
using vkfxsuniblkitem_ptr_t = std::shared_ptr<VkFxShaderUniformBlkItem>;

using vkfbobj_ptr_t = std::shared_ptr<VkFboObject>;
using vkrtbufimpl_ptr_t = std::shared_ptr<VklRtBufferImpl>;
using vkrtgrpimpl_ptr_t = std::shared_ptr<VkRtGroupImpl>;
using vktexasynctask_ptr_t = std::shared_ptr<VkTextureAsyncTask>;
using vktexloadreq_ptr_t = std::shared_ptr<VkTexLoadReq>;
using vkfxshader_bin_t = std::vector<uint32_t>;

extern vkinstance_ptr_t _GVI;

///////////////////////////////////////////////////////////////////////////////

struct VulkanDeviceInfo{

  VkPhysicalDevice _phydev;
  VkPhysicalDeviceProperties _devprops;
  VkPhysicalDeviceFeatures _devfeatures;
  VkPhysicalDeviceMemoryProperties _devmemprops;
  std::vector<VkExtensionProperties> _extensions;
  std::vector<VkMemoryHeap> _heaps;
  std::vector<VkQueueFamilyProperties> _queueprops;
  std::set<std::string> _extension_set;

  bool _is_discrete = false;
  size_t _maxWkgCountX = 0;
  size_t _maxWkgCountY = 0;
  size_t _maxWkgCountZ = 0;

};

///////////////////////////////////////////////////////////////////////////////

struct VulkanDeviceGroup{
  size_t _deviceCount = 0;
  std::vector<vkdeviceinfo_ptr_t> _device_infos;

};

///////////////////////////////////////////////////////////////////////////////

struct VulkanInstance{

  VulkanInstance();
  ~VulkanInstance();

  VkApplicationInfo _appdata;
  VkInstanceCreateInfo _instancedata;
  VkInstance _instance;
  std::vector<VkPhysicalDeviceGroupProperties> _phygroups;
  std::vector<vkdevgrp_ptr_t> _devgroups;
  std::vector<vkdeviceinfo_ptr_t> _device_infos;
  uint32_t _numgpus = 0;
  uint32_t _numgroups = 0;

};

///////////////////////////////////////////////////////////////////////////////

struct VkFboObject {
  static const int kmaxrt = RtGroup::kmaxmrts;
  VkFboObject();
};

///////////////////////////////////////////////////////////////////////////////

struct VklRtBufferImpl {
  svar64_t _teximpl;
  bool _init                = true;
};

///////////////////////////////////////////////////////////////////////////////

struct VkRtGroupImpl {
  vkfbobj_ptr_t _standard;
  vkfbobj_ptr_t _depthonly;
};

///////////////////////////////////////////////////////////////////////////////

struct VkTextureAsyncTask{
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

struct VkTextureObject {

  VkTextureObject(vktxi_ptr_t txi);
  ~VkTextureObject();

  //GLuint mObject;
  //GLuint mFbo;
  //GLuint mDbo;
  //GLenum mTarget;
  
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
struct VkFxShaderUniformSetSampler {
  size_t _binding_id = -1;
  std::string _datatype;
  std::string _identifier;
  std::shared_ptr<FxShaderParam> _orkparam;
};

struct VkFxShaderUniformSet {
  static size_t descriptor_set_counter;
  size_t _descriptor_set_id = 0;
  std::unordered_map<std::string, vkfxsunisetsamp_ptr_t> _samplers_by_name;
  std::unordered_map<std::string, vkfxsunisetitem_ptr_t> _items_by_name;
  std::vector<vkfxsunisetitem_ptr_t> _items_by_order;
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderUniformBlkItem {
  std::string _datatype;
  std::string _identifier;
  std::shared_ptr<FxShaderParam> _orkparam;
};
struct VkFxShaderUniformBlk {
  static size_t descriptor_set_counter;
  size_t _descriptor_set_id = 0;
  std::unordered_map<std::string, vkfxsuniblkitem_ptr_t> _items_by_name;
  std::vector<vkfxsuniblkitem_ptr_t> _items_by_order;
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderFile {
  std::string _shader_name;
  shadlang::SHAST::translationunit_ptr_t _trans_unit;
  std::unordered_map<std::string, vkfxsobj_ptr_t> _vk_shaderobjects;
  std::unordered_map<std::string, vkfxstek_ptr_t> _vk_techniques;
  std::unordered_map<std::string, vkfxsuniset_ptr_t> _vk_uniformsets;
  std::unordered_map<std::string, vkfxsuniblk_ptr_t> _vk_uniformblks;
};

struct VkFxShaderObject {

  VkFxShaderObject(vkcontext_rawptr_t ctx, vkfxshader_bin_t bin);
  ~VkFxShaderObject();

  vkcontext_rawptr_t _contextVK;
  vkfxshader_bin_t _spirv_binary;
  VkShaderModuleCreateInfo _vk_shadermoduleinfo;
  VkShaderModule _vk_shadermodule;
  VkPipelineShaderStageCreateInfo _shaderstageinfo;
  shadlang::SHAST::astnode_ptr_t _astnode; // debug only
  std::unordered_map<std::string, vkfxsuniset_ptr_t> _vk_uniformsets;
  std::unordered_map<std::string, vkfxsuniblk_ptr_t> _vk_uniformblks;
  uint64_t _STAGE = 0;

};

struct VkFxShaderProgram {
  vkfxsobj_ptr_t _vtxshader;
  vkfxsobj_ptr_t _geoshader;
  vkfxsobj_ptr_t _tctshader;
  vkfxsobj_ptr_t _tevshader;
  vkfxsobj_ptr_t _frgshader;
  vkfxsobj_ptr_t _comshader;
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

  bool BindStreamSources(const VertexBufferBase& VBuf, const IndexBufferBase& IBuf);
  bool BindVertexStreamSource(const VertexBufferBase& VBuf);
  void BindVertexDeclaration(EVtxStreamFormat efmt);

  void DrawPrimitiveEML(
      const VertexBufferBase& VBuf, //
      PrimitiveType eType,
      int ivbase,
      int ivcount) final;

#if defined(ENABLE_COMPUTE_SHADERS)

  void DrawPrimitiveEML(
      const FxShaderStorageBuffer* SSBO, //
      PrimitiveType eType = PrimitiveType::NONE,
      int ivbase           = 0,
      int ivcount          = 0) final;

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
};

///////////////////////////////////////////////////////////////////////////////

struct VkFrameBufferInterface final : public FrameBufferInterface {

  VkFrameBufferInterface(vkcontext_rawptr_t ctx);
  ~VkFrameBufferInterface();

  ///////////////////////////////////////////////////////

  void SetRtGroup(RtGroup* Base) final;
  void Clear(const fcolor4& rCol, float fdepth) final;
  void clearDepth(float fdepth) final;
  void _setViewport(int iX, int iY, int iW, int iH) final;
  void _setScissor(int iX, int iY, int iW, int iH) final;
  void _doBeginFrame(void) final;
  void _doEndFrame(void) final;

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

  void _setAsRenderTarget();
  void _initializeContext(DisplayBuffer* pBuf);

  freestyle_mtl_ptr_t utilshader();

protected:

  freestyle_mtl_ptr_t _freestyle_mtl;
  const FxShaderTechnique* _tek_downsample2x2 = nullptr;
  const FxShaderTechnique* _tek_blit = nullptr;
  const FxShaderParam*     _fxpMVP = nullptr;
  const FxShaderParam*     _fxpColorMap = nullptr;

  vkcontext_rawptr_t _contextVK;
  int miCurScissorX;
  int miCurScissorY;
  int miCurScissorW;
  int miCurScissorH;
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

  //std::map<size_t, pbosetptr_t> _pbosets;
  vkcontext_rawptr_t _contextVK;
};

///////////////////////////////////////////////////////////////////////////////

struct VkFxInterface final : public FxInterface {

  VkFxInterface(vkcontext_rawptr_t ctx);
  ~VkFxInterface();

  void _doBeginFrame() final;

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

  vkfxsfile_ptr_t _loadShaderFromShaderText(FxShader* shader, //
                                            const std::string& parser_name, //
                                            const std::string& shadertext);

  // ubo
  FxShaderParamBuffer* createParamBuffer(size_t length) final;
  parambuffermappingptr_t mapParamBuffer(FxShaderParamBuffer* b, size_t base, size_t length) final;
  void unmapParamBuffer(FxShaderParamBufferMapping* mapping) final;
  void bindParamBlockBuffer(const FxShaderParamBlock* block, FxShaderParamBuffer* buffer) final;

  FxShaderTechnique* _currentTEK = nullptr;
  vkcontext_rawptr_t _contextVK;
  std::map<AssetPath, vkfxsfile_ptr_t> _fxshaderfiles;

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

  //PipelineCompute* createComputePipe(ComputeShader* csh);
  //void bindComputeShader(ComputeShader* csh);

  //PipelineCompute* _currentComputePipeline = nullptr;
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
  static ork::MpMcBoundedQueue<void*> _loadTokens;
  //static orkvector<std::string> gVKExtensions;
  //static orkset<std::string> gVKExtensionSet;

  ///////////////////////////////////////////////////////////////////////

  ~VkContext();

  void FxInit();

  ///////////////////////////////////////////////////////////////////////

  void _doResizeMainSurface(int iw, int ih) final;
  void _doBeginFrame() final;
  void _doEndFrame() final;
  void* _doClonePlatformHandle() const final;

  //////////////////////////////////////////////
  // Interfaces

  FxInterface* FXI() final;
  ImmInterface* IMI() final;
  RasterStateInterface* RSI() final;
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
  void swapBuffers(CTXBASE* ctxbase) final;

  void initializeWindowContext(Window* pWin, CTXBASE* pctxbase) final; // make a window
  void initializeOffscreenContext(DisplayBuffer* pBuf) final;        // make a pbuffer
  void initializeLoaderContext() final;

  void debugPushGroup(const std::string str) final;
  void debugPopGroup() final;
  void debugMarker(const std::string str) final;

  void TakeThreadOwnership() final;
  bool SetDisplayMode(DisplayMode* mode) final;
  void* _doBeginLoad() final;
  void _doEndLoad(void* ploadtok) final; // virtual

  //////////////////////////////////////////////
  VkDevice _vkdevice;
  //////////////////////////////////////////////
  void* mhHWND;
  vkcontext_ptr_t _parentTarget;
  std::stack<void*> mDCStack;
  std::stack<void*> mGLRCStack;
  EDepthTest meCurDepthTest;
  bool mTargetDrawableSizeDirty;

  //////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////

  vkdwi_ptr_t _dwi;
  vkimi_ptr_t _imi;
  vkrsi_ptr_t _rsi;
  vkmsi_ptr_t _msi;
  vkfbi_ptr_t _fbi;
  vkgbi_ptr_t _gbi;
  vktxi_ptr_t _txi;
  vkfxi_ptr_t _fxi;

#if defined(ENABLE_COMPUTE_SHADERS)
  vkci_ptr_t _ci;
#endif

};

extern vkinstance_ptr_t _GVI;

} //namespace ork::lev2::vulkan {
