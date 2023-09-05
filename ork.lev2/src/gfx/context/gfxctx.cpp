////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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

ImplementReflectionX(ork::lev2::Context, "Context");

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

bool Context::hiDPI() const {
  return _HIDPI();
}

float Context::currentDPI() const {
  return _currentDPI();
}

void Context::describeX(class_t* clazz) {
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

  ////////////////////////
  _defaultCommandBuffer = _cmdbuf_pool.allocate();
  _doBeginFrame();
  ////////////////////////

  for (auto l : _onBeginFrameCallbacks)
    l();
}

///////////////////////////////////////////////////////////////////////////////

void Context::endFrame(void) {


  for (auto l : _onEndFrameCallbacks)
    l();

  GBI()->EndFrame();
  MTXI()->PopMMatrix();
  MTXI()->PopVMatrix();
  MTXI()->PopPMatrix();
  FBI()->EndFrame();

  PopModColor();
  mbPostInitializeContext = false;
  ////////////////////////
  _doEndFrame();
  _cmdbuf_pool.deallocate(_defaultCommandBuffer);
  _defaultCommandBuffer = nullptr;
  ////////////////////////

  miTargetFrame++;


}

/////////////////////////////////////////////////////////////////////////

commandbuffer_ptr_t Context::beginRecordCommandBuffer(){
  return _beginRecordCommandBuffer();
}
void Context::endRecordCommandBuffer(commandbuffer_ptr_t cmdbuf){
  _endRecordCommandBuffer(cmdbuf);
}

void Context::beginRenderPass(renderpass_ptr_t pass){
  _beginRenderPass(pass);
}
void Context::endRenderPass(renderpass_ptr_t pass){
  _endRenderPass(pass);
}
void Context::beginSubPass(rendersubpass_ptr_t pass){
  _beginSubPass(pass);
}
void Context::endSubPass(rendersubpass_ptr_t pass){
  _endSubPass(pass);
}

/////////////////////////////////////////////////////////////////////////

Context::Context()
    : meTargetType(TargetType::NONE)
    , miW(0)
    , miH(0)
    , miModColorStackIndex(0)
    , miTargetFrame(0)
    , miDrawLock(0)
    , mbPostInitializeContext(true)
    , mFramePerfItem(CreateFormattedString("<target:%p>", this)) {

  static CompositingData _gdata;
  static auto _gimpl = _gdata.createImpl();
  static auto RCFD   = new RenderContextFrameData(this);
  RCFD->pushCompositor(_gimpl);
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

load_token_t Context::BeginLoad() {
  return _doBeginLoad();
}
void Context::EndLoad(load_token_t ploadtok) {
  _doEndLoad(ploadtok);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
