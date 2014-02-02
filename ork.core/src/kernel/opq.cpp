///////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/thread.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/string/string.h>
#include <ork/util/Context.hpp>
#include <ork/kernel/debug.h>
#include <ork/kernel/future.hpp>
#if defined(ORK_LINUX)
#include <sys/prctl.h>
#endif

//#define DEBUG_OPQ_CALLSTACK
///////////////////////////////////////////////////////////////////////
template class ork::util::ContextTLS<ork::OpqTest>;

#if defined(_WIN32)
#include <windows.h>
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

void SetThreadName( DWORD dwThreadID, const char* threadName)
{
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
namespace ork {
void SetCurrentThreadName(const char* threadName)
{
	DWORD dwThreadID = (DWORD) GetCurrentThreadId();
	SetThreadName( dwThreadID, threadName );
}
}
#else
namespace ork {
void SetCurrentThreadName(const char* threadName)
{
	static const int  kMAX_NAME_LEN = 15;
	char name[kMAX_NAME_LEN+1];
	for( int i=0; i<kMAX_NAME_LEN; i++ ) name[i]=0;
	strncpy(name,threadName,kMAX_NAME_LEN);
	name[kMAX_NAME_LEN]=0;
#if defined(ORK_LINUX)
	prctl(PR_SET_NAME,(unsigned long)&name);
#endif
}
}
#endif
///////////////////////////////////////////////////////////////////////
namespace ork {
////////////////////////////////////////////////////////////////////////////////
Op::Op(const void_lambda_t& op,const std::string& name)
	: mName(name)
{
	SetOp(op);
}
////////////////////////////////////////////////////////////////////////////////
Op::Op(const BarrierSyncReq& op,const std::string& name)
	: mName(name)
{
	SetOp(op);
}
////////////////////////////////////////////////////////////////////////////////
Op::Op(const Op& oth)
	: mName(oth.mName)
	, mWrapped(oth.mWrapped)
{
}
////////////////////////////////////////////////////////////////////////////////
Op::Op()
{
}
////////////////////////////////////////////////////////////////////////////////
Op::~Op()
{
}
////////////////////////////////////////////////////////////////////////////////
void Op::SetOp(const op_wrap_t& op)
{
	if( op.IsA<void_lambda_t>() )
	{
		mWrapped = op;
	}
	else if( op.IsA<BarrierSyncReq>() )
	{
		mWrapped = op;
	}
	else // unhandled op type
	{
		assert(false);
	}
}
///////////////////////////////////////////////////////////////////////////
void Op::QueueASync(Opq&q) const
{
	q.push(*this);
}
void Op::QueueSync(Opq&q) const
{
	AssertNotOnOpQ(q);
	q.push(*this);
	Future the_fut;
	BarrierSyncReq R(the_fut);
	q.push(R);
	the_fut.GetResult();
}
///////////////////////////////////////////////////////////////////////////
struct OpqThreadData
{
	Opq* mpOpQ;
	int miThreadID;
	OpqThreadData() : mpOpQ(nullptr), miThreadID(0) {}
};
///////////////////////////////////////////////////////////////////////////
struct OpqDrained : public IOpqSynchrComparison
{
	bool IsConditionMet(const OpqSynchro& synchro) const
	{
		return (int(synchro.mOpCounter)==0);
	}
};
///////////////////////////////////////////////////////////////////////////
struct OpqThreadImpl : public ork::Thread {
	OpqThreadData mData;
	OpqThreadImpl(Opq*popq,int thid)
	{
		mData.mpOpQ = popq;
		mData.miThreadID = thid;
	}
void run() // virtual
{
	OpqThreadData* opqthreaddata = & mData;
	Opq* popq = opqthreaddata->mpOpQ;
	std::string opqn = popq->mName;
	SetCurrentThreadName( opqn.c_str() );

	popq->mThreadsRunning++;

	static int icounter = 0;
	int thid = opqthreaddata->miThreadID+4;
	std::string channam = CreateFormattedString("opqth%d",int(thid));
	//kernel::ProfileGroup* prof_grp = nullptr;
	//if( kernel::gprof )
	//{
	//	prof_grp = kernel::gprof->GetGroup(channam.c_str());
	//	prof_grp->miChannelIndex = thid;
	//}

	OpqTest opqtest(popq);

	while(false==popq->mbGoingDown)
	{
		popq->mSemaphore.wait(); // wait for an op (without spinning)

		if( popq->mbGoingDown ) continue; // exit clause

		bool item_processed = popq->Process();
		//assert(item_processed);

	}

	popq->mThreadsRunning--;

	//printf( "popq<%p> thread exiting...\n", popq );

}
};
///////////////////////////////////////////////////////////////////////////
bool Opq::Process()
{
	bool rval = false;


	Op the_op;

	OpGroup* pexecgrp = nullptr;

	int num_groups = mGroupCounter;
	OpGroup* ptstgrp = nullptr;
	for( auto& grp : mOpGroups )
	{
		int ioif = grp->mOpsInFlightCounter;
		int imax = grp->mLimitMaxOpsInFlight;
		int inumops = grp->mSynchro.NumOps();

		if( (inumops>0) && ((imax==0) || (ioif < imax)) )
		{
			if( grp->try_pop(the_op) )
			{
				pexecgrp = grp;
				break;
			}
		}
	}

	if( pexecgrp )
	{
		pexecgrp->mOpsInFlightCounter++;

		//printf( "  runop OIF<%d>\n", int(pexecgrp->mOpsInFlightCounter) );
		const char* ppnam = "opx";
		
		if( the_op.mName.length() )
		{
			ppnam = the_op.mName.c_str();
		}

		if( the_op.mWrapped.IsA<void_lambda_t>() )
		{
			the_op.mWrapped.Get<void_lambda_t>()();
		}
		else if( the_op.mWrapped.IsA<BarrierSyncReq>() )
		{	
			auto& R = the_op.mWrapped.Get<BarrierSyncReq>();
			R.mFuture.Signal<bool>(true);
		}
		else
		{
			printf( "unknown opq invokable type\n" );
		}

		this->mSynchro.RemItem();
		pexecgrp->mSynchro.RemItem();

		pexecgrp->mOpsInFlightCounter--;
		rval=true;
	}
	return rval;
}
///////////////////////////////////////////////////////////////////////////
void Opq::push(const Op& the_op)
{
	mDefaultGroup->push(the_op);
}
void Opq::push(const void_lambda_t& l,const std::string& name)
{
	mDefaultGroup->push(Op(l,name));
}
void Opq::push(const BarrierSyncReq& s)
{
	mDefaultGroup->push(Op(s));	
}

///////////////////////////////////////////////////////////////////////////
void Opq::push_sync(const Op& the_op)
{
	AssertNotOnOpQ(*this);
	push(the_op);
	Future the_fut;
	BarrierSyncReq R(the_fut);
	push(R);
	the_fut.GetResult();
}
///////////////////////////////////////////////////////////////////////////
void Opq::sync()
{
	AssertNotOnOpQ(*this);
	Future the_fut;
	BarrierSyncReq R(the_fut);
	push(R);
	the_fut.GetResult();
}
///////////////////////////////////////////////////////////////////////////
void Opq::drain()
{
	OpqDrained pred_is_drained;
	mSynchro.WaitOnCondition(pred_is_drained);
}
///////////////////////////////////////////////////////////////////////////
OpGroup* Opq::CreateOpGroup(const char* pname)
{
	OpGroup* pgrp = new OpGroup(this,pname);
	mOpGroups.insert(pgrp);
	mGroupCounter++;
	return pgrp;
}
///////////////////////////////////////////////////////////////////////////
Opq::Opq(int inumthreads, const char* name)
	: mbGoingDown(false)
	, mName(name)
	, mSemaphore(name)
{
	mGroupCounter = 0;
	mThreadsRunning = 0;

	mDefaultGroup = CreateOpGroup("defconq");

	for( int i=0; i<inumthreads; i++ )
	{
	    ork::Thread* thread_handle = new OpqThreadImpl(this,i);
	    thread_handle->start();
	}
}
///////////////////////////////////////////////////////////////////////////
Opq::~Opq()
{
	//drain();
	//sync();

	/////////////////////////////////
	// signal to thread we are going down, then wait for it to go down
	/////////////////////////////////

	mbGoingDown = true;

	while(int(mThreadsRunning)!=0)
	{
		mSemaphore.notify();
		usleep(10);
	}

	/////////////////////////////////
	// trash the groups
	/////////////////////////////////

	for( auto& it : mOpGroups )
	{
		delete it;
	}
	mOpGroups.clear();
	/////////////////////////////////

}
///////////////////////////////////////////////////////////////////////////
OpGroup::OpGroup(Opq*popq, const char* pname)
	: mpOpQ(popq)
	, mLimitMaxOpsInFlight(0)
	, mLimitMaxOpsQueued(0)
	, mGroupName(pname)
{
	mOpsInFlightCounter = 0;
	mOpSerialIndex = 0;
}
///////////////////////////////////////////////////////////////////////////
void OpGroup::push(const Op& the_op)
{
	////////////////////////////////
	// throttle it (limit number of ops in queue)
	////////////////////////////////
	struct OpGroupThrottler : public IOpqSynchrComparison
	{	OpGroupThrottler( OpGroup& grp ) : mGrp(grp) {}
		bool IsConditionMet(const OpqSynchro& synchro) const
		{	int inumq = int(synchro.mOpCounter);
			int imax = int(mGrp.mLimitMaxOpsQueued);
			return (imax==0)||(inumq<imax);
		}
		OpGroup& mGrp;
	};
	OpGroupThrottler throttler(*this);
	mSynchro.WaitOnCondition(throttler);
	////////////////////////////////

	mOps.push(the_op);
	this->mSynchro.AddItem();
	mpOpQ->mSynchro.AddItem();

	mOpSerialIndex++;

	mpOpQ->mSemaphore.notify();
}
///////////////////////////////////////////////////////////////////////////
void OpGroup::drain()
{
	OpqDrained pred_is_drained;
	mSynchro.WaitOnCondition(pred_is_drained);
}
///////////////////////////////////////////////////////////////////////////
bool OpGroup::try_pop( Op& out_op )
{
	bool rval = mOps.try_pop(out_op);
	return rval;
}
///////////////////////////////////////////////////////////////////////////
OpqSynchro::OpqSynchro()
{
	mOpCounter = 0;
}
///////////////////////////////////////////////////////////////////////////
void OpqSynchro::AddItem()
{
	mtx_lock_t lock(mOpWaitMtx);
	mOpCounter++;
	mOpWaitCV.notify_one();
}
///////////////////////////////////////////////////////////////////////////
void OpqSynchro::RemItem()
{
	mtx_lock_t lock(mOpWaitMtx);
	mOpCounter--;
	mOpWaitCV.notify_one();
}
///////////////////////////////////////////////////////////////////////////
void OpqSynchro::WaitOnCondition(const IOpqSynchrComparison& comparator)
{
	mtx_lock_t lock(mOpWaitMtx);
	while( false == comparator.IsConditionMet(*this) )
	{	mOpWaitCV.wait(lock);
	}
}
///////////////////////////////////////////////////////////////////////////
int OpqSynchro::NumOps() const
{
	return int(mOpCounter);
}
///////////////////////////////////////////////////////////////////////
static Opq gmainupdateq(0,"MainUpdateQ");
static Opq gmainthrq(0,"MainThreadQ");
///////////////////////////////////////////////////////////////////////////
Opq& UpdateSerialOpQ()
{
	return gmainupdateq;
}
///////////////////////////////////////////////////////////////////////
Opq& EditorOpQ()
{
	return gmainupdateq;
}
///////////////////////////////////////////////////////////////////////
Opq& MainThreadOpQ()
{
	return gmainthrq;
}
///////////////////////////////////////////////////////////////////////
#if 1
static Opq gconopq(1,"ConcOpQ");
Opq& ConcurrentOpQ()
{
	return gconopq;
}
#endif
///////////////////////////////////////////////////////////////////////
void AssertOnOpQ2( Opq& the_opQ )
{
	auto ot = OpqTest::GetContext();
	assert( ot->mOPQ == & the_opQ );
}
void AssertOnOpQ( Opq& the_opQ )
{
	AssertOnOpQ2( the_opQ );
}
void AssertNotOnOpQ( Opq& the_opQ )
{
	auto ot = OpqTest::GetContext();
	assert( ot->mOPQ != & the_opQ );
}
///////////////////////////////////////////////////////////////////////////

}
