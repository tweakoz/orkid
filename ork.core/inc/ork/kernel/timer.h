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
//   Tick  - nanoseconds (u64). 1 tick = 1 ns. Used in all tick-based APIs.
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
  
  void   setCurrentSecs(double secs);
  double SecsSinceStart() const;
  double SpanInSecs() const;

  void OnInterval(float interval, const void_lambda_t& oper);

  ////////////////////////////////////////

  // Must be called before use of getSyncTick and get_sync_time
  static void   staticInit(); 

  // Ticks since staticInit. 1 tick = 1 nanosecond
  static u64    getSyncTick(); 

  // Seconds since staticInit.
  static double get_sync_time(); 

  // High precision wait until absolute ns tick from staticInit
  static void   sleepUntilTick(u64 target_tick); 

    // High precision sleep in ticks.
  static void   sleepTicks(u64 ticks);

private:
  u64           _start_tick{};
  u64           _end_tick{};
  float         _lambda_interval{};
  void_lambda_t _on_interval{};
  ork::Thread*  _thread{};
  bool          _kill{};
};

using timer_ptr_t = std::shared_ptr<Timer>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
