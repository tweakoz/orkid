///////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////

#include <ork/kernel/debug.h>
#include <ork/kernel/future.hpp>
#include <ork/kernel/opq.h>
#include <ork/kernel/csystem.h>
#include <ork/kernel/string/string.h>
#include <ork/pch.h>
#include <ork/util/Context.hpp>

//#define DEBUG_OPQ_CALLSTACK
///////////////////////////////////////////////////////////////////////
template class ork::util::ContextTLS<ork::opq::TrackCurrent>;
///////////////////////////////////////////////////////////////////////
namespace ork::opq {
void dispersed_sleep(int idx, int iquantausec) {
  static const int ktabsize       = 16;
  static const int ktab[ktabsize] = {
      0,
      1,
      3,
      5,
      17,
      19,
      21,
      23,
      35,
      37,
      39,
      41,
      53,
      55,
      57,
      59,
  };
  usleep(ktab[idx & 0xf] * iquantausec);
}
////////////////////////////////////////////////////////////////////////////////
void CompletionGroup::enqueue(const ork::void_lambda_t& the_op) {
  this->_numpending.fetch_add(1);
  auto wrapped = [=]() mutable {
    the_op();
    int num_pending = this->_numpending.fetch_add(-1);
    printf("num_pending<%d>\n", num_pending);
  };
  _q.enqueue(wrapped);
}
void CompletionGroup::join() {
  while (_numpending.load()) {
    ::usleep(1000);
  }
}
CompletionGroup::CompletionGroup(OperationsQueue& q)
    : _q(q) {
  _numpending.store(0);
}
CompletionGroup::~CompletionGroup() {
  join();
}
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
    , mWrapped(oth.mWrapped) {
}
////////////////////////////////////////////////////////////////////////////////
Op::Op() {
}
////////////////////////////////////////////////////////////////////////////////
Op::~Op() {
}
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
void Op::invoke() {
  if (auto as_lambda = mWrapped.TryAs<void_lambda_t>()) {
    as_lambda.value()();
  } else if (auto as_barrier = mWrapped.TryAs<BarrierSyncReq>()) {
    as_barrier.value().mFuture.Signal<bool>(true);
  } else {
    printf("unknown operation type<%s>\n", mWrapped.GetTypeName());
    OrkAssert(false);
  }
}
///////////////////////////////////////////////////////////////////////////
void Op::QueueASync(OperationsQueue& q) const {
  q.enqueue(*this);
}
void Op::QueueSync(OperationsQueue& q) const {
  assertNotOnQueue(q);
  q.enqueue(*this);
  Future the_fut;
  BarrierSyncReq R(the_fut);
  q.enqueue(R);
  the_fut.GetResult();
}
///////////////////////////////////////////////////////////////////////////
struct OpqDrained : public IOpqSynchrComparison {
  bool IsConditionMet(const OpqSynchro& synchro) const {
    return (int(synchro._pendingOps) == 0);
  }
};
///////////////////////////////////////////////////////////////////////////
OpqThread::OpqThread(OperationsQueue* popq, int thid) {
  _data._queue    = popq;
  _data._threadID = thid;
  _state.store(EPOQSTATE_NEW);
}
///////////////////////////////////////////////////////////////////////////
OpqThread::~OpqThread() {
}
///////////////////////////////////////////////////////////////////////////
void OpqThread::run() // virtual
{
  _state.store(EPOQSTATE_RUNNING);
  OpqThreadData* opqthreaddata = &_data;
  OperationsQueue* popq        = opqthreaddata->_queue;
  std::string opqn             = popq->_name;
  SetCurrentThreadName(opqn.c_str());

  popq->_numThreadsRunning++;

  static int icounter = 0;
  int thid            = opqthreaddata->_threadID + 4;
  std::string channam = CreateFormattedString("opqth%d", int(thid));

  TrackCurrent opqtest(popq);

  int slindex = 0;

  while (EPOQSTATE_OK2KILL != _state.load()) {
    dispersed_sleep(slindex++, 10); // semaphores are slowing us down
    // popq->mSemaphore.wait(); // wait for an op (without spinning)

    switch (_state.load()) {

      case EPOQSTATE_RUNNING: {
        bool item_processed = popq->Process();
        if (item_processed)
          slindex = 0;
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
void OperationsQueue::_internalBeginLock() {
  _lock = true;
  _threads.atomicOp([=](threadset_t& thset) {
    for (auto thread : thset) {
      assert(thread->_state.load() == EPOQSTATE_RUNNING);
      thread->_state.store(EPOQSTATE_ENTERLOCK);
    }
  });
  _threads.atomicOp([=](threadset_t& thset) {
    for (auto thread : thset)
      while (thread->_state.load() != EPOQSTATE_LOCKED) {
        mSemaphore.notify();
        usleep(0);
      }
  });
  printf("Opq<%s> Locked!\n", _name.c_str());
}
///////////////////////////////////////////////////////////////////////////
void OperationsQueue::_internalEndLock() {
  _lock = false;
  _threads.atomicOp([=](threadset_t& thset) {
    for (auto thread : thset) {
      assert(thread->_state.load() == EPOQSTATE_LOCKED);
      thread->_state.store(EPOQSTATE_EXITLOCK);
    }
  });
  _threads.atomicOp([=](threadset_t& thset) {
    for (auto thread : thset)
      while (thread->_state.load() != EPOQSTATE_RUNNING) {
        mSemaphore.notify();
        usleep(0);
      }
  });
  printf("Opq<%s> Unlocked!\n", _name.c_str());
}
///////////////////////////////////////////////////////////////////////////
bool OperationsQueue::Process() {

  ///////////////////////////////////////
  // find a group with pending ops
  ///////////////////////////////////////

  ConcurrencyGroup* pexecgrp = nullptr;


  _linearconcurrencygroups.atomicOp([&pexecgrp](concgroupvect_t&cgv){
    size_t numgroups   = cgv.size();
    size_t numattempts = 0;
    while ((pexecgrp == nullptr) and (numattempts < numgroups)) {

      size_t index = rand() % numgroups;
      numattempts++;

      auto grp = cgv[index];
      grp->_ops.atomicOp([grp, &pexecgrp](ConcurrencyGroup::queue_t& q) {
        if (q.size()) {
          pexecgrp = grp;
        }
      });
    }
  });

  ///////////////////////////////////////
  // do a run on the group
  ///////////////////////////////////////

  if (pexecgrp) {
    // printf( "  runop OIF<%d>\n", int(pexecgrp->mOpsInFlightCounter) );
    const char* ppnam = "opx";

    Op the_op;

    bool keep_going = true;

    int run_index = 0;
    while (keep_going) {
      bool got_one = false;
      pexecgrp->_ops.atomicOp([&the_op, &pexecgrp, &got_one](ConcurrencyGroup::queue_t& q) {
        int numinfl            = pexecgrp->_opsinflight.fetch_add(1);
        bool maxinflight_check = (numinfl < pexecgrp->_limit_maxops_inflight);
        maxinflight_check |= (pexecgrp->_limit_maxops_inflight == 0);
        if (maxinflight_check and (q.size() != 0)) {
          the_op = q.front();
          q.pop();
          got_one = true;
        } else {
          pexecgrp->_opsinflight.fetch_add(-1);
        }
      });
      if (got_one) {
        the_op.invoke();
        if (the_op.mName.length()) {
          ppnam = the_op.mName.c_str();
        }
        this->mSynchro.RemItem();
        pexecgrp->_opsinflight.fetch_add(-1);
        run_index++;
      }
      keep_going = got_one and (run_index < pexecgrp->_limit_maxrunlength);
    } // while (keep_going) {
  }   // if (pexecgrp) {
  ///////////////////////////////////////
  return false;
} // namespace ork
///////////////////////////////////////////////////////////////////////////
void OperationsQueue::enqueue(const Op& the_op) {
  if (false == _goingdown)
    _defaultConcurrencyGroup->enqueue(the_op);
}
void OperationsQueue::enqueue(const void_lambda_t& l, const std::string& name) {
  if (false == _goingdown)
    _defaultConcurrencyGroup->enqueue(Op(l, name));
}
void OperationsQueue::enqueue(const BarrierSyncReq& s) {
  if (false == _goingdown)
    _defaultConcurrencyGroup->enqueue(Op(s));
}
///////////////////////////////////////////////////////////////////////////
void OperationsQueue::enqueueAndWait(const Op& the_op) {
  assertNotOnQueue(*this);
  enqueue(the_op);
  Future the_fut;
  BarrierSyncReq R(the_fut);
  enqueue(R);
  the_fut.GetResult();
}
///////////////////////////////////////////////////////////////////////////
void OperationsQueue::sync() {
  assertNotOnQueue(*this);
  Future the_fut;
  BarrierSyncReq R(the_fut);
  enqueue(R);
  the_fut.GetResult();
}
///////////////////////////////////////////////////////////////////////////
void OperationsQueue::drain() {
  // todo - drain all groups atomically
  concgroupvect_t copy_cgv;
  _linearconcurrencygroups.atomicOp([&copy_cgv](concgroupvect_t&cgv){
    copy_cgv = cgv;
    });
  for (auto g : copy_cgv)
     g->drain();
  for (auto g : copy_cgv)
      g->drain();
}
///////////////////////////////////////////////////////////////////////////
ConcurrencyGroup* OperationsQueue::createConcurrencyGroup(const char* pname) {
  ConcurrencyGroup* pgrp = new ConcurrencyGroup(this, pname);
  _concurrencygroups.insert(pgrp);
  _linearconcurrencygroups.atomicOp([pgrp](concgroupvect_t&cgv){
    cgv.push_back(pgrp);
    });
  mGroupCounter++;
  return pgrp;
}
///////////////////////////////////////////////////////////////////////////
OperationsQueue::OperationsQueue(int inumthreads, const char* name)
    : _name(name)
    , mSemaphore(name) {
  _lock              = false;
  _goingdown         = false;
  mGroupCounter      = 0;
  _numThreadsRunning = 0;
  _numPendingOperations = 0;

  _defaultConcurrencyGroup = createConcurrencyGroup("defconq");

  for (int i = 0; i < inumthreads; i++) {
    auto thread = new OpqThread(this, i);
    _threads.atomicOp([=](threadset_t& thset) { thset.insert(thread); });
    thread->start();
  }
}
///////////////////////////////////////////////////////////////////////////
OperationsQueue::~OperationsQueue() {

  _lock      = true;
  _goingdown = true;

  /////////////////////////////////
  // signal to thread we are going down, then wait for it to go down
  /////////////////////////////////

  // printf( "Opq<%s> signalling OK2KILL\n", _name.c_str());
  size_t numthreads = 0;
  _threads.atomicOp([=,&numthreads](threadset_t& thset) {
    numthreads = thset.size();
    for (auto thread : thset)
      thread->_state.store(EPOQSTATE_OK2KILL);
  });

  //printf( "Opq<%s> joining numthreads<%zu>\n", _name.c_str(),numthreads);
  bool done = false;
  while (false == done) {
    _threads.atomicOp([=, &done,&numthreads](threadset_t& thset) {
      done = thset.empty();
      if (false == done) {
        this->mSemaphore.notify();
        auto thread = *thset.begin();
        thread->join();
        thset.erase(thread);
        numthreads--;
      }
    });
    usleep(0);
  }

   //printf( "Opq<%p:%s> joined numthreadsrem<%zu>\n", this,_name.c_str(), numthreads);

  /////////////////////////////////
  // trash the groups
  /////////////////////////////////

  for (auto& it : _concurrencygroups) {
    delete it;
  }
  _concurrencygroups.clear();
  _linearconcurrencygroups.atomicOp([](concgroupvect_t&cgv){
    cgv.clear();
    });
  /////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////
ConcurrencyGroup::ConcurrencyGroup(OperationsQueue* popq, const char* pname)
    : _queue(popq)
    , _limit_maxops_inflight(0)
    , _limit_maxops_enqueued(0)
    , _limit_maxrunlength(256)
    , _name(pname) {
  _opsinflight   = 0;
  _serialopindex = 0;
}
///////////////////////////////////////////////////////////////////////////
void ConcurrencyGroup::enqueue(const Op& the_op) {

  bool was_enqueued = false;

  while (false == was_enqueued) {

    _ops.atomicOp([&the_op, &was_enqueued, this](ConcurrencyGroup::queue_t& q) {
      bool fits = (_limit_maxops_enqueued == 0) or (q.size() < _limit_maxops_enqueued);
      if (fits) {
        int index = this->_serialopindex.fetch_add(1);
        q.push(the_op);
        was_enqueued = true;
      }
    });

    if (false == was_enqueued) {
      static std::atomic<int> slindex(0);
      dispersed_sleep(slindex++, 10); // semaphores are slowing us down
    }
  }
  _queue->mSemaphore.notify();
}
///////////////////////////////////////////////////////////////////////////
void ConcurrencyGroup::drain() {
  bool was_drained = false;

  while (false == was_drained) {

    _ops.atomicOp([this, &was_drained](ConcurrencyGroup::queue_t& q) {
      was_drained = q.empty();
      was_drained &= (_opsinflight.load() == 0);
    });

    if (false == was_drained) {
      static std::atomic<int> slindex(0);
      dispersed_sleep(slindex++, 10); // semaphores are slowing us down
    }
  }
} // namespace ork
///////////////////////////////////////////////////////////////////////////
bool ConcurrencyGroup::try_pop(Op& out_op) {
  bool got_one = false;
  _ops.atomicOp([&out_op, &got_one](ConcurrencyGroup::queue_t& q) {
    got_one = (q.empty() == false);
    out_op  = q.front();
    q.pop();
  });
  return got_one;
}
///////////////////////////////////////////////////////////////////////////
OpqSynchro::OpqSynchro() {
  _pendingOps = 0;
}
///////////////////////////////////////////////////////////////////////////
void OpqSynchro::AddItem() {
  mtx_lock_t lock(mOpWaitMtx);
  _pendingOps++;
  mOpWaitCV.notify_one();
}
///////////////////////////////////////////////////////////////////////////
void OpqSynchro::RemItem() {
  mtx_lock_t lock(mOpWaitMtx);
  _pendingOps--;
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
int OpqSynchro::pendingOps() const {
  return _pendingOps.load();
}
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
OperationsQueue& updateSerialQueue() {
  static OperationsQueue gupdserq(0, "updateSerialQueue");
  return gupdserq;
}
///////////////////////////////////////////////////////////////////////
OperationsQueue& mainSerialQueue() {
  static OperationsQueue gmainthrq(0, "mainSerialQueue");
  return gmainthrq;
}
///////////////////////////////////////////////////////////////////////
OperationsQueue& concurrentQueue() {
  int numcores   = OldSchool::GetNumCores();
  int numthreads = 1;
  switch (numcores) {
    case 4: // 4 hyperthreaded, 2 physical
      numthreads = 1;
      break;
    default:
      numthreads = (numcores / 2) - 2;
      break;
  }
  static auto gconcurrentq = std::make_shared<OperationsQueue>(numthreads, "concurrentQueue");
  return *(gconcurrentq.get());
}
///////////////////////////////////////////////////////////////////////
std::shared_ptr<OperationsQueue::InternalLock> OperationsQueue::scopedLock() {
  auto l = std::make_shared<InternalLock>(*this);
  return l;
}
OperationsQueue::InternalLock::InternalLock(OperationsQueue& opq)
    : _queue(opq) {
  _queue._internalBeginLock();
}
OperationsQueue::InternalLock::~InternalLock() {
  _queue._internalEndLock();
}
///////////////////////////////////////////////////////////////////////
void assertOnQueue2(OperationsQueue& the_opQ) {
  auto ot = TrackCurrent::GetContext();
  assert(ot->_queue == &the_opQ);
}
void assertOnQueue(OperationsQueue& the_opQ) {
  assertOnQueue2(the_opQ);
}
void assertNotOnQueue(OperationsQueue& the_opQ) {
  auto ot = TrackCurrent::GetContext();
  assert(ot->_queue != &the_opQ);
}
bool TrackCurrent::is(const OperationsQueue&rhs) {
  auto ot = TrackCurrent::GetContext();
  return &rhs==ot->_queue;
}

///////////////////////////////////////////////////////////////////////////

} // namespace ork::opq
