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
#include <ork/util/logger.h>

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

static logchannel_ptr_t logchan_timer = logger()->configureChannel("TIMER", fvec3(0.7, 0.9, 0.7), true);

///////////////////////////////////////////////////////////////////////////////

Timer::Timer()
    : _start_time(0)
    , _end_time(0)
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

double Timer::getEpochMS() {
  timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return double(ts.tv_sec) * 1e3 + double(ts.tv_nsec) * 1e-6;
}

void Timer::Start() {
  _start_time = get_sync_time();
}

void Timer::End() {
  _end_time = get_sync_time();
}

void Timer::setCurrentTime(double secs) {
  _start_time = get_sync_time() - secs;
}

double Timer::SecsSinceStart() const {
  return get_sync_time() - _start_time;
}

double Timer::SpanInSecs() const {
  return _end_time - _start_time;
}

void Timer::spinYield() {
	// TODO AI keeps telling me this is better to use in spinwait rather than sched_yield, but should device benchmark.
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

static u64    s_numer      = 1;
static u64    s_denom      = 1;
static double s_resolution = 1.0; // (numer/denom) * SEC_PER_NS: mach ticks -> seconds
static u64    s_timebase   = 0;

static bool s_mach_init = [](){
  mach_timebase_info_data_t i;
  mach_timebase_info(&i);
  s_numer      = i.numer;
  s_denom      = i.denom;
  s_resolution = (double(i.numer) / double(i.denom)) * SEC_PER_NS;
  return true;
}();

void Timer::staticInit() {
  s_timebase = mach_absolute_time();
}

double Timer::get_sync_time() {
  u64 raw = mach_absolute_time() - s_timebase;
  return double(raw) * s_resolution;
}

u64 Timer::getSystemTick() {
  return (mach_absolute_time() * s_numer) / s_denom;
}

u64 Timer::machAbsoluteToSystemTick(u64 mach_time) {
  return (mach_time * s_numer) / s_denom;
}

void Timer::sleepTicks(u64 ticks) {
  mach_wait_until(mach_absolute_time() + (ticks * s_denom) / s_numer);
}

void Timer::sleepUntilTick(u64 target_tick) {
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

void TimePredictor::markPredictionTargetTick(u64 tick) {

  if (_last_mark_ns > 0) {
    u64 interval_ns = tick - _last_mark_ns;

    _history[_history_index] = interval_ns;
    _history_index = (_history_index + 1) % HISTORY_SIZE;
    if (_history_count < HISTORY_SIZE)
      _history_count++;

    // Integer average
    u64 sum = 0;
    for (size_t i = 0; i < _history_count; i++)
      sum += _history[i];
    _avg_interval_ns = sum / u64(_history_count);

    // Stddev: compute variance in u64, take sqrt via double, round back to u64.
    // Signed difference handles intervals shorter than the mean.
    if (_history_count > 1) {
      u64 m2 = 0;
      for (size_t i = 0; i < _history_count; i++) {
        s64 d = s64(_history[i]) - s64(_avg_interval_ns);
        m2 += u64(d * d);
      }
      _stddev_ns = u64(sqrt(double(m2) / double(_history_count - 1)));
    }
  }
  _last_mark_ns = tick;
  _mark_count++;
  _last_prediction = predictNextTargetSystemTick();
}

u64 TimePredictor::predictNextTargetMarginSystemTick() const {
  if (_history_count == 0)
    return 0;

  u64 step = _avg_interval_ns;
  u64 now  = Timer::getSystemTick();

  // Find the next grid tick strictly after now.
  u64 t = _last_mark_ns;
  while (t <= now)
    t += step;

  return t;
}

u64 TimePredictor::predictNextTargetSystemTick() const {
  return predictNextTargetMarginSystemTick() + _margin_ns;
}

///////////////////////////////////////////////////////////////////////////////

void RunningStats::pollValue(double sample) {
  _last_value = sample;
  if (_warmup > 0) {
    _warmup--;
    return;
  }
  _count++;
  double delta = sample - _mean;
  _mean += delta / double(_count);
  _m2   += delta * (sample - _mean); // Welford's update
  if (sample < _min) _min = sample;
  if (sample > _max) _max = sample;
}

void RunningStats::pollHz() {
  u64 now = Timer::getSystemTick();
  if (_last_tick != 0) {
    u64 delta_ns = now - _last_tick;
    if (delta_ns > 0) {
      double hz = double(NS_PER_SEC) / double(delta_ns);
      pollValue(hz);
    }
  }
  _last_tick = now;
}

void RunningStats::reset() {
  _min        =  1e300;
  _max        = -1e300;
  _mean       =  0.0;
  _m2         =  0.0;
  _last_value =  0.0;
  _count      =  0;
  _warmup     =  10;
  _last_tick  =  0;
}

double RunningStats::stddev() const {
  return _count > 1 ? sqrt(_m2 / double(_count - 1)) : 0.0;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
