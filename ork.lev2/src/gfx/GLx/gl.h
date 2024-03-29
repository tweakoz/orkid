////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
#include <ork/lev2/glfw/ctx_glfw.h>

///////////////////////////////////////////////////////////////////////////////
#if defined(RENDERDOC_API_ENABLED)
#include <renderdoc_app.h>
#endif
///////////////////////////////////////////////////////////////////////////////

#define USE_ORKSL_LANG

#if 0 // defined( _DEBUG )
#define GL_ERRORCHECK()                                                                                                            \
  {                                                                                                                                \
    GLenum iErr = GetGlError();                                                                                                    \
    if (iErr != GL_NO_ERROR)                                                                                                       \
      printf("GL_ERROR<%08x>\n", iErr);                                                                                            \
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

struct GlPlatformObject {
  GlPlatformObject();
  virtual ~GlPlatformObject();
  void makeCurrent();
  void swapBuffers();

  void_lambda_t _bindop;
  CtxGLFW* _ctxbase = nullptr;
	ContextGL*		_context = nullptr;
  bool _needsInit       = true;

  static GlPlatformObject* _current;
};
using glplato_ptr_t = std::shared_ptr<GlPlatformObject>;
class ContextGL;
class GlslFxInterface;
struct GLTextureObject;
struct GlTextureInterface;

using gltexobj_ptr_t = std::shared_ptr<GLTextureObject>;

struct GLTextureAsyncTask{
  GLTextureAsyncTask();
  std::atomic<int> _lock;
  std::queue<void_lambda_t> _onFinished;
};

using gltexasynctask_ptr_t = std::shared_ptr<GLTextureAsyncTask>;

struct GLTextureObject {

  GLTextureObject(GlTextureInterface* txi);
  ~GLTextureObject();

  GLuint _textureObject;
  GLuint mFbo;
  GLuint mDbo;
  GLenum mTarget;
  int _maxmip = 0;
  gltexasynctask_ptr_t _async;
  GlTextureInterface* _txi = nullptr;

  static std::atomic<size_t> _glto_count;
};

struct GlFboObject {
  static const int kmaxrt = RtGroup::kmaxmrts;
  GLuint _fbo = 0;
  GLuint _dsbo = 0;
  GLuint _depthTexObject = 0;
  GlFboObject();
};
using glfbo_ptr_t = std::shared_ptr<GlFboObject>;

struct GlRtBufferImpl {
  svar64_t _teximpl;
  bool _init                = true;
};

struct GlRtGroupImpl {
  glfbo_ptr_t _standard;
  glfbo_ptr_t _depthonly;
};

using glrtgroupimpl_ptr_t = std::shared_ptr<GlRtGroupImpl>;

int GetGlError();

//////////////////////////////////////////////////////////////////////

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

  void DrawPrimitiveEML(
      const FxShaderStorageBuffer* SSBO, //
      PrimitiveType eType,
      int ivbase           = 0,
      int ivcount          = 0) final;

  void
  DrawIndexedPrimitiveEML(const VertexBufferBase& VBuf,
                          const IndexBufferBase& IdxBuf, 
                          PrimitiveType eType,
                          int ivbase, int ivcount) final;

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

  ContextGL& mTargetGL;

  uint32_t mLastComponentMask;

  void _doBeginFrame() final {
    mLastComponentMask = 0;
  }
  // virtual void _doEndFrame() {}
};

///////////////////////////////////////////////////////////////////////////////

struct GlFrameBufferInterface : public FrameBufferInterface {

  GlFrameBufferInterface(ContextGL& mTarget);
  ~GlFrameBufferInterface();

  ///////////////////////////////////////////////////////

  void capture(const RtBuffer* inpbuf, const file::Path& pth) final;
  bool captureToTexture(const CaptureBuffer& capbuf, Texture& tex) final;
  bool captureAsFormat(const RtBuffer* inpbuf, CaptureBuffer* buffer, EBufferFormat destfmt) final;
  void GetPixel(const fvec4& rAt, PixelFetchContext& ctx) final;

