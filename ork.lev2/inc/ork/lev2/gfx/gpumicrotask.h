////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// GpuMicrotaskScheduler MT0 — always-on GPU frame timing + headroom primitive.
//
// SHADOW MODE ONLY (JUL05_GPUMICROTASK.md milestone MT0): this header exists to
// MEASURE and LOG a per-frame GPU headroom budget. Nothing here is enforced —
// no scheduler, no registry, no microtask classes (those are MT1+, out of
// scope here). See ~/JUL05_GPUMICROTASK.md §2 for the full program; this file
// is the §2.1 "GpuFrameTiming" + "GpuSliceTimer" surface only.
//
// Owner laws this obeys:
//   T2 — MoltenVK timestamp emulation is unvalidated. GpuFrameTiming::noteFrame
//        sanity-clamps every raw GPU-ms sample (reject > 4x target or < 0.05x
//        target) and falls back to the present-idle signal for that frame.
//   T3 — NEVER coupled to ORK_PROFILER_ENABLE. This timer is its own always-on
//        primitive; it may feed the compiled-in profiler, never require it.
//   T13 — offscreen/headless (no swapchain) has no present-idle signal either;
//        callers pass 0 and the model degrades to src=idle with idle_ms=0
//        rather than crash or block.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <ork/kernel/timer.h>

namespace ork::lev2 {

struct Context;

///////////////////////////////////////////////////////////////////////////////
// GpuFrameTiming — EMA model of the §2.2 shadow budget computation.
//
//   target_ms   = env ORKID_MT_TARGET_MS (default 16.667, i.e. 60Hz)
//   frame_ms    = EMA(cpu frame wall) — tracked for telemetry, not yet consumed
//                 by the budget formula (kept per the §2.1 model shape; MT1+
//                 may fold it in)
//   gpu_ms      = EMA(sane GPU-frame timestamps), or -1 if never available/sane
//   idle_ms     = EMA(present-idle fence wait)
//   headroom_ms = gpu_ms sane this frame ? (target_ms - gpu_ms) : idle_ms
//   budget_us   = clamp((headroom_ms - safety_ms) * 1000, 0, max_budget_us)
//
// One instance per Context. MT0 wires it into VkContext only — there is no
// live GL backend in this tree today; the null-GpuSliceTimer path below IS
// what an unsupported/GL backend would take (deliverable 5: no GL timer
// queries are built).
///////////////////////////////////////////////////////////////////////////////

struct GpuFrameTiming {

  // Feed one frame's raw measurements. gpu_frame_ms < 0 means "no timestamp
  // reading this frame" (unsupported backend, or ORKID_MT_NO_TIMESTAMPS=1).
  // present_idle_ms is 0 where no swapchain exists (T13) — never block/guess
  // to obtain one.
  void noteFrame(float cpu_frame_ms, float present_idle_ms, float gpu_frame_ms);

  // MT1: stash the scheduler's last-frame activity so the (rate-limited)
  // [gpumt] trace line can append `sched=<slices>/<spent_us>us q=<depth>` —
  // ONLY when slices > 0 (silent otherwise). The scheduler runs in beginFrame;
  // this is fed from _doEndFrame before noteFrame, same frame.
  void noteSchedulerTelemetry(int slices, int64_t spent_us, int queue_depth) {
    _schedSlices     = slices;
    _schedSpentUs    = spent_us;
    _schedQueueDepth = queue_depth;
  }

  int64_t lastBudgetUs() const { return _lastBudgetUs; }
  bool    lastSourceIsTimestamps() const { return _lastSourceIsTs; }
  float   cpuFrameMsEma() const { return _cpuFrameMsEma; }
  float   presentIdleMsEma() const { return _presentIdleMsEma; }
  float   gpuFrameMsEma() const { return _gpuFrameMsEma; } // -1 if never sane

private:
  void _ensureEnvLoaded();
  void _maybeTrace();

  bool    _envLoaded    = false;
  float   _targetMs     = 16.6667f; // ORKID_MT_TARGET_MS
  float   _safetyMs     = 1.5f;     // ORKID_MT_SAFETY_MS
  int64_t _maxBudgetUs   = 25000;   // ORKID_MT_MAX_US
  bool    _traceEnabled = false;    // ORKID_MT_TRACE (rate-limited to 1 line/sec)

  float _cpuFrameMsEma    = -1.0f;
  float _presentIdleMsEma = -1.0f;
  float _gpuFrameMsEma    = -1.0f; // -1 until the first sane timestamp sample

  int64_t _lastBudgetUs   = 0;
  bool    _lastSourceIsTs = false;

  // MT1 scheduler activity for the trace line (see noteSchedulerTelemetry).
  int     _schedSlices     = 0;
  int64_t _schedSpentUs    = 0;
  int     _schedQueueDepth = 0;

  Timer _traceTimer;      // rate-limits quiet-frame ORKID_MT_TRACE lines (1/sec)
  Timer _schedTraceTimer; // separate 1/sec limiter for sched-activity lines (bursts)
};

///////////////////////////////////////////////////////////////////////////////
// GpuSliceTimer — abstract per-context GPU timestamp source. MT0 measures ONE
// whole-frame span (the microtask block doesn't exist yet — MT1 subdivides
// this into per-slice brackets, §2.4). Backend-specific implementations live
// alongside their backend (the Vulkan impl sits next to VkProfilerChannel,
// whose query-pool idioms it copies — but unlike VkProfilerChannel this is
// ALWAYS ON, never gated by ORK_PROFILER_ENABLE, T3).
//
// A null GpuSliceTimer (no instance) IS the "unsupported" path: the caps guard
// (context init, T2) decides whether to construct one at all; when it doesn't,
// callers pass gpu_frame_ms = -1 into GpuFrameTiming and the model falls back
// to present-idle.
///////////////////////////////////////////////////////////////////////////////

struct GpuSliceTimer {
  virtual ~GpuSliceTimer() = default;

  // Record the BEGIN timestamp into the frame's primary command buffer. Must be
  // called after the CB is begun (the query pool is reset here too — mirrors
  // VkProfilerChannel::frameBegin's shape).
  virtual void beginFrame() = 0;

  // Record the END timestamp, still inside the (not-yet-submitted) primary CB.
  virtual void endFrame() = 0;

  // Read back the whole-frame GPU ms. Call AFTER the frame's submit — on the
  // current (blocking, T4) submit path the WAIT_BIT readback is then free of
  // extra stalls. Returns -1.0f if no result is available (defensive only).
  virtual float readbackFrameMs() = 0;
};
using gpuslicetimer_ptr_t = std::shared_ptr<GpuSliceTimer>;

///////////////////////////////////////////////////////////////////////////////
// MT1 — the scheduler core (JUL05_GPUMICROTASK §2.1/§2.3/§2.5).
//
// ONE GpuMicrotaskScheduler per Context. ALL background GPU work trickles
// through it under the §2.2 measured budget (REALTIME) or unbounded (offscreen/
// loader). MT1's only client is LoadingPhase (client #1 — see gpumicrotask.cpp
// LoadingPhaseMicrotask); later milestones add radiance prefilter, terrain
// cook re-driving, etc.
//
// Not reflected — tasks are runtime constructs, nothing serializes. No python
// binding (T15): the scheduler is an engine pacing detail the DSL never sees.
///////////////////////////////////////////////////////////////////////////////

enum class MicrotaskClass : uint8_t {
  SOFT_DEADLINE = 0, // prefetch racing a window; may borrow budget, never exceed frame law
  OPPORTUNISTIC = 1, // probe refresh, LUT recompute, radiance prefilter
  MAINTENANCE   = 2, // cache eviction, mip trickle
};

enum class BudgetMode : uint8_t {
  REALTIME  = 0, // WINDOW context: budget from the §2.2 measured headroom
  UNBOUNDED = 1, // offscreen/movie/loader: budget = MAX (drain fast, no frame deadline)
};

// Handed to runSlice. A slice executes on the context-owner thread with the
// context current — same rights as a LoadingPhase op.
struct MicrotaskContext {
  Context* _gfxctx        = nullptr; // the owning context (render/owner thread)
  int64_t  _sliceBudgetUs = 0;       // what the scheduler grants THIS slice
  uint64_t _frameIndex    = 0;       // scheduler frame counter
};

///////////////////////////////////////////////////////////////////////////////
// GpuMicrotask — a RESUMABLE incremental unit. If work cannot slice at fine
// grain it does not belong here — bake it offline (§0). runSlice returning
// false = "no more slices" → the scheduler drops the task + asyncWorkEnd
// (permanent tasks excepted — see _permanent).
///////////////////////////////////////////////////////////////////////////////

struct GpuMicrotask {
  virtual ~GpuMicrotask() = default;
  virtual int64_t sliceEstimateUs() const           = 0; // honest estimate of ONE slice
  virtual bool    runSlice(MicrotaskContext& mctx)   = 0; // true = more slices remain
  virtual float   progress() const { return -1.0f; }      // optional, for HUD

