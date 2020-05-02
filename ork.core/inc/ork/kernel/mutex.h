////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkstd.h>
#include <ork/orkprotos.h>

///////////////////////////////////////////////////////////////////////////////
#include <condition_variable>
#include <mutex>
#include <atomic>

///////////////////////////////////////////////////////////////////////////////

namespace ork {
class recursive_mutex {
public:
  typedef std::function<void()> atomicop_t;
  recursive_mutex(const char* name)
      : mTheMutex()
      , mName(name)
      , miLockCount(0) {
  }
  void Lock(int lid = -1) {
    mTheMutex.lock();
    mLockIDs.push(lid);
    miLockCount++;
  }
  void UnLock() {
    miLockCount--;
    mLockIDs.pop();
    mTheMutex.unlock();
  }
  bool TryLock() {
    bool bv = mTheMutex.try_lock();
    if (bv) {
      mLockIDs.push(-2);
      miLockCount++;
    }
    return bv;
  }
  void atomicOp(atomicop_t op) {
    Lock();
    op();
    UnLock();
  }
  int GetLockCount() const {
    return miLockCount;
  }
  typedef std::unique_lock<std::recursive_mutex> recursive_scoped_lock;
  const std::string GetName() const {
    return mName;
  }

private:
  std::recursive_mutex mTheMutex;
  std::string mName;
  std::stack<int> mLockIDs;
  int miLockCount;
};
class mutex {
public:
  typedef std::mutex mutex_impl_t;

  mutex(const char* name)
      : mTheMutex()
      , mName(name)
      , miLockID(-1) {
  }
  void Lock(int lid = -1) {
    mTheMutex.lock();
    miLockID = lid;
  }
  void UnLock() {
    mTheMutex.unlock();
  }
  bool TryLock() {
    return mTheMutex.try_lock();
  }

  struct unique_lock {
    typedef std::unique_lock<mutex_impl_t> lock_impl_t;
    unique_lock(ork::mutex& mtx)
        : mLockImpl(mtx.mTheMutex) {
    }
    ~unique_lock() {
    }
    lock_impl_t mLockImpl;
  };

private:
  mutex_impl_t mTheMutex;
  std::string mName;
  int miLockID;
};

}; // namespace ork

///////////////////////////////////////////////////////////////////////////////

namespace ork {

template <typename T> class LockedResource {
  typedef std::function<void(T&)> mutable_atomicop_t;
  typedef std::function<void(const T&)> const_atomicop_t;

  mutable ork::recursive_mutex _mutex;
  std::shared_ptr<T> _resource;

public:
  LockedResource(const char* pname = "ResourceMutex")
      : _mutex(pname) {
    _resource = std::make_shared<T>();
  }
  LockedResource(const LockedResource& oth)
      : _mutex(oth._mutex.GetName().c_str()) {
    _resource = std::make_shared<T>();
  }
  ~LockedResource() {
    _mutex.Lock();
    _resource = nullptr;
    _mutex.UnLock();
  }

  const T& LockForRead(int lid = -1) const {
    _mutex.Lock(lid);
    return (*_resource);
  }
  T& LockForWrite(int lid = -1) {
    _mutex.Lock(lid);
    return (*_resource);
  }
  void UnLock() const {
    _mutex.UnLock();
  }
  int GetLockCount() const {
    return _mutex.GetLockCount();
  }
  void atomicOp(const mutable_atomicop_t& op) {
    LockForWrite();
    op(*_resource);
    UnLock();
  }
  void atomicOp(const const_atomicop_t& op) const {
    LockForRead();
    op(*_resource);
    UnLock();
  }
  void atomicWrite(const T& rhs) {
    LockForWrite();
    (*_resource) = rhs;
    UnLock();
  }
  T atomicCopy() const {
    T rval = LockForRead();
    UnLock();
    return rval;
  }
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct dummy_critsect {
  const char* mname;

  dummy_critsect(const char* name)
      : mname(name) {
  }
  ~dummy_critsect() {
  }

  // for trylock TryEnterCriticalSection

  class standard_lock {
  public:
    dummy_critsect& cs;
    bool mblocked;

    standard_lock(dummy_critsect& c)
        : cs(c)
        , mblocked(false) {
    }

    ~standard_lock() {
      OrkAssert(mblocked == false);
    }

    void Lock(void) {
      mblocked = true;
    }

    void UnLock(void) {
      mblocked = false;
    }

    bool IsLocked(void) const {
      return mblocked;
    }

  private:
    static dummy_critsect gcs;

    standard_lock(const standard_lock& oth)
        : cs(gcs) {
      OrkAssert(false);
    }
  };
};

struct dummy_mutex {
  const char* mname;

  dummy_mutex(const char* name)
      : mname(name) {
  }

  ~dummy_mutex() {
  }

  struct standard_lock {
    dummy_mutex& mMutex;
    bool mblocked;

    standard_lock(dummy_mutex& mtx)
        : mMutex(mtx)
        , mblocked(false) {
      // orkprintf( "standard_lock() Mutex<%08x>\n", & mMutex );
    }

    ~standard_lock() {
      // orkprintf( "~standard_lock() Mutex<%08x>\n", & mMutex );
      OrkAssert(mblocked == false);
    }

    void Lock(void) {
      mblocked = true;
    }

    void UnLock(void) {
      mblocked = false;
    }

    bool IsLocked(void) const {
      return mblocked;
    }
  };

  struct scoped_lock {
    dummy_mutex& mMutex;
    bool mblocked;

    scoped_lock(dummy_mutex& mtx)
        : mMutex(mtx)
        , mblocked(true) {
    }

    ~scoped_lock() {
    }

    bool IsLocked(void) const {
      return mblocked;
    }
  };

  struct try_lock {
    dummy_mutex& mMutex;
    bool mblocked;

    try_lock(dummy_mutex& mtx)
        : mMutex(mtx)
        , mblocked(false) {
    }

    bool lock() {
      bool bret = mblocked == false;
      mblocked  = true;
      return bret;
    }

    void unlock() {
      OrkAssert(mblocked);
      mblocked = false;
    }

    ~try_lock() {
      if (mblocked) {
        unlock();
      }
    }

    bool IsLocked(void) const {
      return mblocked;
    }
  };
};

} // namespace ork
///////////////////////////////////////////////////////////////////////////////
