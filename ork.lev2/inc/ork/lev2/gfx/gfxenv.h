////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Grpahics Environment (Driver/HAL)
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/core/singleton.h>
#include <ork/kernel/timer.h>

#include "gfxenv_enum.h"
#include "gfxvtxbuf.h"
#include "targetinterfaces.h"
#include <ork/lev2/ui/ui.h>

#include <ork/event/Event.h>
#include <ork/kernel/mutex.h>
#include <ork/object/AutoConnector.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

typedef SVtxV12C4T16 TEXT_VTXFMT;

class CTXBASE;

class GfxTarget;
class GfxBuffer;
class GfxWindow;
class RtGroup;

class Texture;
class GfxMaterial;
class GfxMaterialUITextured;

class GfxEnv;

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

struct GfxTargetCreationParams {
  GfxTargetCreationParams()
      : miNumSharedVerts(0)
      , mbFullScreen(false)
      , mbWideScreen(false)
      , miDefaultWidth(640)
      , miDefaultHeight(480)
      , miDefaultMrtWidth(640)
      , miDefaultMrtHeight(480)
      , miQuality(100) {}

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
      , refresh(r) {}

  unsigned int width;
  unsigned int height;
  unsigned int refresh;
};

///////////////////////////////////////////////////////////////////////////////

class GfxTarget : public ork::rtti::ICastable {
  RttiDeclareAbstract(GfxTarget, ork::rtti::ICastable);

  ///////////////////////////////////////////////////////////////////////
public:
  ///////////////////////////////////////////////////////////////////////

  GfxTarget();
  virtual ~GfxTarget();

  //////////////////////////////////////////////
  // Interfaces

  virtual FxInterface* FXI() { return 0; }    // Fx Shader Interface (optional)
  virtual ImmInterface* IMI() { return 0; }   // Immediate Mode Interface (optional)
  virtual RasterStateInterface* RSI()    = 0; // Raster State Interface
  virtual MatrixStackInterface* MTXI()   = 0; // Matrix / Matrix Stack Interface
  virtual GeometryBufferInterface* GBI() = 0; // Geometry Buffer Interface
  virtual FrameBufferInterface* FBI()    = 0; // FrameBuffer/Control Interface
  virtual TextureInterface* TXI()        = 0; // Texture Interface

  virtual void debugPushGroup(const std::string str) {}
  virtual void debugPopGroup() {}
  virtual void debugMarker(const std::string str) {}

  ///////////////////////////////////////////////////////////////////////

  virtual void InitializeContext(GfxWindow* pWin, CTXBASE* pctxbase) = 0;
  virtual void InitializeContext(GfxBuffer* pBuf)                    = 0;

  ///////////////////////////////////////////////////////////////////////

  int GetX(void) const { return miX; }
  int GetY(void) const { return miY; }
  int GetW(void) const { return miW; }
  int GetH(void) const { return miH; }

  virtual void SetSize(int ix, int iy, int iw, int ih) = 0;

  //////////////////////////////////////////////

  void BeginFrame(void);
  void EndFrame(void);

  //////////////////////////////////////////////

  GfxMaterial* GetCurMaterial(void) { return mpCurMaterial; }
  void BindMaterial(GfxMaterial* pmtl);
  void PushMaterial(GfxMaterial* pmtl);
  void PopMaterial();

  ///////////////////////////////////////////////////////////////////////

  virtual U32 fcolor4ToU32(const fcolor4& clr) { return clr.GetRGBAU32(); }
  virtual U32 CColor3ToU32(const CColor3& clr) { return clr.GetRGBAU32(); }
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

  fvec4& RefModColor(void) { return mvModColor; }
  void PushModColor(const fvec4& mclr);
  fvec4& PopModColor(void);

  ///////////////////////////////////////////////////////////////////////

  const ork::rtti::ICastable* GetCurrentObject(void) const { return mpCurrentObject; }
  void SetCurrentObject(const ork::rtti::ICastable* pobj) { mpCurrentObject = pobj; }
  ETargetType GetTargetType(void) const { return meTargetType; }
  int GetTargetFrame(void) const { return miTargetFrame; }
  PerformanceItem& GetFramePerfItem(void) { return mFramePerfItem; }
  CTXBASE* GetCtxBase(void) const { return mCtxBase; }

  ///////////////////////////////////////////////////////

  const RenderContextInstData* GetRenderContextInstData() const { return mRenderContextInstData; }
  void SetRenderContextInstData(const RenderContextInstData* data) { mRenderContextInstData = data; }

  const RenderContextFrameData* topRenderContextFrameData() const { return _rcfdstack.top(); }
  void pushRenderContextFrameData(const RenderContextFrameData* rcfd) { return _rcfdstack.push(rcfd); }
  void popRenderContextFrameData() { _rcfdstack.pop(); }

