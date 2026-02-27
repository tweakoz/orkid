#pragma once

#include <ork/kernel/timer.h>
#include <ork/kernel/kernel.h>
#include <ork/util/crc.h>
#include <deque>
#include <map>
#include <memory>
#include <stack>
#include <vector>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

#define OrkProfilerFrameBegin(_channel)           (_channel)->frameBegin()
#define OrkProfilerFrameEnd(_channel)             (_channel)->frameEnd()
#define OrkProfilerSampleBegin(_channel, _series) (_channel)->sampleBegin(_series)
#define OrkProfilerSampleEnd(_channel, _series)   (_channel)->sampleEnd  (_series)
#define OrkProfilerSampleScope(_channel, _series) auto _ = (_channel)->sampleScope(_series)

#define _CONCAT(a, b) a##b
#define CONCAT(a, b) _CONCAT(a, b)
#define UNIQUE(name) CONCAT(name, __LINE__)

#define _OrkStaticAcquireChannel(_channel_name, _type, _var, _call, ...) \
    static _type* _var = nullptr; \
    if (_var == nullptr) _var = Profiler::acquireChannel<_type>(_channel_name, CRCU(_channel_name)); \
    _var->_call(__VA_ARGS__)

#define _OrkStaticGetChannel(_channel_name, _var, _call) \
    static ProfilerChannel* _var = nullptr; \
    if (_var == nullptr) _var = Profiler::getChannel(_channel_name, CRCU(_channel_name)); \
    _var->_call()

#define _OrkStaticSeries(_channel_name, _series_name, _var, _call) \
    static ProfilerSeries* _var = nullptr; \
    if (_var == nullptr) _var = Profiler::acquireSeries(_channel_name, CRCU(_channel_name), _series_name, CRCU(_series_name)); \
    _var->_call()

#define _OrkStaticScope(_channel_name, _series_name, _var) \
    static ProfilerSeries* _var = nullptr; \
    if (_var == nullptr) _var = Profiler::acquireSeries(_channel_name, CRCU(_channel_name), _series_name, CRCU(_series_name)); \
    auto CONCAT(_var, scope) = _var->sampleScope()

// Initial frame begin defines what type the channel is. If different channel type needed first manually acquire it.
#define OrkProfilerFrameBegin(_channel_name, _type, ...)    _OrkStaticAcquireChannel(_channel_name, _type, UNIQUE(_series), frameBegin, __VA_ARGS__)
#define OrkProfilerFrameEnd(_channel_name)                  _OrkStaticGetChannel(_channel_name, UNIQUE(_series), frameEnd)
#define OrkProfilerSampleBegin(_channel_name, _series_name) _OrkStaticSeries(_channel_name, _series_name, UNIQUE(_series), sampleBegin)
#define OrkProfilerSampleEnd(_channel_name, _series_name)   _OrkStaticSeries(_channel_name, _series_name, UNIQUE(_series), sampleEnd)
#define OrkProfilerSampleScope(_channel_name, _series_name) _OrkStaticScope(_channel_name, _series_name, UNIQUE(_series))

struct ProfilerScope;
struct ProfilerChannel;

struct ProfilerSeries {
  static constexpr u16 MAX_SAMPLES = 128;
  std::string _name;
  ProfilerChannel* _parent;

  struct Sample {
    u64    tick;
    double total_time;
    double isolated_time;
    int    count;
    int    level;
  };

  std::deque<Sample> _samples{}; // should be ring?

  // accumulated frame data used to addSample on endFrame
  double _total_time     = 0;
  double _isolated_time  = 0;
  int    _call_count     = 0;
  int    _max_call_level = -1;
  int    _call_level     = -1;
  bool   _sampling       = false;

  ProfilerSeries(std::string name, ProfilerChannel* parent) : _name(name), _parent(parent) {}

  void sampleBegin();
  void sampleEnd();
  ProfilerScope sampleScope();
  void addSample(Sample sample);
};

using profiler_series_ptr_t = std::shared_ptr<ProfilerSeries>;

///////////////////////////////////////////////////////////////////////////////

