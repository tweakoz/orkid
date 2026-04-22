#pragma once

////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Timer
//
// UNITS
//   Time  - seconds   (double)
//   Tick  - nanoseconds (u64). 1 tick = 1 ns. Monotomic time consistent system-wide.
//   MS    - milliseconds. Suffix on constants/conversions (e.g. NS_PER_MS).
//   US    - microseconds. Suffix on constants/conversions (e.g. NS_PER_US).
//
///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/kernel.h>
#include <ork/kernel/thread.h>
#include <ork/orkstl.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

static constexpr u64    NS_PER_US  = 1000ULL;        // nanoseconds per microsecond
static constexpr u64    NS_PER_MS  = 1000000ULL;     // nanoseconds per millisecond
static constexpr u64    NS_PER_SEC = 1000000000ULL;  // nanoseconds per second
static constexpr double MS_PER_NS  = 1e-6;           // milliseconds per nanosecond (tick)
static constexpr double SEC_PER_NS = 1e-9;           // seconds per nanosecond (tick)
static constexpr double SEC_PER_MS = 1e-3;           // seconds per millisecond
static constexpr double MS_PER_SEC = 1e3;            // milliseconds pers seconds 

///////////////////////////////////////////////////////////////////////////////

struct Timer {

  Timer();
  ~Timer();

  void Start();
  void End();
  double SecsSinceStart() const;
  double SpanInSecs() const;
  void OnInterval(double interval_secs, const void_lambda_t& oper);
  void setCurrentTime(double secs);

  ///////////////////////////////////////////////////////////////////////////////

  // Must be called before use of get_sync_time
  static void   staticInit();

  // Milliseconds since Unix epoch as double (sub-millisecond precision).
  static double getEpochMS();

  // Seconds since staticInit() was called.
  static double get_sync_time();

  // Absolute nanoseconds since system boot. Cross-process consistent. No staticInit required.
  static u64    getSystemTick();

#if defined(ORK_OSX) || defined(ORK_IOS)
  // Convert a mach_absolute_time() value to the same nanosecond unit as getSystemTick().
  static u64    machAbsoluteToSystemTick(u64 mach_time);
#endif

  // Sleep for a duration in nanoseconds.
  static void   sleepTicks(u64 ticks);

  // Sleep until an absolute tick from getSystemTick().
  static void   sleepUntilTick(u64 target_tick);

  // CPU yield hint for spin-wait loops. Stays on-CPU but reduces pipeline pressure.
  static void   spinYield();

  // Pure spin until an absolute tick. Burns 100% of one CPU core. Lowest possible jitter.
  static void   spinUntilTick(u64 target_tick);

  ///////////////////////////////////////////////////////////////////////////////

  double        _start_time{};
  double        _end_time{};
  void_lambda_t _on_interval{};
  ork::Thread*  _thread{};
  bool          _kill{};
};

using timer_ptr_t = std::shared_ptr<Timer>;

///////////////////////////////////////////////////////////////////////////////
// AdaptiveWait
//
// Hybrid sleep+spin that self-tunes its spin margin to eliminate scheduler
// wakeup jitter. Call sleepUntilTick() with an absolute tick from
// Timer::getSystemTick(). The margin grows when the OS overshoots and
// decays slowly when wakeups are on time.
///////////////////////////////////////////////////////////////////////////////

struct AdaptiveWait {

  enum class Mode {
    Coarse,   // Pure sleep, tiny spin margin. Lowest CPU. May overshoot by ~1ms.
    Balanced, // Moderate spin margin and decay. Good CPU/precision tradeoff.
    Precise,  // Large spin margin, slow decay. Lowest jitter. Burns more CPU in spin.
  };

  AdaptiveWait(Mode mode = Mode::Balanced);

  void sleepUntilTick(u64 target_tick);

  // Sleep until an absolute epoch millisecond timestamp (e.g. from Timer::getEpochMS()).
  // Converts epoch ms -> relative duration -> system ticks, then delegates to sleepUntilTick.
  // If wake_epoch_ms is already in the past, returns immediately.
  void sleepUntilEpochMS(double wake_epoch_ms) {
    double sleep_ms = wake_epoch_ms - Timer::getEpochMS();
    if (sleep_ms <= 0.0)
      return;
    u64 now_tick    = Timer::getSystemTick();
    u64 sleep_ticks = u64(sleep_ms * double(NS_PER_MS));
    sleepUntilTick(now_tick + sleep_ticks);
  }

