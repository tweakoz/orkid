////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <thread>
#include <vector>
#include <cstdint>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// AudioJobPool: fixed-size fork/join worker pool for the realtime audio path.
//
//  Why not opq: posting an opq Op copies a std::function (heap allocation)
//  into a mutex-guarded queue serviced by a large general-purpose pool —
//  measured as the dominant source of audio-callback page-fault spikes
//  (cold-start underruns, 2026-07-22). This pool is built around the RT
//  constraints instead:
//
//   - jobs are POD slots (fn pointer + context pointer) owned by the caller;
//     posting never allocates, locks, or copies a closure.
//   - single producer: only the audio thread kicks/joins, once (or twice)
//     per control pass. Workers claim jobs lock-free.
//   - the joining audio thread helps drain the batch, so its wait is
//     productive and bounded by the slowest single job.
//   - idle workers spin briefly (hot within a callback) then sleep via
//     C++20 atomic wait (futex) — zero idle CPU, sub-microsecond wake.
//
//  Claim protocol: _cursor packs {generation:48, next:16} in one atomic
//  word. A claim CASes next+1 only while the generation matches the one the
//  worker observed at wake, so a straggler can never claim into (or count
//  toward) a batch it did not observe — batch state (_jobs/_jobcount/_done)
//  is published before the cursor's release store and never read under a
//  mismatched generation.
///////////////////////////////////////////////////////////////////////////////

struct AudioJob {
  void (*_fn)(void* ctx) = nullptr;
  void* _ctx             = nullptr;
};

struct AudioJobPool {

  AudioJobPool() = default;
  ~AudioJobPool();

  // start numworkers threads (0 = everything runs inline on the caller).
  // not RT-safe; call from setup code.
  void startup(int numworkers);
  void shutdown();

  // fork/join: run jobs[0..count) across workers + the calling thread,
  // return when all have completed. RT-safe. Single producer only —
  // exactly one thread (the audio thread) may call this, and never
  // reentrantly.
  void kickAndJoin(const AudioJob* jobs, int count);

  size_t numWorkers() const { return _threads.size(); }

private:
  static constexpr uint64_t kGenShift = 16;
  static constexpr uint64_t kNextMask = (1ull << kGenShift) - 1;

  void _workerLoop(int index);
  void _drain(uint64_t gen);

  std::atomic<uint64_t> _cursor{0};   // {generation:48, next:16}
  std::atomic<const AudioJob*> _jobs{nullptr};
  std::atomic<int> _jobcount{0};
  std::atomic<int> _done{0};
  std::atomic<bool> _running{false};
  std::vector<std::thread> _threads;

  AudioJobPool(const AudioJobPool&)            = delete;
  AudioJobPool& operator=(const AudioJobPool&) = delete;
};

} // namespace ork::audio::singularity
