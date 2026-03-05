////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/concurrent_queue.h>

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

#include <ork/kernel/kernel.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/mutex.h>

#include <time.h>
#include <stdio.h>
#include <sys/timeb.h>
#include <cmath>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

void Timer::Start() {
  _start_tick = get_sync_tick();
}

void Timer::setCurrentTime(float time) {
  u64 offset  = u64(double(time) * 1000.0 / tick_scale_ms());
  _start_tick = get_sync_tick() - offset;
}

void Timer::End() {
  _end_tick = get_sync_tick();
}

double Timer::InternalSecsSinceStart() const {
  u64 delta = get_sync_tick() - _start_tick;
  return double(delta) * tick_scale_ms() * 0.001;
}

double Timer::SecsSinceStart() const {
  return InternalSecsSinceStart();
}

double Timer::SpanInSecs() const {
  u64 delta = _end_tick - _start_tick;
  return double(delta) * tick_scale_ms() * 0.001;
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

void Timer::OnInterval(float interval, const void_lambda_t& oper) {
  _on_interval = oper;
  _thread      = new ork::Thread;

  if (_on_interval) {
    _thread->start([=](anyp data) {
      while (false == _kill) {
        usleep(uint64_t(interval * 1e6f));
        _on_interval();
      }
    });
  }
}

///////////////////////////////////////////////////////////////////////////////
#if defined(ORK_OSX) || defined(ORK_IOS)
///////////////////////////////////////////////////////////////////////////////

static u64 s_resolution = 0;
static u64 s_timebase   = 0;

void Timer::staticInit() {
  mach_timebase_info_data_t info;
  mach_timebase_info(&info);
  s_resolution = (u64)info.numer / (u64)info.denom;
  s_timebase   = mach_absolute_time();
}

u64 Timer::get_sync_tick() {
  return mach_absolute_time() - s_timebase;
}

double Timer::tick_scale_ms() {
  return double(s_resolution) * 1e-6; // milliseconds per tick
}

double Timer::get_sync_time() {
  return double(Timer::get_sync_tick() * s_resolution) * 1e-9;
}

///////////////////////////////////////////////////////////////////////////////
#elif defined(ORK_CONFIG_IX)
///////////////////////////////////////////////////////////////////////////////

static u64 s_timebase = 0;

void Timer::staticInit() {
  timespec tmsnow;
  clock_gettime(CLOCK_REALTIME, &tmsnow);
  s_timebase = ((tmsnow.tv_sec >> 12) << 12) * 1000;
}

u64 Timer::get_sync_tick() {
  timespec tsnow;
  clock_gettime(CLOCK_REALTIME, &tsnow);
  u64 tms_now = u64(tsnow.tv_sec) * 1000 + u64(tsnow.tv_nsec) / 1000000;
  return tms_now - s_timebase;
}

double Timer::tick_scale_ms() {
  return 1.0; // ticks are already milliseconds
}

double Timer::get_sync_time() {
  return double(Timer::get_sync_tick()) * 0.001;
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
    usleep(1000);
    millisec--;
  }
  // usleep(millisec * 1000);
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
  // TODO: non busy wait version

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
#else
#error // not implemented
#endif
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////