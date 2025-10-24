////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
///////////////////////////////////////////////////////////////////////////////
#include <unordered_map>
#include <queue>
#include <set>
///////////////////////////////////////////////////////////////////////////////
#include <ork/orkstl.h>
#include <ork/kernel/concurrent_queue.h>
#include <ork/kernel/any.h>
#include <ork/kernel/thread.h>
#include <ork/kernel/atomic.h>
#include <ork/kernel/mutex.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/semaphore.h>
#include <ork/util/Context.h>
#include <ork/util/ringbuffer.inl>
///////////////////////////////////////////////////////////////////////////////

#define _DEBUG_OPQ
///////////////////////////////////////////////////////////////////////////////
namespace ork {
struct Future;
void SetCurrentThreadName(const char* threadName);
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
namespace ork::opq {
///////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Opq Ui Progress Indicator
//////////////////////////////////////////////////////////////////////

struct ProgressData {
  std::string _queue_name;
  std::string _task_name;
  int _num_pending = 0;
};
using progressdata_ptr_t   = std::shared_ptr<ProgressData>;
using progressdata_queue_t = std::queue<progressdata_ptr_t>;
using progress_handler_t   = std::function<void(progressdata_ptr_t)>;
void setProgressHandler(progress_handler_t);

///////////////////////////////////////////////////////////////////////////////

typedef any128 op_wrap_t;

struct OperationsQueue;
using opq_ptr_t = std::shared_ptr<OperationsQueue>;

//////////////////////////////////////////////////////////////////////
// OPQ Performance Data Structure
//////////////////////////////////////////////////////////////////////

struct OPQPerfData {
  float aggregate_ops_per_sec = 0.0f;
  int num_threads = 0;
  float avg_latency_ms = 0.0f;
  float max_latency_ms = 0.0f;
  int pending_ops = 0;
  int completed_ops = 0;
  float update_time = 0.0f;
  std::string queue_name;
};

using opq_perfdata_ptr_t = std::shared_ptr<OPQPerfData>;

///////////////////////////////////////////////////////////////////////////////

struct BarrierSyncReq {
  BarrierSyncReq(future_ptr_t f)
      : _future(f) {
  }
  future_ptr_t _future;
};

struct Op {
  op_wrap_t mWrapped;
  double _enqueueTime = 0.0;
  std::string mName;

  Op(const Op& oth);
  Op(const BarrierSyncReq& op, const std::string& name = "");
  Op(const void_lambda_t& op, const std::string& name = "");
  Op();
  ~Op();
  void SetOp(const op_wrap_t& op);
  void invoke();

  void QueueASync(opq_ptr_t q) const;
  void QueueSync(opq_ptr_t q) const;
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

  ConcurrencyGroup(OperationsQueue& q, const char* pname);
  void enqueue(const Op& the_op);
  bool try_pop(Op& out_op);
  void drain(float timeout_seconds = 0.0f);
  void MakeSerial() {
    _limit_maxops_inflight = 1;
  }

  ////////////////////////////////

  using internal_oper_queue_t = std::queue<Op>;

  ////////////////////////////////

  ork::LockedResource<internal_oper_queue_t> _ops;
  std::atomic<int> _opsinflight;
  std::atomic<int> _serialopindex;
  std::string _name;
  OperationsQueue& _queue;

  size_t _limit_maxops_inflight;
  size_t _limit_maxops_enqueued;
  size_t _limit_maxrunlength;
};

using concurrency_group_ptr_t = std::shared_ptr<ConcurrencyGroup>;
// using concurrency_group_ptr_t = ConcurrencyGroup*;

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
  void join(bool with_progress_handler = false);
  ~CompletionGroup();
  inline void dontReportToUI() {
    _reportToUI = false;
  };

private:
  template <class... Args> inline friend std::unique_ptr<CompletionGroup> createCompletionGroup(Args&&... args) {
    return std::unique_ptr<CompletionGroup>(new CompletionGroup(std::forward<Args>(args)...));
  }

  CompletionGroup(opq_ptr_t q, std::string name = "");
  CompletionGroup(const CompletionGroup& oth) = delete;
  CompletionGroup& operator=(const CompletionGroup&) = delete;

