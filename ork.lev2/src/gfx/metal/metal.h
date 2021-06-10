////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once
#if defined(__APPLE__)
///////////////////////////////////////////////////////////////////////////////
#include <functional>
#include <map>
#include <ork/kernel/svariant.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <set>
#include <string>
#include <vector>
///////////////////////////////////////////////////////////////////////////////
#include "orksl/fxi_metal.h"
/////////////////////////////

#include <ork/kernel/concurrent_queue.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/file/chunkfile.inl>
#include <ork/kernel/datablock.h>
#include <ork/lev2/gfx/image.h>

#include <mtlpp.hpp>

///////////////////////////////////////////////////////////////////////////////

namespace ork::dds {
struct DDS_HEADER;
}

namespace ork::lev2::metal {

class ContextMetal;
class MetalFxInterface;
struct MetalTextureObject;

struct MetalFboObject {
  static const int kmaxrt = RtGroup::kmaxmrts;
  //GLuint mFBOMaster;
  //GLuint mDSBO;
  //GLuint _depthTexture;
  MetalFboObject();
};

struct MetalRtBufferImpl {
  //GLuint _texture           = 0;
  MetalTextureObject* _teximpl = nullptr;
  bool _init                = true;
};

//////////////////////////////////////////////////////////////////////

class MetalTextureInterface;

struct MetalDrawingInterface : public DrawingInterface {
  MetalDrawingInterface(ContextMetal& ctx);
};

///////////////////////////////////////////////////////////////////////////////

class MetalImiInterface : public ImmInterface {
  virtual void DrawLine(const fvec4& From, const fvec4& To);
  virtual void DrawPoint(F32 fx, F32 fy, F32 fz);
  virtual void DrawPrim(const fvec4* Points, int inumpoints, PrimitiveType eType);
  virtual void _doBeginFrame() {
  }
  virtual void _doEndFrame() {
  }

public:
  MetalImiInterface(ContextMetal& target);
};

///////////////////////////////////////////////////////////////////////////////

struct MetalRasterStateInterface : public RasterStateInterface {

  MetalRasterStateInterface(Context& target);
  void BindRasterState(const SRasterState& rState, bool bForce) override;

  void SetZWriteMask(bool bv) override;
  void SetRGBAWriteMask(bool rgb, bool a) override;
  void SetBlending(Blending eVal) override;
  void SetDepthTest(EDepthTest eVal) override;
  void SetCullTest(ECullTest eVal) override;
  void setScissorTest(EScissorTest eVal) override;
};

///////////////////////////////////////////////////////////////////////////////

class MetalMatrixStackInterface : public MatrixStackInterface {
  fmtx4 Ortho(float left, float right, float top, float bottom, float fnear, float ffar); // virtual
  fmtx4 Frustum(float left, float right, float top, float bottom, float zn, float zf);    // virtual

public:
  MetalMatrixStackInterface(Context& target);
};

///////////////////////////////////////////////////////////////////////////////

class MetalGeometryBufferInterface final : public GeometryBufferInterface {

public:
  MetalGeometryBufferInterface(ContextMetal& target);

private:
  ///////////////////////////////////////////////////////////////////////
  // VtxBuf Interface

  void* LockVB(VertexBufferBase& VBuf, int ivbase, int icount) override;
  void UnLockVB(VertexBufferBase& VBuf) override;

  const void* LockVB(const VertexBufferBase& VBuf, int ivbase = 0, int icount = 0) override;
  void UnLockVB(const VertexBufferBase& VBuf) override;

  void ReleaseVB(VertexBufferBase& VBuf) override;

  //

  void* LockIB(IndexBufferBase& VBuf, int ivbase, int icount) override;
  void UnLockIB(IndexBufferBase& VBuf) override;

  const void* LockIB(const IndexBufferBase& VBuf, int ibase = 0, int icount = 0) override;
  void UnLockIB(const IndexBufferBase& VBuf) override;

  void ReleaseIB(IndexBufferBase& VBuf) override;

  //

  bool BindStreamSources(const VertexBufferBase& VBuf, const IndexBufferBase& IBuf);
  bool BindVertexStreamSource(const VertexBufferBase& VBuf);
  void BindVertexDeclaration(EVtxStreamFormat efmt);

  void DrawPrimitiveEML(
      const VertexBufferBase& VBuf, //
      PrimitiveType eType,
      int ivbase,
      int ivcount) override;

  void
  DrawIndexedPrimitiveEML(const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, PrimitiveType eType, int ivbase, int ivcount)
      override;

  void DrawInstancedIndexedPrimitiveEML(
      const VertexBufferBase& VBuf,
      const IndexBufferBase& IdxBuf,
      PrimitiveType eType,
      size_t instance_count) override;

  //////////////////////////////////////////////
  // nvidia mesh shaders
  //////////////////////////////////////////////

#if defined(ENABLE_NVMESH_SHADERS)
  void DrawMeshTasksNV(uint32_t first, uint32_t count) override;

  void DrawMeshTasksIndirectNV(int32_t* indirect) override;

  void MultiDrawMeshTasksIndirectNV(int32_t* indirect, uint32_t drawcount, uint32_t stride) override;

  void MultiDrawMeshTasksIndirectCountNV(int32_t* indirect, int32_t* drawcount, uint32_t maxdrawcount, uint32_t stride) override;
#endif
  //////////////////////////////////////////////

  ContextMetal& _targetMetal;

  uint32_t mLastComponentMask;

  void _doBeginFrame() final {
    mLastComponentMask = 0;
  }
  // virtual void _doEndFrame() {}
};

///////////////////////////////////////////////////////////////////////////////

class MetalFrameBufferInterface : public FrameBufferInterface {
public:
  MetalFrameBufferInterface(ContextMetal& mTarget);
  ~MetalFrameBufferInterface();

  ///////////////////////////////////////////////////////

  void SetRtGroup(RtGroup* Base) final;
  void Clear(const fcolor4& rCol, float fdepth) final;
  void clearDepth(float fdepth) final;
  void _setViewport(int iX, int iY, int iW, int iH) final;
  void _setScissor(int iX, int iY, int iW, int iH) final;
  void _doBeginFrame(void) final;
  void _doEndFrame(void) final;
  bool capture(const RtGroup& inpbuf, int irt, CaptureBuffer* buffer) final;
  void Capture(const RtGroup& inpbuf, int irt, const file::Path& pth) final;
  bool CaptureToTexture(const CaptureBuffer& capbuf, Texture& tex) final {
    return false;
  }
  bool captureAsFormat(const RtGroup& inpbuf, int irt, CaptureBuffer* buffer, EBufferFormat destfmt) final;
  void GetPixel(const fvec4& rAt, PixelFetchContext& ctx) final;

  void rtGroupClear(RtGroup* rtg) final;
  void rtGroupMipGen(RtGroup* rtg) final;

  //////////////////////////////////////////////

  void _setAsRenderTarget();
  void _initializeContext(OffscreenBuffer* pBuf);

protected:
  ContextMetal& _targetMetal;
  int miCurScissorX;
  int miCurScissorY;
  int miCurScissorW;
  int miCurScissorH;
};

///////////////////////////////////////////////////////////////////////////////

class VdsTextureAnimation : public TextureAnimationBase {
public:
  VdsTextureAnimation(const AssetPath& pth);
  ~VdsTextureAnimation();                                                                   // virtual
  void UpdateTexture(TextureInterface* txi, Texture* ptex, TextureAnimationInst* ptexanim); // virtual
  float GetLengthOfTime(void) const;                                                        // virtual

  void* ReadFromFrameCache(int iframe, int isize);

  int miW, miH;
  int miNumFrames;
  File* mpFile;
  std::string mPath;
  dds::DDS_HEADER* mpDDSHEADER;
  int miFrameBaseSize;
  int miFrameBaseOffset;
  int miFileLength;

  std::map<int, int> mFrameCache;
  static const int kframecachesize = 60;
  void* mFrameBuffers[kframecachesize];

private:
  void UpdateFBO(MetalTextureObject& metal_texobj, float ftime);
};

struct MetalTextureObject {

  MetalTextureObject();

  /*GLuint mObject;
  GLuint mFbo;
  GLuint mDbo;
  GLenum mTarget;*/
  int _maxmip = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct PboItem {
  anyp _handle;
  size_t _length = 0;
  void* _mapped = nullptr;
};

using pboptr_t = std::shared_ptr<PboItem>;

struct PboSet {

  PboSet(size_t size);
  ~PboSet();
  pboptr_t alloc();
  void free(pboptr_t pbo);
  std::queue<pboptr_t> _pbos;
  std::set<pboptr_t> _pbos_perm;
  const size_t _size;
};

using pbosetptr_t = std::shared_ptr<PboSet>;

///////////////////////////////////////////////////////////////////////////////

struct MetalTexLoadReq {
  Texture* ptex                     = nullptr;
  const dds::DDS_HEADER* _ddsheader = nullptr;
  MetalTextureObject* pTEXOBJ          = nullptr;
  std::string _texname;
  DataBlockInputStream _inpstream;
  std::shared_ptr<CompressedImageMipChain> _cmipchain;
};

///////////////////////////////////////////////////////////////////////////////

class MetalTextureInterface : public TextureInterface {
public:
  void TexManInit(void) override;

  pboptr_t _getPBO(size_t isize);
  void _returnPBO(pboptr_t pbo);
  MetalTextureInterface(ContextMetal& tgt);

