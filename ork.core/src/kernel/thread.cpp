#include <ork/kernel/thread.h>

#if defined(ORK_LINUX)
#include <sys/prctl.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace ork
{

void* Thread::VirtualThreadImpl(void*data)
{
	auto pthr = (Thread*) data;

    if( pthr->mThreadName.length() )
    	SetCurrentThreadName(pthr->mThreadName.c_str());

    pthr->mRunning = true;
	pthr->run();
    pthr->mRunning = false;
	return 0;
}

void* Thread::LambdaThreadImpl(void* pdat)
{

    auto pthr = (Thread*) pdat;
    
    if( pthr->mThreadName.length() )
    	SetCurrentThreadName(pthr->mThreadName.c_str());


    pthr->mRunning = true;
    pthr->mLambda();
    pthr->mRunning = false;
    return nullptr;
}

void Thread::RunSynchronous()
{
	start();
	join();
}
Thread::Thread(const std::string& thread_name)
	: mUserData()
	, mThreadH(0)
	, mRunning(false)
	, mThreadName( thread_name )
{
	mState = 0;
}
void Thread::start()
{
	if( mState.fetch_add(1)==0 )
	{
		mThreadH = new std::thread(&VirtualThreadImpl,(void*)this);
	}
}

void Thread::start( const ork::void_lambda_t& l )
{
	if( mState.fetch_add(1)==0 )
	{
		mLambda = l;
		mThreadH = new std::thread(&LambdaThreadImpl,(void*)this);
	}
}

bool Thread::join()
{
	mThreadH->join();
	return true;
}



///////////////////////////////////////////////////
#if defined(_WIN32)
///////////////////////////////////////////////////

const DWORD MS_VC_EXCEPTION=0x406D1388;
#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
   DWORD dwType; // Must be 0x1000.
   LPCSTR szName; // Pointer to name (in user addr space).
   DWORD dwThreadID; // Thread ID (-1=caller thread).
   DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

void SetCurrentThreadName(const char* threadName)
{
	DWORD dwThreadID = (DWORD) GetCurrentThreadId();
	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = threadName;
	info.dwThreadID = dwThreadID;
	info.dwFlags = 0;

	__try
	{
		RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
	}
	
	__except(EXCEPTION_EXECUTE_HANDLER)
	{

	}
}

///////////////////////////////////////////////////
#else // POSIX
///////////////////////////////////////////////////

void SetCurrentThreadName(const char* threadName)
{
	static const int  kMAX_NAME_LEN = 15;
	char name[kMAX_NAME_LEN+1];
	for( int i=0; i<kMAX_NAME_LEN; i++ ) name[i]=0;
	strncpy(name,threadName,kMAX_NAME_LEN);
	name[kMAX_NAME_LEN]=0;
#if defined(ORK_LINUX)
	prctl(PR_SET_NAME,(unsigned long)&name);
#elif defined(ORK_OSX)
	pthread_setname_np(threadName); 
#endif
}

///////////////////////////////////////////////////
#endif
///////////////////////////////////////////////////

} // namespace ork
