#include <ork/pch.h>
#include <ork/kernel/profiler.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////////

void SampleProfilerSeries::addSample() {
	OrkAssertI(_call_level == -1, "ProfilerSeries did not call endSample!");

	Sample sample = {_total_time, _isolated_time, _call_count, _max_call_level};
	if (!_sample_buffer->push(sample))
		_overflow = true;

	_total_time     = 0;
	_isolated_time  = 0;
	_call_count     = 0;
	_call_level     = -1;
	_max_call_level = -1;
}

bool SampleProfilerSeries::flushBuffer() {
	bool success = _sample_buffer->drain(_samples);
	u16 max_samples = Profiler::maxSamples();
	while (_samples.size() > max_samples)
		_samples.pop_front();
	return success;
}

void SampleProfilerSeries::sampleBegin() { _parent->sampleBegin(this); }
void SampleProfilerSeries::sampleEnd()   { _parent->sampleEnd(this); }
ProfilerScope SampleProfilerSeries::sampleScope() { return _parent->sampleScope(this); }

///////////////////////////////////////////////////////////////////////////////

void EventProfilerSeries::addEvent() {
	if (!_event_buffer->push({_parent->_current_tick}))
		_overflow = true;
}

bool EventProfilerSeries::flushBuffer() {
	bool success = _event_buffer->drain(_events);
	u16 max_samples = Profiler::maxSamples();
	while (_events.size() > max_samples)
		_events.pop_front();
	return success;
}

///////////////////////////////////////////////////////////////////////////////

void ProfilerChannel::frameBegin() {
	OrkAssertI(_current_level == 0, "ProfilerChannel endFrame not called!");
}

void ProfilerChannel::frameEnd() {
	OrkAssertI(_current_level == 0, "ProfilerChannel did not call endSample for every sample! Or no samples recorded!");

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

///////////////////////////////////////////////////////////////////////////////

void CpuProfilerChannel::frameBegin() {
	_recording = Profiler::enabled();
	if (!_recording) return;
	_begin_time = _timer.get_sync_time();
}

void CpuProfilerChannel::frameEnd() {
	if (!_recording) return;
	double frame_time = _timer.get_sync_time() - _begin_time;
	_frame_time.store(frame_time);
  	ProfilerChannel::frameEnd();
}

void CpuProfilerChannel::sampleBegin(SampleProfilerSeries* s) {
	if (!_recording) return;

	// printf("CpuProfilerChannel beginSample %s\n", s->_name.strval());
	OrkAssertI(s->_call_level == -1, "CpuProfilerSeries did not call endSample!");
	double now = _timer.get_sync_time();

	// pause parent by accumulating its time so far
    if (!_span_stack.empty()) {
      auto& parent = _span_stack.top();
      parent.series->_isolated_time += now - parent.start_isolated_time;
    }

	s->_call_level = _current_level++;
	s->_sampling   = true;
	s->_call_count++;
	_span_stack.push({.series = s, .start_total_time = now, .start_isolated_time = now});
}


void CpuProfilerChannel::sampleEnd(SampleProfilerSeries* s) {
	if (!_recording) return;

	// printf("CpuProfilerChannel endSample %s\n", s->_name.strval());
	double now = _timer.get_sync_time();
	OrkAssertI(s->_call_level != -1, "CpuProfilerSeries did not call beginSample!");

	while (!_span_stack.empty()) {
		auto& top = _span_stack.top();

		// exclude time in nested scopes from parent scope
		top.series->_total_time     += (now - top.start_total_time);
		top.series->_isolated_time += (now - top.start_isolated_time);
		top.series->_max_call_level = std::max(top.series->_max_call_level, _current_level);
		top.series->_call_level     = -1;
		top.series->_sampling       = false;
		_current_level--;
		OrkAssertI(_current_level >= 0, "CpuProfilerChannel _current_level never go below 0!");
		_span_stack.pop();

		// resume parent
		if (!_span_stack.empty())
			_span_stack.top().start_isolated_time = now;

		// pop and end samples for all children of passed in series
		if (top.series == s)
			return;
	}

	OrkAssertI(false, "ProfilerSeries beginSample never called!");
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork
