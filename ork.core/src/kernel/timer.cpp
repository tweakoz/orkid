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
static u64 s_numer    = s_info.numer;
static u64 s_denom    = s_info.denom;
static u64 s_timebase = 0; // raw mach ticks at staticInit time

void Timer::staticInit() {
  s_timebase = mach_absolute_time();
}

u64 Timer::getSystemTick() {
  return (mach_absolute_time() * s_numer) / s_denom;
}

double Timer::get_sync_time() {
  // seconds since staticInit()
  u64 raw = mach_absolute_time() - s_timebase;
  return double((raw * s_numer) / s_denom) * SEC_PER_NS;
}

void Timer::sleepTicks(u64 ticks) {
  // ticks = duration in nanoseconds
  u64 mach_ticks = (ticks * s_denom) / s_numer;
  mach_wait_until(mach_absolute_time() + mach_ticks);
}

void Timer::sleepUntilTick(u64 target_tick) {
  // target_tick = absolute ns from getSystemTick()
  // convert to absolute mach time: mach = (ns * denom) / numer
  mach_wait_until((target_tick * s_denom) / s_numer);
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
