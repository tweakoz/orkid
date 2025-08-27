////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/object/AutoConnector.h>
#include <ork/lev2/gfx/ctxbase.h>
#include <ork/kernel/taskgraph.h>
#include <ork/kernel/opq.h>
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////

bool sbExit = false;

ImplementReflectionX(ork::lev2::Context, "Context");

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

static logchannel_ptr_t logchan_ctx = logger()->configureChannel("GFXCONTEXT", fvec3(0.3, 0.8, 0.8), true);
static logchannel_ptr_t logchan_tg = logger()->getChannel("TASKGRAPH");

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

void LoadingPhase::join() {
  // Ensure we're not on main thread to prevent deadlock
  ork::opq::assertNotOnQueue(opq::mainSerialQueue());
  
  // Wait for all operations to complete
  // The operations are processed by the main thread elsewhere
  // This just waits until they're done
  while (_load_operations.atomicCopy().size() > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
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
  bool done = false;
  int counter = 0;
  float t0 = _ctxtimer.SecsSinceStart();
  while(not done) {
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

      float t1 = _ctxtimer.SecsSinceStart();
      float elapsed = t1 - t0;
      if(elapsed>0.03f) {
        //logchan_ctx->log("Context: breaking out of loading phase operation loop after %f seconds", t1-t0);
        _ctxtimer.Start();
        done = true;
      }
    }
    else {
      done = true;
    }

  }
}

///////////////////////////////////////////////////////////////////////////////

void Context::beginFrame(bool visual) {

  OrkAssert(_currentPhase == 0); 
  _currentPhase = "INFRAME"_crcu;

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


  mRenderContextInstData = 0;
  _doBeginFrame();
  FBI()->PushRtGroup(FBI()->_ensureMainRtg().get()); // implicit renderpass api

  /////////////////////////////////////
  // call onBeginFrame callbacks
  /////////////////////////////////////

  for (auto l : _onBeginFrameCallbacks)
    l();

  _onBeginFrameCallbacks.clear();
  
  /////////////////////////////////////
  // Process deferred context operations
  /////////////////////////////////////
  
  GfxEnv::GetRef().processDeferredContextOps(this);

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


  FBI()->PopRtGroup(); // pop main rtg

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

  OrkAssert(_currentPhase == "INFRAME"_crcu); 
  _currentPhase = 0;

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

  printf("Context::Context() this<%p>\n", this);
  _ctxtimer.Start();
  _primitives_interface = std::make_shared<PrimitivesInterface>(this);

  static CompositingData _gdata;
  static auto _gimpl = _gdata.createImpl();
  auto RCFD          = std::make_shared<RenderContextFrameData>(this);
  RCFD->pushCompositor(_gimpl);
  _defaultrcfd = RCFD;
  pushRenderContextFrameData(RCFD);

    mpCurrentObject = nullptr;

}

///////////////////////////////////////////////////////////////////////////////

Context::~Context() {
}

///////////////////////////////////////////////////////////////////////////////

void Context::gpuInit() {
  // Initialize GPU-dependent resources
  if (_primitives_interface) {
    _primitives_interface->gpuInit();
  }
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

contextexecutor_ptr_t Context::createContextExecutor() {
  return std::make_shared<ContextExecutor>(this);
}

///////////////////////////////////////////////////////////////////////////////

ContextExecutor::ContextExecutor(context_rawptr_t ctx)
  : _context(ctx) {
}

///////////////////////////////////////////////////////////////////////////////

void ContextExecutor::executePhase(taskphase_ptr_t phase) {

  // Ensure we're not on main thread to prevent deadlock
  ork::opq::assertNotOnQueue(opq::mainSerialQueue());

  if (phase->_tasks.empty()) {
    return;
  }
    
  // Create a loading phase for GPU operations
  auto loading_phase = _context->newLoadingPhase();
  auto graph = phase->_graph.lock();
  // Enqueue all tasks to the loading phase
  for (auto task : phase->_tasks) {
    loading_phase->enqueueOperation([=](Context* ctx) {
      task->_func(graph); 
      TaskGraph::g_task_index += 1;
      if((TaskGraph::g_task_index&0x3)==0) {
        logchan_tg->log("TaskGraphs tasks completed: %zu", TaskGraph::g_task_index.load());
      }
    });
  }
  
  // Wait for all GPU operations in this phase to complete
  loading_phase->join();
}
void ContextExecutor::emptyFrame( taskgraph_ptr_t self,                            //
                                  const std::string& name,                         //
                                  contextexecutor_ptr_t executor,                     //
                                  taskphasecomplete_func_t on_completion) {        //
  auto new_phase = std::make_shared<TaskPhase>(self,name,executor,on_completion);
  self->_phases.push_back(new_phase);
  new_phase->task("fence", [=](taskgraph_ptr_t g) {
    ::usleep(1<<20);
    // No-op task to act as a synchronization point
  });  
}
///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
