////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////


#include <ork/pch.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/concurrent_queue.h>

//////////////////////////////////////////////////////////////////////////////
#if defined(ORK_OSX) || defined(ORK_IOS)
#include <mach/mach_time.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#endif
#if defined(ORK_CONFIG_IX)
#include <unistd.h>
#include <sys/time.h>
#include <sched.h>
#include <time.h>
#endif
//////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/kernel.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/mutex.h>

#include <time.h>
#include <stdio.h>
#include <sys/timeb.h>
#include <cmath>
namespace ork {

///////////////////////////////////////////////////////////////////////////////

void Timer::Start() {
    mStartTime = get_sync_time();
}

void Timer::setCurrentTime(float time) {
    float now = get_sync_time();
	mStartTime = now - time;
}

void Timer::End() {
    mEndTime = get_sync_time();
}

///////////////////////////////////////////////////////////////////////////////

float Timer::InternalSecsSinceStart() const {
    float now = get_sync_time();
    return (now-mStartTime);
}

///////////////////////////////////////////////////////////////////////////////

float Timer::SecsSinceStart() const {
    float rval = InternalSecsSinceStart();
    return rval;
}

///////////////////////////////////////////////////////////////////////////////

float Timer::SpanInSecs() const {
    return (mEndTime-mStartTime);
}

Timer::Timer()
	: mOnInterval(nullptr)
	, mThread(nullptr)
	, mKill(false) {

}

Timer::~Timer() {
	mKill = true;
	if(mThread)
		mThread->join();
	delete mThread;
}

void Timer::OnInterval( float interval, const void_lambda_t& oper ) {
	mOnInterval = oper;

	mThread = new ork::Thread;

	if( mOnInterval )
	{
		mThread->start( [=](anyp data)
		{
			while(false==mKill)
			{
				usleep(uint64_t(interval*1e6f) );
				mOnInterval();
			}
		});
	}
}

svar64_t Timer::_gimpl;

///////////////////////////////////////////////////////////////////////////////
#if defined(ORK_OSX) || defined(ORK_IOS)
///////////////////////////////////////////////////////////////////////////////
struct TimerGlobalImpl {
	mach_timebase_info_data_t _timebase_info;
	double _resolution = 0.0;
	uint64_t _timebase = 0;
};
void Timer::staticInit() {
	auto gimpl = _gimpl.makeShared<TimerGlobalImpl>();
	mach_timebase_info(&gimpl->_timebase_info);
	gimpl->_resolution = (double)gimpl->_timebase_info.numer / (double)gimpl->_timebase_info.denom / 1000000.0;
	uint64_t tms_now = mach_absolute_time();
	gimpl->_timebase = ((tms_now>>16)<<16);
}
///////////////////////////////////////////////////////////////////////////////
#elif defined(ORK_CONFIG_IX)
///////////////////////////////////////////////////////////////////////////////
struct TimerGlobalImpl {
	uint64_t _timebase;
};
void Timer::staticInit() {
	auto gimpl = _gimpl.makeShared<TimerGlobalImpl>();
	timespec tmsnow;
  clock_gettime(CLOCK_REALTIME,&tmsnow);
  gimpl->_timebase = ((tmsnow.tv_sec>>12)<<12)*1000;
}
///////////////////////////////////////////////////////////////////////////////
#endif
///////////////////////////////////////////////////////////////////////////////

float Timer::get_sync_time() {
	static auto gimpl = Timer::_gimpl.getShared<TimerGlobalImpl>();
	////////////////////////////////
	#if defined(ORK_OSX) || defined(ORK_IOS)
	////////////////////////////////
	uint64_t tms_now = mach_absolute_time();
	uint64_t tms_del = tms_now-gimpl->_timebase;
	double millis = double(tms_del) * gimpl->_resolution;
	//printf( "resolution<%g> tms_del<%zu> millis<%g>\n", resolution, tms_del, millis );
	return float(millis*0.001);
	////////////////////////////////
	#elif defined(ORK_CONFIG_IX)
	////////////////////////////////
	struct timespec tsnow;
	clock_gettime(CLOCK_REALTIME,&tsnow);
	uint64_t tms_now = uint64_t(tsnow.tv_sec)*1000+uint64_t(tsnow.tv_nsec)/1000000;
	uint64_t tms_del = tms_now-gimpl->_timebase;
	float sec = float(tms_del)*0.001f;
	return sec;
	////////////////////////////////
	#else
	#error // not implemented
	#endif
	////////////////////////////////
}

static ork::MpMcBoundedQueue<PerfItem2,1024> gpiq;

static bool gmena = false;

typedef std::stack<bool> perf_ena_stack_t;

static ork::LockedResource<perf_ena_stack_t> gPES;


void PerfMarkerPushState() {
	perf_ena_stack_t& pes = gPES.LockForWrite();
	pes.push(gmena);
	gPES.UnLock();
}
void PerfMarkerPopState() {
	perf_ena_stack_t& pes = gPES.LockForWrite();
	gmena = pes.top();
	pes.pop();
	gPES.UnLock();
}

void PerfMarkerEnable() {
	gmena = true;
}
void PerfMarkerDisable() {
	gmena = false;
}

ork::atomic<int> gctr;

void PerfMarkerPush( const char* mkrname ) {
	if( gmena ) {
		f32	ftime = Timer::get_sync_time();
		PerfItem2 pi;
		pi.mpMarkerName = mkrname;
		pi.mfMarkerTime = ftime;
		if( gpiq.try_push( pi ) ) {
			gctr++;
			//printf( "gctr<%d>\n", int(gctr) );
		}
	}
}
bool PerfMarkerPop( PerfItem2& outmkr ) {
	bool rval = false;

	if( gmena ) {
		rval = gpiq.try_pop( outmkr );
		if( rval )
			gctr--;

	}
	return rval;
}


#if defined(__APPLE__) || defined(ORK_CONFIG_IX)

void msleep( int millisec ) {
	while( millisec>0 ) {
		usleep( 1000 );
		//sched_yield();
		millisec--;
	}
}

void usleep( int microsec ) {
	::usleep( microsec );
}

#elif defined( ORK_WIN32 )
void msleep( int millisec ) {
	Sleep( millisec );
}
void usleep(int microsec) {
	// TODO: non busy wait version

	__int64 time1 = 0;
	__int64 time2 = 0;
	__int64 sysFreq = 0;

	QueryPerformanceCounter( (LARGE_INTEGER*) & time1);
	QueryPerformanceFrequency( (LARGE_INTEGER*) & sysFreq );

	do {
		QueryPerformanceCounter( (LARGE_INTEGER*) & time2);
	}
	while((time2-time1) < microsec);
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void ProfilerSeries::addSample(Sample sample) {
	_samples.push_back(sample);
	while (_samples.size() > MAX_SAMPLES) 
		_samples.pop_front();
}

///////////////////////////////////////////////////////////////////////////////

profiler_series_ptr_t ProfilerChannel::createSeries(CrcString name) {
	printf("Creating ProfilerSeries: %s for ProfilerChannel: %s\n", name.strval(), _name.strval());
	auto [it, inserted] = _series.insert({name.hashed(), std::make_shared<ProfilerSeries>(name)});
	OrkAssertI(inserted, "Inserting ProfilerSeries twice!\n");
	_series_iter.push_back(it->second.get());
	return it->second;
}

void ProfilerChannel::beginProfilerFrame() {
	OrkAssertI(_current_level == 0, "ProfilerChannel endFrame not called!");
}

void ProfilerChannel::endProfilerFrame() { 
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
	beginSample(series);
	return ProfilerScope(this, series);
}

///////////////////////////////////////////////////////////////////////////////

void CpuProfilerChannel::beginSample(ProfilerSeries* s) {
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

void CpuProfilerChannel::endSample(ProfilerSeries* s) {
	// printf("CpuProfilerChannel endSample %s\n", s->_name.strval());
	double now = _timer.get_sync_time();

	OrkAssertI(s->_call_level != -1, "ProfilerSeries did not call beginSample!");

	while (!_span_stack.empty()) {
		auto& top = _span_stack.top();
		
		// exclude time in nested scopes from parent scope
		top.series->_total_time     = (now - top.start_total_time);
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
