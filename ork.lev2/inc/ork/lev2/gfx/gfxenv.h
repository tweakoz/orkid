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
#include <ork/kernel/taskgraph.h>
#include <ork/kernel/timer.h>
#include <ork/object/Object.h>

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

extern bool _HIDPI();
extern bool _MIXEDDPI();
extern float _currentDPI();

typedef SVtxV12C4T16 TEXT_VTXFMT;

struct GpuEvent {
  std::string _eventID;
  varmap::VarMap _vars;
};

using gpuevent_queue_t = std::queue<gpuevent_ptr_t>;
using gpuevent_cb_t    = std::function<void(gpuevent_ptr_t)>;

/// ////////////////////////////////////////////////////////////////////////////
/// GpuPerfBlock: GPU timestamp query block for measuring GPU execution time
/// ////////////////////////////////////////////////////////////////////////////

struct GpuPerfBlock {
  std::string _name;
  double _duration = -1.0;  // seconds, populated on readback
  size_t _sample_index = 0; // reserved slot index for backfilling
  std::function<void(gpuperfblock_ptr_t)> _on_result;  // callback when result ready
  // Internal (set by VkContext):
  uint32_t _begin_query = 0;
  uint32_t _end_query = 0;
  int _pool_index = -1;     // which double-buffered pool
};
struct GpuEventSink {
  std::string _eventID;
  gpuevent_cb_t _onEvent;
};
using gpueventsink_map_t = std::unordered_map<std::string, gpueventsink_ptr_t>;

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

struct LoadingPhase {

  void enqueueOperation(gfxcontext_lambda_t l);
  void join();

  LockedResource<gfxcontext_lambda_list_t> _load_operations;
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

using sticky_cb_t = std::function<bool()>;
using load_token_t = svar32_t;
using ctx_platform_handle_t = svar64_t;

struct DebugGroup {
  DebugGroup(Context*);
  ~DebugGroup();
  Context* _context = nullptr;
};

struct RenderingConventions {
  // Coordinate system conventions (currently matching OpenGL)
  bool _isRightHanded = true;      // true for RH (GL/Orkid), false for LH
  bool _isLogicalYUp = true;               // true for Y-up (GL/Orkid), false for Y-down (Vulkan native)
  bool _isNativeYUp = true;          // true for Y-up (GL/Orkid), false for Y-down (Vulkan native)
  bool _ndcZRange01 = false;        // false for [-1,1] (GL), true for [0,1] (Vulkan/D3D)
  
  // Winding order conventions  
  bool _defaultWindingCCW = true;   // true for CCW (GL default), false for CW
  bool _frontFaceWindingCCW = true; // true if front faces use CCW winding in screen space
  
  // Helper to determine which quad function to use for correct winding
  bool useClockwiseWinding() const {
    // Use CW winding when front faces are expected to be CW
    return !_frontFaceWindingCCW;
  }
};

////////////////////////////////////////////////////////////////////////////////
// ContextExecutor - TaskExecutor that runs tasks on GPU main thread
////////////////////////////////////////////////////////////////////////////////

struct ContextExecutor : public ::ork::TaskExecutor {
  
  ContextExecutor(context_rawptr_t ctx);
  
  void executePhase(taskphase_ptr_t phase) override;
  static void emptyFrame( taskgraph_wkptr_t self,                            //
                          const std::string& name,                         //
                          contextexecutor_ptr_t executor,                  //
                          taskphasecomplete_func_t on_completion=nullptr); //
  
  context_rawptr_t _context;
};

struct Context : public ::ork::Object {
  DeclareAbstractX(Context, ::ork::Object);

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
  virtual ComputeInterface* CI()         = 0; // ComputeShader Interface
  virtual ImmInterface* IMI() {
    return 0;
  } // Immediate Mode Interface (optional)
  pri_rawptr_t PRI() {
    return _primitives_interface.get();
  } // Primitives Interface
  
  void gpuPreInit(); // Initialize GPU-dependent resources
  void gpuPostInit();

  ///////////////////////////////////////////////////////////////////////
  void triggerFrameDebugCapture();
  virtual void _doTriggerFrameDebugCapture() {
  }
  ///////////////////////////////////////////////////////////////////////
  /// push command group onto debugstack (for renderdoc,apitrace,nsight,etc..)
  DebugGroup debugPushGroupAutoRelease(const std::string str);
  void debugPushGroup(const std::string str);
  virtual void debugPushGroup(const std::string str, const fvec4& color) {
  }
  ///////////////////////////////////////////////////////////////////////
  /// pop command group from debugstack (for renderdoc,apitrace,nsight,etc..)
  virtual void debugPopGroup() {
  }
  ///////////////////////////////////////////////////////////////////////
  virtual void debugPushGroup(secondary_commandbuffer_ptr_t cb, const std::string str, const fvec4& color) {}
  virtual void debugPopGroup(secondary_commandbuffer_ptr_t cb) {}
  ///////////////////////////////////////////////////////////////////////
  /// insert marker into commandstream (for renderdoc,apitrace,nsight,etc..)
  void debugMarker(const std::string str);
  virtual void debugMarker(const std::string str, const fvec4& color) {
  }

  ///////////////////////////////////////////////////////////////////////
  /// make rendercontext current on current thread
  ///  probably a GLism, might need to be reworked for vulkan,metal.
  virtual void makeCurrentContext() {
  }

  ///////////////////////////////////////////////////////////////////////

  virtual void initializeWindowContext(Window* pWin, CTXBASE* pctxbase) = 0;
  virtual void initializeOffscreenContext(DisplayBuffer* pBuf)          = 0;
  virtual void initializeLoaderContext()                                = 0;

  ///////////////////////////////////////////////////////////////////////

  int mainSurfaceWidth() const;
  int mainSurfaceHeight() const;
  float mainSurfaceAspectRatio() const;
  ViewportRect mainSurfaceRectAtWindowPos() const;
  ViewportRect mainSurfaceRectAtOrigin() const;
  void resizeMainSurface(int iw, int ih);

  //////////////////////////////////////////////

  void beginFrame(bool visual = true);
  void endFrame();

  ///////////////////////////////////////////////////////////////////////
  // command buffers / renderpasses
  ///////////////////////////////////////////////////////////////////////

  secondary_commandbuffer_ptr_t beginRecordCommandBuffer(std::string name, rtgroup_rawptr_t rtg = nullptr);
  void endRecordCommandBuffer(secondary_commandbuffer_ptr_t cmdbuf);
  void enqueueSecondaryCommandBuffer(secondary_commandbuffer_ptr_t cmdbuf);

  virtual void _doEnqueueSecondaryCommandBuffer(secondary_commandbuffer_ptr_t cmdbuf);
  virtual secondary_commandbuffer_ptr_t _beginRecordCommandBuffer(std::string name, rtgroup_rawptr_t rtg);
  virtual void _endRecordCommandBuffer(secondary_commandbuffer_ptr_t cmdbuf);


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

  fvec4& RefModColor() {
    return mvModColor;
  }
  void PushModColor(const fvec4& mclr);
  fvec4& PopModColor();

  ///////////////////////////////////////////////////////////////////////

  const ::ork::rtti::ICastable* GetCurrentObject() const {
    return mpCurrentObject;
  }
  void SetCurrentObject(const ::ork::rtti::ICastable* pobj) {
    mpCurrentObject = pobj;
  }
  TargetType GetTargetType() const {
    return meTargetType;
  }
  int GetTargetFrame() const {
    return miTargetFrame;
  }
  PerformanceItem& GetFramePerfItem() {
    return mFramePerfItem;
  }
  CTXBASE* GetCtxBase() const {
    return mCtxBase;
  }

  ///////////////////////////////////////////////////////

  const RenderContextInstData* GetRenderContextInstData() const {
    return mRenderContextInstData;
  }
  void SetRenderContextInstData(const RenderContextInstData* data) {
    mRenderContextInstData = data;
  }

  const rcfd_ptr_t topRenderContextFrameData() const {
    return _rcfdstack.top();
  }
  void pushRenderContextFrameData(rcfd_ptr_t rcfd) {
    return _rcfdstack.push(rcfd);
  }
  void popRenderContextFrameData() {
    _rcfdstack.pop();
  }

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
  void bindPlatformHandle(ctx_platform_handle_t phandle) {
    _doBindPlatformHandle(phandle);
  }
  virtual void _doBindPlatformHandle(ctx_platform_handle_t phandle) {
  }

  virtual void TakeThreadOwnership() {
  }

  virtual void stateDebugger() const {
  }

  void beginPrimaryCommandBuffer();
  void endPrimaryCommandBuffer();
  void submitPrimaryCommandBuffer();
  virtual void _doBeginPrimaryCommandBuffer();
  virtual void _doEndPrimaryCommandBuffer();
  virtual void _doSubmitPrimaryCommandBuffer();

  load_token_t beginLoad();
  void endLoad(load_token_t ploadtok);

  template <typename vtx_t> std::shared_ptr<DynamicVertexBuffer<vtx_t>> miscVertexBuffer(uint32_t id, uint32_t numverts) {
    using vtxbuf_t     = DynamicVertexBuffer<vtx_t>;
    using vtxbuf_ptr_t = std::shared_ptr<vtxbuf_t>;
    auto it            = _miscVBs.find(id);
    if (it != _miscVBs.end()) {
      return it->second.get<vtxbuf_ptr_t>();
    } else {
      auto vbp = std::make_shared<vtxbuf_t>(numverts, 0);
      vbp->SetRingLock(true);

      _miscVBs[id] = vbp;
      return vbp;
    }
  }

  bool hiDPI() const;
  float currentDPI() const;

  void scheduleOnBeginFrame(void_lambda_t l) {
    _onBeginFrameCallbacks.push_back(l);
  }
  void scheduleBeforeDoEndFrameOneShot(void_lambda_t l) {
    _onBeforeDoEndFrameOneShotCallbacks.push_back(l);
  }
  void scheduleOnEndFrame(void_lambda_t l) {
    _onEndFrameCallbacks.push_back(l);
  }

  virtual void swapBuffers(CTXBASE* ctxbase) {
  }

  void enqueueGpuEvent(gpuevent_ptr_t evt);
  void registerGpuEventSink(gpueventsink_ptr_t sink);

  ///////////////////////////////////////////////////////////////////////
  /// GPU performance timing (timestamp queries)
  ///////////////////////////////////////////////////////////////////////
  virtual gpuperfblock_ptr_t gpuPerfBlockBegin(const std::string& name) { return nullptr; }
  virtual void gpuPerfBlockEnd(gpuperfblock_ptr_t block) {}
  double gpuPerfResult(const std::string& name) const;  // last-frame duration in seconds (-1 if not found)
  std::map<std::string, double> _gpuPerfResults;  // populated during readback
  gpuperfblock_ptr_t _frameAllPerfBlock;  // spans beginFrame→endFrame

  loadingphase_ptr_t newLoadingPhase();
  
  contextexecutor_ptr_t createContextExecutor();
  
  //////////////////////////////////////////////////////////
  // Rendering conventions for this backend
  //////////////////////////////////////////////////////////
  
  const RenderingConventions& renderingConventions() const { 
    return _renderingConventions; 
  }
  
  //////////////////////////////////////////////////////////

  static orkvector<DisplayMode*> mDisplayModes;

  std::stack<rcfd_ptr_t> _rcfdstack;

  LockedResource<loadingphase_list_t> _loadingPhases;

  static const int kiModColorStackMax = 8;

  CTXBASE* mCtxBase                                   = nullptr;
  ctx_platform_handle_t                               _impl;
  const RenderContextInstData* mRenderContextInstData = nullptr;
  const ::ork::rtti::ICastable* mpCurrentObject         = nullptr;
  RtGroup* _defaultRTG                                = nullptr;

  uint64_t _currentPhase = 0;
  TargetType meTargetType;
  int miW, miH;
  int miModColorStackIndex;
  int miTargetFrame;
  int miDrawLock;
  bool mbPostInitializeContext;
  bool _is_visual_frame = false;
  bool _isFrameDebugCapture = false;
  fvec4 maModColorStack[kiModColorStackMax];
  fvec4 mvModColor;
  PerformanceItem mFramePerfItem;
  std::unordered_map<uint32_t, svar64_t> _miscVBs;
  std::vector<sticky_cb_t> _beginFrameBlockers;

  secondary_commandbuffer_ptr_t _recordCommandBuffer;
  
  Timer _ctxtimer;

  // Per-frame timing breakdown (uses _ctxtimer for timestamps)
  float _perf_frame_t0 = 0.0f;             // timestamp at start of beginFrame
  double _perf_beginFrame_duration = 0.0;   // total beginFrame() time
  double _perf_endFrame_duration = 0.0;     // total endFrame() time
  double _perf_acquire_duration = 0.0;      // swapchain acquire (set by backend)
  double _perf_fence_wait_duration = 0.0;   // fence wait (set by backend)
  double _perf_submit_duration = 0.0;       // vkQueueSubmit (set by backend)
  double _perf_present_duration = 0.0;      // vkQueuePresentKHR (set by backend)

  svar64_t _pyimpl_beforeEndFrame;
protected:
  RenderingConventions _renderingConventions;

private:
  pri_ptr_t _primitives_interface;
  std::vector<void_lambda_t> _onBeginFrameCallbacks;
  std::vector<void_lambda_t> _onEndFrameCallbacks;
  std::vector<void_lambda_t> _onBeforeDoEndFrameOneShotCallbacks;
  LockedResource<gpueventsink_map_t> _gpuEventSinks;
  gpuevent_queue_t _gpuEventQueue;

  void _processBeginFrameBlockers();
  void _loadingPhaseOperations();
  virtual void _onGpuPreInit() {}
  virtual void _onGpuPostInit() {}
  virtual void _doPreBeginFrame() {}
  virtual void _doBeginFrame() = 0;
  virtual void _doEndFrame()   = 0;
  virtual load_token_t _doBeginLoad() {
    return nullptr;
  }
  virtual void _doEndLoad(load_token_t ploadtok) {
  }