  ///////////////////////////////////////////////////////

  void rtGroupClear(RtGroup* rtg) final;
  void rtGroupMipGen(RtGroup* rtg) final;
  void msaaBlit(rtgroup_ptr_t src, rtgroup_ptr_t dst) final;
  void blit(rtgroup_ptr_t src, rtgroup_ptr_t dst) final;
  void downsample2x2(rtgroup_ptr_t src, rtgroup_ptr_t dst) final;

  ///////////////////////////////////////////////////////

  void _initializeContext(DisplayBuffer* pBuf);
  void _setViewport(int iX, int iY, int iW, int iH) final;
  void _setScissor(int iX, int iY, int iW, int iH) final;
  void _doBeginFrame(void) final;
  void _doEndFrame(void) final;
  void _pushRtGroup(RtGroup* Base) final;
  RtGroup* _popRtGroup(bool continue_render) final;

  void _setRtGroup(RtGroup* rtgroup);

  //////////////////////////////////////////////

  void _setAsRenderTarget();
  //void Clear(const fcolor4& rCol, float fdepth) final;
  //void clearDepth(float fdepth) final;

  freestyle_mtl_ptr_t utilshader();

  freestyle_mtl_ptr_t _freestyle_mtl;
  const FxShaderTechnique* _tek_downsample2x2 = nullptr;
  const FxShaderTechnique* _tek_blit = nullptr;
  const FxShaderParam*     _fxpMVP = nullptr;
  const FxShaderParam*     _fxpColorMap = nullptr;

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
  float GetLengthOfTime() const;                                                        // virtual

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

///////////////////////////////////////////////////////////////////////////////

struct PboItem {

#if defined(OPENGL_46)
  void copyPersistentMapped(const TextureInitData& tid, size_t length, const void* src_data);
#endif

  void copyWithTempMapped(const TextureInitData& tid, size_t length, const void* src_data);

  GLuint _handle = 0xffffffff;
  size_t _length = 0;
  void* _mapped  = nullptr;
};

using pboptr_t = std::shared_ptr<PboItem>;

struct PboSet {

  PboSet(size_t size);
  ~PboSet();


  pboptr_t alloc(GlTextureInterface* txi);
  void free(pboptr_t pbo);
  std::queue<pboptr_t> _pbos;
  std::set<pboptr_t> _pbos_perm;
  const size_t _size;
};

using pbosetptr_t = std::shared_ptr<PboSet>;

///////////////////////////////////////////////////////////////////////////////

struct GlTexLoadReq {
  texture_ptr_t ptex;
  const dds::DDS_HEADER* _ddsheader = nullptr;
  gltexobj_ptr_t pTEXOBJ          = nullptr;
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

struct GlTextureInterface : public TextureInterface {

  GlTextureInterface(ContextGL& tgt);

  void TexManInit() override;

  bool destroyTexture(texture_ptr_t ptex) final;
  void ApplySamplingMode(Texture* ptex) final;
  void UpdateAnimatedTexture(Texture* ptex, TextureAnimationInst* tai) final;
  void initTextureFromData(Texture* ptex, TextureInitData tid) final;
  void generateMipMaps(Texture* ptex) final;
  Texture* createFromMipChain(MipChain* from_chain) final;

  //bool LoadTexture(const AssetPath& fname, texture_ptr_t ptex) final;
  //void SaveTexture(const ork::AssetPath& fname, Texture* ptex) final;
  //bool LoadTexture(texture_ptr_t ptex, datablock_ptr_t inpdata) final;
  ///////////////////////////////////////////////

  pboptr_t _getPBO(size_t isize);
  void _returnPBO(pboptr_t pbo);

  void bindTextureToUnit(const Texture* tex, GLenum tex_target, int tex_unit);

  bool _loadImageTexture(texture_ptr_t ptex, datablock_ptr_t inpdata);

  bool _loadXTXTexture(texture_ptr_t ptex, datablock_ptr_t inpdata);
  void _loadXTXTextureMainThreadPart(GlTexLoadReq req);

  void _loadDDSTextureMainThreadPart(GlTexLoadReq req);
  bool _loadDDSTexture(const AssetPath& fname, texture_ptr_t ptex);
  bool _loadDDSTexture(texture_ptr_t ptex, datablock_ptr_t inpdata);
  bool _loadVDSTexture(const AssetPath& fname, texture_ptr_t ptex);

  std::map<size_t, pbosetptr_t> _pbosets;
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
  DeclareConcreteX(ContextGL, Context);
  friend class GfxEnv;

  static const CClass* gpClass;

  ///////////////////////////////////////////////////////////////////////

public:
  ContextGL();

  void FxInit();

  ///////////////////////////////////////////////////////////////////////

  void _doTriggerFrameDebugCapture() final;

  void _doResizeMainSurface(int iw, int ih) final;
  void _doBeginFrame() final;
  void _doEndFrame() final;
  svar64_t _doClonePlatformHandle() const final;

public:
  //////////////////////////////////////////////
  // Interfaces

  FxInterface* FXI() final {
    return &mFxI;
  }
  ImmInterface* IMI() final {
    return &mImI;
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
  ComputeInterface* CI() final {
    return &mCI;
  };
  DrawingInterface* DWI() final {
    return &mDWI;
  }

  ///////////////////////////////////////////////////////////////////////

  ~ContextGL();

  //////////////////////////////////////////////

  void makeCurrentContext() final;

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


  //void swapBuffers(CTXBASE* ctxbase) final;

  void initializeWindowContext(Window* pWin, CTXBASE* pctxbase) final; // make a window
  void initializeOffscreenContext(DisplayBuffer* pBuf) final;        // make a pbuffer
  void initializeLoaderContext() final;

  void debugPushGroup(const std::string str, const fvec4& color) final;
  void debugPopGroup() final;
  void debugPushGroup(commandbuffer_ptr_t cb, const std::string str, const fvec4& color) final;
  void debugPopGroup(commandbuffer_ptr_t cb) final;

  void debugMarker(const std::string str, const fvec4& color) final;

  void TakeThreadOwnership() final;
  bool SetDisplayMode(DisplayMode* mode) final;
  load_token_t _doBeginLoad() final;
  void _doEndLoad(load_token_t ploadtok) final; // virtual

  void _doPushCommandBuffer(commandbuffer_ptr_t cmdbuf, rtgroup_ptr_t rtg) final;
  void _doPopCommandBuffer() final;
  void _doEnqueueSecondaryCommandBuffer(commandbuffer_ptr_t cmdbuf) final;

  void* mhHWND;
  void* mGLXContext;
  ContextGL* mpParentTarget;
  bool _SUPPORTS_BINARY_PIPELINE = true;
  bool _SUPPORTS_BUFFER_STORAGE = true;
  bool _SUPPORTS_PERSISTENT_MAP = true;
  bool _SUPPORTS_EXTERNAL_MEMORY_OBJECT = true;
  std::string _GL_RENDERER;
  
  std::stack<void*> mDCStack;
  std::stack<void*> mGLRCStack;

  //////////////////////////////////////////////

  static ork::MpMcBoundedQueue<load_token_t> _loadTokens;

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
  GlMatrixStackInterface mMtxI;
  GlGeometryBufferInterface mGbI;
  GlFrameBufferInterface mFbI;
  GlTextureInterface mTxI;
  GlDrawingInterface mDWI;
  glslfx::ComputeInterface mCI;

  bool mTargetDrawableSizeDirty;
};

}} // namespace ork::lev2

///////////////////////////////////////////////////////////////////////////////

#if !defined(GL_RGBA16F)
#define GL_RGBA16F GL_RGBA16F_ARB
#endif

///////////////////////////////////////////////////////////////////////////////
