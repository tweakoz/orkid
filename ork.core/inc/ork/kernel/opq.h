///////////////////////////////////////////////////////////////////////////////
// Orkid - Copyright 2012 Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once
///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/concurrent_queue.h>
#include <ork/kernel/any.h>
#define _DEBUG_OPQ
#include <ork/orkstl.h>
#include <ork/util/Context.h>
#include <ork/kernel/thread.h>

#include <ork/kernel/atomic.h>

#include "mutex.h"
#include "semaphore.h"
#include <queue>
#include <set>

namespace ork {
struct Future;
void SetCurrentThreadName(const char* threadName);
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
namespace ork::opq {
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

typedef any128 op_wrap_t;

struct OperationsQueue;

struct BarrierSyncReq {
  BarrierSyncReq(Future& f)
      : mFuture(f) {
  }
  Future& mFuture;
};

struct Op {
  op_wrap_t mWrapped;
  std::string mName;

  Op(const Op& oth);
  Op(const BarrierSyncReq& op, const std::string& name = "");
  Op(const void_lambda_t& op, const std::string& name = "");
  Op();
  ~Op();
  void SetOp(const op_wrap_t& op);
  void invoke();

  void QueueASync(OperationsQueue& q) const;
  void QueueSync(OperationsQueue& q) const;
};

//////////////////////////////////////////////////////////////////////

struct IOpqSynchrComparison;

struct OpqSynchro {
  typedef std::function<bool(OpqSynchro* psyn, int icomparator)> comparison_block_t;
  typedef std::unique_lock<std::mutex> mtx_lock_t;

  OpqSynchro();

  void AddItem();
  void RemItem();
  void WaitOnCondition(const IOpqSynchrComparison& comparator); // wait til _pendingOps changes
  int pendingOps() const;

  ork::atomic<int> _pendingOps;
  std::condition_variable mOpWaitCV;
  std::mutex mOpWaitMtx;
};

//////////////////////////////////////////////////////////////////////

struct IOpqSynchrComparison {
  virtual bool IsConditionMet(const OpqSynchro& synchro) const = 0;
};

//////////////////////////////////////////////////////////////////////
// ConcurrencyGroup :
//  opq subdivision which enforces custom max concurrency within
//  the thread pool of an opq. In other words, if an OperationsQueue
//  has a threadpool size of 10 and a child ConcurrencyGroup has a
//  max inflight set to 3 then when enqueueing operations to
//  the group no more than 3 operations assigned to that group will be in
//  flight simultaneously, despite the opq's threadpool size of 10
//////////////////////////////////////////////////////////////////////

struct ConcurrencyGroup {

  ConcurrencyGroup(OperationsQueue* popq, const char* pname);
  void enqueue(const Op& the_op);
  bool try_pop(Op& out_op);
  void drain();
  void MakeSerial() {
    _limit_maxops_inflight = 1;
  }

  ////////////////////////////////

  typedef std::queue<Op> queue_t;

  ////////////////////////////////

  ork::LockedResource<queue_t> _ops;
  std::atomic<int> _opsinflight;
  std::atomic<int> _serialopindex;
  std::string _name;
  OperationsQueue* _queue;

  size_t _limit_maxops_inflight;
  size_t _limit_maxops_enqueued;
  size_t _limit_maxrunlength;
};

//////////////////////////////////////////////////////////////////////
// CompletionGroup
//  opq synchronization primitive which allows to batch together a set of
//  and wait until all of the operations assigned to the completion
//   group are finished
//////////////////////////////////////////////////////////////////////

struct CompletionGroup;

template <class... Args> std::unique_ptr<CompletionGroup> createCompletionGroup(Args&&... args);

struct CompletionGroup {

  void enqueue(const ork::void_lambda_t& the_op);
  void join();
  ~CompletionGroup();

private:
  template <class... Args> inline friend std::unique_ptr<CompletionGroup> createCompletionGroup(Args&&... args) {
    return std::unique_ptr<CompletionGroup>(new CompletionGroup(std::forward<Args>(args)...));
  }

  CompletionGroup(OperationsQueue& q);
  CompletionGroup(const CompletionGroup& oth) = delete;
  CompletionGroup& operator=(const CompletionGroup&) = delete;

  OperationsQueue& _q;
  std::atomic<int> _numpending;
};

///////////////////////////////////////////////////////////////////////////

struct OpqThreadData {
  OperationsQueue* _queue = nullptr;
  int _threadID           = 0;
};
enum OpqThreadState {
  EPOQSTATE_NEW = 0,
  EPOQSTATE_RUNNING,
  EPOQSTATE_ENTERLOCK,
  EPOQSTATE_LOCKED,
  EPOQSTATE_EXITLOCK,
  EPOQSTATE_OK2KILL,
  EPOQSTATE_DEAD
};
struct OpqThread : public ork::Thread {
  OpqThreadData _data;
  std::atomic<int> _state;
  OpqThread(OperationsQueue* popq, int thid);
  ~OpqThread();
  void run() final;
};
//////////////////////////////////////////////////////////////////////

struct OperationsQueue {
  OperationsQueue(int inumthreads, const char* name = "DefOpQ");
  ~OperationsQueue();

  struct InternalLock {

    InternalLock(OperationsQueue& opq);
    ~InternalLock();
    OperationsQueue& _queue;
  };

  std::shared_ptr<InternalLock> scopedLock();

  void enqueue(const Op& the_op);
  void enqueue(const void_lambda_t& l, const std::string& name = "");
  void enqueue(const BarrierSyncReq& s);
  void enqueueAndWait(const Op& the_op);
  void sync();
  void drain();

  void _internalBeginLock();
  void _internalEndLock();

  ConcurrencyGroup* createConcurrencyGroup(const char* pname);

  bool Process();

  typedef std::set<OpqThread*> threadset_t;

  ConcurrencyGroup* _defaultConcurrencyGroup;
  ork::atomic<int> mGroupCounter;
  LockedResource<threadset_t> _threads;
  OpqSynchro mSynchro;

  typedef std::vector<ConcurrencyGroup*> concgroupvect_t;

  std::set<ConcurrencyGroup*> _concurrencygroups;
  LockedResource<concgroupvect_t> _linearconcurrencygroups;

  ork::semaphore mSemaphore;

  std::atomic<bool> _lock;
  std::atomic<bool> _goingdown;
  std::atomic<int> _numThreadsRunning;
  std::atomic<int> _numPendingOperations;
  std::string _name;
  std::string _debuginfo;
};

//////////////////////////////////////////////////////////////////////

struct TrackCurrent : public ork::util::ContextTLS<TrackCurrent> {
  inline TrackCurrent(OperationsQueue* q)
      : _queue(q) {
  }
  static bool is(const OperationsQueue&rhs);
  OperationsQueue* _queue;
};
void assertOnQueue2(OperationsQueue& the_opQ);
void assertOnQueue(OperationsQueue& the_opQ);
void assertNotOnQueue(OperationsQueue& the_opQ);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

OperationsQueue& updateSerialQueue();
OperationsQueue& mainSerialQueue();
OperationsQueue& concurrentQueue();

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::opq
///////////////////////////////////////////////////////////////////////////////