  u64  _spin_margin;
  u64  _margin_min;
  u64  _margin_max;
  u64  _decay;
};

using adaptive_wait_ptr_t = std::shared_ptr<AdaptiveWait>;

///////////////////////////////////////////////////////////////////////////////
// RunningStats
//   Feed samples one at a time with the poll functions
//   to calculate min, mac, mean, and stddev.
///////////////////////////////////////////////////////////////////////////////

struct RunningStats {

  // Feed a new sample. Updates all stats immediately.
  void pollValue(double sample);

  // Record a call timestamp via getSystemTick() and feed the computed Hz into pollValue.
  // Call this once per event (e.g. each pose frame) to track event rate.
  void pollHz();

  // Reset to initial state.
  void reset();

  // sample stddev — returns 0 until 2+ samples
  double stddev() const;

  // upper end of the distribution: mean + sqrt(3) * stddev
  // for a uniform distribution this equals the max of the range
  double upperRange() const { return _mean + sqrt(3.0) * stddev(); }

  void printStats(const char* label = "") const {
    printf("[RunningStats] %s last:%.4f count:%lld min:%.4f max:%.4f mean:%.4f stddev:%.4f\n",
           label, _last_value, _count, _min, _max, _mean, stddev());
  }

  // Print every `interval` samples (by total count). No-op otherwise.
  void printStats(const char* label, u32 interval) const {
    if (_count > 0 && (_count % interval) == 0)
      printStats(label);
  }

  double  _min        =  1e300;
  double  _max        = -1e300;
  double  _mean       =  0.0;
  double  _m2         =  0.0;
  double  _last_value =  0.0;
  int64_t _count      =  0;
  int64_t _warmup     =  10;   // samples to discard before accumulating stats
  u64     _last_tick  =  0;
};

using running_stats_ptr_t = std::shared_ptr<RunningStats>;

///////////////////////////////////////////////////////////////////////////////
// TimePredictor
//   Tracks a recurring event and predicts when it will next occur.
//   Call markPredictionTarget() each time the event happens.
//   Call predictNextTarget() at any time to get the predicted epoch ms.
///////////////////////////////////////////////////////////////////////////////

struct TimePredictor {

  static constexpr size_t HISTORY_SIZE = 16;

  // Record that the tracked event occurred at the given CLOCK_MONOTONIC tick (ns).
  // Updates the rolling average interval.
  void markPredictionTargetTick(u64 tick);

  // Predicted absolute CLOCK_MONOTONIC tick (ns) of the next occurrence
  // strictly after now, plus one refresh period for pipeline lag.
  // Returns 0 until 2 marks have been recorded.
  u64 predictNextTargetSystemTick() const;

  u64    avgIntervalNS() const { return _avg_interval_ns; }
  double avgIntervalMS() const { return double(_avg_interval_ns) * MS_PER_NS; }
  u64    stddevNS() const      { return _stddev_ns; }
  double stddevMS() const      { return double(_stddev_ns) * MS_PER_NS; }

  u64    _last_mark_ns      = 0;  // CLOCK_MONOTONIC tick of last mark
  u64    _avg_interval_ns   = 0;  // rolling average interval in ticks (ns)
  u64    _stddev_ns         = 0;  // stddev in ticks (ns); sqrt rounded to nearest ns
  u64    _refresh_period_ns = 0;  // nominal display refresh period (rational, from CVDisplayLink or equivalent)
  u64    _history[HISTORY_SIZE] = {};  // recent intervals in ticks (ns)
  size_t _history_index     = 0;
  size_t _history_count     = 0;
  u64    _mark_count        = 0;  // absolute mark count, never wraps
  u64    _last_prediction   = 0;  // CLOCK_MONOTONIC tick
};

using time_predictor_ptr_t = std::shared_ptr<TimePredictor>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
