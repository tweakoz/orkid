#include <ork/kernel/semaphore.h>

#if defined(ORK_OSX)
#include <sys/time.h>
#endif

namespace ork {

semaphore::semaphore(const char* name)
    : mMutex(name)
	, mCount(0)
{}

void semaphore::notify()
{
    ork::mutex::unique_lock lock(mMutex);
	++mCount;
    mCondition.notify_one();
}

void semaphore::wait()
{
   ork::mutex::unique_lock lock(mMutex);
    while(!mCount)
        mCondition.wait(lock.mLockImpl);
    --mCount;
}

condition_variable::condition_variable(bool do_init)
{
    if(do_init) 
        init();
}

void condition_variable::init()
{	mWaitCount = mReleaseCount = 0;
	pthread_mutex_init(&mMutex, NULL);
	//pthread_mutex_lock(&mMutex);
	pthread_cond_init(&mCondVar, NULL);
}//

void condition_variable::notify_one()
{
	//pthread_mutex_lock(&mMutex); // Wow these are horrible... going commando
	if(mWaitCount)
		pthread_cond_signal(&mCondVar);
	else
		mReleaseCount++;
	//pthread_mutex_unlock(&mMutex);
};

void condition_variable::notify_all()
{
	while(mWaitCount)
		notify_one();
};

int condition_variable::wait(unsigned long usec)
{
	const int oneK = 1000;
	const int oneM = 1000000;
	const int oneG = 1000000000;

	struct timespec now;

#if defined(ORK_OSX)
	struct timeval tv_now;
	gettimeofday(&tv_now,nullptr);
	now.tv_sec = tv_now.tv_sec;
	now.tv_nsec = tv_now.tv_usec*oneK;
#else

	clock_gettime(CLOCK_REALTIME, &now);
#endif

	now.tv_sec += usec / oneM;
	now.tv_nsec += (usec % oneM) * oneK;
	if(now.tv_nsec > oneG)
	{	now.tv_nsec -= oneG;
		now.tv_sec += 1;
	}

	do
	{
		int err = 0;
		//pthread_mutex_lock(&mMutex);
		if(mReleaseCount > 0)
		{
			mReleaseCount--;
		}
		else
		{
			mWaitCount++;
			err = pthread_cond_timedwait(&mCondVar, &mMutex, &now);
			mWaitCount--;
		}
		//pthread_mutex_unlock(&mMutex);
		switch(err)
		{	case EINTR: break;
			case ETIMEDOUT: return 0;
			case 0: return 1;
			case -1:
			default: return -1; // Should handle the error, but will leave this for later
		}

	} while(1);
}


}
