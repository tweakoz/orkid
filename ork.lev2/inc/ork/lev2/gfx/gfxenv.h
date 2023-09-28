////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Grpahics Environment (Driver/HAL)
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/RTTIX.inl>
#include <ork/kernel/core/singleton.h>
#include <ork/kernel/timer.h>

#include <ork/lev2/gfx/config.h>

#include "gfxenv_enum.h"
#include "gfxvtxbuf.h"
#include "targetinterfaces.h"
#include <ork/lev2/ui/ui.h>

#include <ork/event/Event.h>
#include <ork/kernel/mutex.h>
#include <ork/kernel/datablock.h>
#include <ork/object/AutoConnector.h>
#include <ork/lev2/lev2_types.h>
#include <ork/util/Context.h>
#include <ork/kernel/shared_pool.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

extern size_t DEFAULT_WINDOW_WIDTH;
extern size_t DEFAULT_WINDOW_HEIGHT;

extern bool _HIDPI();
extern bool _MIXEDDPI();
extern float _currentDPI();

typedef SVtxV12C4T16 TEXT_VTXFMT;

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

struct ContextCreationParams {
  ContextCreationParams()
      : miNumSharedVerts(0)
      , mbFullScreen(false)
      , mbWideScreen(false)
      , miDefaultWidth(640)
      , miDefaultHeight(480)
      , miDefaultMrtWidth(640)
      , miDefaultMrtHeight(480)
      , miQuality(100) {
  }

  int miNumSharedVerts;
  bool mbFullScreen;
  bool mbWideScreen;
  int miDefaultWidth;
  int miDefaultHeight;
  int miDefaultMrtWidth;
  int miDefaultMrtHeight;
  int miQuality;
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

struct RenderQueueSortingData {
  RenderQueueSortingData();

  int miSortingPass;
  int miSortingOffset;
  bool mbTransparency;
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

struct DisplayMode {
  DisplayMode(unsigned int w = 0, unsigned int h = 0, unsigned int r = 0)
      : width(w)
      , height(h)
      , refresh(r) {
  }

  unsigned int width;
  unsigned int height;
  unsigned int refresh;
};

///////////////////////////////////////////////////////////////////////////////
/// Context: rendering context device abstraction
///   abstracts OpenGL, Vulkan, Metal, etc..
///  all user code should utilize the Context abstraction instead of
///  calling directly into the low level rendering api. This aids in
///   keeping code cross platform
///  The amount of abstraction is pretty low. If some abstraction becomes a
///   bottleneck, then the api will be augmented to expose the lowlevel api feature
///   that enhances performance.
///
///  A context is a composite of multiple domain specific subinterfaces:
///   FXI : Effects Interface. Responsible for management of shaders,
///          and shader related resources such as UBO's, SSBO's, etc..
///   TXI : Texturing interface. Responsible for management of textures
///   FBI : FrameBuffer Interface. FBO's, MRT's, DepthBuffers, scissoring,
///          viewports, clearing, etc....
///   MTXI : Matrix Interface. Matrix utilities, lookat, perspective, etc..)
///   RSI : RasterState Interface
///         (responsible for management of rasterstate, blending, writemasks, etc.)
///   GBI : GeometryBuffer Interface. Management of vertex and index buffers,
///           also currently responsible for drawing primitives
///           Primitives will be moved to a new interface (DWI)
///   CI : Compute Interface. all things ComputeShader related
///   IMI : ImmediateMode interface. convenience methods for oldschool type gfx
///////////////////////////////////////////////////////////////////////////////

using load_token_t = svar32_t;
using ctx_platform_handle_t = svar32_t;

struct Context : public ork::Object {
  DeclareAbstractX(Context, ork::Object);

  ///////////////////////////////////////////////////////////////////////
public:
  ///////////////////////////////////////////////////////////////////////

  Context();
  virtual ~Context();

  //////////////////////////////////////////////
  /// Interfaces

  virtual FxInterface* FXI()             = 0; // Fx Shader Interface
  virtual MatrixStackInterface* MTXI()   = 0; // Matrix / Matrix Stack Interface
  virtual GeometryBufferInterface* GBI() = 0; // Geometry Buffer Interface
  virtual FrameBufferInterface* FBI()    = 0; // FrameBuffer/Control Interface
  virtual TextureInterface* TXI()        = 0; // Texture Interface
  virtual DrawingInterface* DWI()        = 0; // Drawing Interface

#if defined(ENABLE_COMPUTE_SHADERS)
  virtual ComputeInterface* CI() = 0; // ComputeShader Interface
#endif
  virtual ImmInterface* IMI() {
    return 0;
  } // Immediate Mode Interface (optional)

  ///////////////////////////////////////////////////////////////////////
  /// push command group onto debugstack (for renderdoc,apitrace,nsight,etc..)
  virtual void debugPushGroup(const std::string str, const fvec4& color=fvec4(1,1,1,1)) {
  }
  ///////////////////////////////////////////////////////////////////////
  /// pop command group from debugstack (for renderdoc,apitrace,nsight,etc..)
  virtual void debugPopGroup() {
  }
  ///////////////////////////////////////////////////////////////////////
  /// insert marker into commandstream (for renderdoc,apitrace,nsight,etc..)
  virtual void debugMarker(const std::string str, const fvec4& color=fvec4(1,1,1,1)) {
  }

  ///////////////////////////////////////////////////////////////////////
  /// make rendercontext current on current thread
  ///  probably a GLism, might need to be reworked for vulkan,metal.
  virtual void makeCurrentContext() {
  }

  ///////////////////////////////////////////////////////////////////////

  virtual void initializeWindowContext(Window* pWin, CTXBASE* pctxbase) = 0;
  virtual void initializeOffscreenContext(DisplayBuffer* pBuf)        = 0;
  virtual void initializeLoaderContext()                                = 0;

  ///////////////////////////////////////////////////////////////////////

  inline int mainSurfaceWidth(void) const {
    return miW;
  }
  inline int mainSurfaceHeight(void) const {
    return miH;
  }
  inline float mainSurfaceAspectRatio() const {
    return float(miW) / float(miH);
  }
  inline ViewportRect mainSurfaceRectAtWindowPos() const {
    return ViewportRect(0, 0, miW, miH);
  }
  inline ViewportRect mainSurfaceRectAtOrigin() const {
    return ViewportRect(0, 0, miW, miH);
  }
  inline void resizeMainSurface(int iw, int ih) {
    _doResizeMainSurface(iw, ih);
  }

  //////////////////////////////////////////////

  void beginFrame(void);
  void endFrame(void);


  commandbuffer_ptr_t beginRecordCommandBuffer(renderpass_ptr_t rpass=nullptr);
  void endRecordCommandBuffer(commandbuffer_ptr_t cmdbuf);

  void beginRenderPass(renderpass_ptr_t);
  void endRenderPass(renderpass_ptr_t);
  void beginSubPass(rendersubpass_ptr_t);
  void endSubPass(rendersubpass_ptr_t);

  ///////////////////////////////////////////////////////////////////////

  virtual commandbuffer_ptr_t _beginRecordCommandBuffer(renderpass_ptr_t rpass) { return nullptr; }
  virtual void _endRecordCommandBuffer(commandbuffer_ptr_t cmdbuf) {}
  virtual void _beginRenderPass(renderpass_ptr_t) {}
  virtual void _endRenderPass(renderpass_ptr_t) {}
  virtual void _beginSubPass(rendersubpass_ptr_t) {}
  virtual void _endSubPass(rendersubpass_ptr_t) {}

  ///////////////////////////////////////////////////////////////////////

  virtual U32 fcolor4ToU32(const fcolor4& clr) {
    return clr.RGBAU32();
  }
  virtual U32 fcolor3ToU32(const fcolor3& clr) {
    return clr.RGBAU32();
  }
  virtual fcolor4 U32Tofcolor4(const U32 uclr) {
    fcolor4 clr;
    clr.setRGBAU32(uclr);
    return clr;
  }
  virtual fcolor3 U32Tofcolor3(const U32 uclr) {
    fcolor3 clr;
    clr.setRGBAU32(uclr);
    return clr;
  }

  ///////////////////////////////////////////////////////////////////////

  fvec4& RefModColor(void) {
    return mvModColor;
  }
  void PushModColor(const fvec4& mclr);
  fvec4& PopModColor(void);

  ///////////////////////////////////////////////////////////////////////

  const ork::rtti::ICastable* GetCurrentObject(void) const {
    return mpCurrentObject;
  }
  void SetCurrentObject(const ork::rtti::ICastable* pobj) {
    mpCurrentObject = pobj;
  }
  TargetType GetTargetType(void) const {
    return meTargetType;
  }
  int GetTargetFrame(void) const {
    return miTargetFrame;
  }
  PerformanceItem& GetFramePerfItem(void) {
    return mFramePerfItem;
  }
  CTXBASE* GetCtxBase(void) const {
    return mCtxBase;
  }

  ///////////////////////////////////////////////////////

  const RenderContextInstData* GetRenderContextInstData() const {
    return mRenderContextInstData;
  }
  void SetRenderContextInstData(const RenderContextInstData* data) {
    mRenderContextInstData = data;
  }

  const RenderContextFrameData* topRenderContextFrameData() const {
    return _rcfdstack.top();
  }
  void pushRenderContextFrameData(const RenderContextFrameData* rcfd) {
    return _rcfdstack.push(rcfd);
  }
  void popRenderContextFrameData() {
    _rcfdstack.pop();
  }

  //////////////////////////////////////////////

  inline void pushCommandBuffer(commandbuffer_ptr_t cmdbuf, rtgroup_ptr_t rtg=nullptr) {
    _cmdbuf_stack.push(cmdbuf);
    _current_cmdbuf = cmdbuf;
    _doPushCommandBuffer(cmdbuf,rtg);
  }
  inline commandbuffer_ptr_t popCommandBuffer() {
    _doPopCommandBuffer();
    _cmdbuf_stack.pop();
    commandbuffer_ptr_t next = nullptr;
    if (not _cmdbuf_stack.empty()) {
      next = _cmdbuf_stack.top();
    }
    _current_cmdbuf = next;
    return next;
  }
  void enqueueSecondaryCommandBuffer(commandbuffer_ptr_t cmdbuf) {
    _doEnqueueSecondaryCommandBuffer(cmdbuf);
  }

  virtual void _doPushCommandBuffer(commandbuffer_ptr_t cmdbuf, rtgroup_ptr_t rtg=nullptr) {}
  virtual void _doPopCommandBuffer() {}
  virtual void _doEnqueueSecondaryCommandBuffer(commandbuffer_ptr_t cmdbuf) {}

  //////////////////////////////////////////////

  static const orkvector<DisplayMode*>& GetDisplayModes() {
    return mDisplayModes;
  }

  bool SetDisplayMode(unsigned int index);
  virtual bool SetDisplayMode(DisplayMode* mode) = 0;

  ctx_platform_handle_t clonePlatformHandle() const {
    return _doClonePlatformHandle();
  }
  virtual ctx_platform_handle_t _doClonePlatformHandle() const {
    return nullptr;
  }
  virtual void TakeThreadOwnership() {
  }

  load_token_t BeginLoad();
  void EndLoad(load_token_t ploadtok);

  void scheduleOnBeginFrame(void_lambda_t l) {
    _onBeginFrameCallbacks.push_back(l);
  }
  void scheduleOnEndFrame(void_lambda_t l) {
    _onEndFrameCallbacks.push_back(l);
  }

  virtual void present(CTXBASE* ctxbase) {}

  static const int kiModColorStackMax = 8;
  static orkvector<DisplayMode*> mDisplayModes;

