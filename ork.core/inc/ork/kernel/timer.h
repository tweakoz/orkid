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

  // Seconds since staticInit() was called.
  static double get_sync_time();

  // Absolute nanoseconds since system boot. Cross-process consistent. No staticInit required.
  static u64    getSystemTick();

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

  u64  _spin_margin;
  u64  _margin_min;
  u64  _margin_max;
  u64  _decay;
};

using adaptive_wait_ptr_t = std::shared_ptr<AdaptiveWait>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
