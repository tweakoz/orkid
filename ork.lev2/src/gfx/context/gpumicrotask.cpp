////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gpumicrotask.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/kernel/async_tracker.h>
#include <ork/util/logger.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_gpumt = logger()->configureChannel("GPUMICROTASK", fvec3(0.9, 0.4, 0.1), true);

void GpuFrameTiming::_ensureEnvLoaded() {
  if (_envLoaded)
    return;
  _envLoaded = true;
  if (const char* v = std::getenv("ORKID_MT_TARGET_MS"))
    _targetMs = float(atof(v));
  if (const char* v = std::getenv("ORKID_MT_SAFETY_MS"))
    _safetyMs = float(atof(v));
  if (const char* v = std::getenv("ORKID_MT_MAX_US"))
    _maxBudgetUs = int64_t(atoll(v));
  _traceEnabled = (std::getenv("ORKID_MT_TRACE") != nullptr);
  _traceTimer.Start();
  _schedTraceTimer.Start(); // un-started Timer reads ~0 elapsed -> the sched-active
                            // branch could never reach its 1s threshold and the
                            // sched= line never printed (found in joinnarrow gates)
}

///////////////////////////////////////////////////////////////////////////////

void GpuFrameTiming::noteFrame(float cpu_frame_ms, float present_idle_ms, float gpu_frame_ms) {
  _ensureEnvLoaded();

  constexpr float kAlpha = 0.1f; // EMA smoothing
  auto ema = [kAlpha](float& acc, float sample) {
    acc = (acc < 0.0f) ? sample : (1.0f - kAlpha) * acc + kAlpha * sample;
  };

  ema(_cpuFrameMsEma, cpu_frame_ms);
  ema(_presentIdleMsEma, present_idle_ms);
  // COMFORT-1: what the scheduler's slices cost the CPU last frame. Fed from
  // noteSchedulerTelemetry (same frame, set just above this call), and the ONLY
  // signal that sees an inline GPU job's fence wait — see the header.
  ema(_microtaskCpuMsEma, float(_schedSpentUs) * 0.001f);

  // T2: reject any raw reading outside the sane window before it can pollute
  // the EMA or be used for this frame's budget — unvalidated MoltenVK
  // timestamp emulation must never produce a negative/huge budget.
  bool sane = gpu_frame_ms >= 0.0f
      && gpu_frame_ms <= 4.0f * _targetMs
      && gpu_frame_ms >= 0.05f * _targetMs;
  if (sane)
    ema(_gpuFrameMsEma, gpu_frame_ms);

  // Â§2.2: the budget source is the EMA's EXISTENCE, not this frame's raw â
  // per-frame availability legitimately flaps (lag-2 no-wait reads; MoltenVK
  // async completion) and must not flap the budget with it.
  bool have_ts      = (_gpuFrameMsEma >= 0.0f);
  _lastSourceIsTs   = have_ts;
  float headroom_ms = have_ts ? (_targetMs - _gpuFrameMsEma) : _presentIdleMsEma;
  // COMFORT-1: charge the microtask slices' own wall against the headroom that
  // is about to be handed back to them.
  headroom_ms -= std::max(_microtaskCpuMsEma, 0.0f);
  float budget_ms   = headroom_ms - _safetyMs;
  float max_ms      = float(_maxBudgetUs) * 0.001f;
  budget_ms         = std::clamp(budget_ms, 0.0f, max_ms);
  _lastBudgetUs     = int64_t(budget_ms * 1000.0f);

  _maybeTrace();
}

///////////////////////////////////////////////////////////////////////////////

