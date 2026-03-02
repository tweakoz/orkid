#pragma once

#include <ork/kernel/timer.h>
#include <ork/kernel/kernel.h>
#include <ork/util/crc.h>
#include <deque>
#include <map>
#include <memory>
#include <stack>
#include <vector>
#include <shared_mutex>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

#define CHANNEL_MAIN   "MainThread"
#define CHANNEL_UPDATE "UpdateThread"
#define CHANNEL_AUDIO  "AudioThread"
#define CHANNEL_GPU    "GPU"

#define _CONCAT(a, b) a##b
#define CONCAT(a, b) _CONCAT(a, b)
#define UNIQUE(name) CONCAT(name, __LINE__)

// We use macros and stamp down copies of the static var and if statement to evade std::map lookup every time
// and rely on CPU prediction to optimize away the overhead of the profiler marker after first call.
#define _OrkStaticAcquireChannel(_channel_name, _type, _var, _call, _params) \
    static _type* _var = nullptr; \
    if (_var == nullptr) [[unlikely]] _var = Profiler::acquireChannel<_type>(_channel_name, CRCU(_channel_name)); \
    _var->_call(_params)

#define _OrkStaticGetChannel(_channel_name, _var, _call) \
    static ProfilerChannel* _var = nullptr; \
    if (_var == nullptr) [[unlikely]] _var = Profiler::getChannel(_channel_name, CRCU(_channel_name)); \
    _var->_call()

#define _OrkStaticSeries(_channel_name, _series_name, _var, _call) \
    static ProfilerSeries* _var = nullptr; \
    if (_var == nullptr) [[unlikely]] _var = Profiler::acquireSeries(_channel_name, CRCU(_channel_name), _series_name, CRCU(_series_name)); \
    _var->_call()

#define _OrkStaticScope(_channel_name, _series_name, _var) \
    static ProfilerSeries* _var = nullptr; \
    if (_var == nullptr) [[unlikely]] _var = Profiler::acquireSeries(_channel_name, CRCU(_channel_name), _series_name, CRCU(_series_name)); \
    auto CONCAT(_var, scope) = _var->sampleScope()

// Initial frame begin defines what type the channel is and lazy allocates on first call. Additional optional parameters can be passed in.
#define OrkProfilerFrameBegin(_channel_name, _type, _params) _OrkStaticAcquireChannel(_channel_name, _type, UNIQUE(_series), frameBegin, _params)
#define OrkProfilerFrameEnd(_channel_name)                   _OrkStaticGetChannel(_channel_name,            UNIQUE(_series), frameEnd)
#define OrkProfilerSampleBegin(_channel_name, _series_name)  _OrkStaticSeries(_channel_name, _series_name,  UNIQUE(_series), sampleBegin)
#define OrkProfilerSampleEnd(_channel_name, _series_name)    _OrkStaticSeries(_channel_name, _series_name,  UNIQUE(_series), sampleEnd)
#define OrkProfilerSampleScope(_channel_name, _series_name)  _OrkStaticScope(_channel_name,  _series_name,  UNIQUE(_series))

///////////////////////////////////////////////////////////////////////////////

template<typename T, size_t N>
struct SPSCQueue {
    std::array<T, N> _buf;
    std::atomic<size_t> _head{0};
    std::atomic<size_t> _tail{0};

    // return false if overflow
    bool push(const T& val) {
        size_t h = _head.load(std::memory_order_relaxed);
        size_t next = (h + 1) % N;
        if (next == _tail.load(std::memory_order_acquire)) return false; 
        _buf[h] = val;
        _head.store(next, std::memory_order_release);
        return true;
    }

    // return false if there was overflow in prior push
    bool drain(std::deque<T>& out) {
      size_t t = _tail.load(std::memory_order_relaxed);
      size_t h = _head.load(std::memory_order_acquire);
      if (h >= t) {
          out.insert(out.end(), &_buf[t], &_buf[h]);
      } else {
          out.insert(out.end(), &_buf[t], &_buf[N]);
          out.insert(out.end(), &_buf[0], &_buf[h]);
      }
      _tail.store(h, std::memory_order_release);
      bool overflow = (h - t) > N;
      return !overflow;
    }
};

