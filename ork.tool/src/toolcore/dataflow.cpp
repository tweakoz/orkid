////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <orktool/toolcore/dataflow.h>
#include <ork/kernel/thread.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool {
///////////////////////////////////////////////////////////////////////////////

bool gbdflow_exit = false;

class SchedulerThread : public ork::Thread
{
	void run() // virtual
	{
		dataflow::scheduler* psched = mUserData.Get<dataflow::scheduler*>();

		///////////////////////////////////////////////////////////////
		// Process all the work at a random rate slower than it was queued
		///////////////////////////////////////////////////////////////

		while( false == gbdflow_exit )
		{
			//psched->Process(); // BLOCKING
			ork::msleep(1);
		}
		
		gbdflow_exit = false;

		///////////////////////////////////////////////////////////////
		// send terminate signal to attached processor threads
		///////////////////////////////////////////////////////////////
		psched->terminate();
		///////////////////////////////////////////////////////////////
		// goodbye
		///////////////////////////////////////////////////////////////
		//return 0;
	}
};

///////////////////////////////////////////////////////////////////////////////

class ProcessThread : public ork::Thread
{
	void run() // virtual
	{
		dataflow::processor* proc = mUserData.Get<dataflow::processor*>();
		proc->ProcessorThreadMain();
		//return 0;
	}
};


///////////////////////////////////////////////////////////////////////////

dataflow::scheduler* GetGlobalDataFlowScheduler()
{	
	static dataflow::scheduler* gDataFlowScheduler = 0;

	if( 0 == gDataFlowScheduler )
	{
		/////////////////////////////
		/////////////////////////////
		const bool ENABLE_GPU_PROCESSING = false;
		/////////////////////////////
		/////////////////////////////
		gDataFlowScheduler = new dataflow::scheduler();
		dataflow::processor* proc0 = new dataflow::processor("proc_cpu0");
		//dataflow::processor* proc1 = new dataflow::processor("proc_cpu1");
		proc0->SetAffinity(1<<0);
		//proc1->SetAffinity(1<<1);
		gDataFlowScheduler->AddProcessor( proc0 );
		//gDataFlowScheduler->AddProcessor( proc1 );
		/////////////////////////////
		SchedulerThread* pschedthread = new SchedulerThread;
		pschedthread->UserData().Set<dataflow::scheduler*>(gDataFlowScheduler);

		ProcessThread* pprocthread1 = new ProcessThread;
		//ProcessThread* pprocthread2 = new ProcessThread;
		/////////////////////////////
		pprocthread1->UserData().Set<dataflow::processor*>(proc0);
	//	pprocthread2->UserData().Set<dataflow::processor*>(proc1);
		/////////////////////////////
		pschedthread->start();
		pprocthread1->start();
	//	pprocthread2->start();
		/////////////////////////////
		if( ENABLE_GPU_PROCESSING )
		{
			dataflow::processor* proc_gpu = new dataflow::processor("proc_gpu");
			proc_gpu->SetAffinity(dataflow::scheduler::GpuAffinity);
			gDataFlowScheduler->AddProcessor( proc_gpu );
			ProcessThread* pprocthreadGPU = new ProcessThread;
			pprocthreadGPU->UserData().Set<dataflow::processor*>(proc_gpu);
			pprocthreadGPU->start();
		}
		/////////////////////////////
		/////////////////////////////
	}
	
	return gDataFlowScheduler;
}

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
