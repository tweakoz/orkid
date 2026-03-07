////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/concurrent_queue.h>
#include <ork/kernel/kernel.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/mutex.h>

#if defined(ORK_OSX) || defined(ORK_IOS)
#include <mach/mach_time.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#if defined(ORK_CONFIG_IX)
#include <unistd.h>
#include <sys/time.h>
#include <sched.h>
#include <time.h>
#endif

#include <time.h>
#include <stdio.h>
#include <sys/timeb.h>
#include <cmath>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

Timer::Timer()
    : _start_tick(0)
    , _end_tick(0)
    , _on_interval(nullptr)
    , _thread(nullptr)
    , _kill(false) {
}

Timer::~Timer() {
  _kill = true;
  if (_thread)
    _thread->join();
  delete _thread;
}

void Timer::Start() {
  _start_tick = getSystemTick();
}

void Timer::End() {
  _end_tick = getSystemTick();
}

void Timer::setCurrentTime(double secs) {
  u64 offset  = u64(secs * double(NS_PER_SEC));
  _start_tick = getSystemTick() - offset;
}

double Timer::SecsSinceStart() const {
  u64 delta = getSystemTick() - _start_tick;
  return double(delta) * SEC_PER_NS;
}

double Timer::SpanInSecs() const {
  u64 delta = _end_tick - _start_tick;
  return double(delta) * SEC_PER_NS;
}

void Timer::spinYield() {
#if defined(ORK_ARCHITECTURE_ARM_64)
  __builtin_arm_yield();
#elif defined(ORK_ARCHITECTURE_X86_64)
  __builtin_ia32_pause();
#endif
}

void Timer::spinUntilTick(u64 target_tick) {
  while (getSystemTick() < target_tick)
    spinYield();
}


void Timer::OnInterval(double interval_secs, const void_lambda_t& oper) {
  _on_interval = oper;
  _thread      = new ork::Thread;
  u64 interval_ticks = u64(interval_secs * double(NS_PER_SEC));
  if (_on_interval) {
    _thread->start([=](anyp data) {
      while (false == _kill) {
        sleepTicks(interval_ticks);
        _on_interval();
      }
    });
  }
}

///////////////////////////////////////////////////////////////////////////////
#if defined(ORK_OSX) || defined(ORK_IOS)
///////////////////////////////////////////////////////////////////////////////

static mach_timebase_info_data_t s_info = [](){
  mach_timebase_info_data_t i;
  mach_timebase_info(&i);
  return i;
}();

// Should move away from using staticInit and needing to rely on it. 
// Everything should just use the system-wide monotomic tick.
static u64 s_timebase = 0; // raw mach ticks at staticInit time
void Timer::staticInit() {
  s_timebase = mach_absolute_time();
}

double Timer::get_sync_time() {
  // seconds since staticInit()
  u64 raw = mach_absolute_time() - s_timebase;
  return double((raw * s_info.numer) / s_info.denom) * SEC_PER_NS;
}

u64 Timer::getSystemTick() {
  return (mach_absolute_time() * s_info.numer) / s_info.denom;
}

void Timer::sleepTicks(u64 ticks) {
  // ticks = duration in nanoseconds
  u64 mach_ticks = (ticks * s_info.denom) / s_info.numer;
  mach_wait_until(mach_absolute_time() + mach_ticks);
}

void Timer::sleepUntilTick(u64 target_tick) {
  // target_tick = absolute ns from getSystemTick()
  // convert to absolute mach time: mach = (ns * denom) / numer
  mach_wait_until((target_tick * s_info.denom) / s_info.numer);
}

///////////////////////////////////////////////////////////////////////////////
#elif defined(ORK_CONFIG_IX)
///////////////////////////////////////////////////////////////////////////////

static u64 s_timebase = 0; // absolute ns at staticInit time
void Timer::staticInit() {
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  s_timebase = u64(ts.tv_sec) * NS_PER_SEC + u64(ts.tv_nsec);
}

u64 Timer::getSystemTick() {
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return u64(ts.tv_sec) * NS_PER_SEC + u64(ts.tv_nsec);
}

double Timer::get_sync_time() {
  return double(getSystemTick() - s_timebase) * SEC_PER_NS;
}

