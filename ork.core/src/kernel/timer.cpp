////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#include <ork/pch.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/concurrent_queue.h>

//////////////////////////////////////////////////////////////////////////////
#if defined(WIN32) && ! defined(_XBOX)
#pragma comment( lib, "Winmm.lib" )
#endif
//////////////////////////////////////////////////////////////////////////////
#if defined(ORK_OSX)
#include <mach/mach_time.h>
#endif
#if defined(IX)
#include <unistd.h>
#include <sys/time.h>
#include <ork/kernel/mutex.h>
#include <sched.h>
#include <time.h>
//#include <tbb/tbb.h>
#endif
//////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/kernel.h>
#include <ork/kernel/timer.h>
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
		mThread->start( [=]()
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
#elif defined(IX)
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
#elif defined(ORK_VS2012)
////////////////////////////////
	return CSystem::GetRef().GetLoResTime();
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


#if defined(_DARWIN) || defined(IX)

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
	//f32	ftime1 = CSystem::GetRef().GetLoResTime();
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

int CSystem::GetNumCores()
{
	#if defined(IX)
	int numCPUs = sysconf(_SC_NPROCESSORS_ONLN);
	#elif defined(_XBOX) || defined(ORK_WIN32)
	int numCPUs=3;
	#else
	SYSTEM_INFO sysinfo;
	GetSystemInfo( &sysinfo );
	int numCPUs = sysinfo.dwNumberOfProcessors;
	#endif
	orkprintf( "NumCpus<%d>\n", numCPUs );
	fflush(stdout);
	return numCPUs;

}


S64 CSystem::GetClockCycle(void)
{
#if defined(ORK_WIN32)
	f64 ftime = GetRef().GetHiResTime();
	S64 output = S64(ftime*GetRef().mfClockRate);
    return output;
#elif defined(ORK_OSX) || defined(IX)
	S64 output;
    U32 high_end, low_end;
    __asm__ __volatile__("     rdtsc" :"=a" (low_end), "=d" (high_end));
    output = high_end;
    output = output << 32;
    output += low_end;
    return output;
#elif defined( GCC ) && defined( _PS2 )
	S64 output;
    U32 high_end, low_end;
	//__asm__ __volatile__(
	//	"\n nop \n"
	//	:"=a" (low_end), "=d" (high_end)
	//	:
	//);
	output = high_end;
    output = output << 32;
    output += low_end;
    return output;
#else
	OrkAssert(false);
	return 0;
#endif
}

S64 CSystem::ClockCyclesToMicroSeconds(S64 cycles)
{
#if defined( NITRO )
	return S64(OS_TicksToMicroSeconds(cycles));
#elif defined( _MSVC )
	f64 rval = 1000000.0*(f64( cycles )/GetRef().mfClockRate);
	return S64(rval);
#else
	OrkAssert(false);//not impl
	return S64(0);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// lower res higher reliability timing
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////
#if defined( _DARWIN )
///////////////////////////////////////////////
f32	CSystem::GetLoResTime( void )
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
#elif defined( IX )
///////////////////////////////////////////////
f32	CSystem::GetLoResTime( void )
{
	static const float kbasetime = get_sync_time();
	return get_sync_time()-kbasetime;
}
///////////////////////////////////////////////
#elif defined( ORK_WIN32 ) && defined( _MSVC )
///////////////////////////////////////////////
static volatile int inumtimercallbacks = 0;
static void CALLBACK TimerCallback(UINT wTimerID, UINT msg, DWORD_PTR dwUser, DWORD dw1, DWORD dw2 )
{
	inumtimercallbacks++;
}
f32	CSystem::GetLoResTime( void )
{
#if defined (_XBOX)
	//static int basetime = GetTickCount();
	//inumtimercallbacks = (GetTickCount()-basetime);


	static bool binit = true;
	static LARGE_INTEGER TicksPerSecond;
	static DOUBLE fTicksPerMicrosecond;

	if( binit )
	{	QueryPerformanceFrequency( &TicksPerSecond );
		fTicksPerMicrosecond = (DOUBLE)TicksPerSecond.QuadPart * 0.000001;
		binit = false;
	}

	LARGE_INTEGER Current;
    QueryPerformanceCounter( &Current );
	static LARGE_INTEGER TimeBase = Current;
    __int64 reltime = Current.QuadPart-TimeBase.QuadPart;
	float ftimsecs = float((double(reltime)/fTicksPerMicrosecond)/1000000.0);
	//orkprintf( "time %f\n", ftimsecs );
	return ftimsecs;
#else
	static TIMECAPS tc;
	static UINT     wTimerRes = 0;

	if( 0 == wTimerRes )
	{
		if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR)
		{
			OrkAssert(false);
			// Error; application can't continue.
		}

		static const UINT TARGET_RESOLUTION = 1; // 1-millisecond target resolution
		wTimerRes = std::min(std::max(tc.wPeriodMin, TARGET_RESOLUTION), tc.wPeriodMax);
		MMRESULT result = timeBeginPeriod(wTimerRes);

		UINT timerid = timeSetEvent(
			1,                    // delay
			wTimerRes,            // resolution (global variable)
			(LPTIMECALLBACK) TimerCallback,        // callback function
			0,					  // user data
			TIME_PERIODIC );      // single timer event

		OrkAssert( timerid != 0 );

		orkprintf( "Starting Timer<%d>\n", timerid );
	}
	return float(inumtimercallbacks)*0.001f;


	/*static bool binit = true;
	static LARGE_INTEGER TicksPerSecond;
	static DOUBLE fTicksPerMicrosecond;

	if( binit )
	{	QueryPerformanceFrequency( &TicksPerSecond );
		fTicksPerMicrosecond = (DOUBLE)TicksPerSecond.QuadPart * 0.000001;
		binit = false;
	}

	LARGE_INTEGER Current;
    QueryPerformanceCounter( &Current );
	static LARGE_INTEGER TimeBase = Current;
    __int64 reltime = Current.QuadPart-TimeBase.QuadPart;
	float ftimsecs = float((double(reltime)/fTicksPerMicrosecond)/1000000.0);
	//orkprintf( "time %f\n", ftimsecs );
	return ftimsecs;*/

#endif
}
///////////////////////////////////////////////////////////////////////////////
#elif defined( WII )
f32	CSystem::GetLoResTime( void )
{
	const double ktimerkonst = double(1000)/double(OS_TIMER_CLOCK);
	static OSTime base_time = OSGetTime();
	OSTime new_time = OSGetTime();
	OSTime reltime = (new_time-base_time);
	double dwiitime = double(reltime);
	double dwiitime_msec = dwiitime*ktimerkonst;
	return float(dwiitime_msec)*0.001f;
}
#endif
///////////////////////////////////////////////////////////////////////////////
f32	CSystem::GetLoResRelTime( void )
{
    static f32 fBaseTime = GetLoResTime();
    f32 fCurTime = GetLoResTime();
    f32 fRelTime = fCurTime-fBaseTime;
    return fRelTime;
}
///////////////////////////////////////////////////////////////////////////////
// higher res lower reliability timing
///////////////////////////////////////////////////////////////////////////////

f64	CSystem::GetHiResRelTime( void )
{
    static f64 fBaseTime = GetHiResTime();
    f64 fCurTime = GetHiResTime();
    f64 fRelTime = fCurTime-fBaseTime;

    return fRelTime;
}

f64 CSystem::GetHiResTime( void )
{
    ///////////////////////////////////////////////
    #if defined( _WIN32 ) && defined( _MSVC ) && ! defined(_XBOX)
    ///////////////////////////////////////////////
	static LARGE_INTEGER llfreq;

	DWORD_PTR oldmask=SetThreadAffinityMask(GetCurrentThread(), 1);

	bool bqpf = QueryPerformanceFrequency( & llfreq );
    double dfreq = li2d( llfreq );
    mfClockRate = dfreq;
    static LARGE_INTEGER base_llctr;
    LARGE_INTEGER llctr;
	static bool yo = QueryPerformanceCounter( & base_llctr );
    bool bqpc = QueryPerformanceCounter( & llctr );
	__int64 i64_base(base_llctr.QuadPart);
	__int64 i64val(llctr.QuadPart);
	__int64 irelctr = i64val - i64_base;
    //static double dctr_base = li2d( base_llctr );

	//double dctr = li2d( (llctr-base_llctr) ) - dctr_base;
    double dtim = double(irelctr) / dfreq;
    f64 fTime = (f64) dtim;

	//////////////////////////////////////
	// check for negative time delta's
	// this shouldnt happen anymore with
	// the SetThreadAffinityMask() fix
	// see http://channel9.msdn.com/ShowPost.aspx?PostID=156175

	static f64 LastTime = fTime;

	if( (fTime-LastTime) < 0.0f )
	{
		fTime = LastTime;
	}

	//////////////////////////////////////

	LastTime = fTime;
    mfWallClockTime = fTime;

	SetThreadAffinityMask(GetCurrentThread(), oldmask);

    ///////////////////////////////////////////////
    #elif defined( GCC ) && defined( _WIN32 )
    ///////////////////////////////////////////////
        time_t CourseTime;
        struct timeb FineTime;

        ///////////////////
        time( &CourseTime );
        ftime( &FineTime );
        ///////////////////
        F32 fMillis = ((F32)FineTime.millitm)*0.001f;
        static S32 iBase = CourseTime;
        static F32 fBaseMilli = fMillis;
        static F32 fBase = ((F32)iBase)+fMillis;
        F32 fTime = (((F32)CourseTime)+fMillis) - fBase;

        mfWallClockTime = fTime;
	///////////////////////////////////////////////
    #elif defined( NITRO )
	///////////////////////////////////////////////
        F64 fTime = (F64)OS_TicksToMicroSeconds( OS_GetTick() ) / 1000000.0;
        mfWallClockTime = fTime;
    ///////////////////////////////////////////////
    #else
	///////////////////////////////////////////////
        F32 fTime = 0.0f;
        mfWallClockTime = fTime;
    #endif

    return fTime;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CPerformanceItem::CPerformanceItem( std::string nam )
	: mName(nam)
	, miAvgCycle( 0 )
	, miStartCycle( 0 )
	, miEndCycle( 0 )
	, miChildAvgCycle( 0 )
	, miAccumCycle( 0 )
{


	for(int i = 0; i < TIMER_AVGAMT; i++)
		miAverageSumCycle[i] = 0;
}

///////////////////////////////////////////////////////////////////////////////

void CPerformanceItem::AddItem( CPerformanceItem& Item )
{
	OrkSTXMapInsert( mChildrenMap, Item.mName, & Item );
	mChildrenList.push_back( & Item );
}

///////////////////////////////////////////////////////////////////////////////

s64 CPerformanceItem::Calculate( void )
{
	miChildAvgCycle = 0;

	if(mChildrenList.size() > 0)
	{
		for( orklist< CPerformanceItem* >::iterator it=mChildrenList.begin(); it!=mChildrenList.end(); it++ )
			(*it)->Calculate();

		miChildAvgCycle = 0;

		for( orklist< CPerformanceItem* >::iterator it=mChildrenList.begin(); it!=mChildrenList.end(); it++ )
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

void CPerformanceItem::Enter()
{
	//miStartCycle = CSystem::GetClockCycle();
	f64 ftime = CSystem::GetRef().GetLoResTime();
	S64 output = S64(ftime*CSystem::GetRef().mfClockRate);
	miStartCycle = output;
}

///////////////////////////////////////////////////////////////////////////////

void CPerformanceItem::Exit()
{
	//miStartCycle = CSystem::GetClockCycle();
	f64 ftime = CSystem::GetRef().GetLoResTime();
	S64 output = S64(ftime*CSystem::GetRef().mfClockRate);
	miEndCycle = output;
	miAccumCycle += (miEndCycle-miStartCycle);
}

///////////////////////////////////////////////////////////////////////////////

CPerformanceTracker::CPerformanceTracker()
	: NoRttiSingleton<CPerformanceTracker>()
{
	mRoots[EPS_UPDTHREAD] = new CPerformanceItem( "update_root" );
	mRoots[EPS_GFXTHREAD] = new CPerformanceItem( "render_root" );
}

///////////////////////////////////////////////////////////////////////////////

s64 CPerformanceTracker::Calculate( void )
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

void CPerformanceTracker::AddItem( eperfset eset, CPerformanceItem& Item )
{
	if( Item.GetName() != (std::string) "root" )
		GetRef().mRoots[eset]->AddItem( Item );
}

///////////////////////////////////////////////////////////////////////////////

orklist<CPerformanceItem*>* CPerformanceTracker::GetItemList( eperfset eset )
{
	return GetRef().mRoots[eset]->GetChildrenList();
}

///////////////////////////////////////////////////////////////////////////////

void CPerformanceTracker::TextDump( void )
{
	CPerformanceTracker::Calculate();

	/*orklist<CPerformanceItem*>* PerfItemList = CPerformanceTracker::GetItemList();
	s64 PerfTotal = CPerformanceTracker::GetRef().mpRoot->miAvgCycle;

	for( orklist<CPerformanceItem*>::iterator it=PerfItemList->begin(); it!=PerfItemList->end(); it++ )
	{
		CPerformanceItem* pItem = *it;
		std::string name = pItem->GetName();

		orkprintf( "%s\n", (char*)CreateFormattedString( "%s [%d microsec]", (char*)name.c_str(), pItem->miAvgCycle).c_str() );
	}*/
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork
