#include <nitro.h>

#include "../UnitTest++.h"
#include "../TestReporterStdout.h"
#include "../Config.h"

#ifdef UNITTEST_PSP
#include <moduleexport.h>
SCE_MODULE_INFO(TestUnitTestPP, 0, 1, 1);
#endif

#ifdef UNITTEST_NDS
extern "C" void NitroStartUp();
extern "C" void NitroStartUp()
{

#define MAIN_HEAP_SIZE 0x8000
	void *nstart;
	void *heapstart;
	OSHeapHandle handle;
	
	OS_Init();
	OS_InitThread();
	OS_InitTick();
	OS_InitAlarm();
	OS_InitArena();
	
	nstart = OS_InitAlloc(OS_ARENA_MAIN, OS_GetMainArenaLo(), OS_GetMainArenaHi(), 2);
	OS_SetMainArenaLo(nstart);
	
	heapstart = OS_AllocFromMainArenaLo(MAIN_HEAP_SIZE, 32);
	handle = OS_CreateHeap(OS_ARENA_MAIN, heapstart, (void *) ((u32) heapstart + MAIN_HEAP_SIZE));
	
	OS_SetCurrentHeap(OS_ARENA_MAIN, handle);
}

void NitroMain();
void NitroMain()
{
	UnitTest::RunAllTests();

	OS_Terminate();
}
#else
int main(int, char const *[])
{
    return UnitTest::RunAllTests();
}
#endif
