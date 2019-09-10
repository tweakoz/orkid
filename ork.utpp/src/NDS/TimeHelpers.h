#ifndef UNITTEST_TIMEHELPERS_H
#define UNITTEST_TIMEHELPERS_H

namespace UnitTest {

class Timer
{
public:
    Timer();
    void Start();
    int GetTimeInMs() const;    

private:
	unsigned long long int m_startTime;

};


namespace TimeHelpers
{
void SleepMs (int ms);
}


}



#endif
