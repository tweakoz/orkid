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

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::Context, "Context")

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

bool Context::hiDPI() const {
  return _HIDPI();
}

float Context::currentDPI() const {
  return _currentDPI();
}

///////////////////////////////////////////////////////////////////////////////

void Context::beginFrame(void) {

  makeCurrentContext();
  auto mainrect = mainSurfaceRectAtOrigin();
  FBI()->setViewport(mainrect);
  FBI()->setScissor(mainrect);

  FBI()->BeginFrame();
  GBI()->BeginFrame();
  FXI()->BeginFrame();

  PushModColor(fcolor4::White());
  MTXI()->PushMMatrix(fmtx4::Identity());
  MTXI()->PushVMatrix(fmtx4::Identity());
  MTXI()->PushPMatrix(fmtx4::Identity());

  mpCurrentObject = 0;

  mRenderContextInstData = 0;

  _doBeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

void Context::endFrame(void) {
  GBI()->EndFrame();
  MTXI()->PopMMatrix();
  MTXI()->PopVMatrix();
  MTXI()->PopPMatrix();
  FBI()->EndFrame();

  PopModColor();
  mbPostInitializeContext = false;
  _doEndFrame();

  miTargetFrame++;
}

/////////////////////////////////////////////////////////////////////////

Context::Context()
    : miModColorStackIndex(0)
    , mbPostInitializeContext(true)
    , mCtxBase(nullptr)
    , mpCurrentObject(nullptr)
    , mFramePerfItem(CreateFormattedString("<target:%p>", this))
    , miW(0)
    , miH(0)
    , miTargetFrame(0)
    , mRenderContextInstData(nullptr)
    , miDrawLock(0)
    , mPlatformHandle(nullptr)
    , meTargetType(TargetType::NONE) {

  static CompositingData _gdata;
  static auto _gimpl = _gdata.createImpl();
  static auto RCFD   = new RenderContextFrameData(this);
  RCFD->_cimpl       = _gimpl;
  _defaultrcfd       = RCFD;
  pushRenderContextFrameData(_defaultrcfd);
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

void* Context::BeginLoad() {
  return _doBeginLoad();
}
void Context::EndLoad(void* ploadtok) {
  _doEndLoad(ploadtok);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
