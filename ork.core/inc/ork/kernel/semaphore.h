////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
  void wait();

private:
  ork::mutex mMutex;
  std::condition_variable mCondition;
  ork::atomic<int> mCount;
};

} // namespace ork
