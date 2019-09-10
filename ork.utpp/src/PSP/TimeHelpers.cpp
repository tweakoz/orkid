#include "TimeHelpers.h"

#include <kernel.h>
#include <thread.h>
#include <cassert>

namespace UnitTest {

Timer::Timer()
	: m_startTime(0L)
{
	SceKernelSystemStatus status;
	int result = sceKernelReferSystemStatus(&status);
	assert(result == SCE_KERNEL_ERROR_OK);
}

void Timer::Start()
{
	m_startTime = sceKernelGetSystemTimeWide();
}

int Timer::GetTimeInMs() const
{    
	return int(sceKernelGetSystemTimeWide() - m_startTime) / 1000;
}


void TimeHelpers::SleepMs(int const ms)
{
    int result = sceKernelDelayThread(SceUInt(ms * 1000));
	assert(result == SCE_KERNEL_ERROR_OK);
}

}
