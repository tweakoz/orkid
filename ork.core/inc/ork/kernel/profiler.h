#pragma once

////////////////////////////////////////////////////////////////////////////////
// Profiler - Hierarchical CPU timing profiler with per-channel sample series.
//
// CONCEPTS
//   Channel  - A named timing domain, typically one per thread (e.g. "MainThread").
//              Must be initialized with a frameBegin before any series are used.
//   Series   - A named timer within a channel (e.g. "RenderScene").
//              Tracks total time, isolated time (excluding nested series), call
//              count, and nesting level per frame.
//   Frame    - Delimited by frameBegin / frameEnd.  All series samples are
//              accumulated during the frame and committed on frameEnd.
//
// CHANNEL NAME CONSTANTS (predefined for common threads)
//   CHANNEL_MAIN   "MainThread"
//   CHANNEL_UPDATE "UpdateThread"
//   CHANNEL_AUDIO  "AudioThread"
//   CHANNEL_GPU    "GPU"
//
// USAGE - MACROS (preferred, zero-overhead after first call)
//
//   1. Start a frame on a channel (lazy-creates the channel on first call).
//      The second argument is the channel type; use CpuProfilerChannel for CPU timing.
//      Optional params struct can be passed (e.g. to enable FPS capture).
//
//        OrkProfilerFrameBegin(CHANNEL_MAIN, CpuProfilerChannel);
//        OrkProfilerFrameBegin(CHANNEL_MAIN, CpuProfilerChannel, {.capture_fps=true});
//
//   2. End the frame (flushes all series samples for the channel):
//
//        OrkProfilerFrameEnd(CHANNEL_MAIN);
//
//   3. Bracket a region of code with explicit begin/end (matched pairs required):
//
//        OrkProfilerSampleBegin(CHANNEL_MAIN, "MySystem::update");
//        // ... work ...
//        OrkProfilerSampleEnd(CHANNEL_MAIN, "MySystem::update");
//
//   4. RAII scope guard (recommended — exception-safe, no matching end needed):
//
//        OrkProfilerSampleScope(CHANNEL_MAIN, "MySystem::update");
//        // scope ends automatically when the enclosing block exits
//
// USAGE - PROGRAMMATIC API (for dynamic channel names or tooling)
//
//        auto* ch = Profiler::acquireChannel<CpuProfilerChannel>("MyChannel");
//        ch->frameBegin();
//        auto* s  = Profiler::acquireSeries("MyChannel", "MyWork");
//        s->sampleBegin();
//        // ... work ...
//        s->sampleEnd();
//        ch->frameEnd();
//
// CROSS-THREAD MARKERS
//   Series use a lock-free SPSC queue so that non-owner threads can push samples
//   without contention.  The display/consumer thread must call
//   ProfilerSeries::flushBuffer() to transfer queued samples into _samples before
//   reading them.  Returns false if the 1024-entry queue overflowed.
//
// GLOBAL CONTROLS
//   Profiler::enabled(bool)     - enable / disable all sampling globally
//   Profiler::maxSamples(u16)   - cap the number of retained samples per series
//
////////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/timer.h>
#include <ork/kernel/kernel.h>
#include <ork/util/crc.h>
#include <ork/orkstd.h>
#include <ork/kernel/concurrent_queue.h>
#include <deque>
#include <map>
#include <memory>
#include <stack>
#include <vector>
#include <shared_mutex>

