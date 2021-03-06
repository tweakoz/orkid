////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

extern bool _HIDPI();
extern bool _MIXEDDPI();
extern float _currentDPI();

typedef SVtxV12C4T16 TEXT_VTXFMT;

class CTXBASE;

class Context;
class OffscreenBuffer;
class Window;
class RtGroup;

class Texture;
class GfxMaterial;
class GfxMaterialUITextured;
class PBRMaterial;

class GfxEnv;

using context_ptr_t = std::shared_ptr<Context>;

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

class RenderContextInstData;
class RenderContextFrameData;

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

class Context : public ork::Object {
  DeclareAbstractX(Context, ork::Object);

  ///////////////////////////////////////////////////////////////////////
public:
  ///////////////////////////////////////////////////////////////////////

  Context();
  virtual ~Context();

  //////////////////////////////////////////////
  /// Interfaces

  virtual FxInterface* FXI()             = 0; // Fx Shader Interface
  virtual RasterStateInterface* RSI()    = 0; // Raster State Interface
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
  virtual void debugPushGroup(const std::string str) {
  }
  ///////////////////////////////////////////////////////////////////////
  /// pop command group from debugstack (for renderdoc,apitrace,nsight,etc..)
  virtual void debugPopGroup() {
  }
  ///////////////////////////////////////////////////////////////////////
  /// insert marker into commandstream (for renderdoc,apitrace,nsight,etc..)
  virtual void debugMarker(const std::string str) {
  }

  ///////////////////////////////////////////////////////////////////////
  /// make rendercontext current on current thread
  ///  probably a GLism, might need to be reworked for vulkan,metal.
  virtual void makeCurrentContext() {
  }

  ///////////////////////////////////////////////////////////////////////

  virtual void initializeWindowContext(Window* pWin, CTXBASE* pctxbase) = 0;
  virtual void initializeOffscreenContext(OffscreenBuffer* pBuf)        = 0;
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

  ///////////////////////////////////////////////////////////////////////

