#pragma once

#include "mutex.h"

namespace ork {

class semaphore
{
private:
    ork::mutex mMutex;
    std::condition_variable mCondition;
    int mCount;

public:
    inline semaphore(const char* name)
        : mMutex(name)
		, mCount(0)
    {}

    inline void notify()
    {
        ork::mutex::unique_lock lock(mMutex);
		++mCount;
        mCondition.notify_one();
    }

    inline void wait()
    {
       ork::mutex::unique_lock lock(mMutex);
        while(!mCount)
            mCondition.wait(lock.mLockImpl);
        --mCount;
    }
};

} // namespace ork