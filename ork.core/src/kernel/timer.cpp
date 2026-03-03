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

} // namespace ork
