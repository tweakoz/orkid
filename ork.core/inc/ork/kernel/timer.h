////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/kernel/kernel.h>
#include <tbb/tbb.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {

float get_sync_time();

///////////////////////////////////////////////////////////////////////////////

struct Timer
{
    tbb::tick_count mStartTime;
    tbb::tick_count mEndTime;

    void Start();
    void End();
    float InternalSecsSinceStart() const;
    float SecsSinceStart() const;
    float SpanInSecs() const;
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

///////////////////////////////////////////////////////////////////////////////
namespace lev2 { class GfxTarget; }
///////////////////////////////////////////////////////////////////////////////


class CPerformanceItem
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

	orkmap<std::string, CPerformanceItem*> mChildrenMap;
	orklist<CPerformanceItem*> mChildrenList;

	////////////////////////////////////////////

	CPerformanceItem( std::string nam );

	////////////////////////////////////////////

	void AddItem( CPerformanceItem& Item );
	s64 Calculate();

	void Enter();
	void Exit();

	////////////////////////////////////////////

	orklist<CPerformanceItem*>* GetChildrenList() { return & mChildrenList;	}
	const std::string GetName() const { return mName; }
};

///////////////////////////////////////////////////////////////////////////////

class CPerformanceTracker : public NoRttiSingleton<CPerformanceTracker>
{
public:

	enum eperfset 
	{
		EPS_UPDTHREAD = 0,
		EPS_GFXTHREAD,
		EPS_END,
	};
	CPerformanceItem* mRoots[EPS_END];

	////////////////////////////////////////////

	CPerformanceTracker();
	//static void ClassInit() { GetClassStatic(); }

	////////////////////////////////////////////

	static s64 Calculate();
	static void AddItem( eperfset eset, CPerformanceItem& Item );
	static orklist<CPerformanceItem*>* GetItemList(eperfset eset);

	static void Draw(ork::lev2::GfxTarget *pTARG);
	static void TextDump();

};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