  virtual void _doResizeMainSurface(int iw, int ih) = 0;

  rcfd_ptr_t _defaultrcfd = nullptr;
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

  fcolor4 mColor;
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

struct DisplayBuffer : public ::ork::Object {
  DeclareAbstractX(DisplayBuffer, ::ork::Object);

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

  RtGroup* GetParentMrt() const {
    return _parentRtGroup;
  }
  ui::Widget* GetRootWidget() const {
    return _rootWidget.get();
  }
  bool IsDirty() const {
    return mbDirty;
  }
  bool IsSizeDirty() const {
    return mbSizeIsDirty;
  }
  const std::string& GetName() const {
    return _name;
  }
  const fcolor4& GetClearColor() const {
    return mClearColor;
  }
  DisplayBuffer* GetParent() const {
    return _parent;
  }
  TargetType GetTargetType() const {
    return meTargetType;
  }
  EBufferFormat format() const {
    return meFormat;
  }
  Texture* GetTexture() const {
    return _texture;
  }
  Context* context() const;

  int GetContextW() const {
    return context()->mainSurfaceWidth();
  }
  int GetContextH() const {
    return context()->mainSurfaceHeight();
  }

  int GetBufferW() const {
    return miWidth;
  }
  int GetBufferH() const {
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

  virtual void BeginFrame();
  virtual void EndFrame();
  virtual void initContext();

  context_ptr_t _sharedcontext;
  uiwidget_ptr_t _rootWidget = nullptr;
  Texture* _texture          = nullptr;
  DisplayBuffer* _parent     = nullptr;
  RtGroup* _parentRtGroup    = nullptr;
  void* _IMPL                = nullptr;

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
  // Deferred Context Operations
  
  void enqueueDeferredContextOp(ctx_lambda_t op);
  void processDeferredContextOps(context_rawptr_t ctx);

  //////////////////////////////////////////////////////////////////////////////
  // Contex Factory

  GfxEnv();

  void RegisterWinContext(Window* pWin);

  //////////////////////////////////////////////////////////////////////////////

  DisplayBuffer* GetMainWindow() {
    return mpMainWindow;
  }
  void SetMainWindow(Window* pWin) {
    mpMainWindow = pWin;
  }

//////////////////////////////////////////////////////////////////////////////
#if defined(_WIN32) && (!(defined(_XBOX)))
  static HWND GetMainHWND() {
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

  static dvb_V12C4T16_ptr_t GetSharedDynamicVB();
  static dvb_V12N12B12T8C4_ptr_t GetSharedDynamicVB2();
  static dvb_V16T16C16_ptr_t GetSharedDynamicV16T16C16();

  static bool initialized();
  static void initializeWithContext(context_ptr_t ctx);

  using lockset_t      = std::unordered_set<uint64_t>;
  using locknotifset_t = std::vector<void_lambda_t>;

  static uint64_t createLock();
  static void releaseLock(uint64_t l);
  static void onLocksDone(void_lambda_t l);
  static lockset_t dumpLocks();

  static bool supportsBC7();
  static void disableBC7();

  //////////////////////////////////////////////////////////////////////////////
protected:
  //////////////////////////////////////////////////////////////////////////////

  static bool _bc7Disabled;

  Window* mpMainWindow;

  orkvector<DisplayBuffer*> mvActivePBuffers;
  orkvector<DisplayBuffer*> mvActiveWindows;
  orkvector<DisplayBuffer*> mvInactiveWindows;

  
  dvb_V12C4T16_ptr_t _vtxbuf_shared_V12C4T16;
  dvb_V12N12B12T8C4_ptr_t _vtxbuf_shared_V12N12B12T8C4;
  dvb_V16T16C16_ptr_t _vtxbuf_shared_V16T16C16;
  orkmap<std::string, std::string> mRuntimeEnvironment;
  orkstack<ContextCreationParams> mCreationParams;
  recursive_mutex mGfxEnvMutex;
  bool _initialized = false;
  
  // Queue for deferred operations that need a context
  using defctx_opq_t = std::queue<ctx_lambda_t>;
  LockedResource<defctx_opq_t> _deferredContextOps;

  struct WaitLockData {
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

class DrawHudEvent : public ::ork::event::Event {

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

struct PrimaryCommandBuffer {
  PrimaryCommandBuffer(std::string name = "---")
      : _debugName(name) {
  }
  svarshp_t _impl;
  std::string _debugName;
  bool _no_draw    = false;
};

struct SecondaryCommandBuffer {
  SecondaryCommandBuffer(std::string name = "---");
  ~SecondaryCommandBuffer();
  svarshp_t _impl;
  std::string _debugName;
  bool _no_draw    = false;
  static std::atomic<int> _num_alive;
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2

#define gGfxEnv ork::lev2::GfxEnv::GetRef()
