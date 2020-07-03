////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Win32GL Specific
///////////////////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <functional>
#include <map>
#include <ork/kernel/svariant.h>
#include <ork/lev2/gfx/gfxenv.h>
#if defined(__APPLE__)
#include <ork/lev2/gfx/glheaders.h>
#else
#include "glad/glad.h"
#endif
#include <set>
#include <string>
#include <vector>
///////////////////////////////////////////////////////////////////////////////
#include "glfx/glslfxi.h"
/////////////////////////////

#include <ork/kernel/concurrent_queue.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/file/chunkfile.inl>
#include <ork/kernel/datablock.h>
#include <ork/lev2/gfx/image.h>

///////////////////////////////////////////////////////////////////////////////

#if 1 // defined( _DEBUG )
#define GL_ERRORCHECK()                                                                                                            \
  {                                                                                                                                \
    GLenum iErr = GetGlError();                                                                                                    \
    OrkAssert(iErr == GL_NO_ERROR);                                                                                                \
  }
#else
#define GL_ERRORCHECK()                                                                                                            \
  {}
#endif
#define GL_NF_ERRORCHECK()                                                                                                         \
  {                                                                                                                                \
    GLenum iErr = GetGlError();                                                                                                    \
    if (iErr != GL_NO_ERROR)                                                                                                       \
      printf("GLERROR FILE<%s> LINE<%d>\n", __FILE__, __LINE__);                                                                   \
  }

///////////////////////////////////////////////////////////////////////////////

namespace ork::dds {
struct DDS_HEADER;
}

namespace ork { namespace lev2 {

class ContextGL;
class GlslFxInterface;
struct GLTextureObject;

struct GlFboObject {
  static const int kmaxrt = RtGroup::kmaxmrts;
  GLuint mFBOMaster;
  GLuint mDSBO;
  GLuint _depthTexture;
  GlFboObject();
};

struct GlRtBufferImpl {
  GLuint _texture           = 0;
  GLTextureObject* _teximpl = nullptr;
  bool _init                = true;
};

int GetGlError(void);

//////////////////////////////////////////////////////////////////////

class GlTextureInterface;

struct GlDrawingInterface : public DrawingInterface {
  GlDrawingInterface(ContextGL& ctx);
};

///////////////////////////////////////////////////////////////////////////////

class GlImiInterface : public ImmInterface {
  virtual void DrawLine(const fvec4& From, const fvec4& To);
  virtual void DrawPoint(F32 fx, F32 fy, F32 fz);
  virtual void DrawPrim(const fvec4* Points, int inumpoints, PrimitiveType eType);
  virtual void _doBeginFrame() {
  }
  virtual void _doEndFrame() {
  }

public:
  GlImiInterface(ContextGL& target);
};

///////////////////////////////////////////////////////////////////////////////

struct GlRasterStateInterface : public RasterStateInterface {

  GlRasterStateInterface(Context& target);
  void BindRasterState(const SRasterState& rState, bool bForce) override;

  void SetZWriteMask(bool bv) override;
  void SetRGBAWriteMask(bool rgb, bool a) override;
  void SetBlending(Blending eVal) override;
  void SetDepthTest(EDepthTest eVal) override;
  void SetCullTest(ECullTest eVal) override;
  void setScissorTest(EScissorTest eVal) override;
};

///////////////////////////////////////////////////////////////////////////////

class GlMatrixStackInterface : public MatrixStackInterface {
  fmtx4 Ortho(float left, float right, float top, float bottom, float fnear, float ffar); // virtual
  fmtx4 Frustum(float left, float right, float top, float bottom, float zn, float zf);    // virtual

public:
  GlMatrixStackInterface(Context& target);
};

///////////////////////////////////////////////////////////////////////////////

class GlGeometryBufferInterface final : public GeometryBufferInterface {

public:
  GlGeometryBufferInterface(ContextGL& target);

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

  void DrawIndexedPrimitiveEML(
      const VertexBufferBase& VBuf,
      const IndexBufferBase& IdxBuf,
      PrimitiveType eType,
      int ivbase,
      int ivcount) override;

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

  ContextGL& mTargetGL;

  uint32_t mLastComponentMask;

  void _doBeginFrame() final {
    mLastComponentMask = 0;
  }
  // virtual void _doEndFrame() {}
};

///////////////////////////////////////////////////////////////////////////////

class GlFrameBufferInterface : public FrameBufferInterface {
public:
  GlFrameBufferInterface(ContextGL& mTarget);
  ~GlFrameBufferInterface();

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
  ContextGL& mTargetGL;
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
  void UpdateFBO(GLTextureObject& glto, float ftime);
};

struct GLTextureObject {

  GLTextureObject();

