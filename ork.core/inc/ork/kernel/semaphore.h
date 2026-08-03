////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/atomic.h>
#include <ork/kernel/mutex.h>
#include <pthread.h>

namespace ork {

struct condition_variable {
  condition_variable(bool do_init = false);

  void init();
  void notify_one();
  void notify_all();
  int wait(unsigned long usec);

private:
  pthread_mutex_t mMutex;
  pthread_cond_t mCondVar;
  ork::atomic<int64_t> mWaitCount;
  ork::atomic<int64_t> mReleaseCount;
};

struct semaphore {
  semaphore(const char* name);
  void notify();
  // RT-safe notify: atomic count bump + condvar signal WITHOUT taking mMutex,
  // so it can never block the caller (use from audio/realtime threads). The
  // unheld signal can lose the race with a waiter just entering its wait; that
  // is only usable against wait_for, whose deadline re-check observes the
  // count — worst case the wake arrives at the timeout instead of instantly.
  void notify_rt();
  void wait();
  // timed wait: true = signaled (count consumed), false = timed out (count untouched).
  // Lets a worker block on real work (instant wake on notify) with a bounded backstop
  // against any notify-less state change — replaces poll-sleep loops (OpqThread::run).
  bool wait_for(uint64_t usec);

private:
  ork::mutex mMutex;
  std::condition_variable mCondition;
  ork::atomic<int> mCount;
};

} // namespace ork
