///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/any.h>
#include <ork/kernel/atomic.h>
#include <ork/orktypes.h>
#include <string>

#define USE_STD_THREAD
#include <thread>

namespace ork
{
	void SetCurrentThreadName(const char* threadName);

	struct Thread
	{		
		Thread(const std::string& thread_name = "");
		virtual ~Thread() {}

		virtual void run() {}

		void RunSynchronous();
		void start();
		void start( const ork::void_lambda_t& l );
		bool join();

		anyp& UserData() { return mUserData; }
				
	protected:
		
		std::thread*       mThreadH;
		std::string		   mThreadName;
		anyp		       mUserData;
		ork::atomic<int>   mState;
		ork::void_lambda_t mLambda;
		bool 			   mRunning;
		static void* VirtualThreadImpl(void*data);
   		static void* LambdaThreadImpl(void* pdat);
		
	};
	



} // namespace ork
