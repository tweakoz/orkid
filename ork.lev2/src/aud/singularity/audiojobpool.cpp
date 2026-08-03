////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/audiojobpool.h>
#include <pthread.h>
#include <cstdio>

namespace ork::audio::singularity {

static inline void cpu_relax() {
#if defined(__x86_64__)
  __builtin_ia32_pause();
#elif defined(__aarch64__)
  asm volatile("yield" ::: "memory");
#else
  std::this_thread::yield();
#endif
}

///////////////////////////////////////////////////////////////////////////////

AudioJobPool::~AudioJobPool() {
  shutdown();
}

///////////////////////////////////////////////////////////////////////////////

void AudioJobPool::startup(int numworkers) {
  shutdown();
  if (numworkers <= 0)
    return;
  _running.store(true, std::memory_order_release);
  for (int i = 0; i < numworkers; i++)
    _threads.emplace_back([this, i]() { _workerLoop(i); });
}

///////////////////////////////////////////////////////////////////////////////

void AudioJobPool::shutdown() {
  if (_threads.empty())
    return;
  _running.store(false, std::memory_order_release);
  // bump the generation so sleeping workers wake and observe !_running.
  // no batch can be active here: the producer (audio stream) must already
  // be stopped by teardown ordering (see OrkEzApp::_audioExit).
  _cursor.fetch_add(1ull << kGenShift, std::memory_order_release);
  _cursor.notify_all();
  for (auto& t : _threads)
    t.join();
  _threads.clear();
}

///////////////////////////////////////////////////////////////////////////////

void AudioJobPool::_drain(uint64_t gen) {
  const AudioJob* jobs = _jobs.load(std::memory_order_relaxed);
  const int count      = _jobcount.load(std::memory_order_relaxed);
  uint64_t cur         = _cursor.load(std::memory_order_acquire);
  while (true) {
    if ((cur >> kGenShift) != gen)
      return; // batch changed under us — stale claims are forbidden
    uint64_t next = cur & kNextMask;
    if (int(next) >= count)
      return; // batch fully claimed
    if (_cursor.compare_exchange_weak(cur, cur + 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
      jobs[next]._fn(jobs[next]._ctx);
      _done.fetch_add(1, std::memory_order_release);
      cur = _cursor.load(std::memory_order_acquire);
    }
    // CAS failure reloaded cur
  }
}

///////////////////////////////////////////////////////////////////////////////

void AudioJobPool::_workerLoop(int index) {
  {
    char name[16];
    snprintf(name, sizeof(name), "SingulJob%d", index);
#if defined(__APPLE__)
    pthread_setname_np(name);
#else
    pthread_setname_np(pthread_self(), name);
#endif
    // best-effort realtime priority — these threads execute audio-callback
    // work. failing without privilege is fine (normal priority still works).
    sched_param sp{};
    sp.sched_priority = 40;
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
  }

  uint64_t seen = _cursor.load(std::memory_order_acquire);
  while (_running.load(std::memory_order_acquire)) {
    // batches arrive back-to-back within one device callback, then pause
    // until the next callback: spin briefly while hot, then futex-sleep.
    uint64_t cur = _cursor.load(std::memory_order_acquire);
    if (cur == seen) {
      int spins = 0;
      while (((cur = _cursor.load(std::memory_order_acquire)) == seen) //
             and _running.load(std::memory_order_acquire)) {
        if (++spins > 4000) {
          _cursor.wait(seen, std::memory_order_acquire);
          cur = _cursor.load(std::memory_order_acquire);
          break;
        }
        cpu_relax();
      }
    }
    if (not _running.load(std::memory_order_acquire))
      break;
    _drain(cur >> kGenShift);
    seen = _cursor.load(std::memory_order_acquire);
  }
}

///////////////////////////////////////////////////////////////////////////////

void AudioJobPool::kickAndJoin(const AudioJob* jobs, int count) {
  if (count <= 0)
    return;
  if (_threads.empty() or (count == 1)) {
    // no workers (not started, or shut down) — degrade to inline execution.
    // this keeps every teardown/bringup window correct, just serial.
    for (int i = 0; i < count; i++)
      jobs[i]._fn(jobs[i]._ctx);
    return;
  }
  _jobs.store(jobs, std::memory_order_relaxed);
  _jobcount.store(count, std::memory_order_relaxed);
  _done.store(0, std::memory_order_relaxed);
  // publish: new generation, next=0. the release store orders the batch
  // state above before any worker can observe the new generation.
  uint64_t cur    = _cursor.load(std::memory_order_relaxed);
  uint64_t newgen = (cur >> kGenShift) + 1;
  _cursor.store(newgen << kGenShift, std::memory_order_release);
  _cursor.notify_all();
  // help drain the batch, then wait out stragglers — bounded by the
  // slowest single job since we just helped empty the queue.
  _drain(newgen);
  while (_done.load(std::memory_order_acquire) < count)
    cpu_relax();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