  //////////////////////////////////////////////

  bool IsDeviceAvailable() const { return mbDeviceAvailable; }
  void SetDeviceAvailable(bool bv) { mbDeviceAvailable = bv; }

  static const orkvector<DisplayMode*>& GetDisplayModes() { return mDisplayModes; }

  bool SetDisplayMode(unsigned int index);
  virtual bool SetDisplayMode(DisplayMode* mode) = 0;

  void* GetPlatformHandle() const { return mPlatformHandle; }
  void SetPlatformHandle(void* ph) { mPlatformHandle = ph; }

  virtual void TakeThreadOwnership() {}

  void* BeginLoad();
  void EndLoad(void* ploadtok);

protected:
  static const int kiModColorStackMax = 8;
  int miX, miY, miW, miH;
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

  static orkvector<DisplayMode*> mDisplayModes;

  std::stack<const RenderContextFrameData*> _rcfdstack;

private:
  virtual void DoBeginFrame(void) = 0;
  virtual void DoEndFrame(void)   = 0;
  virtual void* DoBeginLoad() { return nullptr; }
  virtual void DoEndLoad(void* ploadtok) {}

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

class GfxBuffer : public ork::Object {
  RttiDeclareAbstract(GfxBuffer, ork::Object);

public:
  //////////////////////////////////////////////

  GfxBuffer(GfxBuffer* Parent,
            int iX,
            int iY,
            int iW,
            int iH,
            EBufferFormat efmt      = EBUFFMT_RGBA32,
            ETargetType etype       = ETGTTYPE_EXTBUFFER,
            const std::string& name = "NoName");

  virtual ~GfxBuffer();

  //////////////////////////////////////////////

  struct OrthoQuads {
    SRect mViewportRect;
    SRect mOrthoRect;
    GfxMaterial* mpMaterial;
    const OrthoQuad* mpQUADS;
    int miNumQuads;
  };

  //////////////////////////////////////////////

  RtGroup* GetParentMrt(void) const { return mParentRtGroup; }
  ui::Widget* GetRootWidget(void) const { return mRootWidget; }
  bool IsDirty(void) const { return mbDirty; }
  bool IsSizeDirty(void) const { return mbSizeIsDirty; }
  const std::string& GetName(void) const { return msName; }
  const fcolor4& GetClearColor() const { return mClearColor; }
  GfxBuffer* GetParent(void) const { return mParent; }
  ETargetType GetTargetType(void) const { return meTargetType; }
  EBufferFormat GetBufferFormat(void) const { return meFormat; }
  Texture* GetTexture() const { return mpTexture; }
  GfxMaterial* GetMaterial() const { return mpMaterial; }
  GfxTarget* GetContext(void) const;

  int GetContextX(void) const { return GetContext()->GetX(); }
  int GetContextY(void) const { return GetContext()->GetY(); }
  int GetContextW(void) const { return GetContext()->GetW(); }
  int GetContextH(void) const { return GetContext()->GetH(); }

  int GetBufferW(void) const { return miWidth; }
  int GetBufferH(void) const { return miHeight; }
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
  void SetDirty(bool bval) { mbDirty = bval; }
  void SetSizeDirty(bool bv) { mbSizeIsDirty = bv; }
  void SetParentMrt(RtGroup* ParentMrt) { mParentRtGroup = ParentMrt; }
  fcolor4& RefClearColor() { return mClearColor; }
  void SetContext(GfxTarget* pctx) { mpContext = pctx; }
  void SetTexture(Texture* ptex) { mpTexture = ptex; }
  void SetMaterial(GfxMaterial* pmtl) { mpMaterial = pmtl; }

  //////////////////////////////////////////////

  void RenderMatOrthoQuad(const SRect& ViewportRect,
                          const SRect& QuadRect,
                          GfxMaterial* pmat,
                          float fu0          = 0.0f,
                          float fv0          = 0.0f,
                          float fu1          = 1.0f,
                          float fv1          = 1.0f,
                          float* uv2         = NULL,
                          const fcolor4& clr = fcolor4::White());

  void RenderMatOrthoQuads(const OrthoQuads& oquads);

  //////////////////////////////////////////////

  void SetRootWidget(ui::Widget* pwidg) { mRootWidget = pwidg; }

  //////////////////////////////////////////////

  virtual void BeginFrame(void);
  virtual void EndFrame(void);
  virtual void CreateContext();

protected:
  ui::Widget* mRootWidget;
  GfxTarget* mpContext;
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
  GfxBuffer* mParent;
  RtGroup* mParentRtGroup;
  void* mPlatformHandle;
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

class GfxWindow : public GfxBuffer {
public:
  //////////////////////////////////////////////

  GfxWindow(int iX, int iY, int iW, int iH, const std::string& name = "NoName", void* pdata = 0);
  virtual ~GfxWindow();

  //////////////////////////////////////////////

  virtual void CreateContext();

  virtual void OnShow() {}

  virtual void GotFocus(void) { mbHasFocus = true; }
  virtual void LostFocus(void) { mbHasFocus = false; }
  bool HasFocus() const { return mbHasFocus; }

  //////////////////////////////////////////////

  CTXBASE* mpCTXBASE;
  bool mbHasFocus;
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

class GfxEnv : public NoRttiSingleton<GfxEnv> {
  friend class GfxBuffer;
  friend class GfxWindow;
  friend class GfxTarget;
  //////////////////////////////////////////////////////////////////////////////

public:
  GfxTarget* GetLoaderTarget() const { return gLoaderTarget; }
  void SetLoaderTarget(GfxTarget* ptarget);

  recursive_mutex& GetGlobalLock() { return mGfxEnvMutex; }

  //////////////////////////////////////////////////////////////////////////////
  // Contex Factory

  GfxEnv();

  void RegisterWinContext(GfxWindow* pWin);

  //////////////////////////////////////////////////////////////////////////////

  GfxBuffer* GetMainWindow(void) { return mpMainWindow; }
  void SetMainWindow(GfxWindow* pWin) { mpMainWindow = pWin; }

//////////////////////////////////////////////////////////////////////////////
#if defined(_WIN32) && (!(defined(_XBOX)))
  static HWND GetMainHWND(void) { return GetRef().mpMainWindow->GetContext()->GetHWND(); }
#endif
  //////////////////////////////////////////////////////////////////////////////

  static GfxMaterial* GetDefaultUIMaterial(void) { return GetRef().mpUIMaterial; }
  static GfxMaterial* GetDefault3DMaterial(void) { return GetRef().mp3DMaterial; }

  static void SetTargetClass(const rtti::Class* pclass) { gpTargetClass = pclass; }
  static const rtti::Class* GetTargetClass() { return gpTargetClass; }
  void SetRuntimeEnvironmentVariable(const std::string& key, const std::string& val);
  const std::string& GetRuntimeEnvironmentVariable(const std::string& key) const;

  void PushCreationParams(const GfxTargetCreationParams& p) { mCreationParams.push(p); }
  void PopCreationParams() { mCreationParams.pop(); }
  const GfxTargetCreationParams& GetCreationParams() { return mCreationParams.top(); }

  static DynamicVertexBuffer<SVtxV12C4T16>& GetSharedDynamicVB();
  static DynamicVertexBuffer<SVtxV12N12B12T8C4>& GetSharedDynamicVB2();
  static DynamicVertexBuffer<SVtxV16T16C16>& GetSharedDynamicV16T16C16();

  //////////////////////////////////////////////////////////////////////////////
protected:
  //////////////////////////////////////////////////////////////////////////////

  GfxMaterial* mpUIMaterial;
  GfxMaterial* mp3DMaterial;
  GfxWindow* mpMainWindow;
  GfxTarget* gLoaderTarget;

  orkvector<GfxBuffer*> mvActivePBuffers;
  orkvector<GfxBuffer*> mvActiveWindows;
  orkvector<GfxBuffer*> mvInactiveWindows;

  DynamicVertexBuffer<SVtxV12C4T16> mVtxBufSharedVect;
  DynamicVertexBuffer<SVtxV12N12B12T8C4> mVtxBufSharedVect2;
  DynamicVertexBuffer<SVtxV16T16C16> _vtxBufSharedV16T16C16;
  orkmap<std::string, std::string> mRuntimeEnvironment;
  orkstack<GfxTargetCreationParams> mCreationParams;
  recursive_mutex mGfxEnvMutex;

  static const rtti::Class* gpTargetClass;
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

class DrawHudEvent : public ork::event::Event {
  RttiDeclareConcrete(DrawHudEvent, ork::event::Event);

public:
  DrawHudEvent(GfxTarget* target = NULL, int camera_number = 1)
      : mTarget(target)
      , mCameraNumber(camera_number) {}

  GfxTarget* GetTarget() const { return mTarget; }
  void SetTarget(GfxTarget* target) { mTarget = target; }

  int GetCameraNumber() const { return mCameraNumber; }
  void SetCameraNumber(int camera_number) { mCameraNumber = camera_number; }

private:
  GfxTarget* mTarget;
  int mCameraNumber;
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2

#define gGfxEnv ork::lev2::GfxEnv::GetRef()
