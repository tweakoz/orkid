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

void Context::submitLoadingPhase(loadingphase_ptr_t phase) {
  // Atomic publish of a producer-built phase. Pair with `std::make_shared
  // <LoadingPhase>()` + enqueueOperation(...) locally, then submit. This
  // closes the race that newLoadingPhase() opens (empty-phase visible
  // before ops are enqueued) for fire-and-forget producers.
  _loadingPhases.atomicOp([phase](loadingphase_list_t& unlocked) { unlocked.push_back(phase); });
}

///////////////////////////////////////////////////////////////////////////////
// Delayed destruction queue — see gfxenv.h declaration for rationale.
///////////////////////////////////////////////////////////////////////////////

void Context::enqueueDelayedDestroy(::ork::void_lambda_t fn, int delay_frames) {
  if (!fn) return;
  PendingDestroy entry{
    std::move(fn),
    uint64_t(miTargetFrame) + uint64_t(delay_frames < 0 ? 0 : delay_frames)};
  _pendingDestroys.atomicOp([&entry](pending_destroy_queue_t& q) {
    q.push_back(std::move(entry));
  });
}

void Context::_processPendingDestroys() {
  const int budget = _maxDestroysPerFrame;
  if (budget <= 0) return;
  std::vector<::ork::void_lambda_t> ripe;
  ripe.reserve(size_t(budget));
  _pendingDestroys.atomicOp([&](pending_destroy_queue_t& q) {
    const uint64_t now = uint64_t(miTargetFrame);
    while (!q.empty()
           && ripe.size() < size_t(budget)
           && q.front()._destroy_at_frame <= now) {
      ripe.push_back(std::move(q.front()._fn));
      q.pop_front();
    }
  });
  // Outside the lock: invoke each function, then it (and its captures)
  // destruct as `ripe` clears at scope end. By construction the wait is
  // >= MAX_FRAMES_IN_FLIGHT, so vkDestroy* / vkFreeMemory don't stall.
  for (auto& fn : ripe) {
    if (fn) fn();
  }
}

void LoadingPhase::enqueueOperation(gfxcontext_lambda_t l) {
  _load_operations.atomicOp([l](gfxcontext_lambda_list_t& unlocked) {
    unlocked.push_back(l);
  });
}

///////////////////////////////////////////////////////////////////////////////
// Per-context deferred-op queue. Drained at beginFrame against `this`.
// Migrated from GfxEnv's global queue (Phase 6.3 Variant B) — each context
// now owns its queue, so there's no cross-context drain race, and the
// enqueuer picks the target context explicitly.
///////////////////////////////////////////////////////////////////////////////

void Context::enqueueDeferredOp(ctx_lambda_t op) {
  // After shutdown(), resource dtors that fire because the Context is
  // tearing down (VkRtBufferImpl etc.) try to schedule their own
  // cleanup back onto this queue. The queue is mid-destruction by
  // then — pushing crashes the LockedResource teardown. Silently drop:
  // any work that would be deferred this late has nowhere safe to run,
  // and the resources owning the dtor are about to vanish anyway.
  if (_shutdown_done) return;
  _deferredOps.atomicOp([op](deferred_op_queue_t& unlocked) { unlocked.push(op); });
}

void Context::processDeferredOps() {
  _deferredOps.atomicOp([this](deferred_op_queue_t& unlocked) {
    while (!unlocked.empty()) {
      auto op = unlocked.front();
      unlocked.pop();
      op(this);
    }
  });
}

