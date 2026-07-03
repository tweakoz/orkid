////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/loadjoinset.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/kernel/thread.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

LoadJoinSet::LoadJoinSet(std::string name)
    : _name(std::move(name)) {
  asyncWorkBegin(_name); // player settle logic sees the whole set as one unit
}

LoadJoinSet::~LoadJoinSet() {
  asyncWorkEnd(_name);
}

///////////////////////////////////////////////////////////////////////////////

void LoadJoinSet::acquire() {
  _pending.fetch_add(1);
}
void LoadJoinSet::release() {
  int prev = _pending.fetch_sub(1);
  OrkAssert(prev > 0); // over-release = a spawn fn released twice
}

///////////////////////////////////////////////////////////////////////////////

void LoadJoinSet::spawnOnWorkers(std::function<void()> fn) {
  acquire();
  opq::concurrentQueue()->enqueue([this, fn]() {
    fn();
    release();
  }, _name);
}

void LoadJoinSet::spawnOnIO(std::function<void()> fn) {
  acquire();
  opq::ioQueue()->enqueue([this, fn]() {
    fn();
    release();
  }, _name);
}

///////////////////////////////////////////////////////////////////////////////

void LoadJoinSet::adopt(asset::loadrequest_ptr_t req) {
  if (nullptr == req)
    return;
  std::lock_guard<std::mutex> lk(_adopted_mutex);
  _adopted.push_back(req);
}

///////////////////////////////////////////////////////////////////////////////

bool LoadJoinSet::pending() const {
  if (_pending.load() > 0)
    return true;
  std::lock_guard<std::mutex> lk(_adopted_mutex);
  for (auto& req : _adopted)
    if (req->_partial_load_counter.load() > 0)
      return true;
  return false;
}

///////////////////////////////////////////////////////////////////////////////

void LoadJoinSet::join(Context* ctx) {
  while (pending()) {
    if (ctx) {
      ctx->processDeferredOps();      // fence-gated publishes (poll/swap ops)
      ctx->pumpLoadingPhases();       // GPU-upload phases (30ms budget per call)
    }
    ork::usleep(500);
  }
  // one final drain: workers may have enqueued phases/ops that resolved the
  // tickets ABOVE but still carry the actual GPU publish — flush them so the
  // caller sees fully-resident results at return, not next frame.
  if (ctx) {
    ctx->processDeferredOps();
    ctx->pumpLoadingPhases();
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