  opq_ptr_t _q;
  std::string _name;
  std::atomic<int> _numpending;
  bool _reportToUI = true;
  ork::LockedResource<progressdata_queue_t> _progressq;
};

///////////////////////////////////////////////////////////////////////////

struct OpqThreadData {
  OperationsQueue* _queue;
  int _threadID = 0;
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
  static std::atomic<int> _gthreadcount;
  OpqThreadData _data;
  std::atomic<int> _state;
  Timer _timer;
  float _idleTime = 0.0;
  OpqThread(OperationsQueue* q, int thid);
  ~OpqThread();
  void run() final;
};

//////////////////////////////////////////////////////////////////////

enum struct EPerformaceProfile : uint32_t {
  LOW_LATENCY = 0,
  BALANCED = 1,
  IO = 2
};

//////////////////////////////////////////////////////////////////////

struct OperationsQueue : public std::enable_shared_from_this<OperationsQueue> {
  OperationsQueue(int inumthreads, const char* name = "DefOpQ", EPerformaceProfile p = EPerformaceProfile::BALANCED);
  ~OperationsQueue();

  struct InternalLock {

    InternalLock(OperationsQueue& opq);
    ~InternalLock();
    OperationsQueue& _queue;
  };

  using hooklambda_t = std::function<void(svar64_t)>;

  std::shared_ptr<InternalLock> scopedLock();

  void enqueue(const Op& the_op);
  void enqueue(const void_lambda_t& l, const std::string& name = "");
  void enqueue(const BarrierSyncReq& s);
  void enqueueAndWait(const Op& the_op);
  void sync();
  void drain(float timeout_seconds = 0.0f);
  void terminate();
  bool goingDown() const { return _terminated; }

  void _internalBeginLock();
  void _internalEndLock();

  void setHook(std::string,hooklambda_t l);
  void invokeHook(std::string,svar64_t);

  concurrency_group_ptr_t createConcurrencyGroup(const char* pname);

  bool Process();

  // Performance monitoring methods
  opq_perfdata_ptr_t getPerformanceData() const;
  void startPerformanceTracking();
  void stopPerformanceTracking();

  typedef std::set<OpqThread*> threadset_t;

  concurrency_group_ptr_t _defaultConcurrencyGroup;
  ork::atomic<int> mGroupCounter;
  LockedResource<threadset_t> _threads;
  OpqSynchro mSynchro;

  typedef std::vector<concurrency_group_ptr_t> concgroupvect_t;

  std::set<concurrency_group_ptr_t> _concurrencygroups;
  LockedResource<concgroupvect_t> _linearconcurrencygroups;

  ork::semaphore mSemaphore;

  std::atomic<bool> _lock;
  std::atomic<bool> _terminated;
  std::atomic<int> _numThreadsRunning;
  std::atomic<int> _numPendingOperations;
  std::atomic<int> _numCompletedOperations;
  std::atomic<int> _numInFlight;
  std::string _name;
  std::string _debuginfo;
  EPerformaceProfile _perf_profile = EPerformaceProfile::BALANCED;

  using hookmap_t = std::unordered_map<std::string,hooklambda_t>;

  LockedResource<hookmap_t> _hooks;

  // Simple performance tracking members
  bool _perf_tracking_active = false;
  float _perf_start_time = 0.0f;
  int _perf_start_completed = 0;
  
  // Latency tracking
  mutable RingBuffer<float> _recent_latencies{100};  // Keep last 100 latency samples
  float _max_latency_ms = 0.0f;                      // Running maximum individual latency
  mutable float _max_avg_latency_ms = 0.0f;          // Running maximum average latency
};

//////////////////////////////////////////////////////////////////////

struct TrackCurrent : public ork::util::ContextTLS<TrackCurrent> {
  inline TrackCurrent(OperationsQueue* q)
      : _queue(q) {
  }
  inline TrackCurrent(opq_ptr_t q)
      : _queue(q.get()) {
  }
  static bool is(opq_ptr_t rhs);
  OperationsQueue* _queue;
};
void assertOnQueue2(opq_ptr_t the_opQ);
void assertOnQueue(opq_ptr_t the_opQ);
void assertNotOnQueue(opq_ptr_t the_opQ);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void init();
void exit();
opq_ptr_t updateSerialQueue();
opq_ptr_t mainSerialQueue();
opq_ptr_t concurrentQueue();
opq_ptr_t ioQueue();
opq_ptr_t auxSerialQueue();

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::opq
///////////////////////////////////////////////////////////////////////////////
