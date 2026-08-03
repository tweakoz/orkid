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
#include <map>
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
//   frame_ms    = EMA(cpu frame wall) — telemetry only. It is NOT subtracted
//                 from headroom: the app's CPU work overlaps the GPU work
//                 gpu_ms already accounts for, so charging both double-counts
//                 the same frame. The microtask-specific slice cost below is
//                 the part gpu_ms genuinely cannot see.
//   gpu_ms      = EMA(sane GPU-frame timestamps), or -1 if never available/sane
//   idle_ms     = EMA(present-idle fence wait)
//   mt_cpu_ms   = EMA(the scheduler's per-frame slice wall, noteSchedulerTelemetry)
//   headroom_ms = (gpu_ms sane this frame ? (target_ms - gpu_ms) : idle_ms) - mt_cpu_ms
//   budget_us   = clamp((headroom_ms - safety_ms) * 1000, 0, max_budget_us)
//
// COMFORT-1: mt_cpu_ms is why the formula is honest about inline GPU jobs. A
// slice that runs Context::executeInlineGpuJob submits its OWN command buffer
// and blocks on its OWN fence BEFORE the frame's primary CB is submitted, so
// none of that time lands inside the frame timestamp bracket — a frame that
// just spent 17ms fence-waiting on a refilter slice would otherwise be judged
// to have a full frame of headroom and be handed another such slice. The EMA
// decays over the frames where no slice runs, which is what paces a heavy
// sliced job instead of hitching on it.
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
  float   gpuFrameMsEma() const { return _gpuFrameMsEma; }        // -1 if never sane
  float   microtaskCpuMsEma() const { return _microtaskCpuMsEma; } // -1 until the first frame

private:
  void _ensureEnvLoaded();
  void _maybeTrace();

  bool    _envLoaded    = false;
  float   _targetMs     = 16.6667f; // ORKID_MT_TARGET_MS
  float   _safetyMs     = 1.5f;     // ORKID_MT_SAFETY_MS
  int64_t _maxBudgetUs   = 25000;   // ORKID_MT_MAX_US
  bool    _traceEnabled = false;    // ORKID_MT_TRACE (rate-limited to 1 line/sec)

  float _cpuFrameMsEma     = -1.0f;
  float _presentIdleMsEma  = -1.0f;
  float _gpuFrameMsEma     = -1.0f; // -1 until the first sane timestamp sample
  float _microtaskCpuMsEma = -1.0f; // -1 until the first frame (COMFORT-1)

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

  // COMFORT-2: the key the NEXT slice is measured under. A task whose steps are
  // several cost populations (the radiance prefilter's GPU filter steps vs its
  // CPU package steps) overrides this so each population learns its own scale;
  // the scheduler re-seeds _estimateScaleQ16 from the registry whenever the key
  // it returns changes. Default = one population, the task's _costKey.
  virtual const std::string& sliceCostKey() const { return _costKey; }

  std::string    _name;                            // HUD + async-tracker tag
  // COMFORT-1: MicrotaskCostRegistry key — what this task's slice cost is
  // LEARNED AS, across task instances. Empty = no persistence (the task's own
  // estimate is all the scheduler ever has). Keys must discriminate anything
  // that dominates cost (source extent, sample counts), or a cheap client
  // inherits an expensive one's learned scale.
  std::string    _costKey;
  MicrotaskClass _class        = MicrotaskClass::OPPORTUNISTIC;
  int64_t        _deadlineFrame = -1;              // SOFT_DEADLINE: frame it wants completion by; -1 = always-due

  // How many slices of THIS task one frame may run; 0 = no per-task limit (the
  // budget is the only throttle). The budget alone is not enough: an UNBOUNDED
  // context (offscreen/loader — §2.2) has no per-frame deadline, so the drain
  // loop happily runs a whole job's slices back to back in one beginFrame. That
  // is correct for the loader and wrong for a RECURRING job on a context that
  // is presenting frames — the sky IBL refilter's 56 slices measured 89-134ms
  // in a single offscreen frame against a 1.9ms median. A task that spans frames
  // by design says so here.
  int            _maxSlicesPerFrame = 0;

  // COLD START: this task's slices are exempt from the per-frame budget gate —
  // the scheduler runs them back to back until the task completes or hands the
  // frame back, hitching the frame ON PURPOSE. Only legal for a job whose
  // result is required before the frames it would otherwise be paced across
  // (the FIRST sky IBL bake: until it publishes, the scene is lit by
  // placeholder radiance, and pacing that over 50+ frames is the artifact).
  // Set with _maxSlicesPerFrame = 0 (no quota) — a quota would cap the drain.
  // NOT a BudgetMode: the mode is the whole context's and is sticky, this is
  // one task's, and it dies with the task instance, which is what makes the
  // return to normal pacing exact (the next instance is built without it).
  bool           _unboundedDrain = false;

  // Set BY A SLICE to hand the rest of this frame back: the task leaves the
  // frame's active set and resumes at the same step on the next drain. For a
  // slice that made NO progress because it waits on completion this thread
  // cannot advance (a capture readback's conversion worker). The scheduler
  // clears it. Before _unboundedDrain, _maxSlicesPerFrame paced such polling to
  // one per frame as a side effect; with the quota off that side effect is gone
  // and the poll would become a render-thread spin against another thread.
  bool           _yieldFrame = false;

  // scheduler-owned (do not touch from task code) ///////////////////////////
  std::string _activeCostKey;         // key _estimateScaleQ16 is currently seeded for (COMFORT-2)
  int64_t  _estimateScaleQ16 = 65536; // auto-halving multiplier (T1 feedback loop); never recovers (§2.3)
  int64_t  _lastMeasuredUs   = -1;
  uint64_t _slicesRun        = 0;
  bool     _permanent        = false; // permanent tasks (LoadingPhase) are never dropped; false-from-runSlice = idle
  bool     _cancelled        = false; // cooperative cancel — not run again
  bool     _asyncTracked     = false; // asyncWorkBegin fired for this task (so complete/cancel asyncWorkEnd once)
  bool     _loggedLie        = false; // T1: the "lies about slice cost" loud log is once-per-task
  uint64_t _framesWaited     = 0;     // MAINTENANCE starvation-floor accounting
  uint64_t _framesDeferred   = 0;     // COMFORT-1: consecutive frames the est>budget gate skipped this task
  uint64_t _sliceFrameIndex  = 0;     // frame _slicesThisFrame is counted for (_maxSlicesPerFrame)
  int      _slicesThisFrame  = 0;
};
using gpumicrotask_ptr_t = std::shared_ptr<GpuMicrotask>;

