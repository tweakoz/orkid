///////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////

#include <ork/kernel/debug.h>
#include <ork/kernel/future.hpp>
#include <ork/kernel/opq.h>
#include <ork/kernel/string/string.h>
#include <ork/pch.h>
#include <ork/util/Context.hpp>

//#define DEBUG_OPQ_CALLSTACK
///////////////////////////////////////////////////////////////////////
template class ork::util::ContextTLS<ork::OpqTest>;
///////////////////////////////////////////////////////////////////////
namespace ork {
////////////////////////////////////////////////////////////////////////////////
Op::Op(const void_lambda_t& op, const std::string& name)
    : mName(name) {
  SetOp(op);
}
////////////////////////////////////////////////////////////////////////////////
Op::Op(const BarrierSyncReq& op, const std::string& name)
    : mName(name) {
  SetOp(op);
}
////////////////////////////////////////////////////////////////////////////////
Op::Op(const Op& oth)
    : mName(oth.mName)
    , mWrapped(oth.mWrapped) {}
////////////////////////////////////////////////////////////////////////////////
Op::Op() {}
////////////////////////////////////////////////////////////////////////////////
Op::~Op() {}
////////////////////////////////////////////////////////////////////////////////
void Op::SetOp(const op_wrap_t& op) {
  if (op.IsA<void_lambda_t>()) {
    mWrapped = op;
  } else if (op.IsA<BarrierSyncReq>()) {
    mWrapped = op;
  } else // unhandled op type
  {
    assert(false);
  }
}
///////////////////////////////////////////////////////////////////////////
void Op::QueueASync(Opq& q) const { q.push(*this); }
void Op::QueueSync(Opq& q) const {
  AssertNotOnOpQ(q);
  q.push(*this);
  Future the_fut;
  BarrierSyncReq R(the_fut);
  q.push(R);
  the_fut.GetResult();
}
///////////////////////////////////////////////////////////////////////////
struct OpqDrained : public IOpqSynchrComparison {
  bool IsConditionMet(const OpqSynchro& synchro) const { return (int(synchro.mOpCounter) == 0); }
};
///////////////////////////////////////////////////////////////////////////
OpqThread::OpqThread(Opq* popq, int thid){
    _data._opq      = popq;
    _data._threadID = thid;
  _state.store(EPOQSTATE_NEW);
}
///////////////////////////////////////////////////////////////////////////
OpqThread::~OpqThread(){

}
///////////////////////////////////////////////////////////////////////////
void OpqThread::run() // virtual
{
  _state.store(EPOQSTATE_RUNNING);
  OpqThreadData* opqthreaddata = &_data;
  Opq* popq                    = opqthreaddata->_opq;
  std::string opqn             = popq->_name;
  SetCurrentThreadName(opqn.c_str());

  popq->_numThreadsRunning++;

  static int icounter = 0;
  int thid            = opqthreaddata->_threadID + 4;
  std::string channam = CreateFormattedString("opqth%d", int(thid));

  OpqTest opqtest(popq);

  while (EPOQSTATE_OK2KILL != _state.load()) {
    popq->mSemaphore.wait(); // wait for an op (without spinning)

    switch( _state.load() ){

      case EPOQSTATE_RUNNING:{
        bool item_processed = popq->Process();
        break;
      }
      case EPOQSTATE_LOCKED:
        break;
      case EPOQSTATE_ENTERLOCK:
        _state.store(EPOQSTATE_LOCKED);
        break;
      case EPOQSTATE_EXITLOCK:
        _state.store(EPOQSTATE_RUNNING);
        break;
      case EPOQSTATE_OK2KILL:
        break;

    }
  }

  popq->_numThreadsRunning--;

  // printf( "popq<%p> thread exiting...\n", popq );
}
///////////////////////////////////////////////////////////////////////////
void Opq::_internalBeginLock() {
  _lock = true;
  _threads.atomicOp([=](threadset_t& thset){
    for( auto thread : thset ){
      assert(thread->_state.load() == EPOQSTATE_RUNNING );
      thread->_state.store(EPOQSTATE_ENTERLOCK);
    }
  });
  _threads.atomicOp([=](threadset_t& thset){
    for( auto thread : thset )
      while( thread->_state.load() != EPOQSTATE_LOCKED ){
        mSemaphore.notify();
        usleep(100);
      }
  });
  printf( "Opq<%s> Locked!\n", _name.c_str());
}
///////////////////////////////////////////////////////////////////////////
void Opq::_internalEndLock() {
  _lock = false;
  _threads.atomicOp([=](threadset_t& thset){
    for( auto thread : thset ){
       assert(thread->_state.load() == EPOQSTATE_LOCKED );
       thread->_state.store(EPOQSTATE_EXITLOCK);
    }
  });
  _threads.atomicOp([=](threadset_t& thset){
    for( auto thread : thset )
      while( thread->_state.load() != EPOQSTATE_RUNNING ){
        mSemaphore.notify();
        usleep(100);
      }
  });
  printf( "Opq<%s> Unlocked!\n", _name.c_str());
}
///////////////////////////////////////////////////////////////////////////
bool Opq::Process() {
  bool rval = false;

  Op the_op;

  OpGroup* pexecgrp = nullptr;

  int num_groups   = mGroupCounter;
  OpGroup* ptstgrp = nullptr;
  for (auto& grp : mOpGroups) {
    int ioif    = grp->mOpsInFlightCounter;
    int imax    = grp->mLimitMaxOpsInFlight;
    int inumops = grp->mSynchro.NumOps();

    if ((inumops > 0) && ((imax == 0) || (ioif < imax))) {
      if (grp->try_pop(the_op)) {
        pexecgrp = grp;
        break;
      }
    }
  }

  if (pexecgrp) {
    pexecgrp->mOpsInFlightCounter++;

    // printf( "  runop OIF<%d>\n", int(pexecgrp->mOpsInFlightCounter) );
    const char* ppnam = "opx";

    if (the_op.mName.length()) {
      ppnam = the_op.mName.c_str();
    }

    if (the_op.mWrapped.IsA<void_lambda_t>()) {
      the_op.mWrapped.Get<void_lambda_t>()();
    } else if (the_op.mWrapped.IsA<BarrierSyncReq>()) {
      auto& R = the_op.mWrapped.Get<BarrierSyncReq>();
      R.mFuture.Signal<bool>(true);
    } else {
      printf("unknown opq invokable type\n");
    }

    this->mSynchro.RemItem();
    pexecgrp->mSynchro.RemItem();

    pexecgrp->mOpsInFlightCounter--;
    rval = true;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////
void Opq::push(const Op& the_op) { mDefaultGroup->push(the_op); }
void Opq::push(const void_lambda_t& l, const std::string& name) { mDefaultGroup->push(Op(l, name)); }
void Opq::push(const BarrierSyncReq& s) { mDefaultGroup->push(Op(s)); }
///////////////////////////////////////////////////////////////////////////
void Opq::push_sync(const Op& the_op) {
  AssertNotOnOpQ(*this);
  push(the_op);
  Future the_fut;
  BarrierSyncReq R(the_fut);
  push(R);
  the_fut.GetResult();
}
///////////////////////////////////////////////////////////////////////////
void Opq::sync() {
  AssertNotOnOpQ(*this);
  Future the_fut;
  BarrierSyncReq R(the_fut);
  push(R);
  the_fut.GetResult();
}
///////////////////////////////////////////////////////////////////////////
void Opq::drain() {
  OpqDrained pred_is_drained;
  mSynchro.WaitOnCondition(pred_is_drained);
}
///////////////////////////////////////////////////////////////////////////
OpGroup* Opq::CreateOpGroup(const char* pname) {
  OpGroup* pgrp = new OpGroup(this, pname);
  mOpGroups.insert(pgrp);
  mGroupCounter++;
  return pgrp;
}
///////////////////////////////////////////////////////////////////////////
Opq::Opq(int inumthreads, const char* name)
    : _name(name)
    , mSemaphore(name) {
  _lock = false;
  mGroupCounter   = 0;
  _numThreadsRunning = 0;

  mDefaultGroup = CreateOpGroup("defconq");

  for (int i = 0; i < inumthreads; i++) {
    auto thread = new OpqThread(this, i);
    _threads.atomicOp([=](threadset_t& thset){
      thset.insert(thread);
    });
    thread->start();
  }
}
///////////////////////////////////////////////////////////////////////////
Opq::~Opq() {
  // drain();
  // sync();

  /////////////////////////////////
  // signal to thread we are going down, then wait for it to go down
  /////////////////////////////////

  _threads.atomicOp([=](threadset_t& thset){
    for( auto thread : thset )
      thread->_state.store(EPOQSTATE_OK2KILL);
  });

  _threads.atomicOp([=](threadset_t& thset){
    for( auto thread : thset ){
      thread->join();
      mSemaphore.notify();
    }
  });

  while (_numThreadsRunning.load() != 0) {
    usleep(10);
  }

  /////////////////////////////////
  // trash the groups
  /////////////////////////////////

  for (auto& it : mOpGroups) {
    delete it;
  }
  mOpGroups.clear();
  /////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////
OpGroup::OpGroup(Opq* popq, const char* pname)
    : mpOpQ(popq)
    , mLimitMaxOpsInFlight(0)
    , mLimitMaxOpsQueued(0)
    , mGroupName(pname) {
  mOpsInFlightCounter = 0;
  mOpSerialIndex      = 0;
}
///////////////////////////////////////////////////////////////////////////
void OpGroup::push(const Op& the_op) {
  ////////////////////////////////
  // throttle it (limit number of ops in queue)
  ////////////////////////////////
  struct OpGroupThrottler : public IOpqSynchrComparison {
    OpGroupThrottler(OpGroup& grp)
        : mGrp(grp) {}
    bool IsConditionMet(const OpqSynchro& synchro) const {
      int inumq = int(synchro.mOpCounter);
      int imax  = int(mGrp.mLimitMaxOpsQueued);
      return (imax == 0) || (inumq < imax);
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
void OpGroup::drain() {
  OpqDrained pred_is_drained;
  mSynchro.WaitOnCondition(pred_is_drained);
}
///////////////////////////////////////////////////////////////////////////
bool OpGroup::try_pop(Op& out_op) {
  bool rval = mOps.try_pop(out_op);
  return rval;
}
///////////////////////////////////////////////////////////////////////////
OpqSynchro::OpqSynchro() { mOpCounter = 0; }
///////////////////////////////////////////////////////////////////////////
void OpqSynchro::AddItem() {
  mtx_lock_t lock(mOpWaitMtx);
  mOpCounter++;
  mOpWaitCV.notify_one();
}
///////////////////////////////////////////////////////////////////////////
void OpqSynchro::RemItem() {
  mtx_lock_t lock(mOpWaitMtx);
  mOpCounter--;
  mOpWaitCV.notify_one();
}
///////////////////////////////////////////////////////////////////////////
void OpqSynchro::WaitOnCondition(const IOpqSynchrComparison& comparator) {
  mtx_lock_t lock(mOpWaitMtx);
  while (false == comparator.IsConditionMet(*this)) {
    mOpWaitCV.wait(lock);
  }
}
///////////////////////////////////////////////////////////////////////////
int OpqSynchro::NumOps() const { return int(mOpCounter); }
///////////////////////////////////////////////////////////////////////
static Opq gmainupdateq(0, "MainUpdateQ");
static Opq gmainthrq(0, "MainThreadQ");
///////////////////////////////////////////////////////////////////////////
Opq& UpdateSerialOpQ() { return gmainupdateq; }
///////////////////////////////////////////////////////////////////////
Opq& EditorOpQ() { return gmainupdateq; }
///////////////////////////////////////////////////////////////////////
Opq& MainThreadOpQ() { return gmainthrq; }
///////////////////////////////////////////////////////////////////////
std::shared_ptr<Opq::InternalLock> Opq::scopedLock() {
  auto l = std::make_shared<InternalLock>(*this);
  return l;
}
Opq::InternalLock::InternalLock(Opq& opq)
    : _opq(opq) {
  _opq._internalBeginLock();
}
Opq::InternalLock::~InternalLock() {
  _opq._internalEndLock();
}
///////////////////////////////////////////////////////////////////////
#if 1
static Opq gconopq(1, "ConcOpQ");
Opq& ConcurrentOpQ() { return gconopq; }
#endif
///////////////////////////////////////////////////////////////////////
void AssertOnOpQ2(Opq& the_opQ) {
  auto ot = OpqTest::GetContext();
  assert(ot->mOPQ == &the_opQ);
}
void AssertOnOpQ(Opq& the_opQ) { AssertOnOpQ2(the_opQ); }
void AssertNotOnOpQ(Opq& the_opQ) {
  auto ot = OpqTest::GetContext();
  assert(ot->mOPQ != &the_opQ);
}
///////////////////////////////////////////////////////////////////////////

} // namespace ork