  GLuint mObject;
  GLuint mFbo;
  GLuint mDbo;
  GLenum mTarget;
  int _maxmip = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct PboSet {

  PboSet(size_t size);
  ~PboSet();
  GLuint alloc();
  void free(GLuint pbo);
  std::set<GLuint> _pbos;
  std::set<GLuint> _pbos_perm;
  const size_t _size;
};

///////////////////////////////////////////////////////////////////////////////

struct GlTexLoadReq {
  Texture* ptex                     = nullptr;
  const dds::DDS_HEADER* _ddsheader = nullptr;
  GLTextureObject* pTEXOBJ          = nullptr;
  std::string _texname;
  DataBlockInputStream _inpstream;
  std::shared_ptr<CompressedImageMipChain> _cmipchain;
};

///////////////////////////////////////////////////////////////////////////////

constexpr uint16_t kRGB_DXT1  = 0x83F0;
constexpr uint16_t kRGBA_DXT1 = 0x83F1;
constexpr uint16_t kRGBA_DXT3 = 0x83F2;
constexpr uint16_t kRGBA_DXT5 = 0x83F3;
constexpr GLuint PBOOBJBASE   = 0x12340000;

class GlTextureInterface : public TextureInterface {
public:
  void TexManInit(void) override;

  GLuint _getPBO(size_t isize);
  void _returnPBO(size_t isize, GLuint pbo);

  GlTextureInterface(ContextGL& tgt);

private:
  bool _loadImageTexture(Texture* ptex, datablock_ptr_t inpdata);

  bool _loadXTXTexture(Texture* ptex, datablock_ptr_t inpdata);
  void _loadXTXTextureMainThreadPart(GlTexLoadReq req);

  void _loadDDSTextureMainThreadPart(GlTexLoadReq req);
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

  std::map<size_t, PboSet*> mPBOSets;
  ContextGL& mTargetGL;
};

struct texcfg {
  GLuint mInternalFormat;
  GLuint mFormat;
  int mBPP;
  int mNumC;
};

texcfg GetInternalFormat(GLuint fmt, GLuint typ);
void Set2D(
    GlTextureInterface* txi,
    Texture* tex,
    GLuint numC,
    GLuint fmt,
    GLuint typ,
    GLuint tgt,
    int BPP,
    int inummips,
    int& iw,
    int& ih,
    DataBlockInputStream inpstream);
void Set3D(
    GlTextureInterface* txi,
    Texture* tex,
    /*GLuint numC,*/ GLuint fmt,
    GLuint typ,
    GLuint tgt,
    /*int BPP,*/ int inummips,
    int& iw,
    int& ih,
    int& id,
    DataBlockInputStream inpstream);
void Set2DC(
    GlTextureInterface* txi,
    Texture* tex,
    GLuint fmt,
    GLuint tgt,
    int BPP,
    int inummips,
    int& iw,
    int& ih,
    DataBlockInputStream inpstream);
void Set3DC(
    GlTextureInterface* txi,
    Texture* tex,
    GLuint fmt,
    GLuint tgt,
    int BPP,
    int inummips,
    int& iw,
    int& ih,
    int& id,
    DataBlockInputStream inpstream);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

class ContextGL : public Context {
  RttiDeclareConcrete(ContextGL, Context);
  friend class GfxEnv;

  static const CClass* gpClass;

  ///////////////////////////////////////////////////////////////////////

public:
  ContextGL();

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
    return &mFxI;
  }
  ImmInterface* IMI() final {
    return &mImI;
  }
  RasterStateInterface* RSI() final {
    return &mRsI;
  }
  MatrixStackInterface* MTXI() final {
    return &mMtxI;
  }
  GeometryBufferInterface* GBI() final {
    return &mGbI;
  }
  FrameBufferInterface* FBI() final {
    return &mFbI;
  }
  TextureInterface* TXI() final {
    return &mTxI;
  }
#if defined(ENABLE_COMPUTE_SHADERS)
  ComputeInterface* CI() final {
    return &mCI;
  };
#endif
  DrawingInterface* DWI() final {
    return &mDWI;
  }

  ///////////////////////////////////////////////////////////////////////

  ~ContextGL();

  //////////////////////////////////////////////

  void makeCurrentContext(void) final;

  void debugLabel(GLenum target, GLuint object, std::string name);

  //////////////////////////////////////////////

  static void GLinit();
  static bool HaveGLExtension(const std::string& extname);

  //////////////////////////////////////////////

  void AttachGLContext(CTXBASE* pCTFL);
  void SwapGLContext(CTXBASE* pCTFL);

  GlFrameBufferInterface& GLFBI() {
    return mFbI;
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
  ContextGL* mpParentTarget;

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

  GlImiInterface mImI;
  glslfx::Interface mFxI;
  GlRasterStateInterface mRsI;
  GlMatrixStackInterface mMtxI;
  GlGeometryBufferInterface mGbI;
  GlFrameBufferInterface mFbI;
  GlTextureInterface mTxI;
  GlDrawingInterface mDWI;

#if defined(ENABLE_COMPUTE_SHADERS)
  glslfx::ComputeInterface mCI;
#endif

  bool mTargetDrawableSizeDirty;
};

}} // namespace ork::lev2

///////////////////////////////////////////////////////////////////////////////

#if !defined(GL_RGBA16F)
#define GL_RGBA16F GL_RGBA16F_ARB
#endif

///////////////////////////////////////////////////////////////////////////////