  CTXBASE* mCtxBase                                   = nullptr;
  ctx_platform_handle_t _impl;
  const RenderContextInstData* mRenderContextInstData = nullptr;
  const ork::rtti::ICastable* mpCurrentObject         = nullptr;
  RtGroup* _defaultRTG                                = nullptr;

  TargetType meTargetType;
  int miW, miH;
  int miModColorStackIndex;
  int miTargetFrame;
  int miDrawLock;
  bool mbPostInitializeContext;

  fvec4 maModColorStack[kiModColorStackMax];
  fvec4 mvModColor;
  PerformanceItem mFramePerfItem;
  commandbuffer_ptr_t _recordCommandBuffer;
  commandbuffer_ptr_t _defaultCommandBuffer;
  shared_pool::fixed_pool<CommandBuffer,4> _cmdbuf_pool;
  std::stack<commandbuffer_ptr_t> _cmdbuf_stack;
  commandbuffer_ptr_t _current_cmdbuf;
  bool hiDPI() const;
  float currentDPI() const;

  std::stack<const RenderContextFrameData*> _rcfdstack;

  std::vector<void_lambda_t> _onBeginFrameCallbacks;
  std::vector<void_lambda_t> _onEndFrameCallbacks;

private:

  virtual void _doBeginFrame(void) = 0;
  virtual void _doEndFrame(void)   = 0;
  virtual load_token_t _doBeginLoad() {
    return nullptr;
  }
  virtual void _doEndLoad(load_token_t ploadtok) {
  }

  virtual void _doResizeMainSurface(int iw, int ih) = 0;

  const RenderContextFrameData* _defaultrcfd = nullptr;
};

struct ThreadGfxContext : public util::ContextTLS<ThreadGfxContext> {
  ThreadGfxContext(Context* c)
      : _context(c) {
  }
  Context* _context;
};

Context* contextForCurrentThread();

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

struct OrthoQuad {
  OrthoQuad();

  ork::fcolor4 mColor;
  SRect mQrect;
  float mfu0a;
  float mfv0a;
  float mfu0b;
  float mfv0b;
  float mfu1a;
  float mfv1a;
  float mfu1b;
  float mfv1b;
  float mfrot;
};

struct DisplayBuffer : public ork::Object {
  DeclareAbstractX(DisplayBuffer, ork::Object);

public:
  //////////////////////////////////////////////

  DisplayBuffer(
      DisplayBuffer* Parent,
      int iX,
      int iY,
      int iW,
      int iH,
      EBufferFormat efmt      = EBufferFormat::RGBA8,
      const std::string& name = "NoName");

  virtual ~DisplayBuffer();

  //////////////////////////////////////////////

  RtGroup* GetParentMrt(void) const {
    return _parentRtGroup;
  }
  ui::Widget* GetRootWidget(void) const {
    return _rootWidget.get();
  }
  bool IsDirty(void) const {
    return mbDirty;
  }
  bool IsSizeDirty(void) const {
    return mbSizeIsDirty;
  }
  const std::string& GetName(void) const {
    return _name;
  }
  const fcolor4& GetClearColor() const {
    return mClearColor;
  }
  DisplayBuffer* GetParent(void) const {
    return _parent;
  }
  TargetType GetTargetType(void) const {
    return meTargetType;
  }
  EBufferFormat format(void) const {
    return meFormat;
  }
  Texture* GetTexture() const {
    return _texture;
  }
  Context* context(void) const;

  int GetContextW(void) const {
    return context()->mainSurfaceWidth();
  }
  int GetContextH(void) const {
    return context()->mainSurfaceHeight();
  }

  int GetBufferW(void) const {
    return miWidth;
  }
  int GetBufferH(void) const {
    return miHeight;
  }
  void SetBufferWidth(int iw) {
    mbSizeIsDirty = (miWidth != iw);
    miWidth       = iw;
  }
  void SetBufferHeight(int ih) {
    mbSizeIsDirty = (miHeight != ih);
    miHeight      = ih;
  }

