///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/any.h>
#if defined(IX)
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
			, mThreadH()
		{
			
		}

		inline void start()
		{
			if (pthread_create(&mThreadH, NULL, & RunThread, (void*) this ) != 0)
			{
				OrkAssert(false);
			}
			
		}

		inline bool join()
		{
			void* pret = 0;
			int iret = pthread_join(mThreadH, & pret);
			return ( pret == ((void*) 0) );			
		}
		virtual void run() = 0;

		anyp& UserData() { return mUserData; }
		
	protected:
		
		pthread_t	mThreadH;
		anyp		mUserData;
		
		
	};
	
	}
