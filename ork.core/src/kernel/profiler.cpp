#include <ork/pch.h>
#include <ork/kernel/profiler.h>
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {

logchannel_ptr_t logchan_prof = logger()->configureChannel("PROF", fvec3(0.1, 0.5, 0.9), true);

// #define PROF_LOG(...) do { printf(__VA_ARGS__); fflush(stdout); } while(0)
#define PROF_LOG(...) ((void)0)

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
std::shared_mutex Profiler::_channel_mtx;

////////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////