  //////////////////////////////////////////////

  void Resize(int ix, int iy, int iw, int ih);
  void SetDirty(bool bval) {
    mbDirty = bval;
  }
  void SetSizeDirty(bool bv) {
    mbSizeIsDirty = bv;
  }
  void SetParentMrt(RtGroup* ParentMrt) {
    _parentRtGroup = ParentMrt;
  }
  fcolor4& RefClearColor() {
    return mClearColor;
  }
  void SetContext(context_ptr_t ctx) {
    _sharedcontext = ctx;
  }
  void SetTexture(Texture* ptex) {
    _texture = ptex;
  }

  //////////////////////////////////////////////

  void RenderMatOrthoQuad(
      const SRect& ViewportRect,
      const SRect& QuadRect,
      GfxMaterial* pmat,
      float fu0          = 0.0f,
      float fv0          = 0.0f,
      float fu1          = 1.0f,
      float fv1          = 1.0f,
      float* uv2         = NULL,
      const fcolor4& clr = fcolor4::White());

  void RenderMatOrthoQuad(
      const SRect& ViewportRect,
      const SRect& QuadRect,
      GfxMaterial* pmat,
      fvec2 uv0,
      fvec2 uv1,
      fvec2 uv2,
      fvec2 uv3,
      const fcolor4& clr = fcolor4::White());

  void Render2dQuadEML(const fvec4& QuadRect, const fvec4& UvRect, const fvec4& UvRect2);
  void Render2dQuadsEML(size_t count, const fvec4* QuadRects, const fvec4* UvRects, const fvec4* UvRect2s);

  //////////////////////////////////////////////

  virtual void BeginFrame(void);
  virtual void EndFrame(void);
  virtual void initContext();

  context_ptr_t _sharedcontext;
  uiwidget_ptr_t _rootWidget  = nullptr;
  Texture* _texture        = nullptr;
  DisplayBuffer* _parent = nullptr;
  RtGroup* _parentRtGroup  = nullptr;
  void* _IMPL              = nullptr;
  int miWidth;
  int miHeight;
  EBufferFormat meFormat;
  TargetType meTargetType;
  bool mbDirty;
  bool mbSizeIsDirty;
  std::string _name;
  fcolor4 mClearColor;
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

struct Window : public DisplayBuffer {
public:
  //////////////////////////////////////////////

  Window(int iX, int iY, int iW, int iH, const std::string& name = "NoName", void* pdata = 0);
  virtual ~Window();

  //////////////////////////////////////////////

  virtual void initContext();

  virtual void OnShow() {
  }

  virtual void GotFocus() {
    mbHasFocus = true;
  }
  virtual void LostFocus() {
    mbHasFocus = false;
  }
  bool HasFocus() const {
    return mbHasFocus;
  }

  //////////////////////////////////////////////

  CTXBASE* mpCTXBASE;
  bool mbHasFocus;
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

class GfxEnv : public NoRttiSingleton<GfxEnv> {
  friend struct DisplayBuffer;
  friend struct Window;
  friend struct Context;
  //////////////////////////////////////////////////////////////////////////////

public:

  recursive_mutex& GetGlobalLock() {
    return mGfxEnvMutex;
  }

  //////////////////////////////////////////////////////////////////////////////
  // Contex Factory

  GfxEnv();

  void RegisterWinContext(Window* pWin);

  //////////////////////////////////////////////////////////////////////////////

  DisplayBuffer* GetMainWindow(void) {
    return mpMainWindow;
  }
  void SetMainWindow(Window* pWin) {
    mpMainWindow = pWin;
  }

//////////////////////////////////////////////////////////////////////////////
#if defined(_WIN32) && (!(defined(_XBOX)))
  static HWND GetMainHWND(void) {
    return GetRef().mpMainWindow->context()->GetHWND();
  }
#endif
  //////////////////////////////////////////////////////////////////////////////