///////////////////////////////////////////////////////////////////////////////

struct ProfilerScope;
struct ProfilerChannel;

struct ProfilerSeries {
  std::string _name;
  ProfilerChannel* _parent;

  struct Sample {
    u64    tick;
    double total_time;
    double isolated_time;
    int    count;
    int    level;
  };

  std::deque<Sample> _samples{};

  // Other threads first push their sample to this thread-safe buffer
  // then the mainthread displaying the ProfilerSeries must call flushBuffer
  // to transfer them to _samples before display. It assumes flushBuffer will be 
  // called frequently enough to keep this from overflowing.
  std::unique_ptr<SPSCQueue<Sample, 1024>> _sample_buffer = std::make_unique<SPSCQueue<Sample, 1024>>();
  
  // accumulated frame data used to addSample on endFrame
  double _total_time     = 0;
  double _isolated_time  = 0;
  int    _call_count     = 0;
  int    _max_call_level = -1;
  int    _call_level     = -1;
  bool   _sampling       = false;
  bool   _overflow       = false;

  ProfilerSeries(std::string name, ProfilerChannel* parent) : _name(name), _parent(parent) {}

  void addSample(Sample sample);

  // Returns false if there was an overflow in the sample_buffer due to flush not being called frequently enough.
  bool flushBuffer(); 

  void sampleBegin();
  void sampleEnd();
  ProfilerScope sampleScope();
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

  bool _capture_fps = false;
  double _begin_time{};
  std::atomic<double> _frame_time{};

  ProfilerChannel(std::string&& name) : _name(name) {}
  
  // prepare frame
  virtual void frameBegin() = 0;
  virtual void frameEnd()   = 0;

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

  void frameBegin() override;
  void frameEnd() override;

  struct BeginParams {
    bool capture_fps;
  };
  void frameBegin(BeginParams params) { 
    _capture_fps = params.capture_fps;
    frameBegin();
  }

  void sampleBegin(ProfilerSeries* series) override;
  void sampleEnd(ProfilerSeries* series) override;
};

///////////////////////////////////////////////////////////////////////////////

struct Profiler {

  // global state values to control all profiler sampling
  static inline std::atomic<bool> _enabled     = true;
  static inline std::atomic<u16>  _max_samples = 256;

  static void enabled(bool state) { return _enabled.store(state); }
  static bool enabled() { return _enabled.load(); }

  static void maxSamples(u16 value) { return _max_samples.store(value); }
  static u16  maxSamples() { return _max_samples.load(); }

  // Global catalong of all channels.
  static inline std::unordered_map<u64, std::shared_ptr<ProfilerChannel>> _channels;

  // We must lock global catalog on acquire and get. Sample points return a pointer so lookup only happens once.
  static inline std::shared_mutex _channel_mtx;

  template <typename T>
  static T* acquireChannel(const char* name, u64 namecrc) {
    std::unique_lock lock(_channel_mtx);
    auto& c = _channels[namecrc];
    if (!c) c = std::make_shared<T>(std::string(name));
    return static_cast<T*>(c.get());
  }

  static ProfilerChannel* getChannel(const char* name, u64 namecrc) {
    OrkAssertI(_channels.contains(namecrc), "First acquireChannel get trying to getChannel!");
    std::unique_lock lock(_channel_mtx);
    auto& c = _channels[namecrc];
    return (ProfilerChannel*)c.get();
  }

  static ProfilerSeries* acquireSeries(const char* channel_name, u64 channel_namecrc, const char* series_name, u64 series_namecrc) {
    OrkAssertI(_channels.contains(channel_namecrc), "First acquireChannel. Call frameBegin before trying to acquireSeries!");
    std::unique_lock lock(_channel_mtx);
    auto& c = _channels[channel_namecrc];
    auto& s = c->_series[series_namecrc]; 
    if (!s) {
      s = std::make_shared<ProfilerSeries>(series_name, c.get());
      c->_series_iter.push_back(s.get());
    }
    return s.get();
  }

  // Methods to retrieve channels dynamically with std::string for manual customizaiton.
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
