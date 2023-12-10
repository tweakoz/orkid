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

void Context::triggerFrameDebugCapture(){
  _isFrameDebugCapture = true;
  _doTriggerFrameDebugCapture();
}

///////////////////////////////////////////////////////////////////////////////

void Context::beginFrame(bool visual) {
  _is_visual_frame = visual;
  _doBeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

void Context::endFrame(void) {
  for (auto l : _onBeforeDoEndFrameOneShotCallbacks)
    l();
  _onBeforeDoEndFrameOneShotCallbacks.clear();
  _doEndFrame();
}

/////////////////////////////////////////////////////////////////////////

commandbuffer_ptr_t Context::beginRecordCommandBuffer(renderpass_ptr_t rpass,std::string name){
  return _beginRecordCommandBuffer(rpass,name);
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

RenderSubPass::RenderSubPass(){
  _commandbuffer = std::make_shared<CommandBuffer>();
}

/////////////////////////////////////////////////////////////////////////

size_t DEFAULT_WINDOW_WIDTH = 1280;
size_t DEFAULT_WINDOW_HEIGHT = 720;

Context::Context()
    : meTargetType(TargetType::NONE)
    , miW(DEFAULT_WINDOW_WIDTH)
    , miH(DEFAULT_WINDOW_HEIGHT)
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
