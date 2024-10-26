////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////


#include <ork/kernel/debug.h>
#include <ork/kernel/future.hpp>
#include <ork/kernel/opq.h>
#include <ork/kernel/csystem.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/string/deco.inl>
#include <ork/pch.h>
#include <ork/util/Context.hpp>
#include <ork/util/logger.h>
#include <ork/profiling.inl>

//#define DEBUG_OPQ_CALLSTACK
///////////////////////////////////////////////////////////////////////
template class ork::util::ContextTLS<ork::opq::TrackCurrent>;
///////////////////////////////////////////////////////////////////////
namespace ork::opq {
static logchannel_ptr_t logchan_opq = logger()->createChannel("OPQ", fvec3(0.4, 0.7, 0.7), true);
////////////////////////////////////////////////////////////////////////
static int MAX_THREADS = 0;
static int MIN_THREADS = 0;
////////////////////////////////////////////////////////////////////////
static void _coordinatorThreadStartup() {
  auto coordinator_thread_impl = [](anyp data){
    int num_completed = 0;
    int check_index = 0;
    while( OpqThread::_gthreadcount > 0 ){
      ork::usleep(1<<20);
      auto cq = concurrentQueue();
      int nt = cq->_numThreadsRunning;
      int nc= cq->_numCompletedOperations;
      int np = cq->_numPendingOperations;
      if((check_index&7)==0){
        //logchan_opq->log( "concurrentQueue numthreads<%d> completed<%d> pending<%d>", nt, nc, np );
      }
      ///////////////////////////////////////////////////////////
      // thread creation (if stalled)
      ///////////////////////////////////////////////////////////
      if((np>0) and num_completed<=nc){
        if(nt>=MAX_THREADS){
          //logchan_opq->log( "concurrentQueue stalled, max threads reached" );
          continue;
        }
        //logchan_opq->log( "concurrentQueue stalled, adding a new thread" );
        int numthreads = 0;
        cq->_threads.atomicOp([&numthreads](OperationsQueue::threadset_t& thset) { numthreads = thset.size(); });
        auto thread = new OpqThread(cq.get(), numthreads);
        cq->_threads.atomicOp([=](OperationsQueue::threadset_t& thset) { thset.insert(thread); });
        thread->start();
      }
      ///////////////////////////////////////////////////////////
      // thread deletion (if idle)
      ///////////////////////////////////////////////////////////
      else if(np==0 and (nt>MIN_THREADS)){ 
        //logchan_opq->log( "concurrentQueue too many idle threads, removing one" );
        OpqThread* thread = nullptr;
        cq->_threads.atomicOp([=,&thread](OperationsQueue::threadset_t& thset) {
          if(thset.size()>MIN_THREADS){
            thread = *thset.begin();
            thset.erase(thread);
          }
        });
        if(thread){
          thread->_state.store(EPOQSTATE_OK2KILL);
          thread->join();
          delete thread;
        }
      }
      ///////////////////////////////////////////////////////////
      num_completed = nc;
      check_index++;
    }
  };
  static auto coordinator_thread = std::make_shared<Thread>(coordinator_thread_impl, nullptr, "opq_coordinator_thread");
}
//////////////////////////////////////////////////////////////////////
static progress_handler_t g_handler = [](progressdata_ptr_t data) {
  auto name_str = deco::decorate(fvec3(1, 0.5, 0.1), data->_queue_name);
  auto grpn_str = deco::decorate(fvec3(1, 0.3, 0.0), data->_task_name);
  auto pend_str = deco::format(fvec3(1, 1, 0.1), "%d", data->_num_pending);
  printf(
      "opq<%s> CompletionGroup<%s> ops pending<%s>     \r", //
      name_str.c_str(),
      grpn_str.c_str(),
      pend_str.c_str());
  ork::usleep(10);
};
///////////////////////////////////////////////////////////////////////
void setProgressHandler(progress_handler_t new_handler) {
  mainSerialQueue()->enqueue([new_handler]() { //
    g_handler = new_handler;
  });
}
///////////////////////////////////////////////////////////////////////

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
  ork::usleep(ktab[idx & 0xf] * iquantausec);
}
static void _assertNotOnQueue(OperationsQueue* the_opQ) {
  auto ot = TrackCurrent::context();
  assert(ot->_queue != the_opQ);
}
////////////////////////////////////////////////////////////////////////////////
void CompletionGroup::enqueue(const ork::void_lambda_t& the_op) {
  this->_numpending.fetch_add(1);
  auto wrapped = [=]() mutable {
    the_op();
    /////////////////////////////////////
    // update UI with progress ?
    /////////////////////////////////////
    if (_reportToUI) {
      auto data          = std::make_shared<ProgressData>();
      data->_queue_name  = _q->_name;
      data->_task_name   = _name;
      _progressq.atomicOp([data](progressdata_queue_t& pq) { pq.push(data); });
      /////////////////////////////////////
      data->_num_pending = this->_numpending.fetch_add(-1);
    }
    else{
      this->_numpending.fetch_add(-1);
    }
  };
  _q->enqueue(wrapped);
}
////////////////////////////////////////////////////////////////////////////////
void CompletionGroup::join() {
  // todo implement with something better than sleep
  while (_numpending.load()) {
    auto ot = TrackCurrent::context();
    ///////////////////////////////////////
    auto main_thread_handler_op = [&]() {
      progressdata_ptr_t last_item;
      _progressq.atomicOp([&last_item](progressdata_queue_t& pq) {
        while (not pq.empty()) {
          last_item = pq.front();
          pq.pop();
        }
      });
      if (last_item)
        g_handler(last_item);
    };
    ///////////////////////////////////////
    // calling from main thread?
    ///////////////////////////////////////
    if (ot->_queue == mainSerialQueue().get()) {
      // if so run the progress handler here...
      main_thread_handler_op();
    }
    ///////////////////////////////////////
    // nope, not calling from main thread.
    ///////////////////////////////////////
    else {
      // run main_thread_handler_op on main thread synchronously somehow
      OrkAssert(false);
    }
    ///////////////////////////////////////
  }
}
////////////////////////////////////////////////////////////////////////////////
CompletionGroup::CompletionGroup(opq_ptr_t q, std::string name)
    : _q(q)
    , _name(name) {
  _numpending.store(0);
}
////////////////////////////////////////////////////////////////////////////////
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
    : mWrapped(oth.mWrapped)
    , mName(oth.mName) {
}
////////////////////////////////////////////////////////////////////////////////
Op::Op() {
}
////////////////////////////////////////////////////////////////////////////////
Op::~Op() {
}
////////////////////////////////////////////////////////////////////////////////
void Op::SetOp(const op_wrap_t& op) {
  if (op.isA<void_lambda_t>()) {
    mWrapped = op;
  } else if (op.isA<BarrierSyncReq>()) {
    mWrapped = op;
  } else // unhandled op type
  {
    assert(false);
  }
}
///////////////////////////////////////////////////////////////////////////
void Op::invoke() {
  if (auto as_lambda = mWrapped.tryAs<void_lambda_t>()) {
    as_lambda.value()();
  } else if (auto as_barrier = mWrapped.tryAs<BarrierSyncReq>()) {
    as_barrier.value()._future->Signal<bool>(true);
  } else {
    printf("unknown operation type<%s>\n", mWrapped.typeName());
    OrkAssert(false);
  }
}
///////////////////////////////////////////////////////////////////////////
void Op::QueueASync(opq_ptr_t q) const {
  q->enqueue(*this);
}
void Op::QueueSync(opq_ptr_t q) const {
  assertNotOnQueue(q);
  q->enqueue(*this);
  auto the_fut = std::make_shared<Future>();
  BarrierSyncReq R(the_fut);
  q->enqueue(R);
  the_fut->GetResult();
}
///////////////////////////////////////////////////////////////////////////
struct OpqDrained : public IOpqSynchrComparison {
  bool IsConditionMet(const OpqSynchro& synchro) const {
    return (int(synchro._pendingOps) == 0);
  }
};
///////////////////////////////////////////////////////////////////////////
std::atomic<int> OpqThread::_gthreadcount = 0;
///////////////////////////////////////////////////////////////////////////
OpqThread::OpqThread(OperationsQueue* q, int thid) {
  _data._queue    = q;
  _data._threadID = thid;
  _state.store(EPOQSTATE_NEW);
  _gthreadcount++;
}
///////////////////////////////////////////////////////////////////////////
OpqThread::~OpqThread() {
  _gthreadcount--;
}
///////////////////////////////////////////////////////////////////////////
void OpqThread::run() // virtual
{
  _state.store(EPOQSTATE_RUNNING);
  OpqThreadData* opqthreaddata = &_data;
  OperationsQueue* q           = opqthreaddata->_queue;
  std::string opqn             = q->_name;
  SetCurrentThreadName(opqn.c_str());

  q->_numThreadsRunning++;

  static int icounter = 0;
  int thid            = opqthreaddata->_threadID + 4;
  std::string channam = CreateFormattedString("opqth%d", int(thid));

  TrackCurrent opqtest(q);

  int slindex = 0;

  _timer.Start();

  while (EPOQSTATE_OK2KILL != _state.load()) {

    dispersed_sleep(slindex++, 10); // semaphores are slowing us down
    // popq->mSemaphore.wait(); // wait for an op (without spinning)

    switch (_state.load()) {

      case EPOQSTATE_RUNNING: {
        bool item_processed = q->Process();
        if (item_processed){
          _timer.Start();
          slindex = 0;
        }
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
    icounter++;
    if( icounter&0xfff){
      _idleTime = _timer.SecsSinceStart();
    }
  }

  q->_numThreadsRunning--;

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
        ork::usleep(0);
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
        ork::usleep(0);
      }
  });
  printf("Opq<%s> Unlocked!\n", _name.c_str());
}
///////////////////////////////////////////////////////////////////////////
bool OperationsQueue::Process() {

  bool item_processed = false;

  ///////////////////////////////////////
  // find a group with pending ops
  ///////////////////////////////////////

  concurrency_group_ptr_t pexecgrp = nullptr;

  _linearconcurrencygroups.atomicOp([&pexecgrp](concgroupvect_t& cgv) {
    size_t numgroups   = cgv.size();
    size_t numattempts = 0;
    while ((pexecgrp == nullptr) and (numattempts < numgroups)) {

      size_t index = rand() % numgroups;
      numattempts++;

      auto grp = cgv[index];
      grp->_ops.atomicOp([grp, &pexecgrp](ConcurrencyGroup::internal_oper_queue_t& q) {
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
      pexecgrp->_ops.atomicOp([&the_op, &pexecgrp, &got_one](ConcurrencyGroup::internal_oper_queue_t& q) {
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
        EASY_BLOCK("opq", profiler::colors::Magenta);
        the_op.invoke();
        _numCompletedOperations.fetch_add(1);
        _numPendingOperations.fetch_add(-1);
        item_processed = true;

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
  return item_processed;
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
  _assertNotOnQueue(this);
  enqueue(the_op);
  auto the_fut = std::make_shared<Future>();
  BarrierSyncReq R(the_fut);
  enqueue(R);
  the_fut->GetResult();
}
///////////////////////////////////////////////////////////////////////////
void OperationsQueue::sync() {
  auto the_fut = std::make_shared<Future>();
  BarrierSyncReq R(the_fut);
  enqueue(R);
  auto ot = TrackCurrent::context();
  if (ot->_queue == this) {
    while (false == the_fut->IsSignaled()) {
      this->Process();
    }
  }
  the_fut->GetResult();
}
///////////////////////////////////////////////////////////////////////////
void OperationsQueue::drain() {
  // todo - drain all groups atomically
  concgroupvect_t copy_cgv;
  _linearconcurrencygroups.atomicOp([&copy_cgv](concgroupvect_t& cgv) { copy_cgv = cgv; });
  for (auto g : copy_cgv)
    g->drain();
  for (auto g : copy_cgv)
    g->drain();
}
/////////////////////////////////////////////////////////////////////////////
void OperationsQueue::setHook(std::string hookname,hooklambda_t l){
  _hooks.atomicOp([hookname,l](hookmap_t& unlocked){
    unlocked[hookname] = l;
  });
}
/////////////////////////////////////////////////////////////////////////////
void OperationsQueue::invokeHook(std::string hookname,svar64_t data){
  hooklambda_t l = [](svar64_t){};
  _hooks.atomicOp([hookname,&l](hookmap_t& unlocked){
    auto it = unlocked.find(hookname);
    if(it!=unlocked.end()){
      l = it->second;
    }
  });
  l(data);
}
/////////////////////////////////////////////////////////////////////////////
concurrency_group_ptr_t OperationsQueue::createConcurrencyGroup(const char* pname) {
  auto pgrp = std::make_shared<ConcurrencyGroup>(*this, pname);
  _concurrencygroups.insert(pgrp);
  _linearconcurrencygroups.atomicOp([pgrp](concgroupvect_t& cgv) { cgv.push_back(pgrp); });
  mGroupCounter++;
  return pgrp;
}
///////////////////////////////////////////////////////////////////////////
OperationsQueue::OperationsQueue(int inumthreads, const char* name)
    : mSemaphore(name)
    , _name(name) {
  _lock                 = false;
  _goingdown            = false;
  mGroupCounter         = 0;
  _numThreadsRunning    = 0;
  _numPendingOperations = 0;
  _numCompletedOperations = 0;

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
  _threads.atomicOp([=, &numthreads](threadset_t& thset) {
    numthreads = thset.size();
    for (auto thread : thset)
      thread->_state.store(EPOQSTATE_OK2KILL);
  });

  // printf( "Opq<%s> joining numthreads<%zu>\n", _name.c_str(),numthreads);
  bool done = false;
  while (false == done) {
    _threads.atomicOp([=, &done, &numthreads](threadset_t& thset) {
      done = thset.empty();
      if (false == done) {
        this->mSemaphore.notify();
        auto thread = *thset.begin();
        thread->join();
        thset.erase(thread);
        numthreads--;
      }
    });
    ork::usleep(0);
  }

  // printf( "Opq<%p:%s> joined numthreadsrem<%zu>\n", this,_name.c_str(), numthreads);

  /////////////////////////////////
  // trash the groups
  /////////////////////////////////
  _concurrencygroups.clear();
  _linearconcurrencygroups.atomicOp([](concgroupvect_t& cgv) { cgv.clear(); });
  /////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////
ConcurrencyGroup::ConcurrencyGroup(OperationsQueue& q, const char* pname)
    : _name(pname)
    , _queue(q)
    , _limit_maxops_inflight(0)
    , _limit_maxops_enqueued(0)
    , _limit_maxrunlength(256) {
  _opsinflight   = 0;
  _serialopindex = 0;
}
///////////////////////////////////////////////////////////////////////////
void ConcurrencyGroup::enqueue(const Op& the_op) {

  bool was_enqueued = false;
  _queue._numPendingOperations.fetch_add(1);

  while (false == was_enqueued) {

    _ops.atomicOp([&the_op, &was_enqueued, this](ConcurrencyGroup::internal_oper_queue_t& q) {
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
  _queue.mSemaphore.notify();
}
///////////////////////////////////////////////////////////////////////////
void ConcurrencyGroup::drain() {
  bool was_drained = false;

  while (false == was_drained) {

    _ops.atomicOp([this, &was_drained](ConcurrencyGroup::internal_oper_queue_t& q) {
      int opsinfl = _opsinflight.load();
      was_drained = q.empty();
      was_drained &= (opsinfl == 0);
      //printf( "qempty<%d> opsinfl<%d>\n", int(q.empty()), opsinfl );
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
  _ops.atomicOp([&out_op, &got_one](ConcurrencyGroup::internal_oper_queue_t& q) {
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
opq_ptr_t updateSerialQueue() {
  static opq_ptr_t gupdserq = std::make_shared<OperationsQueue>(0, "updateSerialQueue");
  return gupdserq;
}
///////////////////////////////////////////////////////////////////////
opq_ptr_t mainSerialQueue() {
  static opq_ptr_t gmainthrq = std::make_shared<OperationsQueue>(0, "mainSerialQueue");
  return gmainthrq;
}
///////////////////////////////////////////////////////////////////////
opq_ptr_t concurrentQueue() {
  /////////////////////////////////////////////////////////
  int numcores = OldSchool::GetNumCores();
  MIN_THREADS = (numcores/2);
  MAX_THREADS = (numcores*2);
  if(MIN_THREADS<4){
    MIN_THREADS = 4;
  }
  if(MAX_THREADS<12){
    MAX_THREADS = 12;
  }
  /////////////////////////////////////////////////////////
  static opq_ptr_t gconcurrentq = std::make_shared<OperationsQueue>(MIN_THREADS, "concurrentQueue");
  return gconcurrentq;
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
void assertOnQueue2(opq_ptr_t the_opQ) {
  auto ot = TrackCurrent::context();
  assert(ot->_queue == the_opQ.get());
}
void assertOnQueue(opq_ptr_t the_opQ) {
  assertOnQueue2(the_opQ);
}
void assertNotOnQueue(opq_ptr_t the_opQ) {
  auto ot = TrackCurrent::context();
  assert(ot->_queue != the_opQ.get());
}
bool TrackCurrent::is(opq_ptr_t rhs) {
  auto ot = TrackCurrent::context();
  return rhs.get() == ot->_queue;
}
///////////////////////////////////////////////////////////////////////
void init() {
  concurrentQueue();
  mainSerialQueue();
  updateSerialQueue();
  _coordinatorThreadStartup();
}
///////////////////////////////////////////////////////////////////////////

} // namespace ork::opq
