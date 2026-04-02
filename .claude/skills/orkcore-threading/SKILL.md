---
name: orkcore-threading
description: Answer questions about orkid's threading, concurrency, operation queues (OPQ), LockedResource, thread pools, futures, and task graphs. Use when the user asks about async, threading, mutexes, concurrent queues, or parallelism in orkid.
user-invocable: false
---

# Orkid Core Threading & Concurrency Reference

When answering questions about threading or concurrency in orkid, consult the source files listed below. All paths are relative to the orkid repo root.

## Key Files

| Component | Header | Implementation |
|-----------|--------|----------------|
| Operation Queue (OPQ) | `ork.core/inc/ork/kernel/opq.h` | `ork.core/src/kernel/opq.cpp` |
| Thread | `ork.core/inc/ork/kernel/thread.h` | `ork.core/src/kernel/thread.cpp` |
| Mutex / LockedResource | `ork.core/inc/ork/kernel/mutex.h` | (header-only) |
| Concurrent Queues | `ork.core/inc/ork/kernel/concurrent_queue.h` | (header-only templates) |
| Ring Buffer (MPMC) | `ork.core/inc/ork/kernel/ringbuffer.hpp` | (header-only template) |
| Thread Pool | `ork.core/inc/ork/kernel/thread_pool.h` | `ork.core/src/kernel/thread_pool.cpp` |
| Future | `ork.core/inc/ork/kernel/future.hpp` | `ork.core/src/kernel/future.cpp` |
| Task Graph | `ork.core/inc/ork/kernel/taskgraph.h` | `ork.core/src/kernel/taskgraph.cpp` |
| Semaphore | `ork.core/inc/ork/kernel/semaphore.h` | `ork.core/src/kernel/semaphore.cpp` |
| Atomic | `ork.core/inc/ork/kernel/atomic.h` | (header-only wrapper) |

## Architecture Overview

### Operation Queue (OPQ) - Primary Concurrency Mechanism
- `OperationsQueue` — dynamic thread pool with enqueue/drain/sync
- Global queues: `opq::concurrentQueue()`, `opq::mainSerialQueue()`, `opq::updateSerialQueue()`, `opq::ioQueue()`
- `ConcurrencyGroup` — subdivide OPQ with max-inflight limits; `MakeSerial()` for serial execution
- `CompletionGroup` — batch operations and `join()` to wait for all
- Performance profiles: `LOW_LATENCY` (5us), `BALANCED` (25us), `IO` (2500us)

### LockedResource<T> - Thread-Safe Data Wrapper
- RAII wrapper with `LockForRead()`, `LockForWrite()`, `UnLock()`
- `atomicOp(lambda)` — execute lambda under lock
- `atomicCopy()`, `atomicExchange()` — convenience methods

### Concurrent Queues
- `SPSCQueue<T,N>` — lock-free single-producer single-consumer, fixed capacity
- `MpMcBoundedQueue<T,N>` — multi-producer multi-consumer via `MpMcRingBuf` (Vyukov algorithm)
- Both support blocking (`push`/`pop`) and non-blocking (`try_push`/`try_pop`)

### Task Graph
- `TaskGraph` — DAG of phases with async execution
- `TaskPhase` — collection of tasks sharing an executor
- `TaskExecutor::createOPQParallel()` — parallel via OPQ
- `TaskExecutor::createSerial()` — sequential execution
- Shared state via `LockedResource<VarMap>`

### Future
- Lightweight async result: `signal(result)`, `waitForSignal()`, `isSignaled()`
- Result stored in `svar160_t` (160-bit variant)
- Optional callback on signal

## How to Answer

1. For OPQ usage: read `opq.h` for the API, `opq.cpp` for thread management details
2. For thread safety: check if code uses `LockedResource` or `atomicOp` patterns
3. For queue selection: `concurrentQueue()` for CPU work, `ioQueue()` for I/O, serial queues for ordered work
4. Always check actual header signatures — training data may be outdated
