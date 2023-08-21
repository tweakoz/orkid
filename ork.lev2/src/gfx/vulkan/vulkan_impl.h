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

#include <ork/kernel/svariant.h>
#include <ork/kernel/concurrent_queue.h>
#include <ork/kernel/datablock.h>
#include <ork/file/chunkfile.inl>

///////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/shadlang.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/image.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::dds {
  struct DDS_HEADER;
}

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

struct VkContext;
struct VkFxInterface;
struct VkTextureInterface;
//
struct VkTextureObject;
struct VkFboObject;
struct VklRtBufferImpl;
struct VkRtGroupImpl;
struct VkTextureAsyncTask;
struct VkTexLoadReq;
//
using vkcontext_ptr_t = std::shared_ptr<VkContext>;
using vkfxi_ptr_t = std::shared_ptr<VkFxInterface>;
using vktxi_ptr_t = std::shared_ptr<VkTextureInterface>;
//
using vktexobj_ptr_t = std::shared_ptr<VkTextureObject>;
using vkfbobj_ptr_t = std::shared_ptr<VkFboObject>;
using vkrtbufimpl_ptr_t = std::shared_ptr<VklRtBufferImpl>;
using vkrtgrpimpl_ptr_t = std::shared_ptr<VkRtGroupImpl>;
using vktexasynctask_ptr_t = std::shared_ptr<VkTextureAsyncTask>;
using vktexloadreq_ptr_t = std::shared_ptr<VkTexLoadReq>;

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

struct VkDrawingInterface final : public DrawingInterface {
  VkDrawingInterface(vkcontext_ptr_t ctx);
  vkcontext_ptr_t _contextVK;
};

///////////////////////////////////////////////////////////////////////////////

struct VkImiInterface final : public ImmInterface {
  VkImiInterface(vkcontext_ptr_t ctx);
  void _doBeginFrame() final;
  void _doEndFrame() final;
  vkcontext_ptr_t _contextVK;
};

///////////////////////////////////////////////////////////////////////////////

struct VkRasterStateInterface final : public RasterStateInterface {

  VkRasterStateInterface(vkcontext_ptr_t ctx);
  void BindRasterState(const SRasterState& rState, bool bForce) final;

  void SetZWriteMask(bool bv) final;
  void SetRGBAWriteMask(bool rgb, bool a) final;
  RGBAMask SetRGBAWriteMask(const RGBAMask& newmask) final;
  void SetBlending(Blending eVal) final;
  void SetDepthTest(EDepthTest eVal) final;
  void SetCullTest(ECullTest eVal) final;
  void setScissorTest(EScissorTest eVal) final;

  vkcontext_ptr_t _contextVK;
};

///////////////////////////////////////////////////////////////////////////////

struct VkMatrixStackInterface final : public MatrixStackInterface {

  VkMatrixStackInterface(vkcontext_ptr_t ctx);

  fmtx4 Ortho(float left, float right, float top, float bottom, float fnear, float ffar); // virtual
  fmtx4 Frustum(float left, float right, float top, float bottom, float zn, float zf);    // virtual

  vkcontext_ptr_t _contextVK;
};

///////////////////////////////////////////////////////////////////////////////

struct VkGeometryBufferInterface final : public GeometryBufferInterface {

  VkGeometryBufferInterface(vkcontext_ptr_t ctx);

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

  vkcontext_ptr_t _contextVK;
  uint32_t _lastComponentMask = 0xFFFFFFFF;
};

///////////////////////////////////////////////////////////////////////////////

struct VkFrameBufferInterface final : public FrameBufferInterface {

  VkFrameBufferInterface(vkcontext_ptr_t ctx);
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
  bool captureToTexture(const CaptureBuffer& capbuf, Texture& tex) final {
    return false;
  }
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

  vkcontext_ptr_t _contextVK;
  int miCurScissorX;
  int miCurScissorY;
  int miCurScissorW;
  int miCurScissorH;
};

///////////////////////////////////////////////////////////////////////////////

struct VkTextureInterface final : public TextureInterface {

  VkTextureInterface(vkcontext_ptr_t ctx);

  void TexManInit(void) final;

  //pboptr_t _getPBO(size_t isize);
  //void _returnPBO(pboptr_t pbo);

  //void bindTextureToUnit(const Texture* tex, GLenum tex_target, int tex_unit);

private:
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
  vkcontext_ptr_t _contextVK;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

struct VkContext : public Context {

  DeclareConcreteX(VkContext, Context);
public:

  static const CClass* gpClass;

  ///////////////////////////////////////////////////////////////////////

  VkContext();
  ~VkContext();

  void FxInit();

  ///////////////////////////////////////////////////////////////////////

  void _doResizeMainSurface(int iw, int ih) final;
  void _doBeginFrame(void) final {
  }
  void _doEndFrame(void) final {
  }
  void* _doClonePlatformHandle() const final;

  //////////////////////////////////////////////
  // Interfaces

  FxInterface* FXI() final {
    return nullptr;
  }
  ImmInterface* IMI() final {
    return nullptr;
  }
  RasterStateInterface* RSI() final {
    return nullptr;
  }
  MatrixStackInterface* MTXI() final {
    return nullptr;
  }
  GeometryBufferInterface* GBI() final {
    return nullptr;
  }
  FrameBufferInterface* FBI() final {
    return nullptr;
  }
  TextureInterface* TXI() final {
    return nullptr;
  }
#if defined(ENABLE_COMPUTE_SHADERS)
  ComputeInterface* CI() final {
    return nullptr;
  };
#endif
  DrawingInterface* DWI() final {
    return nullptr;
  }
  //GlFrameBufferInterface& GLFBI() {
    //return mFbI;
  //}

  ///////////////////////////////////////////////////////////////////////

  void makeCurrentContext(void) final;

  //void debugLabel(GLenum target, GLuint object, std::string name);

  //////////////////////////////////////////////

  static void VKinit();
  static bool HaveExtension(const std::string& extname);

  //////////////////////////////////////////////

  //void AttachGLContext(CTXBASE* pCTFL);
  //void SwapGLContext(CTXBASE* pCTFL);

  

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

  void* mhHWND;
  void* mGLXContext;
  vkcontext_ptr_t _parentTarget;

  std::stack<void*> mDCStack;
  std::stack<void*> mGLRCStack;

  //////////////////////////////////////////////

  static ork::MpMcBoundedQueue<void*> _loadTokens;

  ///////////////////////////////////////////////////////////////////////////
  // Rendering State Info

  EDepthTest meCurDepthTest;

  ////////////////////////////////////////////////////////////////////
  // Rendering Path Variables

  static orkvector<std::string> gGLExtensions;
  static orkset<std::string> gGLExtensionSet;

  ///////////////////////////////////////////////////////////////////////////

  //GlImiInterface mImI;
  //glslfx::Interface mFxI;
  //GlRasterStateInterface mRsI;
  //GlMatrixStackInterface mMtxI;
  //GlGeometryBufferInterface mGbI;
  //GlFrameBufferInterface mFbI;
  //GlTextureInterface mTxI;
  //GlDrawingInterface mDWI;

#if defined(ENABLE_COMPUTE_SHADERS)
  //glslfx::ComputeInterface mCI;
#endif

  bool mTargetDrawableSizeDirty;
};

} //namespace ork::lev2::vulkan {
