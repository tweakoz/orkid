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

#include <ork/kernel/core/singleton.h>
#include <ork/kernel/timer.h>

#include <ork/lev2/gfx/config.h>

#include "gfxenv_enum.h"
#include "gfxvtxbuf.h"
#include "targetinterfaces.h"
#include <ork/lev2/ui/ui.h>

#include <ork/event/Event.h>
#include <ork/kernel/mutex.h>
#include <ork/kernel/datablock.inl>
#include <ork/object/AutoConnector.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
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

class Context : public ork::rtti::ICastable {
  RttiDeclareAbstract(Context, ork::rtti::ICastable);

  ///////////////////////////////////////////////////////////////////////
public:
  ///////////////////////////////////////////////////////////////////////

  Context();
  virtual ~Context();

  //////////////////////////////////////////////
  // Interfaces

  virtual FxInterface* FXI() {
    return 0;
  } // Fx Shader Interface (optional)
  virtual ImmInterface* IMI() {
    return 0;
  }                                           // Immediate Mode Interface (optional)
  virtual RasterStateInterface* RSI()    = 0; // Raster State Interface
  virtual MatrixStackInterface* MTXI()   = 0; // Matrix / Matrix Stack Interface
  virtual GeometryBufferInterface* GBI() = 0; // Geometry Buffer Interface
  virtual FrameBufferInterface* FBI()    = 0; // FrameBuffer/Control Interface
  virtual TextureInterface* TXI()        = 0; // Texture Interface
  virtual DrawingInterface* DWI()        = 0; // Drawing Interface

#if defined(ENABLE_COMPUTE_SHADERS)
  virtual ComputeInterface* CI() = 0; // ComputeShader Interface
#endif

  virtual void debugPushGroup(const std::string str) {
  }
  virtual void debugPopGroup() {
  }
  virtual void debugMarker(const std::string str) {
  }

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

  //////////////////////////////////////////////

  GfxMaterial* currentMaterial(void) {
    return mpCurMaterial;
  }
  void BindMaterial(GfxMaterial* pmtl);
  void PushMaterial(GfxMaterial* pmtl);
  void PopMaterial();

  ///////////////////////////////////////////////////////////////////////

  virtual U32 fcolor4ToU32(const fcolor4& clr) {
    return clr.GetRGBAU32();
  }
  virtual U32 CColor3ToU32(const CColor3& clr) {
    return clr.GetRGBAU32();
  }
  virtual fcolor4 U32Tofcolor4(const U32 uclr) {
    fcolor4 clr;
    clr.SetRGBAU32(uclr);
    return clr;
  }
  virtual CColor3 U32ToCColor3(const U32 uclr) {
    CColor3 clr;
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
  ETargetType GetTargetType(void) const {
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

  bool IsDeviceAvailable() const {
    return mbDeviceAvailable;
  }
  void SetDeviceAvailable(bool bv) {
    mbDeviceAvailable = bv;
  }

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
  ETargetType meTargetType;
  fvec4 maModColorStack[kiModColorStackMax];
  int miModColorStackIndex;
  const ork::rtti::ICastable* mpCurrentObject;
  fvec4 mvModColor;
  bool mbPostInitializeContext;
  int miTargetFrame;
  PerformanceItem mFramePerfItem;
  const RenderContextInstData* mRenderContextInstData;
  GfxMaterial* mpCurMaterial;
  std::stack<GfxMaterial*> mMaterialStack;
  bool mbDeviceAvailable;
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
  RttiDeclareAbstract(OffscreenBuffer, ork::Object);

public:
  //////////////////////////////////////////////

  OffscreenBuffer(
      OffscreenBuffer* Parent,
      int iX,
      int iY,
      int iW,
      int iH,
      EBufferFormat efmt      = EBUFFMT_RGBA8,
      const std::string& name = "NoName");

  virtual ~OffscreenBuffer();

  //////////////////////////////////////////////

  struct OrthoQuads {
    SRect mViewportRect;
    SRect mOrthoRect;
    GfxMaterial* mpMaterial;
    const OrthoQuad* mpQUADS;
    int miNumQuads;
  };

  //////////////////////////////////////////////

  RtGroup* GetParentMrt(void) const {
    return mParentRtGroup;
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
    return msName;
  }
  const fcolor4& GetClearColor() const {
    return mClearColor;
  }
  OffscreenBuffer* GetParent(void) const {
    return mParent;
  }
  ETargetType GetTargetType(void) const {
    return meTargetType;
  }
  EBufferFormat format(void) const {
    return meFormat;
  }
  Texture* GetTexture() const {
    return mpTexture;
  }
  GfxMaterial* GetMaterial() const {
    return mpMaterial;
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
    mParentRtGroup = ParentMrt;
  }
  fcolor4& RefClearColor() {
    return mClearColor;
  }
  void SetContext(Context* pctx) {
    mpContext = pctx;
  }
  void SetTexture(Texture* ptex) {
    mpTexture = ptex;
  }
  void SetMaterial(GfxMaterial* pmtl) {
    mpMaterial = pmtl;
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

  void Render2dQuadEML(const fvec4& QuadRect, const fvec4& UvRect, const fvec4& UvRect2);
  void Render2dQuadsEML(size_t count, const fvec4* QuadRects, const fvec4* UvRects, const fvec4* UvRect2s);

  void RenderMatOrthoQuads(const OrthoQuads& oquads);

  //////////////////////////////////////////////

  void SetRootWidget(ui::Widget* pwidg) {
    mRootWidget = pwidg;
  }

  //////////////////////////////////////////////

  virtual void BeginFrame(void);
  virtual void EndFrame(void);
  virtual void initContext();

protected:
  ui::Widget* mRootWidget;
  Context* mpContext;
  GfxMaterial* mpMaterial;
  Texture* mpTexture;
  int miWidth;
  int miHeight;
  EBufferFormat meFormat;
  ETargetType meTargetType;
  bool mbDirty;
  bool mbSizeIsDirty;
  std::string msName;
  fcolor4 mClearColor;
  OffscreenBuffer* mParent;
  RtGroup* mParentRtGroup;
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

  static GfxMaterial* GetDefaultUIMaterial(void) {
    return GetRef().mpUIMaterial;
  }
  static PBRMaterial* GetDefault3DMaterial(void) {
    return GetRef().mp3DMaterial;
  }

  static void setContextClass(const rtti::Class* pclass) {
    gpTargetClass = pclass;
  }
  static const rtti::Class* contextClass() {
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

  //////////////////////////////////////////////////////////////////////////////
protected:
  //////////////////////////////////////////////////////////////////////////////

  GfxMaterial* mpUIMaterial;
  PBRMaterial* mp3DMaterial;
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

  static const rtti::Class* gpTargetClass;
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

class DrawHudEvent : public ork::event::Event {
  RttiDeclareConcrete(DrawHudEvent, ork::event::Event);

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

}} // namespace ork::lev2

#define gGfxEnv ork::lev2::GfxEnv::GetRef()
