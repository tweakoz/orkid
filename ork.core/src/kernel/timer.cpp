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
  _start_tick = getSyncTick();
}

void Timer::End() {
  _end_tick = getSyncTick();
}

void Timer::setCurrentSecs(double secs) {
  u64 offset  = u64(secs * double(NS_PER_SEC));
  _start_tick = getSyncTick() - offset;
}

double Timer::SecsSinceStart() const {
  u64 delta = getSyncTick() - _start_tick;
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

static u64 s_numer    = 1;
static u64 s_denom    = 1;
static u64 s_timebase = 0; // raw mach ticks at init

void Timer::staticInit() {
  mach_timebase_info_data_t info;
  mach_timebase_info(&info);
  s_numer    = info.numer;
  s_denom    = info.denom;
  s_timebase = mach_absolute_time();
}

u64 Timer::getSyncTick() {
  // returns nanoseconds: (raw * numer) / denom, all integer
  u64 raw = mach_absolute_time() - s_timebase;
  return (raw * s_numer) / s_denom;
}

double Timer::get_sync_time() {
  return double(Timer::getSyncTick()) * SEC_PER_NS;
}

void Timer::sleepTicks(u64 ticks) {
  u64 ns = (ticks * s_denom) / s_numer;
  mach_wait_until(mach_absolute_time() + ns);
}

void Timer::sleepUntilTick(u64 target_tick) {
  // convert absolute ns tick back to absolute mach tick
  u64 abs_ns = s_timebase + (target_tick * s_denom) / s_numer;
  mach_wait_until(abs_ns);
}

///////////////////////////////////////////////////////////////////////////////
#elif defined(ORK_CONFIG_IX)
///////////////////////////////////////////////////////////////////////////////

static u64 s_timebase = 0;

void Timer::staticInit() {
  timespec tmsnow;
  clock_gettime(CLOCK_REALTIME, &tmsnow);
  // store timebase in nanoseconds
  s_timebase = u64(tmsnow.tv_sec) * NS_PER_SEC + u64(tmsnow.tv_nsec);
}

u64 Timer::getSyncTick() {
  // returns nanoseconds
  timespec tsnow;
  clock_gettime(CLOCK_REALTIME, &tsnow);
  return u64(tsnow.tv_sec) * NS_PER_SEC + u64(tsnow.tv_nsec) - s_timebase;
}

double Timer::get_sync_time() {
  return double(Timer::getSyncTick()) * SEC_PER_NS;
}

void Timer::sleepTicks(u64 ticks) {
  timespec ts = {
    .tv_sec  = (time_t)(nanoseconds / NS_PER_SEC),
    .tv_nsec = (long)  (nanoseconds % NS_PER_SEC)
  };
  nanosleep(&ts, nullptr);
}

void Timer::sleepUntilTick(u64 target_tick) {
  // convert absolute ns tick to absolute CLOCK_REALTIME timespec
  u64 abs_ns = s_timebase + target_tick;
  timespec ts = {
    .tv_sec  = (time_t)(abs_ns / NS_PER_SEC),
    .tv_nsec = (long)  (abs_ns % NS_PER_SEC)
  };
  clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, nullptr);
}

///////////////////////////////////////////////////////////////////////////////
#elif defined(ORK_WIN32)
///////////////////////////////////////////////////////////////////////////////

static u64 s_freq     = 1;
static u64 s_timebase = 0; // raw QPC ticks at init

void Timer::staticInit() {
  LARGE_INTEGER freq, now;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&now);
  s_freq     = u64(freq.QuadPart);
  s_timebase = u64(now.QuadPart);
}

u64 Timer::getSyncTick() {
  // returns nanoseconds via integer split to avoid overflow
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  u64 raw  = u64(now.QuadPart) - s_timebase;
  u64 secs = raw / s_freq;
  u64 rem  = raw % s_freq;
  return secs * NS_PER_SEC + rem * NS_PER_SEC / s_freq;
}

double Timer::get_sync_time() {
  return double(getSyncTick()) * SEC_PER_NS;
}

void Timer::sleepTicks(u64 ticks) {
  // High-resolution waitable timer: precise and no CPU burn (Win10 1803+)
  HANDLE timer = CreateWaitableTimerEx(
    NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
  LARGE_INTEGER due;
  due.QuadPart = -(__int64)(nanoseconds / 100ULL); // 100ns units, negative = relative
  SetWaitableTimerEx(timer, &due, 0, NULL, NULL, NULL, 0);
  WaitForSingleObject(timer, INFINITE);
  CloseHandle(timer);
}

void Timer::sleepUntilTick(u64 target_tick) {
  u64 now = Timer::getSyncTick();
  if (target_tick > now)
    Timer::sleepTicks(target_tick - now);
}

///////////////////////////////////////////////////////////////////////////////
#else
#error // not implemented
#endif
///////////////////////////////////////////////////////////////////////////////

#if defined(__APPLE__) || defined(ORK_CONFIG_IX)

void msleep(int millisec) {
  while (millisec > 0) {
    ::usleep(1000);
    millisec--;
  }
}
void usleep(int microsec) {
  ::usleep(microsec);
}

#elif defined(ORK_WIN32)

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

#endif

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
