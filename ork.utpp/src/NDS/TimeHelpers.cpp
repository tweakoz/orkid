#include <nitro.h>
#include "TimeHelpers.h"

namespace UnitTest {

Timer::Timer() : m_startTime(0L)
{
	OS_InitTick();
}

void Timer::Start()
{
    m_startTime = OS_GetTick();
}

int Timer::GetTimeInMs() const
{    
    return int(OS_TicksToMilliSeconds32( OS_GetTick() - m_startTime ));
}


void TimeHelpers::SleepMs(int const ms)
{
	OS_SpinWait(u32(ms * 100000)); // in cycles... =)
    //OS_Sleep( u32(ms) );
}

}