bool Context::hasDeferredOps() const {
  bool has = false;
  _deferredOps.atomicOp([&has](const deferred_op_queue_t& unlocked) {
    has = !unlocked.empty();
  });
  return has;
}
// No waitForDeferredOps — see gfxenv.h declaration. Polling from the
// owning thread deadlocks; cross-thread waits should use a completion
// callback enqueued onto the requesting thread's own context.

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
      if(0)printf("[VKMT-DBG] _loadingPhaseOperations ctx<%p> popped phase<%p>\n",
             (void*)this, (void*)phase.get());
      //fflush(stdout);
      // NOTE: this `ops` MUST be a local, not `static`. Multiple contexts
      // drain their own _loadingPhases concurrently (e.g. loader thread on
      // gloadercontext + render thread on its own context), so a static
      // would be a cross-thread race — one thread's snapshot overwriting
      // another's mid-iteration, leading to torn std::function objects
      // whose corrupt captures produce shared_ptrs with stale control
      // blocks (observed: shared_ptr<Texture>::~shared_ptr decrement on
      // freed memory inside brdfSetOp). Locals reallocate per call; the
      // cost is trivial vs. correctness.
      gfxcontext_lambda_list_t ops;
      phase->_load_operations.atomicOp([&ops](gfxcontext_lambda_list_t& unlocked) {
        ops = unlocked;
        unlocked.clear();
      });
      for (auto op : ops) {
        op(this);
        counter++;
      }
      ops.clear();

      float t1 = _ctxtimer.SecsSinceStart();
      float elapsed = t1 - t0;
      if(elapsed>0.03f) {
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

void Context::beginPrimaryCommandBuffer() {
  _doBeginPrimaryCommandBuffer();
}
void Context::endPrimaryCommandBuffer() {
  _doEndPrimaryCommandBuffer();
}
void Context::submitPrimaryCommandBuffer(){
  _doSubmitPrimaryCommandBuffer();
}
void Context::_doBeginPrimaryCommandBuffer() {
}
void Context::_doEndPrimaryCommandBuffer() {
}
void Context::_doSubmitPrimaryCommandBuffer(){
}

///////////////////////////////////////////////////////////////////////////////

void Context::beginFrame(bool visual) {
  OrkProfilerSampleBegin(CHANNEL_MAIN, SERIES_FRAME_ALL);
  OrkProfilerSampleScope(CHANNEL_MAIN, "begin_frame");

  OrkAssert(_currentPhase == 0);
  _currentPhase = "INFRAME"_crcu;

  _is_visual_frame = visual;

  makeCurrentContext();
  _doPreBeginFrame();

  _processBeginFrameBlockers();
  _processPendingDestroys();
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
  
  processDeferredOps();

  /////////////////////////////////////

  // TODO could this be changed to lockless?
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
  OrkProfilerSampleScope(CHANNEL_MAIN, "end_frame");

  FBI()->PopRtGroup(); // pop main rtg

  for (auto l : _onEndFrameCallbacks)
    l();

  GBI()->EndFrame();
  MTXI()->PopMMatrix();
  MTXI()->PopVMatrix();
  MTXI()->PopPMatrix();
  FBI()->EndFrame();
  FXI()->EndFrame();

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
  if(0)printf("exit Context::endFrame this<%p>\n", this);

  OrkProfilerSampleEnd(CHANNEL_MAIN, SERIES_FRAME_ALL);
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
    , mbPostInitializeContext(true) {

  if(0)printf("Context::Context() this<%p>\n", this);
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

void Context::shutdown() {
  // Idempotent — second call is a no-op so multiple teardown paths
  // (e.g. lev2 stopLoaderThread, app shutdown, dtor fallback) can all
  // call it without coordinating.
  if (_shutdown_done) return;

  // Phase 1: drain anything already queued, with the device + backend
  // resources still live. Self-enqueuing ops need multiple passes;
  // cap the loop to surface a runaway op instead of hanging.
  constexpr int kMaxDrainPasses = 1024;
  for (int i = 0; i < kMaxDrainPasses && hasDeferredOps(); ++i) {
    processDeferredOps();
  }

  // Phase 2: backend teardown. Subclass releases GPU resources
  // (VkRtBuffer dtors etc. — they enqueue more cleanup work onto our
  // queue as they release). Must run BEFORE we set _shutdown_done so
  // those enqueues actually land.
  _doShutdown();

  // Phase 3: drain the work the backend just produced.
  for (int i = 0; i < kMaxDrainPasses && hasDeferredOps(); ++i) {
    processDeferredOps();
  }

  // Phase 4: lock down further enqueues. Any later dtors (firing as
  // shared_ptr chains release during static destruction) will try to
  // push to this queue while it's mid-destruction — drop them
  // silently. Any work that arrives this late has nothing live to
  // process it anyway.
  _shutdown_done = true;
}

///////////////////////////////////////////////////////////////////////////////

void Context::gpuPreInit() {
  _onGpuPreInit();
  // Initialize GPU-dependent resources
  if (_primitives_interface) {
    _primitives_interface->gpuInit();
  }
}

void Context::gpuPostInit() {
  _onGpuPostInit();
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
  auto graph = phase->_graph;

  // Enqueue all tasks to the loading phase
  for (auto task : phase->_tasks) {
    loading_phase->enqueueOperation([=](Context* ctx) {
      task->_func(graph);
      TaskGraph::g_task_index += 1;
      size_t num_tasks = TaskGraph::g_tasks_pending.fetch_sub(1);
      logchan_tg->log("TaskGraph tasks pending: %zu", num_tasks);
    });
  }

  // Wait for all GPU operations in this phase to complete
  loading_phase->join();
}
void ContextExecutor::emptyFrame( taskgraph_wkptr_t self,                            //
                                  const std::string& name,                         //
                                  contextexecutor_ptr_t executor,                     //
                                  taskphasecomplete_func_t on_completion) {        //
  auto new_phase = std::make_shared<TaskPhase>(self,name,executor,on_completion);
  self.lock()->_phases.push_back(new_phase);
  new_phase->task("fence", [=](taskgraph_wkptr_t g) {
    //::usleep(1<<20);
    // No-op task to act as a synchronization point
  });  
}
///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