  static void atomicOp(recursive_mutex::atomicop_t op);

  static void setContextClass(const object::ObjectClass* pclass) {
    gpTargetClass = pclass;
  }
  static const object::ObjectClass* contextClass() {
    return gpTargetClass;
  }
  void SetRuntimeEnvironmentVariable(const std::string& key, const std::string& val);
  const std::string& GetRuntimeEnvironmentVariable(const std::string& key) const;

  void PushCreationParams(const ContextCreationParams& p) {
    mCreationParams.push(p);
  }
  void PopCreationParams() {
    mCreationParams.pop();
  }
  const ContextCreationParams& GetCreationParams() {
    return mCreationParams.top();
  }

  static DynamicVertexBuffer<SVtxV12C4T16>& GetSharedDynamicVB();
  static DynamicVertexBuffer<SVtxV12N12B12T8C4>& GetSharedDynamicVB2();
  static DynamicVertexBuffer<SVtxV16T16C16>& GetSharedDynamicV16T16C16();

  static bool initialized();
  static void initializeWithContext(context_ptr_t ctx);

  using lockset_t = std::unordered_set<uint64_t>;
  using locknotifset_t = std::vector<void_lambda_t>;

  static uint64_t createLock();
  static void releaseLock(uint64_t l);
  static void onLocksDone(void_lambda_t l);

  static lockset_t dumpLocks();

  //////////////////////////////////////////////////////////////////////////////
protected:
  //////////////////////////////////////////////////////////////////////////////

  Window* mpMainWindow;

  orkvector<DisplayBuffer*> mvActivePBuffers;
  orkvector<DisplayBuffer*> mvActiveWindows;
  orkvector<DisplayBuffer*> mvInactiveWindows;

  DynamicVertexBuffer<SVtxV12C4T16> mVtxBufSharedVect;
  DynamicVertexBuffer<SVtxV12N12B12T8C4> mVtxBufSharedVect2;
  DynamicVertexBuffer<SVtxV16T16C16> _vtxBufSharedV16T16C16;
  orkmap<std::string, std::string> mRuntimeEnvironment;
  orkstack<ContextCreationParams> mCreationParams;
  recursive_mutex mGfxEnvMutex;
  bool _initialized = false;


  struct WaitLockData{
    lockset_t _locks;
    locknotifset_t _notifs;
  };

  std::atomic<uint64_t> _lockCounter;

  LockedResource<WaitLockData> _waitlockdata;

  static const object::ObjectClass* gpTargetClass;
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

class DrawHudEvent : public ork::event::Event {

public:
  DrawHudEvent(Context* target = NULL, int camera_number = 1)
      : mTarget(target)
      , mCameraNumber(camera_number) {
  }

  Context* GetTarget() const {
    return mTarget;
  }
  void setContext(Context* target) {
    mTarget = target;
  }

  int GetCameraNumber() const {
    return mCameraNumber;
  }
  void SetCameraNumber(int camera_number) {
    mCameraNumber = camera_number;
  }

private:
  Context* mTarget;
  int mCameraNumber;
};

struct RenderPass{
  svarp_t _impl;
  std::vector<rendersubpass_ptr_t> _subpasses;
  bool _immutable = false;
  bool _allow_clear = true;
  std::string _debugName;
};

struct RenderSubPass{

  RenderSubPass();
  
  std::vector<rendersubpass_ptr_t> _subpass_dependencies;
  rtgroup_ptr_t _rtg_input;
  rtgroup_ptr_t _rtg_output;
  svarp_t _impl;
  std::string _debugName;
  commandbuffer_ptr_t _commandbuffer;
};

struct CommandBuffer{
  CommandBuffer(std::string name="---")
    : _debugName(name)
  {
  }
  svarp_t _impl;
  std::string _debugName;
  bool _is_primary = false;
  bool _no_draw = false;
};



/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2

#define gGfxEnv ork::lev2::GfxEnv::GetRef()
