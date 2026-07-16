////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/opq.h>
#include <ork/kernel/async_tracker.h>
#include <ork/asset/Asset.h>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <mutex>
#include <vector>

namespace ork::lev2 {

struct Context;

///////////////////////////////////////////////////////////////////////////////
// LoadJoinSet — the LOADX WS1 spawn/join primitive for load-time work.
//
// spawn*() tickets work onto the opq worker/io pools (tagged with the async
// tracker so the player's settle logic sees it); adopt() tracks an asset
// LoadRequest's partial-load counter. join(ctx) blocks the CALLING thread but
// NEVER blindly: it PUMPS ctx's deferred ops and loading phases while waiting,
// so GPU-upload phases spawned by the workers keep flowing even though the
// frame loop isn't running (GPU hooks fire OUTSIDE beginFrame — the LOADX §1.1
// caveat; a blind wait here is the exact shape of the 2026-07-02 shutdown
// deadlock). Generalizes CommonStuff::requestRadianceMapsSync's hand-rolled
// loop — the ONE place this pattern previously existed.
//
// Contracts:
//  * join(ctx) must run on ctx's owning thread (typically the render thread,
//    inside a GPU-phase rendezvous).
//  * spawned fns must not touch ctx directly — GPU work goes through ctx
//    loading phases / deferred ops (the fence-gated publish pattern; see
//    ork.dox/vk_deferred_updates_postmortem.md).
//  * the set is one-shot per load wave: spawn..spawn..join. Re-arm after join
//    is permitted (counters return to zero) but not concurrent with it.
///////////////////////////////////////////////////////////////////////////////

struct LoadJoinSet {

  explicit LoadJoinSet(std::string name);
  ~LoadJoinSet();

  // CPU-heavy work (decode/parse/cook) -> concurrent worker pool
  void spawnOnWorkers(std::function<void()> fn);
  // disk-bound work (file reads) -> io pool
  void spawnOnIO(std::function<void()> fn);
  // track an asset LoadRequest: resolved when its partial-load counter drains
  void adopt(asset::loadrequest_ptr_t req);
  // manual ticket for externally-managed async — pair with release()
  void acquire();
  void release();

  // true while any ticket, spawned fn, or adopted request is unresolved
  bool pending() const;

  // pump ctx (deferred ops + loading phases) until pending()==false.
  // Must run on ctx's owning thread.
  void join(Context* ctx);

  std::string _name;
  std::atomic<int> _pending{0};
  std::vector<asset::loadrequest_ptr_t> _adopted;
  mutable std::mutex _adopted_mutex;

  // The async-tracker marker (asyncWorkBegin/End on _name) is HELD across a live
  // load wave and released at join() completion — NOT pinned to object lifetime.
  // A dtor-only release breaks the sim, which keeps its LoadJoinSet as a member
  // (never destructed until teardown): the settle gate that waits on
  // asyncWorkPending() would then never see it clear. _ensureMarker re-acquires
  // on a re-armed wave (spawn/adopt after join) so the documented re-arm contract
  // keeps its async visibility.
  std::atomic<bool> _marker_active{true};
  void _ensureMarker();
};

using loadjoinset_ptr_t = std::shared_ptr<LoadJoinSet>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
