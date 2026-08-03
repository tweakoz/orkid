#include <ork/pch.h>
#include <ork/kernel/profiler.h>
#include <ork/util/logger.h>
#include <cstdio>
#include <cstdlib>
#include <cctype>

///////////////////////////////////////////////////////////////////////////////
namespace ork {

logchannel_ptr_t logchan_prof = logger()->configureChannel("PROF", fvec3(0.1, 0.5, 0.9), true);

// #define PROF_LOG(...) do { printf(__VA_ARGS__); fflush(stdout); } while(0)
#define PROF_LOG(...) ((void)0)

////////////////////////////////////////////////////////////////////////////////

#ifdef ORK_PROFILER_ENABLE
namespace {
// ORKID_PROFILER_DUMP=<path> : the headless readout. The GUI ProfilerView is the only
// other consumer of these series and it needs a window, so an offscreen bench had no way
// to see per-phase cost at all. Unset = never opened, nothing written, no clock read.
//
// One whitespace-separated line per (frame,series) that actually ran:
//    <t_s> <channel> <frame> <series> <total_ms> <isolated_ms> <count>
// written from ProfilerChannel::frameEnd BEFORE addSample zeroes the accumulators, so the
// numbers are exactly what the frame committed. ATTRIBUTION: VkProfilerChannel::frameEnd
// resolves its timestamp queries with VK_QUERY_RESULT_WAIT_BIT in that same call - the
// ticks belong to the frame being closed, never to a later one - so <frame> (the channel's
// own frameEnd counter) is the frame the sample measured.
//
// Series with count==0 are SKIPPED rather than written as zeros: a phase that did not run
// this frame has no duration, and zero rows would drag its percentiles toward zero.
//
// frameEnd runs on whatever thread owns each channel (GPU/Main/Update are distinct
// channels on distinct threads), so the file is mutex-guarded. Flushed every _kflush
// lines because a bench run ends by SIGTERM, which runs no destructor.
struct ProfilerDump {
  static constexpr int _kflush = 256;

  static ProfilerDump& instance() {
    static ProfilerDump _dump;
    return _dump;
  }

  ~ProfilerDump() {
    if (_file)
      fclose(_file);
  }

  // names reach the file as single tokens - a thread name with a space in it would
  // silently shift every column to its right.
  static std::string _tokenize(const std::string& inp) {
    std::string out = inp;
    for (auto& ch : out)
      if (std::isspace((unsigned char)ch))
        ch = '_';
    return out;
  }

  void writeFrame(ProfilerChannel* channel) {
    std::lock_guard<std::mutex> lock(_mtx);
    if (not _opened) {
      _opened   = true;
      auto path = std::getenv("ORKID_PROFILER_DUMP");
      if (path) {
        _file = fopen(path, "w");
        if (nullptr == _file)
          fprintf(stderr, "ORKID_PROFILER_DUMP<%s> could not be opened for writing\n", path);
        else {
          fprintf(_file, "# ORKID_PROFILER_DUMP v1: t_s channel frame series total_ms isolated_ms count\n");
          _timer.Start();
        }
      }
    }
    if (nullptr == _file)
      return;

    double t_s        = _timer.SecsSinceStart();
    auto channel_name = _tokenize(channel->_name);
    double scale      = channel->_tick_to_ms;

    for (auto series : channel->_series_iter) {
      if (series->_style != ProfilerSeries::Style::Sample)
        continue;
      auto s = static_cast<SampleProfilerSeries*>(series);
      if (0 == s->_call_count)
        continue;
      fprintf(
          _file,
          "%.4f %s %llu %s %.4f %.4f %d\n",
          t_s,
          channel_name.c_str(),
          (unsigned long long)channel->_current_tick,
          _tokenize(series->_name).c_str(),
          double(s->_total_ticks) * scale,
          double(s->_isolated_ticks) * scale,
          s->_call_count);
      if (0 == (++_lines % _kflush))
        fflush(_file);
    }
  }

