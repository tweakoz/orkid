////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/util/shmobject.h>
#include <atomic>
#include <functional>
#include <thread>
#include <unistd.h>
#include <sys/types.h>

namespace ork {

////////////////////////////////////////////////////////////////
// Shared memory safe mutex using only atomics
// Can be used across process boundaries
////////////////////////////////////////////////////////////////

struct ShmMutex {
  std::atomic<uint32_t> _lock_state{0};     // 0=free, 1=locked
  std::atomic<pid_t> _owner_pid{0};         // For recursive locking
  std::atomic<uint32_t> _owner_tid{0};      // Thread ID for intra-process
  std::atomic<uint32_t> _recursion_count{0};
  
  void lock() {
    pid_t my_pid = getpid();
    uint32_t my_tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
    
    // Check for recursive lock
    if (_owner_pid.load(std::memory_order_acquire) == my_pid &&
        _owner_tid.load(std::memory_order_acquire) == my_tid) {
      _recursion_count.fetch_add(1, std::memory_order_acq_rel);
      return;
    }
    
    // Spin until we acquire the lock
    uint32_t expected = 0;
    while (!_lock_state.compare_exchange_weak(expected, 1, 
                                               std::memory_order_acquire,
                                               std::memory_order_relaxed)) {
      expected = 0;
      // Exponential backoff
      std::this_thread::yield();
      for (volatile int i = 0; i < 100; ++i) {} // Small busy wait
    }
    
    // We have the lock
    _owner_pid.store(my_pid, std::memory_order_release);
    _owner_tid.store(my_tid, std::memory_order_release);
    _recursion_count.store(1, std::memory_order_release);
  }
  
  bool try_lock() {
    pid_t my_pid = getpid();
    uint32_t my_tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
    
    // Check for recursive lock
    if (_owner_pid.load(std::memory_order_acquire) == my_pid &&
        _owner_tid.load(std::memory_order_acquire) == my_tid) {
      _recursion_count.fetch_add(1, std::memory_order_acq_rel);
      return true;
    }
    
    // Try to acquire
    uint32_t expected = 0;
    if (_lock_state.compare_exchange_strong(expected, 1,
                                           std::memory_order_acquire,
                                           std::memory_order_relaxed)) {
      _owner_pid.store(my_pid, std::memory_order_release);
      _owner_tid.store(my_tid, std::memory_order_release);
      _recursion_count.store(1, std::memory_order_release);
      return true;
    }
    
    return false;
  }
  
  void unlock() {
    pid_t my_pid = getpid();
    uint32_t my_tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
    
    // Verify we own the lock
    if (_owner_pid.load(std::memory_order_acquire) != my_pid ||
        _owner_tid.load(std::memory_order_acquire) != my_tid) {
      // Fatal error - unlocking mutex we don't own
      abort();
    }
    
    uint32_t count = _recursion_count.fetch_sub(1, std::memory_order_acq_rel);
    if (count == 1) {
      // Last unlock - release the mutex
      _owner_pid.store(0, std::memory_order_release);
      _owner_tid.store(0, std::memory_order_release);
      _lock_state.store(0, std::memory_order_release);
    }
  }
};

////////////////////////////////////////////////////////////////
// Shared memory safe locked resource
// All data including mutex lives in shared memory
////////////////////////////////////////////////////////////////

template<typename T>
struct ShmLockedResource {
  static_assert(std::is_trivially_copyable<T>::value, 
                "T must be POD/trivially copyable for shared memory");
  
  using mutable_atomicop_t = std::function<void(T&)>;
  using const_atomicop_t = std::function<void(const T&)>;
  
  ShmMutex _mutex;
  T _data;
  
  ////////////////////////////////////////////////////////////////
  // RAII lock guard
  ////////////////////////////////////////////////////////////////
  
  struct ScopedLock {
    ShmLockedResource* _resource;
    bool _locked;
    
    ScopedLock(ShmLockedResource* res) 
      : _resource(res), _locked(false) {
      _resource->_mutex.lock();
      _locked = true;
    }
    
    ~ScopedLock() {
      if (_locked) {
        _resource->_mutex.unlock();
      }
    }
    
    // Delete copy/move to prevent misuse
    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;
    ScopedLock(ScopedLock&&) = delete;
    ScopedLock& operator=(ScopedLock&&) = delete;
  };
  
  ////////////////////////////////////////////////////////////////
  // Initialization for shared memory
  ////////////////////////////////////////////////////////////////
  
  void initializeShmImage() {
    // Initialize mutex atomics (they're already zero from memory allocation)
    _mutex._lock_state.store(0);
    _mutex._owner_pid.store(0);
    _mutex._owner_tid.store(0);
    _mutex._recursion_count.store(0);
    
    // Initialize data to default
    _data = T{};
  }
  
  void uninitializeShmImage() {
    // No special cleanup needed for atomics
  }
  
  ////////////////////////////////////////////////////////////////
  // API matching LockedResource
  ////////////////////////////////////////////////////////////////
  
  T& lockForWrite() {
    _mutex.lock();
    return _data;
  }
  
  const T& lockForRead() {
    _mutex.lock();
    return _data;
  }
  
  void unlock() {
    _mutex.unlock();
  }
  
  bool tryLock() {
    return _mutex.try_lock();
  }
  
  // Atomic operations with automatic locking
  void atomicOp(const mutable_atomicop_t& op) {
    ScopedLock lock(this);
    op(_data);
  }
  
  void atomicOp(const const_atomicop_t& op) const {
    // Const cast needed for mutex operations
    auto* mutable_this = const_cast<ShmLockedResource*>(this);
    ScopedLock lock(mutable_this);
    op(_data);
  }
  
  void atomicWrite(const T& value) {
    ScopedLock lock(this);
    _data = value;
  }
  
  T atomicCopy() const {
    auto* mutable_this = const_cast<ShmLockedResource*>(this);
    ScopedLock lock(mutable_this);
    return _data;
  }
  
  T atomicExchange(const T& new_value) {
    ScopedLock lock(this);
    T old_value = _data;
    _data = new_value;
    return old_value;
  }
  
  // Direct access to data - use with extreme caution!
  T& _unprotected_ref() { return _data; }
  const T& _unprotected_ref() const { return _data; }
};

////////////////////////////////////////////////////////////////
// Helper to create in shared memory
////////////////////////////////////////////////////////////////

template<typename T>
class ShmLockedResourcePtr {
public:
  using resource_t = ShmLockedResource<T>;
  using sharedmem_t = ShmObject<resource_t>;
  
  static std::shared_ptr<sharedmem_t> create(const std::string& name, const T& initial_value = T{}) {
    auto shm = sharedmem_t::realize(name, sizeof(resource_t));
    
    // Check if we need to initialize (first time creation)
    static std::atomic<bool> initialized{false};
    bool expected = false;
    if (initialized.compare_exchange_strong(expected, true)) {
      // Placement new to construct in shared memory
      new (shm->data()) resource_t(initial_value);
    }
    
    return shm;
  }
};

} // namespace ork