  virtual U32 fcolor4ToU32(const fcolor4& clr) {
    return clr.GetRGBAU32();
  }
  virtual U32 fcolor3ToU32(const fcolor3& clr) {
    return clr.GetRGBAU32();
  }
  virtual fcolor4 U32Tofcolor4(const U32 uclr) {
    fcolor4 clr;
    clr.SetRGBAU32(uclr);
    return clr;
  }
  virtual fcolor3 U32Tofcolor3(const U32 uclr) {
    fcolor3 clr;
    clr.SetRGBAU32(uclr);
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

  static const orkvector<DisplayMode*>& GetDisplayModes() {
    return mDisplayModes;
  }

  bool SetDisplayMode(unsigned int index);
  virtual bool SetDisplayMode(DisplayMode* mode) = 0;

  void* GetPlatformHandle() const {
    return mPlatformHandle;
  }
  void SetPlatformHandle(void* ph) {
    mPlatformHandle = ph;
  }

  virtual void TakeThreadOwnership() {
  }

  void* BeginLoad();
  void EndLoad(void* ploadtok);

  static const int kiModColorStackMax = 8;
  int miW, miH;
  CTXBASE* mCtxBase;
  void* mPlatformHandle;
  TargetType meTargetType;
  fvec4 maModColorStack[kiModColorStackMax];
  int miModColorStackIndex;
  const ork::rtti::ICastable* mpCurrentObject;
  fvec4 mvModColor;
  bool mbPostInitializeContext;
  int miTargetFrame;
  PerformanceItem mFramePerfItem;
  const RenderContextInstData* mRenderContextInstData;
  int miDrawLock;
  bool hiDPI() const;
  float currentDPI() const;
  RtGroup* _defaultRTG = nullptr;

  static orkvector<DisplayMode*> mDisplayModes;

  std::stack<const RenderContextFrameData*> _rcfdstack;

private:
  virtual void _doBeginFrame(void) = 0;
  virtual void _doEndFrame(void)   = 0;
  virtual void* _doBeginLoad() {
    return nullptr;
  }
  virtual void _doEndLoad(void* ploadtok) {
  }

  virtual void _doResizeMainSurface(int iw, int ih) = 0;

  const RenderContextFrameData* _defaultrcfd = nullptr;
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

struct OrthoQuad {
  OrthoQuad();

  ork::fcolor4 mColor;
  SRect mQrect;
  float mfu0a;
  float mfv0a;
  float mfu1a;
  float mfv1a;
  float mfu0b;
  float mfv0b;
  float mfu1b;
  float mfv1b;
  float mfrot;
};

class OffscreenBuffer : public ork::Object {
  DeclareAbstractX(OffscreenBuffer, ork::Object);

public:
  //////////////////////////////////////////////

  OffscreenBuffer(
      OffscreenBuffer* Parent,
      int iX,
      int iY,
      int iW,
      int iH,
      EBufferFormat efmt      = EBufferFormat::RGBA8,
      const std::string& name = "NoName");

  virtual ~OffscreenBuffer();

  //////////////////////////////////////////////

  RtGroup* GetParentMrt(void) const {
    return _parentRtGroup;
  }
  ui::Widget* GetRootWidget(void) const {
    return mRootWidget;
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
  OffscreenBuffer* GetParent(void) const {
    return _parent;
  }
  TargetType GetTargetType(void) const {
    return meTargetType;
  }
  EBufferFormat format(void) const {
    return meFormat;
  }
  Texture* GetTexture() const {
    return mpTexture;
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
    mpContext      = ctx.get(); // todo get rid of me..
  }
  void SetTexture(Texture* ptex) {
    mpTexture = ptex;
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

  void SetRootWidget(ui::Widget* pwidg) {
    mRootWidget = pwidg;
  }

  //////////////////////////////////////////////

  virtual void BeginFrame(void);
  virtual void EndFrame(void);
  virtual void initContext();

protected:
  context_ptr_t _sharedcontext;
  ui::Widget* mRootWidget;
  Context* mpContext;
  Texture* mpTexture;
  int miWidth;
  int miHeight;
  EBufferFormat meFormat;
  TargetType meTargetType;
  bool mbDirty;
  bool mbSizeIsDirty;
  std::string _name;
  fcolor4 mClearColor;
  OffscreenBuffer* _parent;
  RtGroup* _parentRtGroup;
  void* mPlatformHandle;
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

class Window : public OffscreenBuffer {
public:
  //////////////////////////////////////////////

  Window(int iX, int iY, int iW, int iH, const std::string& name = "NoName", void* pdata = 0);
  virtual ~Window();

  //////////////////////////////////////////////

  virtual void initContext();

  virtual void OnShow() {
  }

  virtual void GotFocus(void) {
    mbHasFocus = true;
  }
  virtual void LostFocus(void) {
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
  friend class OffscreenBuffer;
  friend class Window;
  friend class Context;
  //////////////////////////////////////////////////////////////////////////////

public:
  Context* loadingContext() const {
    return gLoaderTarget;
  }
  void SetLoaderTarget(Context* ptarget);

  recursive_mutex& GetGlobalLock() {
    return mGfxEnvMutex;
  }

  //////////////////////////////////////////////////////////////////////////////
  // Contex Factory

  GfxEnv();

  void RegisterWinContext(Window* pWin);

  //////////////////////////////////////////////////////////////////////////////

  OffscreenBuffer* GetMainWindow(void) {
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

  //////////////////////////////////////////////////////////////////////////////
protected:
  //////////////////////////////////////////////////////////////////////////////

  Window* mpMainWindow;
  Context* gLoaderTarget;

  orkvector<OffscreenBuffer*> mvActivePBuffers;
  orkvector<OffscreenBuffer*> mvActiveWindows;
  orkvector<OffscreenBuffer*> mvInactiveWindows;

  DynamicVertexBuffer<SVtxV12C4T16> mVtxBufSharedVect;
  DynamicVertexBuffer<SVtxV12N12B12T8C4> mVtxBufSharedVect2;
  DynamicVertexBuffer<SVtxV16T16C16> _vtxBufSharedV16T16C16;
  orkmap<std::string, std::string> mRuntimeEnvironment;
  orkstack<ContextCreationParams> mCreationParams;
  recursive_mutex mGfxEnvMutex;
  bool _initialized = false;

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

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2

#define gGfxEnv ork::lev2::GfxEnv::GetRef()