void Timer::sleepTicks(u64 ticks) {
  // ticks = duration in nanoseconds
  timespec ts = {
    .tv_sec  = (time_t)(ticks / NS_PER_SEC),
    .tv_nsec = (long)  (ticks % NS_PER_SEC)
  };
  nanosleep(&ts, nullptr);
}

void Timer::sleepUntilTick(u64 target_tick) {
  // target_tick = absolute ns from getSystemTick() (CLOCK_MONOTONIC)
  timespec ts = {
    .tv_sec  = (time_t)(target_tick / NS_PER_SEC),
    .tv_nsec = (long)  (target_tick % NS_PER_SEC)
  };
  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr);
}

///////////////////////////////////////////////////////////////////////////////
#else
#error // not implemented
#endif
///////////////////////////////////////////////////////////////////////////////

AdaptiveWait::AdaptiveWait(Mode mode) {
  switch (mode) {
    case Mode::Coarse:
      _margin_min  = 50ULL  * NS_PER_US;  //   50 µs — prevents collapse to zero
      _margin_max  = 500ULL * NS_PER_US;  //  500 µs — caps scheduler spike absorption
      _spin_margin = 50ULL  * NS_PER_US;  //   50 µs — starts tight, OS sleep dominates
      _decay       = 5ULL   * NS_PER_US;  //    5 µs — snaps back to min quickly
      break;
    case Mode::Balanced:
      _margin_min  = 200ULL  * NS_PER_US; //  200 µs — reliably wakes before target on a normal system
      _margin_max  = 2000ULL * NS_PER_US; // 2000 µs — absorbs jitter spikes without runaway spin
      _spin_margin = 200ULL  * NS_PER_US; //  200 µs — covers typical OS wakeup latency
      _decay       = 1ULL    * NS_PER_US; //    1 µs — holds margin through on-time wakes
      break;
    case Mode::Precise:
      _margin_min  = 500ULL  * NS_PER_US; //  500 µs — wide floor, almost always wakes early enough to spin
      _margin_max  = 2000ULL * NS_PER_US; // 2000 µs — same ceiling as Balanced
      _spin_margin = 500ULL  * NS_PER_US; //  500 µs — starts wide, first frames land in spin window immediately
      _decay       = 250ULL;              // 0.25 µs — near-zero decay, stays biased early
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

void AdaptiveWait::sleepUntilTick(u64 target_tick) {

  if (target_tick > _spin_margin)
    Timer::sleepUntilTick(target_tick - _spin_margin);

  u64 wake_tick = Timer::getSystemTick();
  if (wake_tick >= target_tick) {
    // Overshot. Increase margin by the overshoot amount.
    u64 overshoot = wake_tick - target_tick;
    _spin_margin += overshoot;

    if (_spin_margin > _margin_max)
      _spin_margin = _margin_max;

  } else {
    // Woke early. Spin the remaining time and decay margin slightly.
    while (Timer::getSystemTick() < target_tick)
      Timer::spinYield();

    if (_spin_margin > _margin_min + _decay)
      _spin_margin -= _decay;
  }
}

///////////////////////////////////////////////////////////////////////////////
#if defined(__APPLE__) || defined(ORK_CONFIG_IX)
///////////////////////////////////////////////////////////////////////////////

void msleep(int millisec) {
  while (millisec > 0) {
    ::usleep(1000);
    millisec--;
  }
}
void usleep(int microsec) {
  ::usleep(microsec);
}

///////////////////////////////////////////////////////////////////////////////
#elif defined(ORK_WIN32)
///////////////////////////////////////////////////////////////////////////////

void msleep(int millisec) {
  Sleep(millisec);
}
void usleep(int microsec) {
  __int64 time1   = 0;
  __int64 time2   = 0;
  __int64 sysFreq = 0;

  QueryPerformanceCounter((LARGE_INTEGER*)&time1);
  QueryPerformanceFrequency((LARGE_INTEGER*)&sysFreq);

  do {
    QueryPerformanceCounter((LARGE_INTEGER*)&time2);
  } while ((time2 - time1) < microsec);
}

///////////////////////////////////////////////////////////////////////////////
#endif
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
