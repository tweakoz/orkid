////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#include <ork/pch.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/concurrent_queue.h>

//////////////////////////////////////////////////////////////////////////////
#if defined(ORK_OSX)
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

float get_sync_time();

///////////////////////////////////////////////////////////////////////////////

void Timer::Start()
{
    mStartTime = get_sync_time();
}

///////////////////////////////////////////////////////////////////////////////

void Timer::End()
{
    mEndTime = get_sync_time();
}

///////////////////////////////////////////////////////////////////////////////

float Timer::InternalSecsSinceStart() const
{
    float now = get_sync_time();
    return (now-mStartTime);
}

///////////////////////////////////////////////////////////////////////////////

float Timer::SecsSinceStart() const
{
    float rval = InternalSecsSinceStart();
    return rval;
}

///////////////////////////////////////////////////////////////////////////////

float Timer::SpanInSecs() const
{
    return (mEndTime-mStartTime);
}

Timer::Timer()
	: mOnInterval(nullptr)
	, mThread(nullptr)
	, mKill(false)
{

}

Timer::~Timer()
{
	mKill = true;
	if(mThread)
		mThread->join();
	delete mThread;
}

void Timer::OnInterval( float interval, const void_lambda_t& oper )
{
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

///////////////////////////////////////////////////////////////////////////////

float get_sync_time()
{
////////////////////////////////
#if defined(ORK_OSX)
////////////////////////////////
	auto getres = []()->double
	{
		mach_timebase_info_data_t timebase;
		mach_timebase_info(&timebase);
		return (double)timebase.numer / (double)timebase.denom / 1000000.0;
	};
	static double resolution = getres();
    uint64_t tms_now = mach_absolute_time();
    static uint64_t tms_base = ((tms_now>>16)<<16);
    uint64_t tms_del = tms_now-tms_base;
    double millis = double(tms_del) * resolution;
    //printf( "resolution<%g> tms_del<%zu> millis<%g>\n", resolution, tms_del, millis );
	return float(millis*0.001);
////////////////////////////////
#elif defined(ORK_CONFIG_IX)
////////////////////////////////
    static struct timespec ts1st;
    static bool b1sttime = true;
    if( b1sttime )
    {
        clock_gettime(CLOCK_REALTIME,&ts1st);
        b1sttime = false;
    }
    struct timespec tsnow;
    clock_gettime(CLOCK_REALTIME,&tsnow);

    uint64_t tms_base = ((ts1st.tv_sec>>12)<<12)*1000;
    uint64_t tms_now = uint64_t(tsnow.tv_sec)*1000+uint64_t(tsnow.tv_nsec)/1000000;
    uint64_t tms_del = tms_now-tms_base;

    float sec = float(tms_del)*0.001f;
    return sec;
////////////////////////////////
#else
////////////////////////////////
#error // not implemented
////////////////////////////////
#endif
////////////////////////////////
}

static ork::MpMcBoundedQueue<PerfItem2,1024> gpiq;

static bool gmena = false;

typedef std::stack<bool> perf_ena_stack_t;

static ork::LockedResource<perf_ena_stack_t> gPES;


void PerfMarkerPushState()
{
	perf_ena_stack_t& pes = gPES.LockForWrite();
	pes.push(gmena);
	gPES.UnLock();
}
void PerfMarkerPopState()
{
	perf_ena_stack_t& pes = gPES.LockForWrite();
	gmena = pes.top();
	pes.pop();
	gPES.UnLock();
}

void PerfMarkerEnable()
{
	gmena = true;
}
void PerfMarkerDisable()
{
	gmena = false;
}

ork::atomic<int> gctr;

void PerfMarkerPush( const char* mkrname )
{
	if( gmena )
	{
		f32	ftime = get_sync_time();
		PerfItem2 pi;
		pi.mpMarkerName = mkrname;
		pi.mfMarkerTime = ftime;
		if( gpiq.try_push( pi ) )
		{
			gctr++;

			//printf( "gctr<%d>\n", int(gctr) );
		}
	}
}
bool PerfMarkerPop( PerfItem2& outmkr )
{
	bool rval = false;

	if( gmena )
	{
		rval = gpiq.try_pop( outmkr );
		if( rval )
			gctr--;

	}
	return rval;
}


#if defined(__APPLE__) || defined(ORK_CONFIG_IX)

void msleep( int millisec )
{
	/*ork::mutex mtx("msleep");
	mtx.Lock();

	__block ork::mutex* pMTX = & mtx;
	auto handler_blk = ^ void ()
	{
		pMTX->UnLock();
	};

	dispatch_time_t at = dispatch_time(DISPATCH_TIME_NOW, millisec*1000*1000 ); // in nanos
	dispatch_after( at, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH,0), handler_blk );

	mtx.Lock();
	mtx.UnLock();*/
	//sched_yield();
	//f32	ftime1 = OldSchool::GetRef().GetLoResTime();
	//f32 ftime2 = ftime1 + f32(millisec)*0.001f;
	while( millisec>0 )
	{
		usleep( 1000 );
		//sched_yield();
		millisec--;
	}
}
void usleep( int microsec )
{
	::usleep( microsec );
}
#elif defined( ORK_WIN32 )
void msleep( int millisec )
{
	Sleep( millisec );
}
void usleep(int microsec)
{
	// TODO: non busy wait version

    __int64 time1 = 0;
	__int64 time2 = 0;
	__int64 sysFreq = 0;

    QueryPerformanceCounter( (LARGE_INTEGER*) & time1);
    QueryPerformanceFrequency( (LARGE_INTEGER*) & sysFreq );

    do
	{
	    QueryPerformanceCounter( (LARGE_INTEGER*) & time2);
	}
	while((time2-time1) < microsec);
}
#endif

int OldSchool::GetNumCores()
{
#if defined(ORK_CONFIG_IX)
	int numCPUs = sysconf(_SC_NPROCESSORS_ONLN);
#else
	static int numCPUs = -1;
	if(-1 == numCPUs ){
    size_t count_len = sizeof(numCPUs);
	 sysctlbyname("hw.logicalcpu", &numCPUs, &count_len, NULL, 0);

	//fprintf(stderr,"you have %i cpu cores\n", numCPUs);
	fflush(stdout);
}
#endif
	return numCPUs;

}


S64 OldSchool::GetClockCycle(void) {
	uint64_t counter = 0; 
	
	#if defined(ORK_ARCHITECTURE_X86_64)
	counter = __builtin_readcyclecounter();
	#elif defined(ORK_ARCHITECTURE_ARM_64)
	__asm __volatile("mrs %0, CNTVCT_EL0" : "=&r" (counter));
	#else
	#error // not implemented
	#endif
    
    return S64(counter);
}

S64 OldSchool::ClockCyclesToMicroSeconds(S64 cycles)
{
	OrkAssert(false);//not impl
	return S64(0);
}

///////////////////////////////////////////////////////////////////////////////
// lower res higher reliability timing
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////
#if defined( __APPLE__ )
///////////////////////////////////////////////
f32	OldSchool::GetLoResTime( void )
{
	auto getres = []()->double
	{
		mach_timebase_info_data_t timebase;
		mach_timebase_info(&timebase);
		return (double)timebase.numer / (double)timebase.denom / 1000000.0;
	};
	static double resolution = getres();
    static uint64_t gmachtime_ref = mach_absolute_time();
    uint64_t machtime_cur = mach_absolute_time();
    double millis = double(machtime_cur-gmachtime_ref) * resolution;
	return float(millis*0.001);
}
///////////////////////////////////////////////
#elif defined(ORK_CONFIG_IX)
///////////////////////////////////////////////
f32	OldSchool::GetLoResTime( void )
{
	static const float kbasetime = get_sync_time();
	return get_sync_time()-kbasetime;
}
#endif
///////////////////////////////////////////////////////////////////////////////
f32	OldSchool::GetLoResRelTime( void )
{
    static f32 fBaseTime = GetLoResTime();
    f32 fCurTime = GetLoResTime();
    f32 fRelTime = fCurTime-fBaseTime;
    return fRelTime;
}
///////////////////////////////////////////////////////////////////////////////
// higher res lower reliability timing
///////////////////////////////////////////////////////////////////////////////

f64	OldSchool::GetHiResRelTime( void )
{
    static f64 fBaseTime = GetHiResTime();
    f64 fCurTime = GetHiResTime();
    f64 fRelTime = fCurTime-fBaseTime;

    return fRelTime;
}

f64 OldSchool::GetHiResTime( void )
{
    f64 fTime = 0.0f;
    mfWallClockTime = fTime;
    return fTime;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

PerformanceItem::PerformanceItem( std::string nam )
	: mName(nam)
	, miStartCycle( 0 )
	, miEndCycle( 0 )
	, miAccumCycle( 0 )
	, miAvgCycle( 0 )
	, miChildAvgCycle( 0 )
{


	for(int i = 0; i < TIMER_AVGAMT; i++)
		miAverageSumCycle[i] = 0;
}

///////////////////////////////////////////////////////////////////////////////

void PerformanceItem::AddItem( PerformanceItem& Item )
{
	OldStlSchoolMapInsert( mChildrenMap, Item.mName, & Item );
	mChildrenList.push_back( & Item );
}

///////////////////////////////////////////////////////////////////////////////

s64 PerformanceItem::Calculate( void )
{
	miChildAvgCycle = 0;

	if(mChildrenList.size() > 0)
	{
		for( orklist< PerformanceItem* >::iterator it=mChildrenList.begin(); it!=mChildrenList.end(); it++ )
			(*it)->Calculate();

		miChildAvgCycle = 0;

		for( orklist< PerformanceItem* >::iterator it=mChildrenList.begin(); it!=mChildrenList.end(); it++ )
			miChildAvgCycle += (*it)->miAccumCycle;
	}

	////////////////////////////////
	for(int i = TIMER_AVGAMT - 1; i > 0; i--)
	{
		miAverageSumCycle[i] = miAverageSumCycle[i - 1];
	}
	miAverageSumCycle[0] = miAccumCycle;
	miAvgCycle = 0;
	for(int i = 0; i < TIMER_AVGAMT; i++)
	{
		miAvgCycle += miAverageSumCycle[i];
	}
	miAvgCycle /= TIMER_AVGAMT;
	////////////////////////////////

	miAccumCycle = 0;

	return miAvgCycle;
}

///////////////////////////////////////////////////////////////////////////////

void PerformanceItem::Enter()
{
	//miStartCycle = OldSchool::GetClockCycle();
	f64 ftime = OldSchool::GetRef().GetLoResTime();
	S64 output = S64(ftime*OldSchool::GetRef().mfClockRate);
	miStartCycle = output;
}

///////////////////////////////////////////////////////////////////////////////

void PerformanceItem::Exit()
{
	//miStartCycle = OldSchool::GetClockCycle();
	f64 ftime = OldSchool::GetRef().GetLoResTime();
	S64 output = S64(ftime*OldSchool::GetRef().mfClockRate);
	miEndCycle = output;
	miAccumCycle += (miEndCycle-miStartCycle);
}

///////////////////////////////////////////////////////////////////////////////

PerformanceTracker::PerformanceTracker()
	: NoRttiSingleton<PerformanceTracker>()
{
	mRoots[EPS_UPDTHREAD] = new PerformanceItem( "update_root" );
	mRoots[EPS_GFXTHREAD] = new PerformanceItem( "render_root" );
}

///////////////////////////////////////////////////////////////////////////////

s64 PerformanceTracker::Calculate( void )
{
	GetRef().mRoots[EPS_UPDTHREAD]->Exit();
	GetRef().mRoots[EPS_GFXTHREAD]->Exit();
	s64 value = GetRef().mRoots[EPS_UPDTHREAD]->Calculate();
	s64 value2 = GetRef().mRoots[EPS_GFXTHREAD]->Calculate();
	GetRef().mRoots[EPS_UPDTHREAD]->Enter();
	GetRef().mRoots[EPS_GFXTHREAD]->Enter();
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

void PerformanceTracker::AddItem( eperfset eset, PerformanceItem& Item )
{
	if( Item.GetName() != (std::string) "root" )
		GetRef().mRoots[eset]->AddItem( Item );
}

///////////////////////////////////////////////////////////////////////////////

orklist<PerformanceItem*>* PerformanceTracker::GetItemList( eperfset eset )
{
	return GetRef().mRoots[eset]->GetChildrenList();
}

///////////////////////////////////////////////////////////////////////////////

void PerformanceTracker::TextDump( void )
{
	PerformanceTracker::Calculate();

	/*orklist<PerformanceItem*>* PerfItemList = PerformanceTracker::GetItemList();
	s64 PerfTotal = PerformanceTracker::GetRef().mpRoot->miAvgCycle;

	for( orklist<PerformanceItem*>::iterator it=PerfItemList->begin(); it!=PerfItemList->end(); it++ )
	{
		PerformanceItem* pItem = *it;
		std::string name = pItem->GetName();

		orkprintf( "%s\n", (char*)CreateFormattedString( "%s [%d microsec]", (char*)name.c_str(), pItem->miAvgCycle).c_str() );
	}*/
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork
