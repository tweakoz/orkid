////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/kernel/kernel.h>
#include <ork/kernel/thread.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {

float get_sync_time();

///////////////////////////////////////////////////////////////////////////////

struct Timer
{
	Timer();
	~Timer();

    void Start();
    void End();
    float InternalSecsSinceStart() const;
    float SecsSinceStart() const;
    float SpanInSecs() const;
	void OnInterval( float interval, const void_lambda_t& oper );

private:

    float mStartTime;
    float mEndTime;
  	float mLambdaInterval;
    void_lambda_t mOnInterval;
    ork::Thread* mThread;
    bool mKill;
};

///////////////////////////////////////////////////////////////////////////////

struct PerfItem2
{
	const char* mpMarkerName;
	float		mfMarkerTime;
};

void PerfMarkerPush( const char* str );
bool PerfMarkerPop( PerfItem2& outmkr );
void PerfMarkerEnable();
void PerfMarkerDisable();
void PerfMarkerPushState();
void PerfMarkerPopState();

///////////////////////////////////////////////////////////////////////////////
namespace lev2 { class Context; }
///////////////////////////////////////////////////////////////////////////////


class PerformanceItem
{
public:
	static const int TIMER_AVGAMT = 10;

	std::string	mName;
	s64			miStartCycle;
	s64			miEndCycle;
	s64			miAccumCycle;
	s64			miAvgCycle;
	s64			miChildAvgCycle;
	s64			miAverageSumCycle[ TIMER_AVGAMT ];

	orkmap<std::string, PerformanceItem*> mChildrenMap;
	orklist<PerformanceItem*> mChildrenList;

	////////////////////////////////////////////

	PerformanceItem( std::string nam );

	////////////////////////////////////////////

	void AddItem( PerformanceItem& Item );
	s64 Calculate();

	void Enter();
	void Exit();

	////////////////////////////////////////////

	orklist<PerformanceItem*>* GetChildrenList() { return & mChildrenList;	}
	const std::string GetName() const { return mName; }
};

///////////////////////////////////////////////////////////////////////////////

class PerformanceTracker : public NoRttiSingleton<PerformanceTracker>
{
public:

	enum eperfset 
	{
		EPS_UPDTHREAD = 0,
		EPS_GFXTHREAD,
		EPS_END,
	};
	PerformanceItem* mRoots[EPS_END];

	////////////////////////////////////////////

	PerformanceTracker();
	//static void ClassInit() { GetClassStatic(); }

	////////////////////////////////////////////

	static s64 Calculate();
	static void AddItem( eperfset eset, PerformanceItem& Item );
	static orklist<PerformanceItem*>* GetItemList(eperfset eset);

	static void Draw(ork::lev2::Context *pTARG);
	static void TextDump();

};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
