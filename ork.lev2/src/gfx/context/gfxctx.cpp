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

int Context::mainSurfaceWidth() const {
  float content_scale = mCtxBase ? mCtxBase->_contentScaleX : 1.0f;
  return int(miW);
}
int Context::mainSurfaceHeight() const {
  float content_scale = mCtxBase ? mCtxBase->_contentScaleY : 1.0f;
  return int(miH);
}
float Context::mainSurfaceAspectRatio() const {
  return float(mainSurfaceWidth()) / float(mainSurfaceHeight());
}
ViewportRect Context::mainSurfaceRectAtWindowPos() const {
  return ViewportRect(0, 0, mainSurfaceWidth(), mainSurfaceHeight());
}
ViewportRect Context::mainSurfaceRectAtOrigin() const {
  return ViewportRect(0, 0, mainSurfaceWidth(), mainSurfaceHeight());
}
void Context::resizeMainSurface(int iw, int ih) {
  if((iw!=miW) or (ih!=miH)) {
    _doResizeMainSurface(iw, ih);
    miW = iw;
    miH = ih;
  }
}

///////////////////////////////////////////////////////////////////////////////

loadingphase_ptr_t Context::newLoadingPhase() {
  auto phase = std::make_shared<LoadingPhase>();
  _loadingPhases.atomicOp([phase](loadingphase_list_t& unlocked) { unlocked.push_back(phase); });
  return phase;
}

void LoadingPhase::enqueueOperation(gfxcontext_lambda_t l) {
  _load_operations.atomicOp([l](gfxcontext_lambda_list_t& unlocked) { unlocked.push_back(l); });
}

///////////////////////////////////////////////////////////////////////////////

void Context::enqueueGpuEvent(gpuevent_ptr_t evt) {
  _gpuEventQueue.push(evt);
}
void Context::registerGpuEventSink(gpueventsink_ptr_t sink) {
  _gpuEventSinks.atomicOp([sink](gpueventsink_map_t& unlocked) { unlocked.insert(std::make_pair(sink->_eventID, sink)); });
}

///////////////////////////////////////////////////////////////////////////////

bool Context::hiDPI() const {
  return _HIDPI();
}

float Context::currentDPI() const {
  return _currentDPI();
}

void Context::describeX(class_t* clazz) {
}

void Context::triggerFrameDebugCapture() {
  _isFrameDebugCapture = true;
  _doTriggerFrameDebugCapture();
}

///////////////////////////////////////////////////////////////////////////////

void Context::_processBeginFrameBlockers() {
  bool keep_going = true;
  while (keep_going) {
    keep_going = false;
    auto it    = _beginFrameBlockers.begin();
    if (it != _beginFrameBlockers.end()) {
      auto cb        = *it;
      bool processed = cb();
      if (processed) {
        _beginFrameBlockers.erase(it);
        keep_going = true;
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void Context::_loadingPhaseOperations() {
  loadingphase_ptr_t phase = nullptr;
  _loadingPhases.atomicOp([&phase](loadingphase_list_t& unlocked) {
    if (unlocked.size()) {
      phase = unlocked.front();
      unlocked.pop_front();
    }
  });
  if (phase) {
    static gfxcontext_lambda_list_t ops;
    phase->_load_operations.atomicOp([phase](gfxcontext_lambda_list_t& unlocked) {
      ops = unlocked;
      unlocked.clear();
    });

    for (auto op : ops) {
      op(this);
    }
    ops.clear();
  }
}

///////////////////////////////////////////////////////////////////////////////

void Context::beginFrame(bool visual) {

  _is_visual_frame = visual;

  makeCurrentContext();
  _doPreBeginFrame();

  _processBeginFrameBlockers();
  _loadingPhaseOperations();

  /////////////////////////////////////

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

  /////////////////////////////////////
  // call onBeginFrame callbacks
  /////////////////////////////////////

  for (auto l : _onBeginFrameCallbacks)
    l();

  _onBeginFrameCallbacks.clear();

  /////////////////////////////////////

  _gpuEventSinks.atomicOp([this](gpueventsink_map_t& unlocked) {
    while (not _gpuEventQueue.empty()) {
      auto event = _gpuEventQueue.front();
      auto it    = unlocked.find(event->_eventID);
      if (it != unlocked.end()) {
        auto sink = it->second;
        if (sink->_onEvent) {
          sink->_onEvent(event);
        }
        // it->second->onGpuEvent(event);
      }
      _gpuEventQueue.pop();
    }
  });
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

  for (auto l : _onBeforeDoEndFrameOneShotCallbacks)
    l();

  _onBeforeDoEndFrameOneShotCallbacks.clear();

  _doEndFrame();

  miTargetFrame++;
  _isFrameDebugCapture = false;
}

/////////////////////////////////////////////////////////////////////////

secondary_commandbuffer_ptr_t Context::beginRecordCommandBuffer(std::string named, rtgroup_rawptr_t rtg) {
  return _beginRecordCommandBuffer(named,rtg);
}
void Context::endRecordCommandBuffer(secondary_commandbuffer_ptr_t cmdbuf) {
  _endRecordCommandBuffer(cmdbuf);
}

void Context::enqueueSecondaryCommandBuffer(secondary_commandbuffer_ptr_t cmdbuf) {
  _doEnqueueSecondaryCommandBuffer(cmdbuf);
}

secondary_commandbuffer_ptr_t Context::_beginRecordCommandBuffer(std::string named,rtgroup_rawptr_t rtg) {
  return nullptr;
}
void Context::_endRecordCommandBuffer(secondary_commandbuffer_ptr_t cmdbuf) {
}

void Context::_doEnqueueSecondaryCommandBuffer(secondary_commandbuffer_ptr_t cmdbuf) {
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
  auto RCFD          = std::make_shared<RenderContextFrameData>(this);
  RCFD->pushCompositor(_gimpl);
  _defaultrcfd = RCFD;
  pushRenderContextFrameData(RCFD);
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

load_token_t Context::beginLoad() {
  return _doBeginLoad();
}
void Context::endLoad(load_token_t ploadtok) {
  _doEndLoad(ploadtok);
}

DebugGroup::DebugGroup(Context* ctx)
    : _context(ctx) {
}
DebugGroup::~DebugGroup() {
  if (_context) {
    _context->debugPopGroup();
  }
}

DebugGroup Context::debugPushGroupAutoRelease(const std::string str) {
  debugPushGroup(str, fvec4::Red());
  return DebugGroup(this);
}
void Context::debugPushGroup(const std::string str) {
  debugPushGroup(str, fvec4::Red());
}
void Context::debugMarker(const std::string str) {
  debugMarker(str, fvec4::Red());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
