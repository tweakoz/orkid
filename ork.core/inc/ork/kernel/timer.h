////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/kernel.h>
#include <ork/kernel/thread.h>
#include <ork/kernel/svariant.h>
#include <ork/orkstl.h>
#include <ork/util/crc.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

struct Timer {

  Timer();
  ~Timer();

  void Start();
  void End();
  float InternalSecsSinceStart() const;
  float SecsSinceStart() const;
  float SpanInSecs() const;
  void OnInterval(float interval, const void_lambda_t& oper);
  void setCurrentTime(float value);
  static float get_sync_time();

  static void staticInit();

private:
  static svar64_t _gimpl;

  float mStartTime;
  float mEndTime;
  float mLambdaInterval;
  void_lambda_t mOnInterval;
  ork::Thread* mThread;
  bool mKill;
};

using timer_ptr_t = std::shared_ptr<Timer>;

///////////////////////////////////////////////////////////////////////////////

struct PerfItem2 {
  const char* mpMarkerName;
  float mfMarkerTime;
};

void PerfMarkerPush(const char* str);
bool PerfMarkerPop(PerfItem2& outmkr);
void PerfMarkerEnable();
void PerfMarkerDisable();
void PerfMarkerPushState();
void PerfMarkerPopState();

///////////////////////////////////////////////////////////////////////////////

struct ProfilerSeries {
  static constexpr u16 MAX_SAMPLES = 128;
  CrcString _name;

  struct Sample {
    u64    tick;
    double total_time;
    double isolated_time;
    int    count;
    int    level;
  };

  // Rolling Data
  std::deque<Sample> _samples{}; // should be ring?

  // accumulated frame data used to addSample on endFrame

  double _total_time     = 0;
  double _isolated_time  = 0;
  int    _call_count     = 0;
  int    _max_call_level = -1;
  int    _call_level     = -1;
  bool   _sampling       = false;

  ProfilerSeries(CrcString name) : _name(name) {}
  void addSample(Sample sample);
};

using profiler_series_ptr_t = std::shared_ptr<ProfilerSeries>;

///////////////////////////////////////////////////////////////////////////////

struct ProfilerScope;

struct ProfilerChannel {
  CrcString _name;

  // Map of all series that belong to this channel
  std::map<u64, profiler_series_ptr_t> _series{};
  std::vector<ProfilerSeries*> _series_iter{};

   // Transient data to determine hiearchy level
  int _current_level  = 0;
  u64 _current_tick   = 0;

  ProfilerChannel(CrcString&& name) : _name(name) {}

  profiler_series_ptr_t createSeries(CrcString name);

  // prepare frame 
  virtual void beginProfilerFrame();
  virtual void endProfilerFrame();

  virtual void beginSample(ProfilerSeries* series) = 0;
  virtual void endSample(ProfilerSeries* series)   = 0;
  void beginSample(profiler_series_ptr_t s) { beginSample(s.get()); }
  void endSample(profiler_series_ptr_t s)   { endSample(s.get()); }

  ProfilerScope sampleScope(ProfilerSeries* series);
  ProfilerScope sampleScope(profiler_series_ptr_t s); // defined after ProfilerScope
};

using profiler_channel_ptr_t = std::shared_ptr<ProfilerChannel>;

struct CpuProfilerChannel final : ProfilerChannel {
  Timer _timer{}; // TODO change to __rdtsc ?

  struct Timespan {
    ProfilerSeries* series;
    double start_total_time;
    double start_isolated_time;
  };
  std::stack<Timespan> _span_stack{};

  using ProfilerChannel::ProfilerChannel;

  void beginSample(ProfilerSeries* series) override;
  void endSample(ProfilerSeries* series) override;
};

struct ProfilerScope {
  ProfilerScope(ProfilerChannel* channel, ProfilerSeries* series) : _channel(channel), _series(series) {}
  ~ProfilerScope() {
    if (!_series->_sampling) return;
    _channel->endSample(_series);
  }
  ProfilerChannel* _channel;
  ProfilerSeries*  _series;
};

inline ProfilerScope ProfilerChannel::sampleScope(profiler_series_ptr_t s) { return sampleScope(s.get()); }

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
