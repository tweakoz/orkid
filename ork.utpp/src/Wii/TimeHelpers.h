#ifndef UNITTEST_TIMEHELPERS_H
#define UNITTEST_TIMEHELPERS_H

//class SceProfilerRegs;

namespace UnitTest {

class Timer
{
public:
    Timer();
    void Start();
    int GetTimeInMs() const;    

private:
    //SceProfilerRegs* m_profiler;
	unsigned long long int m_startTime;

};


namespace TimeHelpers
{
void SleepMs (int ms);
}


}



#endif
