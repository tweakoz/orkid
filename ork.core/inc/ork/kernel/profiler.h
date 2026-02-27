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

#define _OrkCpuStaticChannel(_channel_name, _var, _type, _call) \
    static ProfilerChannel* _var = nullptr; \
    if (_var == nullptr) _var = Profiler::acquireChannel<_type>(_channel_name, CRCU(_channel_name)); \
    _var->_call()

#define _OrkCpuStaticSeries(_channel_name, _series_name, _var, _type, _call) \
    static ProfilerSeries* _var = nullptr; \
    if (_var == nullptr) _var = Profiler::acquireSeries<_type>(_channel_name, CRCU(_channel_name), _series_name, CRCU(_series_name)); \
    _var->_call()

#define _OrkCpuStaticScope(_channel_name, _series_name, _var, _type) \
    static ProfilerSeries* _var = nullptr; \
    if (_var == nullptr) _var = Profiler::acquireSeries<_type>(_channel_name, CRCU(_channel_name), _series_name, CRCU(_series_name)); \
    auto CONCAT(_var, scope) = _var->sampleScope()

#define OrkCpuProfilerFrameBegin(_channel_name) _OrkCpuStaticChannel(_channel_name, UNIQUE(_series), CpuProfilerChannel, frameBegin)
#define OrkCpuProfilerFrameEnd(_channel_name)   _OrkCpuStaticChannel(_channel_name, UNIQUE(_series), CpuProfilerChannel, frameEnd)
#define OrkCpuProfilerSampleBegin(_channel_name, _series_name) _OrkCpuStaticSeries(_channel_name, _series_name, UNIQUE(_series), CpuProfilerChannel, sampleBegin)
#define OrkCpuProfilerSampleEnd(_channel_name, _series_name)   _OrkCpuStaticSeries(_channel_name, _series_name, UNIQUE(_series), CpuProfilerChannel, sampleEnd)
#define OrkCpuProfilerSampleScope(_channel_name, _series_name) _OrkCpuStaticScope(_channel_name, _series_name, UNIQUE(_series), CpuProfilerChannel)

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

  profiler_series_ptr_t createSeries(std::string name);

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
  static ProfilerChannel* acquireChannel(const char* name, u64 namecrc) {
    auto& c = _channels[namecrc];
    if (!c) c = std::make_shared<T>(std::string(name));
    return (ProfilerChannel*)c.get();
  }

  template <typename T>
  static ProfilerSeries* acquireSeries(const char* channel_name, u64 channel_namecrc, const char* series_name, u64 series_namecrc) {
    auto& c = _channels[channel_namecrc]; if (!c) c = std::make_shared<T>(std::string(channel_name));
    auto& s = c->_series[series_namecrc]; if (!s) {
      s = std::make_shared<ProfilerSeries>(series_name, c.get());
      c->_series_iter.push_back(s.get());
    }
    return s.get();
  }

  template <typename T>
  static void frameBegin(const char* channel_name, u64 channel_namecrc) {
    auto& c = _channels[channel_namecrc]; if (!c) c = std::make_shared<T>(std::string(channel_name));
    c->frameBegin();
  }

  template <typename T>
  static void frameEnd(const char* channel_name, u64 channel_namecrc) {
    auto& c = _channels[channel_namecrc]; if (!c) c = std::make_shared<T>(std::string(channel_name));
    c->frameEnd();
  }

  template <typename T>
  static void sampleBegin(const char* channel_name, u64 channel_namecrc, const char* series_name, u64 series_namecrc) {
    auto& c = _channels[channel_namecrc]; if (!c) c = std::make_shared<T>(std::string(channel_name));
    auto& s = c->_series[series_namecrc]; if (!s) s = std::make_shared<ProfilerSeries>(series_name, c.get());
    s->sampleBegin();
  }

  template <typename T>
  static void sampleEnd(const char* channel_name, u64 channel_namecrc, const char* series_name, u64 series_namecrc) {
    auto& c = _channels[channel_namecrc]; if (!c) c = std::make_shared<T>(std::string(channel_name));
    auto& s = c->_series[series_namecrc]; if (!s) s = std::make_shared<ProfilerSeries>(series_name, c.get());
    s->sampleEnd();
  }

  template <typename T>
  static ProfilerScope sampleScope(const char* channel_name, u64 channel_namecrc, const char* series_name, u64 series_namecrc) {
    auto& c = _channels[channel_namecrc]; if (!c) c = std::make_shared<T>(std::string(channel_name));
    auto& s = c->_series[series_namecrc]; if (!s) s = std::make_shared<ProfilerSeries>(series_name, c.get());
    return s->sampleScope();
  }
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
