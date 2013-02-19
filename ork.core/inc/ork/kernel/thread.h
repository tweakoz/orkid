///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/any.h>

#if defined(ORK_VS2012) // builtin mutex
#define USE_STD_THREAD
#include <thread>
#elif defined(IX)
#define USE_PTHREAD
#include <pthread.h>
#else
#include <pthread/pthread.h>
#endif

namespace ork
{
	class Thread
	{
		static void* RunThread(void*data)
		{
			Thread* pthread = (Thread*) data;
			pthread->run();
			return 0;
		}
		
	public:
		
		void RunSynchronous()
		{
			start();
			join();
		}
		inline Thread()
			: mUserData()
			, mThreadH(0)
		{
			
		}

		inline void start()
		{
#if defined(USE_STD_THREAD)
			mThreadH = new std::thread(&RunThread,(void*)this);
#elif defined(USE_PTHREAD)
			if (pthread_create(&mThreadH, NULL, & RunThread, (void*) this ) != 0)
			{
				OrkAssert(false);
			}
#endif	
		}

		inline bool join()
		{
#if defined(USE_STD_THREAD)
			mThreadH->join();
#elif defined(USE_PTHREAD)
			void* pret = 0;
			int iret = pthread_join(mThreadH, & pret);
			return ( pret == ((void*) 0) );			
#endif
		}
		virtual void run() = 0;

		anyp& UserData() { return mUserData; }
		
	protected:
		
#if defined(USE_STD_THREAD)
		std::thread* mThreadH;
#elif defined(USE_PTHREAD)
		pthread_t	mThreadH;
#endif
		anyp		mUserData;
		
		
	};
	
	}
