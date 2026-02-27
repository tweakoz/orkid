////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/profiler.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////////

void ProfilerSeries::addSample(Sample sample) {
	_samples.push_back(sample);
	while (_samples.size() > MAX_SAMPLES)
		_samples.pop_front();
}

void ProfilerSeries::sampleBegin() { _parent->sampleBegin(this); }
void ProfilerSeries::sampleEnd()   { _parent->sampleEnd(this); }
ProfilerScope ProfilerSeries::sampleScope() { return _parent->sampleScope(this); }

///////////////////////////////////////////////////////////////////////////////

void ProfilerChannel::frameBegin() {
	OrkAssertI(_current_level == 0, "ProfilerChannel endFrame not called!");
}

void ProfilerChannel::frameEnd() {
	OrkAssertI(_current_level == 0, "ProfilerChannel did not call endSample for every sample! Or no samples recorded!");

	// We add a sample for all of them even if they didn't accumulate a sample so that the sampel vectors lineup.
	// Some samples may have 0 total_accum_time and call_level -1!
    for (auto s : _series_iter) {
		OrkAssertI(s->_call_level == -1, "ProfilerSeries did not call endSample!");
		s->addSample({_current_tick, s->_total_time, s->_isolated_time, s->_call_count, s->_max_call_level});
		s->_total_time     = 0;
		s->_isolated_time  = 0;
		s->_call_count     = 0;
		s->_call_level     = -1;
	}

	_current_level = 0;
	_current_tick++;
}

[[nodiscard]] ProfilerScope ProfilerChannel::sampleScope(ProfilerSeries* series) {
	sampleBegin(series);
	return ProfilerScope(this, series);
}

///////////////////////////////////////////////////////////////////////////////

void CpuProfilerChannel::sampleBegin(ProfilerSeries* s) {
	// printf("CpuProfilerChannel beginSample %s\n", s->_name.strval());
	double now = _timer.get_sync_time();

	OrkAssertI(s->_call_level == -1, "VkProfilerSeries did not call endSample!");

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

void CpuProfilerChannel::sampleEnd(ProfilerSeries* s) {
	// printf("CpuProfilerChannel endSample %s\n", s->_name.strval());
	double now = _timer.get_sync_time();

	OrkAssertI(s->_call_level != -1, "ProfilerSeries did not call beginSample!");

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
