#pragma once

#include "mutex.h"
#include <pthread.h>

namespace ork {

struct condition_variable
{
    condition_variable(bool do_init = false);

    void init();
    void notify_one();
    void notify_all();
    int wait(unsigned long usec);

private:

    pthread_mutex_t mMutex;
    pthread_cond_t  mCondVar;
    std::atomic<int64_t> mWaitCount;
    std::atomic<int64_t> mReleaseCount;
};

struct semaphore
{
    semaphore(const char* name);
    void notify();
    void wait();

private:
    ork::mutex mMutex;
    ork::condition_variable mCondition;
    std::atomic<int> mCount;
};


} // namespace ork