////////////////////////////////////////////////////////////////////////////////
namespace ork {
////////////////////////////////////////////////////////////////////////////////

// Common channel names.
#define CHANNEL_MAIN   "MainThread"
#define CHANNEL_UPDATE "UpdateThread"
#define CHANNEL_AUDIO  "AudioThread"
#define CHANNEL_GPU    "GPU"

// Initial frame begin defines what type the channel is and lazy allocates on first call. Additional optional parameters can be passed in.
#define OrkProfilerFrameBegin(_channel_name, _type, ...) _OrkStaticAcquireChannel(_channel_name, _type, OrkUnique(_series), frameBegin, __VA_ARGS__)
#define OrkProfilerFrameEnd(_channel_name)               _OrkStaticGetChannel(_channel_name, OrkUnique(_series), frameEnd)

// Every begin must be paired with an end.
#define OrkProfilerSampleBegin(_channel_name, _series_name)  _OrkStaticSeries(_channel_name, _series_name, SampleProfilerSeries, OrkUnique(_series), sampleBegin)
#define OrkProfilerSampleEnd(_channel_name, _series_name)    _OrkStaticSeries(_channel_name, _series_name, SampleProfilerSeries, OrkUnique(_series), sampleEnd)

// Scope will automatically call end sample when going out of scope.
#define OrkProfilerSampleScope(_channel_name, _series_name)  _OrkStaticScope(_channel_name,  _series_name,  OrkUnique(_series))

// Events are single occurances that are draw as vertical markers rather than a continuous graph.
#define OrkProfilerEvent(_channel_name, _series_name)  _OrkStaticSeries(_channel_name,  _series_name, EventProfilerSeries, OrkUnique(_series), addEvent)

// We use macros and stamp down copies of the static var and if statement to evade std::map lookup every time
// and rely on CPU prediction to optimize away the overhead of the profiler marker after first call.
#define _OrkStaticAcquireChannel(_channel_name, _type, _var, _call, ...) \
    static _type* _var = nullptr; \
    if (_var == nullptr) [[unlikely]] _var = Profiler::acquireChannel<_type>(_channel_name, CRCU(_channel_name)); \
    _var->_call(__VA_ARGS__)

#define _OrkStaticGetChannel(_channel_name, _var, _call) \
    static ProfilerChannel* _var = nullptr; \
    if (_var == nullptr) [[unlikely]] _var = Profiler::getChannel(_channel_name, CRCU(_channel_name)); \
    _var->_call()

#define _OrkStaticSeries(_channel_name, _series_name, _type, _var, _call) \
    static _type* _var = nullptr; \
    if (_var == nullptr) [[unlikely]] _var = Profiler::acquireSeries<_type>(_channel_name, CRCU(_channel_name), _series_name, CRCU(_series_name)); \
    _var->_call()

#define _OrkStaticScope(_channel_name, _series_name, _var) \
    static SampleProfilerSeries* _var = nullptr; \
    if (_var == nullptr) [[unlikely]] _var = Profiler::acquireSeries<SampleProfilerSeries>(_channel_name, CRCU(_channel_name), _series_name, CRCU(_series_name)); \
    auto OrkConcat(_var, scope) = _var->sampleScope()

////////////////////////////////////////////////////////////////////////////////

struct ProfilerScope;
struct ProfilerChannel;

struct ProfilerSeries {
  static constexpr int BufferSize = 32;

  std::string _name;
  ProfilerChannel* _parent;

  enum class Style {
    Unknown,
    Sample,
    Event,
  };
  Style _style;

  ProfilerSeries(std::string name, ProfilerChannel* parent, Style style) : _name(name), _parent(parent), _style(style) {}

  // Returns false if there was an overflow in the sample_buffer due to flush not being called frequently enough.
  virtual bool flushBuffer() = 0;
};

using profiler_series_ptr_t = std::shared_ptr<ProfilerSeries>;

struct SampleProfilerSeries : ProfilerSeries {

  struct Sample {
    double total_time;
    double isolated_time;
    int    count;
    int    level;
  };

  std::deque<Sample> _samples{};

  // Producer thread first pushes their sample to this thread-safe buffer
  // then the main thread consumer which displays the ProfilerSeries must call 
  // flushBuffer to transfer them to _samples before display. It assumes flushBuffer 
  // will be called frequently enough to keep this from overflowing.
  std::unique_ptr<SPSCQueue<Sample, BufferSize>> _sample_buffer = std::make_unique<SPSCQueue<Sample, BufferSize>>();
  
  // accumulated frame data used to addSample on endFrame
  double _total_time     = 0;
  double _isolated_time  = 0;
  int    _call_count     = 0;
  int    _max_call_level = -1;
  int    _call_level     = -1;
  bool   _sampling       = false;

  SampleProfilerSeries(std::string name, ProfilerChannel* parent) : ProfilerSeries(name, parent, Style::Sample) {}

  void addSample();
  bool flushBuffer() override; 