///////////////////////////////////////////////////////////////////////////////
// MicrotaskCostRegistry (COMFORT-1) — process-wide learned slice cost, keyed by
// GpuMicrotask::_costKey and therefore SURVIVING task instances.
//
// Why it must exist: a client like the sky IBL refilter builds a FRESH task per
// cycle. Everything the scheduler learned about the last cycle (the T1
// auto-halving scale) died with that instance, so the first — and heaviest —
// slices of every cycle sailed through the est>budget gate with a 1x scale on a
// small fixed seed, and hitched the frame they landed in.
//
// What is persisted is the SCALE, as an EMA, not a raw last-measured cost: a
// one-time pipeline-warmup slice must not pin the estimate above every possible
// budget forever (the failure mode the fixed-seed estimate was written to avoid
// — see RadiancePrefilterMicrotask::sliceEstimateUs). The EMA rises fast and
// decays over a handful of slices, so a warmup outlier pays for itself once;
// the scheduler's deferral escape (kDeferralEscapeFrames) is the hard guarantee
// that even a permanently over-budget estimate DEFERS rather than drops.
//
// Also the per-slice CPU-wall measurement surface (the numbers a comfort/hitch
// investigation needs), readable from python via lev2.microtaskCostStats().
///////////////////////////////////////////////////////////////////////////////

struct MicrotaskCostRegistry {

  struct Entry {
    double   _estScaleQ16Ema   = 65536.0; // learned estimate multiplier (Q16, EMA)
    int64_t  _lastSeedScaleQ16 = 65536;   // scale the most recent instance was seeded with
    uint64_t _instancesSeeded  = 0;       // task instances that took a seed from this key
    int64_t  _lastSliceUs      = -1;
    int64_t  _worstSliceUs     = 0;
    // The worst slice a SINGLE outlier does not explain — a job whose final
    // step is structurally heavier than its body (radiance prefilter: filter
    // slices vs the one packaging+publish slice) reads as one number too few
    // otherwise, and "the hitch is one known step" and "every step hitches" are
    // very different defects.
    int64_t  _secondWorstSliceUs = 0;
    int64_t  _totalSliceUs     = 0;
    uint64_t _sliceCount       = 0;
    uint64_t _deferrals        = 0; // est>budget skips (deferred to a later frame)
    uint64_t _escapes          = 0; // slices run under the anti-starvation escape
  };
  using entry_map_t = std::map<std::string, Entry>;

  // Read at enqueue: the scale a new instance starts from (>= 65536 == 1x).
  static int64_t seedScaleQ16(const std::string& key);
  // Written on measure, from the scheduler's drain loop.
  static void noteSlice(const std::string& key, int64_t raw_estimate_us, int64_t measured_us);
  static void noteDeferral(const std::string& key, bool escaped);
  static entry_map_t snapshot();
  // Clears the MEASUREMENTS (worst/total/count/deferrals/escapes) and keeps
  // both the learned scale and the seeding history — a measurement harness
  // wants per-cycle numbers without throwing away what the scheduler knows (and
  // "what it knew going into this cycle" is itself an observable).
  static void resetMeasurements();
};

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
  bool    _forceRealtimeOffscreen = false; // ORKID_MT_FORCE_REALTIME (gate hook, see _ensureEnvLoaded)

  Telemetry _telemetry;
};

} // namespace ork::lev2