  std::string    _name;                            // HUD + async-tracker tag
  MicrotaskClass _class        = MicrotaskClass::OPPORTUNISTIC;
  int64_t        _deadlineFrame = -1;              // SOFT_DEADLINE: frame it wants completion by; -1 = always-due

  // scheduler-owned (do not touch from task code) ///////////////////////////
  int64_t  _estimateScaleQ16 = 65536; // auto-halving multiplier (T1 feedback loop); never recovers (§2.3)
  int64_t  _lastMeasuredUs   = -1;
  uint64_t _slicesRun        = 0;
  bool     _permanent        = false; // permanent tasks (LoadingPhase) are never dropped; false-from-runSlice = idle
  bool     _cancelled        = false; // cooperative cancel — not run again
  bool     _asyncTracked     = false; // asyncWorkBegin fired for this task (so complete/cancel asyncWorkEnd once)
  bool     _loggedLie        = false; // T1: the "lies about slice cost" loud log is once-per-task
  uint64_t _framesWaited     = 0;     // MAINTENANCE starvation-floor accounting
};
using gpumicrotask_ptr_t = std::shared_ptr<GpuMicrotask>;

///////////////////////////////////////////////////////////////////////////////
// GpuMicrotaskScheduler — ONE per Context, owned by Context.
//
// enqueue()  : ANY thread (mutex; wraps asyncWorkBegin(tag=task name)).
// cancel()   : ANY thread, cooperative (not run again; asyncWorkEnd).
// runFrameSlices() : context-owner thread ONLY (the §1.3 beginFrame drain
//              point). Drains the §2.3 loop under the current frame budget.
// setFrameBudgetUs() : written by the frame plumbing after the LAST frame's
//              GpuFrameTiming::noteFrame — the scheduler uses the LAST frame's
//              budget for THIS frame's drain (frame-lagged EMAs make this
//              correct, §2.2).
// setBudgetMode() : REALTIME | UNBOUNDED. Auto-derived from the context's
//              target type (WINDOW→REALTIME, else UNBOUNDED, T13) unless set
//              explicitly here.
///////////////////////////////////////////////////////////////////////////////

struct GpuMicrotaskScheduler {

  struct Telemetry {
    float    _classUsPerFrameEma[3] = {0.0f, 0.0f, 0.0f}; // per-MicrotaskClass µs/frame EMA
    int      _queueDepth[3]         = {0, 0, 0};          // registered task count per class
    int      _slicesLastFrame       = 0;
    int64_t  _spentUsLastFrame      = 0;
    uint64_t _halvings              = 0;                  // cumulative auto-halving count
    int64_t  _lastBudgetUs          = 0;                  // budget granted to the last drain
    int      _pendingWorkDepth      = 0;                  // pending loading phases (the q= trace value, MT1)
  };

  ~GpuMicrotaskScheduler(); // T13: drop cleanly at teardown, asyncWorkEnd tracked tasks

  void enqueue(gpumicrotask_ptr_t task);       // any thread
  void cancel(const gpumicrotask_ptr_t& task); // any thread, cooperative
  void runFrameSlices(Context* ctx);           // context-owner thread ONLY

  void setFrameBudgetUs(int64_t budget_us) { _frameBudgetUs = budget_us; }
  void setBudgetMode(BudgetMode m) {
    _budgetMode         = m;
    _budgetModeExplicit = true;
  }
  const Telemetry& telemetry() const { return _telemetry; }

private:
  void    _ensureEnvLoaded();
  int64_t _computeFrameBudget(Context* ctx);
  void    _ensureLoadingClient();

  std::mutex                        _mutex;       // guards _tasks / enqueue / cancel
  std::vector<gpumicrotask_ptr_t>   _tasks;       // registered tasks (incl. the permanent loading client)
  gpumicrotask_ptr_t                _loadingTask; // permanent LoadingPhaseMicrotask (client #1)

  uint64_t   _frameCounter = 0;
  BudgetMode _budgetMode         = BudgetMode::UNBOUNDED; // safe default: no realtime deadline until proven WINDOW
  bool       _budgetModeExplicit = false;
  int64_t    _frameBudgetUs      = -1; // <0 = not yet plumbed; seeded to max on first drain
  size_t     _rrCursor[3]        = {0, 0, 0}; // per-class round-robin cursors (persist across frames)

  bool    _envLoaded   = false;
  int64_t _maxBudgetUs = 25000; // ORKID_MT_MAX_US (matches GpuFrameTiming cap)
  bool    _traceEnabled = false;

  Telemetry _telemetry;
};

} // namespace ork::lev2