  std::mutex _mtx;
  Timer _timer;
  FILE* _file  = nullptr;
  bool _opened = false;
  u64 _lines   = 0;
};
} // namespace
#else
namespace {
// The dump lives entirely behind the compile gate, so a request for it on a stock binary
// would otherwise produce an empty file and a silent, wrong "no phases cost anything".
struct ProfilerDumpUnavailable {
  ProfilerDumpUnavailable() {
    if (std::getenv("ORKID_PROFILER_DUMP"))
      fprintf(
          stderr,
          "ORKID_PROFILER_DUMP is set, but this binary was built WITHOUT ORK_PROFILER_ENABLE - "
          "no phase timings will be written. Rebuild with 'ork.build.py --profiler'.\n");
  }
};
ProfilerDumpUnavailable _profiler_dump_unavailable;
} // namespace
#endif

////////////////////////////////////////////////////////////////////////////////

void SampleProfilerSeries::addSample() {
  if (_call_level == -1) {
    double scale  = _parent->_tick_to_ms;
    Sample sample = {
        .total_ms    = double(_total_ticks) * scale,
        .isolated_ms = double(_isolated_ticks) * scale,
        .count       = _call_count,
        .level       = _max_call_level};
    _sample_buffer->push(sample);
  } else {
    logchan_prof->log("addSample(%s) but _call_level=%d! Ensure sampleEnd called. Or use sampleScope. Skipping.", _name.c_str(), _call_level);
  }

  _total_ticks    = 0;
  _isolated_ticks = 0;
  _call_count     = 0;
  _call_level     = -1;
  _max_call_level = -1;
}

bool SampleProfilerSeries::flushBuffer() {
  bool success    = _sample_buffer->drain(_samples);
  u16 max_samples = Profiler::maxSamples();
  while (_samples.size() > max_samples)
    _samples.pop_front();
  return success;
}

void SampleProfilerSeries::sampleBegin() {
  _parent->sampleBegin(this);
}
void SampleProfilerSeries::sampleEnd() {
  _parent->sampleEnd(this);
}
ProfilerScope SampleProfilerSeries::sampleScope() {
  return _parent->sampleScope(this);
}

////////////////////////////////////////////////////////////////////////////////

void EventProfilerSeries::addEvent() {
  _event_buffer->push({_parent->_current_tick});
}

bool EventProfilerSeries::flushBuffer() {
  bool success    = _event_buffer->drain(_events);
  u16 max_samples = Profiler::maxSamples();
  while (_events.size() > max_samples)
    _events.pop_front();
  return success;
}

////////////////////////////////////////////////////////////////////////////////

void ProfilerChannel::frameBegin() {

}

void ProfilerChannel::frameEnd() {
#ifdef ORK_PROFILER_ENABLE
  // must precede addSample - that zeroes the accumulators this reads.
  ProfilerDump::instance().writeFrame(this);
#endif
  // We add a sample for all of them even if they didn't accumulate a sample so that the sample vectors lineup.
  // Some samples may have 0 total_accum_time and call_level -1!
    for (auto series : _series_iter) {
    if (series->_style != ProfilerSeries::Style::Sample) continue;
    SampleProfilerSeries* s = static_cast<SampleProfilerSeries*>(series);
    s->addSample();
  }

  _current_level = 0;
  _current_tick++;
}

[[nodiscard]] ProfilerScope ProfilerChannel::sampleScope(SampleProfilerSeries* series) {
  sampleBegin(series);
  return ProfilerScope(this, series);
}

////////////////////////////////////////////////////////////////////////////////

void CpuProfilerChannel::frameBegin() {
  _recording = Profiler::enabled();
  if (!_recording) [[unlikely]] return;
  if (!_span_stack.empty()) [[unlikely]] {
    logchan_prof->log("frameBegin(%s) but _span_stack not empty (size=%zu)! Ensure sampleEnd called. Or use sampleScope. Skipping.",
      _name.c_str(), _span_stack.size());
    while (!_span_stack.empty()) {
      logchan_prof->log("   stale entry: %s", _span_stack.top().series->_name.c_str());
      _span_stack.pop();
    }
    _current_level = 0;
  }
  _tick_to_ms      = MS_PER_NS;
  _begin_time      = Timer::get_sync_time();
}

void CpuProfilerChannel::frameEnd() {
  if (!_recording) [[unlikely]] return;
  if (!_span_stack.empty()) [[unlikely]] { 
    logchan_prof->log("frameEnd(%s) but _span_stack not empty (size=%zu)! Ensure sampleEnd called. Or use sampleScope. Skipping.",
      _name.c_str(), _span_stack.size());
    while (!_span_stack.empty()) {
      logchan_prof->log("   leaked entry: %s", _span_stack.top().series->_name.c_str());
      _span_stack.pop();
    }
    _current_level = 0;
  }
  double frame_time = Timer::get_sync_time() - _begin_time;
  _frame_time.store(frame_time);
  ProfilerChannel::frameEnd();
}

void CpuProfilerChannel::sampleBegin(SampleProfilerSeries* s) {
  if (!_recording) [[unlikely]] return;
  if (s->_call_level != -1) [[unlikely]]  {
    logchan_prof->log("sampleBegin(%s::%s) _call_level=%d already sampling! Ensure sampleEnd called. Or use sampleScope. Skipping.",
      _name.c_str(), s->_name.c_str(), s->_call_level);
    return;
  }
  u64 now = Timer::getSystemTick();

  // pause parent by accumulating its time so far
  if (!_span_stack.empty()) {
    auto& parent = _span_stack.top();
    parent.series->_isolated_ticks += now - parent.start_isolated_tick;
  }

  s->_call_level = _current_level++;
  s->_sampling   = true;
  s->_call_count++;
  PROF_LOG("[PROF-DBG] sampleBegin(%s::%s) level=%d stack_depth=%zu\n",
    _name.c_str(), s->_name.c_str(), _current_level, _span_stack.size() + 1);
  _span_stack.push({.series = s, .start_total_tick = now, .start_isolated_tick = now});
}

void CpuProfilerChannel::sampleEnd(SampleProfilerSeries* s) {
  if (!_recording) [[unlikely]] return;
  if (s->_call_level == -1) [[unlikely]]  {
    logchan_prof->log("sampleEnd(%s::%s) but _call_level=-1 not sampling! Ensure sampleBegin was called or use sampleScope. Skipping.",
      _name.c_str(), s->_name.c_str());
    return;
  }
  u64 now = Timer::getSystemTick();

  while (!_span_stack.empty()) {
    auto& top = _span_stack.top();
    auto  top_series = top.series;

    PROF_LOG("[PROF-DBG] sampleEnd(%s::%s) popping: %s level=%d stack_depth=%zu\n",
      _name.c_str(), s->_name.c_str(), top_series->_name.c_str(), _current_level, _span_stack.size());

    // exclude time in nested scopes from parent scope
    top_series->_total_ticks    += (now - top.start_total_tick);
    top_series->_isolated_ticks += (now - top.start_isolated_tick);
    top_series->_max_call_level = std::max(top_series->_max_call_level, _current_level);
    top_series->_call_level     = -1;
    top_series->_sampling       = false;
    _current_level--;
    OrkAssertI(_current_level >= 0, "CpuProfilerChannel _current_level should never go below 0!");
    _span_stack.pop();

    // resume parent
    if (!_span_stack.empty())
      _span_stack.top().start_isolated_tick = now;

    // pop and end samples for all children of passed in series
    if (top_series == s)
      return;
  }
  logchan_prof->log("sampleEnd(%s::%s) sampleBegin never called for this series!", _name.c_str(), s->_name.c_str());
}

////////////////////////////////////////////////////////////////////////////////

std::atomic<bool> Profiler::_enabled     = true;
std::atomic<u16>  Profiler::_max_samples = 256;
std::unordered_map<u64, profiler_channel_ptr_t> Profiler::_channels;
std::unordered_map<u64, std::vector<ProfilerChannel*>> Profiler::_channels_by_name;
std::shared_mutex Profiler::_channel_mtx;

////////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////