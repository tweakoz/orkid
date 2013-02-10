///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/thread_pool.h>
//#include <tbb/tbb_thread.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace threadpool {
///////////////////////////////////////////////////////////////////////////////

task::task()
{
	mNumSubTasks.Store(kTaskIdle);
}
///////////////////////////////////////////////////////////////////////////////
task::~task()
{
	int ir = mNumSubTasks.Fetch();
	OrkAssert( 	ir == kTaskIdle );
}
///////////////////////////////////////////////////////////////////////////////
void task::divide(thread_pool* tpool)
{
	int ichk = mNumSubTasks.FetchAndStore(0);
	OrkAssert( ichk == kTaskIdle );
	do_divide(tpool);
}
///////////////////////////////////////////////////////////////////////////////
void task::started()
{
	do_onstarted();
}
///////////////////////////////////////////////////////////////////////////////
void task::finished()
{
	do_onfinished();
	int ichk = mNumSubTasks.FetchAndStore(kTaskFinished);
	OrkAssert( ichk == 0 );
}
///////////////////////////////////////////////////////////////////////////////
void task::process( const sub_task* tsk, const thread_pool_worker* ptpw  )
{	//////////////////////////////////
	do_process(tsk,ptpw);
	//////////////////////////////////
	subtask_finished( tsk );
	//////////////////////////////////
	int icount = mNumSubTasks.FetchAndDecrement();
	OrkAssert( icount>0 );
	//////////////////////////////////
	if( 1 == icount )
	{
		finished();
	}
	//////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void task::subtask_finished( const sub_task* pst )
{
	do_subtask_finished( pst );
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void task::wait()
{
	while(false == HasFinished() )
	{
		//tbb::this_tbb_thread::yield();
		ork::msleep(0);
	}
	int ichk = mNumSubTasks.FetchAndStore(kTaskIdle);
	OrkAssert( ichk == kTaskFinished );
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
thread::thread()
	: mpThread(0)
	, mpWorker(0)
{
}
thread::~thread()
{
	if( mpWorker )
	{
		mpWorker->Kill();
	}
	if( mpThread )
	{
		mpThread->join();
		delete mpThread;
	}
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
thread_pool::thread_pool()
	: mTasks()
	, mSubTasks()
{
}
///////////////////////////////////////////////////////////////////////////////
thread_pool::~thread_pool()
{
	int inumthreads = mThreads.size();

	for( int it=0; it<inumthreads; it++ )
	{
		thread* pthread = mThreads[it];
		delete pthread;
	}
}
///////////////////////////////////////////////////////////////////////////////
void thread_pool::init( int inumthreads )
{
	for( int i=0; i<inumthreads; i++ )
	{
		struct thread_exec
		{
			thread_exec(thread_pool_worker*pworker) : mpWorker(pworker) {}
			void operator()()
			{
				if( mpWorker )
				{
					mpWorker->Process();
				}
			}

			thread_pool_worker* mpWorker;
		};

		thread* pthread = new thread;
		pthread->mpWorker = new thread_pool_worker( this );
		thread_exec exec(pthread->mpWorker);
		pthread->mpThread = new boost::thread(exec);
	}
}
///////////////////////////////////////////////////////////////////////////////
void thread_pool::AddTask( task* ptask )
{
//	ptask->OnQueued();
	mTasks.push(ptask);
}
///////////////////////////////////////////////////////////////////////////////
void thread_pool::AddSubTasks( const orkset<const sub_task*>& subtasks )
{
	for( orkset<const sub_task*>::const_iterator it=subtasks.begin(); it!=subtasks.end(); it++ )
	{
		const sub_task* st = *it;
		mSubTasks.push(st);
	}
}
///////////////////////////////////////////////////////////////////////////////
void thread_pool::AddSubTask( const sub_task* subtask )
{
	mSubTasks.push(subtask);
}
void thread_pool::Lock()
{
	mLock.LockForRead();
}
void thread_pool::UnLock()
{
	mLock.UnLock();
}
///////////////////////////////////////////////////////////////////////////////
const sub_task* thread_pool::GetSubTask()
{
	const sub_task* rval(0);
	while( 0 == rval )
	{
		//////////////////////////////////////////////////////
		bool bpoppedsubtask = false;
		Lock();
		{
			bpoppedsubtask = mSubTasks.try_pop(rval);
		}
		UnLock();
		//////////////////////////////////////////////////////
		//else // subtask queue was empty, try to fill it
		//////////////////////////////////////////////////////
		task* ptask = 0;
		bool bpoppedtask = false;
		Lock();
		{
			bpoppedtask = mTasks.try_pop( ptask );
		}
		UnLock();
		//////////////////////////////////////////////////////
		// wait until a task is available
		//////////////////////////////////////////////////////
		//////////////////////////////////////////////////////
		// guaranteed a task
		//////////////////////////////////////////////////////
		if( bpoppedtask )
		{
			int ichk = ptask->RefNumSubTasks().Fetch();
			OrkAssert( ichk==task::kTaskIdle );

			if( ichk==task::kTaskIdle )
			{
				Lock();
				{
					ptask->divide(this);
					ptask->started();
				}
				UnLock();
			}
		}
		if( false == bpoppedsubtask )
		{
			ork::msleep(0);
		}
	}
	return rval;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static MyAtomicNum<int> gtpw;
thread_pool_worker::thread_pool_worker( thread_pool* ppool )
	: mpThreadPool(ppool)
	, mbExitSignal(false)
	, mbExited(false)
	, mTPWIndex( gtpw.FetchAndIncrement() )
{
}
///////////////////////////////////////////////////////////////////////////////
void thread_pool_worker::Process()
{
	while( false == mbExitSignal )
	{
		if( mpThreadPool )
		{
			const sub_task* subtask = mpThreadPool->GetSubTask();
			task* ptask = subtask->GetTask();
			OrkAssert( subtask != 0 );
			if( ptask )
			{
				ptask->process( subtask, this );
			}
		}
		ork::msleep(0);//tbb::this_tbb_thread::yield();
	}
	mbExited=true;
}
///////////////////////////////////////////////////////////////////////////////
void thread_pool_worker::Kill()
{
	while( false == mbExited )
	{
		ork::msleep(0);
		//tbb::this_tbb_thread::yield();
	}
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::threadpool
///////////////////////////////////////////////////////////////////////////////