void GpuFrameTiming::_maybeTrace() {
  if (!_traceEnabled)
    return;
  // Scheduler activity is BURSTY (loading drains in well under a second) — a
  // shared 1/sec tick hides exactly the interesting moments. sched-activity
  // lines therefore rate-limit on their OWN 1/sec timer; quiet frames keep the
  // regular shared tick.
  bool sched_active = (_schedSlices > 0);
  if (sched_active) {
    if (_schedTraceTimer.SecsSinceStart() < 1.0)
      return;
    _schedTraceTimer.Start();
    printf(
        "[gpumt] gpu_ms=%.2f idle_ms=%.2f mt_ms=%.2f budget_us=%lld src=%s sched=%d/%lldus q=%d\n",
        _gpuFrameMsEma,
        _presentIdleMsEma,
        _microtaskCpuMsEma,
        (long long)_lastBudgetUs,
        _lastSourceIsTs ? "ts" : "idle",
        _schedSlices,
        (long long)_schedSpentUs,
        _schedQueueDepth);
  } else {
    if (_traceTimer.SecsSinceStart() < 1.0)
      return;
    _traceTimer.Start();
    printf(
        "[gpumt] gpu_ms=%.2f idle_ms=%.2f mt_ms=%.2f budget_us=%lld src=%s\n",
        _gpuFrameMsEma,
        _presentIdleMsEma,
        _microtaskCpuMsEma,
        (long long)_lastBudgetUs,
        _lastSourceIsTs ? "ts" : "idle");
  }
  fflush(stdout);
}

///////////////////////////////////////////////////////////////////////////////
// LoadingPhaseMicrotask — MT1 client #1 (§2.5). One slice = pop ONE
// LoadingPhase and run ALL its ops (T6 phase granularity — budget is checked
// BETWEEN phases by the scheduler, never inside one). Estimate = EMA of recent
// phase costs (seed 2000µs). PERMANENT: never dropped; a false from runSlice
// means "the queue is empty right now" (idle this frame), not "complete".
///////////////////////////////////////////////////////////////////////////////

namespace {

struct LoadingPhaseMicrotask final : public GpuMicrotask {

  LoadingPhaseMicrotask() {
    _name          = "LoadingPhase";
    _class         = MicrotaskClass::SOFT_DEADLINE; // §2.5: soft-deadline, always-due
    _deadlineFrame = -1;                            // -1 = always past-due
    _permanent     = true;
  }

  int64_t sliceEstimateUs() const override {
    return int64_t(_phaseCostEmaUs);
  }

  bool runSlice(MicrotaskContext& mctx) override {
    Timer t;
    t.Start();
    // T6: exactly one phase. T5: _runOneLoadingPhase snapshots the phase's op
    // list into a LOCAL (never static) before running — the same torn-function
    // discipline as the legacy _loadingPhaseOperations.
    bool ran = mctx._gfxctx->_runOneLoadingPhase();
    if (not ran)
      return false; // empty queue → idle this frame (permanent task not dropped)
    float cost_us   = float(t.SecsSinceStart() * 1.0e6);
    _phaseCostEmaUs = (1.0f - kAlpha) * _phaseCostEmaUs + kAlpha * cost_us; // §2.5 EMA
    return true;    // a phase ran; more may remain (scheduler re-picks; empty → idle)
  }

  static constexpr float kAlpha = 0.2f;
  float _phaseCostEmaUs         = 2000.0f; // §2.5 seed
};

} // anonymous namespace

///////////////////////////////////////////////////////////////////////////////

static constexpr uint64_t kStarvationFrames   = 300;   // §2.3 MAINTENANCE floor
static constexpr int64_t  kMaxEstimateScaleQ16 = int64_t(65536) << 8; // T1 guardrail: 256x cap so a chronically-mis-estimating task can't explode the scale
static constexpr int      kMaxSlicesPerFrame   = 65536; // termination guard (0-cost slices can't spin the drain)
static constexpr int64_t  kUnboundedFrameBudgetUs = int64_t(1) << 50; // ~13 days: effectively-infinite per-frame budget for no-deadline (UNBOUNDED) contexts; far below INT64_MAX so budget-=measured stays overflow-safe
static constexpr uint64_t kDeferralEscapeFrames = 20;  // COMFORT-1: est>budget may DEFER a task, never drop it — after this many consecutive skips one slice runs regardless

///////////////////////////////////////////////////////////////////////////////
// MicrotaskCostRegistry (COMFORT-1) — see the header for the design and the
// failure mode it exists to avoid.
///////////////////////////////////////////////////////////////////////////////

namespace {

// Peak-tracking asymmetry: an under-estimate must be believed almost at once
// (that is the frame that hitches), while recovering from a one-time outlier
// takes a handful of slices. Symmetric smoothing gives one or the other, never
// both.
static constexpr double kScaleAlphaUp   = 0.5;
static constexpr double kScaleAlphaDown = 0.15;
// A single sample carries at most 8x into the EMA. Above that the estimate is
// already past any budget a frame can grant (8x the 3ms-class seeds is ~24ms vs
// the 25ms budget cap), so a bigger number changes no gating decision — it only
// lengthens the recovery. Cold-JIT warmup slices (measured 647ms for a first
// filter-material build) are exactly that case: real, non-recurring, and worth
// nothing as a persistent estimate.
static constexpr double kMaxScaleSampleQ16 = 65536.0 * 8.0;

std::mutex& _costRegistryMutex() {
  static std::mutex mtx;
  return mtx;
}
MicrotaskCostRegistry::entry_map_t& _costRegistry() {
  static MicrotaskCostRegistry::entry_map_t reg;
  return reg;
}

} // anonymous namespace

int64_t MicrotaskCostRegistry::seedScaleQ16(const std::string& key) {
  if (key.empty())
    return 65536;
  std::lock_guard<std::mutex> lk(_costRegistryMutex());
  auto& e = _costRegistry()[key];
  e._instancesSeeded++;
  e._lastSeedScaleQ16 = std::max<int64_t>(65536, int64_t(e._estScaleQ16Ema));
  return e._lastSeedScaleQ16;
}

///////////////////////////////////////////////////////////////////////////////

void MicrotaskCostRegistry::noteSlice(const std::string& key, int64_t raw_estimate_us, int64_t measured_us) {
  if (key.empty())
    return;
  std::lock_guard<std::mutex> lk(_costRegistryMutex());
  auto& e        = _costRegistry()[key];
  e._lastSliceUs = measured_us;
  if (measured_us > e._worstSliceUs) {
    e._secondWorstSliceUs = e._worstSliceUs;
    e._worstSliceUs       = measured_us;
  } else if (measured_us > e._secondWorstSliceUs) {
    e._secondWorstSliceUs = measured_us;
  }
  e._totalSliceUs += measured_us;
  e._sliceCount++;
  if (raw_estimate_us <= 0)
    return;
  // The scale this slice WOULD have needed for est == measured. Floored at 1x:
  // the task's own seed is the honest lower bound, and an estimate below it
  // buys nothing.
  double sample = double(measured_us) * 65536.0 / double(raw_estimate_us);
  sample        = std::clamp(sample, 65536.0, kMaxScaleSampleQ16);
  double alpha  = (sample > e._estScaleQ16Ema) ? kScaleAlphaUp : kScaleAlphaDown;
  e._estScaleQ16Ema = (1.0 - alpha) * e._estScaleQ16Ema + alpha * sample;
  e._estScaleQ16Ema = std::clamp(e._estScaleQ16Ema, 65536.0, double(kMaxEstimateScaleQ16));
}

///////////////////////////////////////////////////////////////////////////////

void MicrotaskCostRegistry::noteDeferral(const std::string& key, bool escaped) {
  if (key.empty())
    return;
  std::lock_guard<std::mutex> lk(_costRegistryMutex());
  auto& e = _costRegistry()[key];
  if (escaped)
    e._escapes++;
  else
    e._deferrals++;
}

///////////////////////////////////////////////////////////////////////////////

MicrotaskCostRegistry::entry_map_t MicrotaskCostRegistry::snapshot() {
  std::lock_guard<std::mutex> lk(_costRegistryMutex());
  return _costRegistry();
}

///////////////////////////////////////////////////////////////////////////////

void MicrotaskCostRegistry::resetMeasurements() {
  std::lock_guard<std::mutex> lk(_costRegistryMutex());
  for (auto& item : _costRegistry()) {
    auto& e               = item.second;
    e._lastSliceUs        = -1;
    e._worstSliceUs       = 0;
    e._secondWorstSliceUs = 0;
    e._totalSliceUs       = 0;
    e._sliceCount         = 0;
    e._deferrals          = 0;
    e._escapes            = 0;
  }
}

///////////////////////////////////////////////////////////////////////////////

GpuMicrotaskScheduler::~GpuMicrotaskScheduler() {
  // T13: teardown drops tasks (they are reconstructible) but must not leave the
  // async tracker holding phantom pending counts — asyncWorkEnd every still-
  // tracked task so a later settle/exit poll can reach zero. Never hangs.
  std::lock_guard<std::mutex> lk(_mutex);
  for (auto& t : _tasks) {
    if (t->_asyncTracked) {
      t->_asyncTracked = false;
      asyncWorkEnd(t->_name);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void GpuMicrotaskScheduler::_ensureEnvLoaded() {
  if (_envLoaded)
    return;
  _envLoaded = true;
  if (const char* v = std::getenv("ORKID_MT_MAX_US"))
    _maxBudgetUs = int64_t(atoll(v));
  _traceEnabled = (std::getenv("ORKID_MT_TRACE") != nullptr);
  // COMFORT-1 measurement hook: budget an OFFSCREEN context as if it presented
  // frames, so a headless gate can exercise the REALTIME budget/deferral path
  // the WINDOW context lives under. Deliberately NOT extended to the LOADING
  // context — that one MUST drain every pending phase per iteration (see the
  // kUnboundedFrameBudgetUs note).
  _forceRealtimeOffscreen = (std::getenv("ORKID_MT_FORCE_REALTIME") != nullptr);
}

///////////////////////////////////////////////////////////////////////////////

void GpuMicrotaskScheduler::_ensureLoadingClient() {
  if (_loadingTask)
    return;
  _loadingTask = std::make_shared<LoadingPhaseMicrotask>();
  // Registered DIRECTLY (not via enqueue): permanent + NO asyncWorkBegin. A
  // permanent task never completes, so tracking THE TASK would wedge the offscreen/
  // movie exit gate at asyncWorkPending()>0 forever (T13). Per-PHASE completion IS
  // tracked instead: submitLoadingPhase() holds a "texture_upload" marker from
  // submission until each phase's ops finish (producer-side; see gfxctx.cpp), so
  // asyncWorkPending() covers in-flight texture uploads without ever tracking this
  // never-ending task.
  std::lock_guard<std::mutex> lk(_mutex);
  _tasks.push_back(_loadingTask);
}

///////////////////////////////////////////////////////////////////////////////

void GpuMicrotaskScheduler::enqueue(gpumicrotask_ptr_t task) {
  if (nullptr == task)
    return;
  // COMFORT-1: a keyed task starts from what previous instances of its kind
  // cost, so the first (heaviest) slices of a repeating job are gated on a real
  // estimate instead of re-learning from scratch every cycle.
  if (not task->_costKey.empty())
    task->_estimateScaleQ16 = MicrotaskCostRegistry::seedScaleQ16(task->_costKey);
  task->_activeCostKey = task->_costKey;
  // asyncWorkBegin BEFORE the task is visible to the drainer, so a settle/exit
  // waiter can never observe pending==0 in the window between publish and begin.
  asyncWorkBegin(task->_name);
  std::lock_guard<std::mutex> lk(_mutex);
  task->_asyncTracked = true;
  _tasks.push_back(task);
}

///////////////////////////////////////////////////////////////////////////////

void GpuMicrotaskScheduler::cancel(const gpumicrotask_ptr_t& task) {
  if (nullptr == task)
    return;
  bool do_end = false;
  {
    std::lock_guard<std::mutex> lk(_mutex);
    if (task->_cancelled)
      return; // idempotent
    task->_cancelled = true;
    if (task->_asyncTracked) {
      task->_asyncTracked = false;
      do_end              = true;
    }
    // Removal from _tasks is left to runFrameSlices (owner thread) — its
    // snapshot skips cancelled tasks and prunes them. Cooperative: an
    // in-flight slice cannot be preempted (T1); cancel only stops future ones.
  }
  if (do_end)
    asyncWorkEnd(task->_name);
}

///////////////////////////////////////////////////////////////////////////////

int64_t GpuMicrotaskScheduler::_computeFrameBudget(Context* ctx) {
  // Auto-detect the budget mode (T11/T13): the WINDOW context is REALTIME
  // (measured headroom); every no-swapchain context (OFFSCREEN/LOADING/NONE)
  // is UNBOUNDED. meTargetType is the principled swapchain-presence signal —
  // it is set exactly where an output attaches (initializeWindow/Offscreen/
  // LoaderContext). setBudgetMode() overrides this.
  if (not _budgetModeExplicit) {
    bool realtime = (ctx->meTargetType == TargetType::WINDOW)
                 or (_forceRealtimeOffscreen and ctx->meTargetType == TargetType::OFFSCREEN);
    _budgetMode = realtime ? BudgetMode::REALTIME : BudgetMode::UNBOUNDED;
  }

  if (_budgetMode == BudgetMode::UNBOUNDED)
    // §2.2: a no-swapchain context (LOADING/OFFSCREEN) has NO per-frame deadline,
    // so its budget must NOT bound the drain — runFrameSlices must empty EVERY
    // pending phase each iteration (pre-MT1 _loadingPhaseOperations drain-all
    // semantics). This MUST exceed any single slice's estimate: a bounded value
    // here (the old 25ms _maxBudgetUs) collides with the est>budget skip
    // (line ~373, exempt only on a task's FIRST-EVER slice), so once one big XIR
    // upload balloons the shared LoadingPhase estimate EMA past the budget every
    // later phase is skipped FOREVER — the loader silently never drains it. Finite
    // (not INT64_MAX) to keep budget-=measured + the telemetry snapshot overflow-safe.
    return kUnboundedFrameBudgetUs;

  // REALTIME: the frame plumbing pushes the LAST frame's measured budget
  // (frame-lagged EMA, §2.2). Before any timing has arrived (_frameBudgetUs<0),
  // seed to max so loading proceeds while the timing model warms up.
  return (_frameBudgetUs < 0) ? _maxBudgetUs : _frameBudgetUs;
}

///////////////////////////////////////////////////////////////////////////////

void GpuMicrotaskScheduler::runFrameSlices(Context* ctx) {
  _ensureEnvLoaded();
  _ensureLoadingClient();
  _frameCounter++;

  int64_t budget       = _computeFrameBudget(ctx);
  int64_t start_budget = budget;

  ////////////////////////////////////////
  // Snapshot the active (non-cancelled) tasks per class under the lock —
  // enqueue()/cancel() race in from other threads. shared_ptrs keep tasks alive
  // across the UNLOCKED runSlice calls below (a slice can be long; holding the
  // mutex would block cross-thread enqueue on it).
  ////////////////////////////////////////

  std::vector<gpumicrotask_ptr_t> active[3];
  int queue_depth[3] = {0, 0, 0};
  {
    std::lock_guard<std::mutex> lk(_mutex);
    for (auto& t : _tasks) {
      if (t->_cancelled)
        continue;
      int ci = int(t->_class);
      active[ci].push_back(t);
      queue_depth[ci]++;
    }
  }

  // Pending loading-phase backlog — the q= trace value for MT1 (client #1). The
  // loading client owns this queue on the Context; peeked directly.
  int pending_phases = 0;
  ctx->_loadingPhases.atomicOp([&pending_phases](loadingphase_list_t& l) {
    pending_phases = int(l.size());
  });

  ////////////////////////////////////////
  // pick(): starved-MAINTENANCE floor > SOFT_DEADLINE(past-due first) >
  // OPPORTUNISTIC > MAINTENANCE; round-robin WITHIN a class (§2.3).
  ////////////////////////////////////////

  auto est_of = [](const gpumicrotask_ptr_t& t) -> int64_t {
    int64_t raw = t->sliceEstimateUs();
    if (raw < 0)
      raw = 0;
    return (raw * t->_estimateScaleQ16) >> 16;
  };

  auto pick_rr = [this](std::vector<gpumicrotask_ptr_t>& vec, int ci, bool prefer_due) -> gpumicrotask_ptr_t {
    size_t n = vec.size();
    if (0 == n)
      return nullptr;
    for (int pass = 0; pass < (prefer_due ? 2 : 1); pass++) {
      for (size_t i = 0; i < n; i++) {
        size_t idx = (_rrCursor[ci] + i) % n;
        auto& t    = vec[idx];
        if (0 == pass && prefer_due) {
          bool due = (t->_deadlineFrame < 0) || (uint64_t(t->_deadlineFrame) <= _frameCounter);
          if (not due)
            continue;
        }
        _rrCursor[ci] = (idx + 1) % n;
        return t;
      }
    }
    return nullptr;
  };

  auto remove_active = [&active](const gpumicrotask_ptr_t& t) {
    auto& vec = active[int(t->_class)];
    for (auto it = vec.begin(); it != vec.end(); ++it)
      if (*it == t) {
        vec.erase(it);
        return;
      }
  };

  ////////////////////////////////////////
  // the §2.3 drain loop
  ////////////////////////////////////////

  int     slices_this_frame = 0;
  int64_t spent_us          = 0;
  int64_t spent_by_class[3] = {0, 0, 0};
  std::vector<GpuMicrotask*>      ran_this_frame;
  std::vector<gpumicrotask_ptr_t> completed;

  // COMFORT-1: the loop is NOT gated on `budget > 0` any more. A frame with no
  // headroom at all (a GPU-bound app, or an offscreen context whose frames are
  // too short for a sane timestamp) used to skip the drain entirely — every
  // pending task silently starved for as long as that lasted, with no
  // accounting that could ever release them. Each iteration now either runs a
  // slice or removes exactly one task from `active`, so it still terminates,
  // and a zero budget defers tasks THROUGH the escape accounting instead of
  // around it.
  while (slices_this_frame < kMaxSlicesPerFrame) {

    gpumicrotask_ptr_t task = nullptr;

    // 1) MAINTENANCE starvation floor: one slice for a task starved > N frames
    //    when budget covers it, regardless of class ordering.
    for (auto& m : active[int(MicrotaskClass::MAINTENANCE)]) {
      if (m->_framesWaited > kStarvationFrames && budget > est_of(m)) {
        task = m;
        break;
      }
    }
    // 2) SOFT_DEADLINE (past-due first) > 3) OPPORTUNISTIC > 4) MAINTENANCE
    if (nullptr == task)
      task = pick_rr(active[int(MicrotaskClass::SOFT_DEADLINE)], int(MicrotaskClass::SOFT_DEADLINE), true);
    if (nullptr == task)
      task = pick_rr(active[int(MicrotaskClass::OPPORTUNISTIC)], int(MicrotaskClass::OPPORTUNISTIC), false);
    if (nullptr == task)
      task = pick_rr(active[int(MicrotaskClass::MAINTENANCE)], int(MicrotaskClass::MAINTENANCE), false);
    if (nullptr == task)
      break; // nothing runnable

    // Per-task frame quota. Checked BEFORE the estimate/deferral accounting: a
    // task that has had its share of this frame is not deferred (nothing was
    // unaffordable), it is simply done for now, so it must not accrue the
    // deferral escape that would let it back in.
    if (task->_maxSlicesPerFrame > 0) {
      if (task->_sliceFrameIndex != _frameCounter) {
        task->_sliceFrameIndex = _frameCounter;
        task->_slicesThisFrame = 0;
      }
      if (task->_slicesThisFrame >= task->_maxSlicesPerFrame) {
        remove_active(task);
        continue;
      }
    }

    // COMFORT-2: a multi-population task announces which population its NEXT
    // slice belongs to. Crossing into one re-seeds the scale from THAT key's
    // learned cost — otherwise the scale the previous population taught prices
    // the new one, which is the mis-pricing per-key learning exists to avoid.
    const std::string slice_key = task->sliceCostKey(); // by value: the cursor moves inside runSlice
    if (slice_key != task->_activeCostKey) {
      task->_activeCostKey    = slice_key;
      task->_estimateScaleQ16 = MicrotaskCostRegistry::seedScaleQ16(slice_key);
    }

    int64_t raw_est     = std::max<int64_t>(task->sliceEstimateUs(), 0);
    int64_t est         = (raw_est * task->_estimateScaleQ16) >> 16;
    bool    first_slice = (0 == task->_slicesRun);
    // COMFORT-1: the gate DEFERS, it must never DROP. A task the budget has
    // skipped for kDeferralEscapeFrames consecutive frames runs one slice
    // anyway — otherwise an estimate that outgrows every attainable budget
    // (warmup outlier, frame-rate collapse) wedges the job forever, which is
    // exactly the failure the fixed-seed estimate was written to dodge.
    bool escape = (task->_framesDeferred >= kDeferralEscapeFrames);

    // Affordable = there is budget left AND the estimate fits in it. First-slice
    // exemption: a task's very first slice always runs (as long as ANY budget
    // remains) so a mis-estimate still learns its real cost (T1).
    // _unboundedDrain (cold start) is exempt outright: its whole point is that
    // the frame it lands in pays for the result rather than the next fifty
    // frames rendering without it. Termination is unaffected — the task's step
    // plan is finite, a slice waiting off-thread hands the frame back
    // (_yieldFrame), and kMaxSlicesPerFrame still bounds the loop.
    bool affordable = task->_unboundedDrain //
                      or ((budget > 0) and ((est <= budget) or first_slice));

    // Would overshoot; leave for a later frame.
    if (not affordable && not escape) {
      task->_framesDeferred++;
      MicrotaskCostRegistry::noteDeferral(slice_key, false);
      remove_active(task);
      continue;
    }
    if (not affordable)
      MicrotaskCostRegistry::noteDeferral(slice_key, true);

    // T1 hard per-slice cap. An ESCAPING slice runs with a zero/negative budget
    // by definition, so it is granted its own estimate rather than what is left.
    int64_t grant = (est > 0) ? std::min(est * 2, std::max(budget, est)) : std::max(budget, int64_t(0));
    MicrotaskContext mctx{ctx, grant, _frameCounter};

    // MT1 measurement = CPU wall around runSlice. Per-slice GPU timestamps are a
    // later refinement (§2.4); the frame-whole GpuSliceTimer already exists but
    // is not yet subdivided per slice.
    uint64_t t0      = Timer::getSystemTick();
    bool     more    = task->runSlice(mctx);
    int64_t  measured = int64_t((Timer::getSystemTick() - t0) / NS_PER_US);

    // A permanent task returning false = "no work available right now" (idle),
    // NOT completion — do not count it, decrement budget, or drop it.
    if (task->_permanent && not more) {
      remove_active(task);
      continue;
    }

    task->_lastMeasuredUs = measured;
    task->_slicesRun++;
    task->_slicesThisFrame++;
    task->_framesDeferred = 0;
    // COMFORT-1: teach the cross-instance registry what this slice really cost
    // (and record it for the python-visible cost stats).
    MicrotaskCostRegistry::noteSlice(slice_key, raw_est, measured);
    slices_this_frame++;
    ran_this_frame.push_back(task.get());
    spent_by_class[int(task->_class)] += measured;

    // Auto-halving (T1): a slice that cost > 2x its estimate → double the
    // estimate scale (the scheduler will grant fewer such slices per frame).
    // Never recovers, per §2.3 — the task's own estimate EMA is the recovering
    // signal. Capped to avoid runaway (footgun guard).
    if (est > 0 && measured > 2 * est) {
      task->_estimateScaleQ16 = std::min(task->_estimateScaleQ16 * 2, kMaxEstimateScaleQ16);
      _telemetry._halvings++;
      if (measured > 4 * est && not task->_loggedLie) {
        task->_loggedLie = true; // once per task
        logchan_gpumt->log(
            "microtask <%s> lies about slice cost: measured %lldus > 4x estimate %lldus (T1)",
            task->_name.c_str(),
            (long long)measured,
            (long long)est);
      } else if (_traceEnabled) {
        logchan_gpumt->log(
            "microtask <%s> auto-halving: measured %lldus > 2x est %lldus, scaleQ16=%lld",
            task->_name.c_str(),
            (long long)measured,
            (long long)est,
            (long long)task->_estimateScaleQ16);
      }
    }

    budget -= measured;
    spent_us += measured;

    if (not more) {
      // Non-permanent completion → drop + asyncWorkEnd.
      completed.push_back(task);
      remove_active(task);
    }

    // OFF-THREAD WAIT HANDBACK — the slice ran (it is measured and accounted
    // like any other, ~0us), it just could not advance, so it leaves this
    // frame's active set instead of being re-picked into a spin.
    if (task->_yieldFrame) {
      task->_yieldFrame = false;
      remove_active(task);
    }
  }

  ////////////////////////////////////////
  // Prune completed/cancelled tasks + MAINTENANCE starvation frame accounting.
  ////////////////////////////////////////

  {
    std::lock_guard<std::mutex> lk(_mutex);
    auto ran = [&ran_this_frame](GpuMicrotask* p) {
      for (auto* r : ran_this_frame)
        if (r == p)
          return true;
      return false;
    };
    auto is_done = [&completed](const gpumicrotask_ptr_t& t) {
      if (t->_cancelled)
        return true;
      for (auto& c : completed)
        if (c == t)
          return true;
      return false;
    };
    _tasks.erase(std::remove_if(_tasks.begin(), _tasks.end(), is_done), _tasks.end());
    for (auto& t : _tasks) {
      if (t->_class != MicrotaskClass::MAINTENANCE)
        continue;
      t->_framesWaited = ran(t.get()) ? 0 : (t->_framesWaited + 1);
    }
  }

  // asyncWorkEnd for naturally-completed tasks (outside the lock).
  for (auto& c : completed) {
    if (c->_asyncTracked) {
      c->_asyncTracked = false;
      asyncWorkEnd(c->_name);
    }
  }

  ////////////////////////////////////////
  // Telemetry.
  ////////////////////////////////////////

  constexpr float kEmaAlpha       = 0.1f;
  _telemetry._slicesLastFrame     = slices_this_frame;
  _telemetry._spentUsLastFrame    = spent_us;
  _telemetry._lastBudgetUs        = start_budget;
  _telemetry._pendingWorkDepth    = pending_phases;
  for (int ci = 0; ci < 3; ci++) {
    _telemetry._queueDepth[ci]         = queue_depth[ci];
    _telemetry._classUsPerFrameEma[ci] = (1.0f - kEmaAlpha) * _telemetry._classUsPerFrameEma[ci] //
                                         + kEmaAlpha * float(spent_by_class[ci]);
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