struct ProfilerChannel {
  std::string _name;

  // Map of all series that belong to this channel
  std::unordered_map<u64, profiler_series_ptr_t> _series{};
  std::vector<ProfilerSeries*> _series_iter{};

  // Transient data to determine hierarchy level
  int _current_level  = 0;
  u64 _current_tick   = 0;

  ProfilerChannel(std::string&& name) : _name(name) {}
  
  // prepare frame
  virtual void frameBegin();
  virtual void frameEnd();

  virtual void sampleBegin(ProfilerSeries* series) = 0;
  virtual void sampleEnd(ProfilerSeries* series)   = 0;
  void sampleBegin(profiler_series_ptr_t s) { sampleBegin(s.get()); }
  void sampleEnd(profiler_series_ptr_t s)   { sampleEnd(s.get()); }

  ProfilerScope sampleScope(ProfilerSeries* series);
  ProfilerScope sampleScope(profiler_series_ptr_t s); // defined after ProfilerScope
};

using profiler_channel_ptr_t = std::shared_ptr<ProfilerChannel>;

struct ProfilerScope {
  ProfilerScope(ProfilerChannel* channel, ProfilerSeries* series) : _channel(channel), _series(series) {}
  ~ProfilerScope() {
    if (!_series->_sampling) return;
    _channel->sampleEnd(_series);
  }
  ProfilerChannel* _channel;
  ProfilerSeries*  _series;
};

inline ProfilerScope ProfilerChannel::sampleScope(profiler_series_ptr_t s) { return sampleScope(s.get()); }

///////////////////////////////////////////////////////////////////////////////

struct CpuProfilerChannel final : ProfilerChannel {
  Timer _timer{}; // TODO change to __rdtsc ?

  struct Timespan {
    ProfilerSeries* series;
    double start_total_time;
    double start_isolated_time;
  };
  std::stack<Timespan> _span_stack{};

  using ProfilerChannel::ProfilerChannel;

  void sampleBegin(ProfilerSeries* series) override;
  void sampleEnd(ProfilerSeries* series) override;
};

///////////////////////////////////////////////////////////////////////////////

struct Profiler {
  static inline std::unordered_map<u64, std::shared_ptr<ProfilerChannel>> _channels;

  template <typename T>
  static T* acquireChannel(const char* name, u64 namecrc) {
    auto& c = _channels[namecrc];
    if (!c) c = std::make_shared<T>(std::string(name));
    return static_cast<T*>(c.get());
  }

  static ProfilerChannel* getChannel(const char* name, u64 namecrc) {
    OrkAssertI(_channels.contains(namecrc), "First acquireChannel get trying to getChannel!");
    auto& c = _channels[namecrc];
    return (ProfilerChannel*)c.get();
  }

  static ProfilerSeries* acquireSeries(const char* channel_name, u64 channel_namecrc, const char* series_name, u64 series_namecrc) {
    OrkAssertI(_channels.contains(channel_namecrc), "First acquireChannel and call frameBegin before trying to acquireSeries!");
    auto& c = _channels[channel_namecrc];
    auto& s = c->_series[series_namecrc]; 
    if (!s) {
      s = std::make_shared<ProfilerSeries>(series_name, c.get());
      c->_series_iter.push_back(s.get());
    }
    return s.get();
  }

  // Methods to dynamically retrieve channels dynamically with std::string for manual customizaiton.
  // Always prefer using the OrkProfiler macros to string on string literals and crc consteval
  template <typename T>
  static T* acquireChannel(const std::string& name) {
    return acquireChannel<T>(name.c_str(), CrcString(name.c_str()).hashed());
  }
  static ProfilerChannel* getChannel(const std::string& name) {
    return getChannel(name.c_str(), CrcString(name.c_str()).hashed());
  }
  static ProfilerSeries* acquireSeries(const std::string& channel_name, const std::string& series_name) {
    return acquireSeries(channel_name.c_str(), CrcString(channel_name.c_str()).hashed(), series_name.c_str(), CrcString(series_name.c_str()).hashed());
  }
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
