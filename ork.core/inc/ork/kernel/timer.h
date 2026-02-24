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
  static constexpr u16 MAX_SAMPLES = 1024;
  CrcString _name;

  struct Sample {
    u64    tick;
    double time;
    int    count;
  };

  // Rolling Data
  std::deque<Sample> _samples{}; // should be ring?

  // accumulated frame data used to addSample on endFrame
  double _time  = 0;
  int    _count = 0;

  // transient recording data
  int _level = -1;

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
  int _current_level{};
  u64 _current_tick{};

  ProfilerChannel(CrcString&& name) : _name(name) {}

  profiler_series_ptr_t createSeries(CrcString name);

  // prepare frame 
  virtual void beginProfilerFrame();
  virtual void endProfilerFrame();

  // sample. must override
  virtual void beginSample(profiler_series_ptr_t series) = 0;
  virtual void endSample(profiler_series_ptr_t series)   = 0;

  ProfilerScope sampleScope(profiler_series_ptr_t series);
};

using profiler_channel_ptr_t = std::shared_ptr<ProfilerChannel>;

struct CpuProfilerChannel final : ProfilerChannel {
  Timer _timer{}; // TODO change to __rdtsc ?

  struct Timespan {
    ProfilerSeries* series;
    double start_time;
    double accum_time;
  };
  std::stack<Timespan> _span_stack{};

  using ProfilerChannel::ProfilerChannel;

  void beginSample(profiler_series_ptr_t series) override;
  void endSample(profiler_series_ptr_t series) override;
};

struct ProfilerScope {
  ProfilerScope(ProfilerChannel* channel, profiler_series_ptr_t series) : _channel(channel), _series(series) { }
  ~ProfilerScope() { 
    // this is a valid scenario if you made a sampleScope but then endSample on something above it in the stack
    if (_series->_level == -1) {
      printf("ProfilerSeries endSample already called for %s\n", _series->_name.strval());
      return;
    }
    _channel->endSample(_series); 
  }
  ProfilerChannel* _channel;
  profiler_series_ptr_t _series;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
