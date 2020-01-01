////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/object/AutoConnector.h>
#include <ork/lev2/gfx/ctxbase.h>

///////////////////////////////////////////////////////////////////////////////

bool sbExit = false;

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CTXBASE, "Lev2CTXBASE");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::Context, "Context")

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

void CTXBASE::Describe() {
  RegisterAutoSlot(ork::lev2::CTXBASE, Repaint);
}

///////////////////////////////////////////////////////////////////////////////

CTXBASE::CTXBASE(Window* pwin)
    : mbInitialize(true)
    , mpWindow(pwin)
    , mpTarget(0)
    , mUIEvent()
    , ConstructAutoSlot(Repaint)

{
  SetupSignalsAndSlots();
  mpWindow->mpCTXBASE = this;
}

void CTXBASE::pushRefreshPolicy(RefreshPolicyItem policy) {
  _policyStack.push(_curpolicy);
  _setRefreshPolicy(policy);
}
void CTXBASE::popRefreshPolicy() {
  auto prev = _policyStack.top();
  _setRefreshPolicy(prev);
}

///////////////////////////////////////////////////////////////////////////////

bool Context::hiDPI() const {
  return _HIDPI();
}

float Context::currentDPI() const {
  return _currentDPI();
}

///////////////////////////////////////////////////////////////////////////////

void Context::beginFrame(void) {
  FBI()->BeginFrame();
  GBI()->BeginFrame();
  FXI()->BeginFrame();

  //	IMI()->BeginFrame();

  if (GfxEnv::GetRef().GetDefaultUIMaterial())
    BindMaterial(GfxEnv::GetRef().GetDefaultUIMaterial());

  PushModColor(fcolor4::White());
  MTXI()->PushMMatrix(fmtx4::Identity);
  MTXI()->PushVMatrix(fmtx4::Identity);
  MTXI()->PushPMatrix(fmtx4::Identity);

  mpCurrentObject = 0;

  mRenderContextInstData = 0;

  _doBeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

void Context::endFrame(void) {
  //	IMI()->EndFrame();
  GBI()->EndFrame();
  MTXI()->PopMMatrix();
  MTXI()->PopVMatrix();
  MTXI()->PopPMatrix();
  FBI()->EndFrame();

  BindMaterial(0);
  PopModColor();
  mbPostInitializeContext = false;
  _doEndFrame();
}

/////////////////////////////////////////////////////////////////////////

Context::Context()
    : miModColorStackIndex(0)
    , mbPostInitializeContext(true)
    , mCtxBase(nullptr)
    , mpCurrentObject(nullptr)
    , mFramePerfItem(CreateFormattedString("<target:%p>", this))
    , miX(0)
    , miY(0)
    , miW(0)
    , miH(0)
    , miTargetFrame(0)
    , mRenderContextInstData(nullptr)
    , mbDeviceAvailable(true)
    , miDrawLock(0)
    , mPlatformHandle(nullptr)
    , mpCurMaterial(nullptr) {

  static CompositingData _gdata;
  static CompositingImpl _gimpl(_gdata);
  static auto RCFD = new RenderContextFrameData(this);
  RCFD->_cimpl     = &_gimpl;
  _defaultrcfd     = RCFD;
  pushRenderContextFrameData(_defaultrcfd);

  // PerformanceTracker::GetRef().AddItem( mFramePerfItem );
  // ork::lev2::GfxEnv::GetRef().SetLoaderTarget( this ) ;
}

///////////////////////////////////////////////////////////////////////////////

Context::~Context() {
}

///////////////////////////////////////////////////////////////////////////////

orkvector<DisplayMode*> Context::mDisplayModes;

bool Context::SetDisplayMode(unsigned int index) {
  if (index < mDisplayModes.size())
    return SetDisplayMode(mDisplayModes[index]);
  return false;
}

///////////////////////////////////////////////////////////////////////////////

void Context::BindMaterial(GfxMaterial* pmtl) {
  if (nullptr == pmtl)
    pmtl = currentMaterial();
  mpCurMaterial = pmtl;
  // OrkAssert( pMat );
}
void Context::PushMaterial(GfxMaterial* pmtl) {
  mMaterialStack.push(mpCurMaterial);
  mpCurMaterial = pmtl;
}
void Context::PopMaterial() {
  mpCurMaterial = mMaterialStack.top();
  mMaterialStack.pop();
}

void* Context::BeginLoad() {
  return _doBeginLoad();
}
void Context::EndLoad(void* ploadtok) {
  _doEndLoad(ploadtok);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