  // Convienence methods to be able to sample directly from the series rather than the channel.
  void sampleBegin();
  void sampleEnd();
  ProfilerScope sampleScope();
};

struct EventProfilerSeries : ProfilerSeries {

  struct Event {
    u64  tick;
  };

  std::deque<Event> _events{};
  std::unique_ptr<SPSCQueue<Event, BufferSize>> _event_buffer = std::make_unique<SPSCQueue<Event, BufferSize>>();

  EventProfilerSeries(std::string name, ProfilerChannel* parent) : ProfilerSeries(name, parent, Style::Event) {}

  void addEvent();
  bool flushBuffer() override; 
};

////////////////////////////////////////////////////////////////////////////////

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

  bool _recording = true;

  ProfilerChannel(std::string&& name) : _name(name) {}

  // prepare frame
  virtual void frameBegin() = 0;
  virtual void frameEnd()   = 0;

  virtual void sampleBegin(SampleProfilerSeries* series) = 0;
  virtual void sampleEnd(SampleProfilerSeries* series)   = 0;
  ProfilerScope sampleScope(SampleProfilerSeries* series);
};

using profiler_channel_ptr_t = std::shared_ptr<ProfilerChannel>;

struct ProfilerScope {
  ProfilerScope(ProfilerChannel* channel, SampleProfilerSeries* series) : _channel(channel), _series(series) {}
  ~ProfilerScope() {
    if (!_series->_sampling) return;
    _channel->sampleEnd(_series);
  }
  ProfilerChannel* _channel;
  SampleProfilerSeries*  _series;
};

////////////////////////////////////////////////////////////////////////////////

struct CpuProfilerChannel final : ProfilerChannel {
  Timer _timer{}; // TODO change to __rdtsc ?

  struct Timespan {
    SampleProfilerSeries* series;
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

  void sampleBegin(SampleProfilerSeries* series) override;
  void sampleEnd(SampleProfilerSeries* series) override;
};

////////////////////////////////////////////////////////////////////////////////

struct Profiler {

  // global state values to control all profiler sampling
  static inline std::atomic<bool> _enabled     = true;
  static inline std::atomic<u16>  _max_samples = 256;

  static void enabled(bool state) { _enabled.store(state); }
  static bool enabled() { return _enabled.load(); }

  static void maxSamples(u16 value) { return _max_samples.store(value); }
  static u16  maxSamples() { return _max_samples.load(); }

  // Global catalong of all channels.
  static inline std::unordered_map<u64, profiler_channel_ptr_t> _channels;

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

  template <typename T>
  static T* acquireSeries(const char* channel_name, u64 channel_namecrc, const char* series_name, u64 series_namecrc) {
    OrkAssertI(_channels.contains(channel_namecrc), "First acquireChannel. Call frameBegin before trying to acquireSeries!");
    std::unique_lock lock(_channel_mtx);
    auto& c = _channels[channel_namecrc];
    auto& s = c->_series[series_namecrc]; 
    if (!s) {
      s = std::make_shared<T>(series_name, c.get());
      c->_series_iter.push_back(s.get());
    }
    return static_cast<T*>(s.get());
  }

  // Methods to retrieve channels dynamically with std::string for manual customizaiton.
  // Always prefer using the OrkProfiler macros to string on string literals and crc consteval
  // Python bindings utilize these methods for samples/events from python. For light profiling that is Okay right now.
  // If we are to start collecting hundrends of samples from python we'd want to create a hot path for that.
  template <typename T>
  static T* acquireChannel(const std::string& name) {
    return acquireChannel<T>(name.c_str(), CrcString(name.c_str()).hashed());
  }

  static ProfilerChannel* getChannel(const std::string& name) {
    return getChannel(name.c_str(), CrcString(name.c_str()).hashed());
  }

  template <typename T>
  static T* acquireSeries(const std::string& channel_name, const std::string& series_name) {
    return acquireSeries<T>(channel_name.c_str(), CrcString(channel_name.c_str()).hashed(), series_name.c_str(), CrcString(series_name.c_str()).hashed());
  }
};

////////////////////////////////////////////////////////////////////////////////
} // namespace ork
////////////////////////////////////////////////////////////////////////////////