  void bindTextureToUnit(const Texture* tex, 
                         GLenum tex_targetMetal,
                         int tex_unit);

private:
  bool _loadImageTexture(Texture* ptex, datablock_ptr_t inpdata);

  bool _loadXTXTexture(Texture* ptex, datablock_ptr_t inpdata);
  void _loadXTXTextureMainThreadPart(MetalTexLoadReq req);

  void _loadDDSTextureMainThreadPart(MetalTexLoadReq req);
  bool _loadDDSTexture(const AssetPath& fname, Texture* ptex);
  bool _loadDDSTexture(Texture* ptex, datablock_ptr_t inpdata);
  bool _loadVDSTexture(const AssetPath& fname, Texture* ptex);

  bool LoadTexture(Texture* ptex, datablock_ptr_t inpdata) final;
  bool DestroyTexture(Texture* ptex) final;
  bool LoadTexture(const AssetPath& fname, Texture* ptex) final;
  void SaveTexture(const ork::AssetPath& fname, Texture* ptex) final;
  void ApplySamplingMode(Texture* ptex) final;
  void UpdateAnimatedTexture(Texture* ptex, TextureAnimationInst* tai) final;
  void initTextureFromData(Texture* ptex, TextureInitData tid) final;
  void generateMipMaps(Texture* ptex) final;
  Texture* createFromMipChain(MipChain* from_chain) final;

  std::map<size_t, pbosetptr_t> _pbosets;
  ContextMetal& _targetMetal;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

class ContextMetal : public Context {
  DeclareConcreteX(ContextMetal, Context);
  friend class GfxEnv;

  static const CClass* _gclazz;

  ///////////////////////////////////////////////////////////////////////

public:
  
  static mtlpp::Device device();

  ContextMetal();

  void FxInit();

  ///////////////////////////////////////////////////////////////////////

  void _doResizeMainSurface(int iw, int ih) final;
  void _doBeginFrame(void) final {
  }
  void _doEndFrame(void) final {
  }

public:
  //////////////////////////////////////////////
  // Interfaces

  FxInterface* FXI() final {
    return &_fxi;
  }
  ImmInterface* IMI() final {
    return &_imi;
  }
  RasterStateInterface* RSI() final {
    return &_rsi;
  }
  MatrixStackInterface* MTXI() final {
    return &_mtxi;
  }
  GeometryBufferInterface* GBI() final {
    return &_gbi;
  }
  FrameBufferInterface* FBI() final {
    return &_fbi;
  }
  TextureInterface* TXI() final {
    return &_txi;
  }
#if defined(ENABLE_COMPUTE_SHADERS)
  ComputeInterface* CI() final {
    return &_ci;
  };
#endif
  DrawingInterface* DWI() final {
    return &_dwi;
  }

  ///////////////////////////////////////////////////////////////////////

  ~ContextMetal();

  //////////////////////////////////////////////

  void makeCurrentContext(void) final;

  //void debugLabel(GLenum target, GLuint object, std::string name);

  //////////////////////////////////////////////

  static void apiInit();
  static bool haveExtension(const std::string& extname);

  //////////////////////////////////////////////

  void AttachGLContext(CTXBASE* pCTFL);
  void SwapGLContext(CTXBASE* pCTFL);

  MetalFrameBufferInterface& GLFBI() {
    return _fbi;
  }

  void initializeWindowContext(Window* pWin, CTXBASE* pctxbase) final; // make a window
  void initializeOffscreenContext(OffscreenBuffer* pBuf) final;        // make a pbuffer
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
  ContextMetal* mpParentTarget;

  std::stack<void*> mDCStack;
  std::stack<void*> mGLRCStack;

  //////////////////////////////////////////////

  static ork::MpMcBoundedQueue<void*> _loadTokens;

  ///////////////////////////////////////////////////////////////////////////
  // Rendering State Info

  EDepthTest meCurDepthTest;

  ////////////////////////////////////////////////////////////////////
  // Rendering Path Variables

  static orkvector<std::string> gExtensionVect;
  static orkset<std::string> gExtensionSet;

  ///////////////////////////////////////////////////////////////////////////

  MetalImiInterface _imi;
  metal::fx::Interface _fxi;
  MetalRasterStateInterface _rsi;
  MetalMatrixStackInterface _mtxi;
  MetalGeometryBufferInterface _gbi;
  MetalFrameBufferInterface _fbi;
  MetalTextureInterface _txi;
  MetalDrawingInterface _dwi;

#if defined(ENABLE_COMPUTE_SHADERS)
  metalfx::ComputeInterface _ci;
#endif

  bool mTargetDrawableSizeDirty;
};

} // namespace ork::lev2::metal
#endif // __APPLE__

///////////////////////////////////////////////////////////////////////////////
