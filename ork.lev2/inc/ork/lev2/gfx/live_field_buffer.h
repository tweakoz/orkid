////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// live_field_buffer.h — S4 progressive-display primitive (JUL13_DFLOW §E5/S4).
//
// LiveFieldBuffer is the "LiveOutput": a double-buffered, frame-coherent, HOST-side
// whole-plane publisher a cook driver flips at each viewable-node checkpoint, plus a
// process-wide registry that names each buffer by its PRODUCT PATH (the join key both
// the bake — capture path — and the consumers — manifest channel file — already share).
//
// DESIGN (from the three cited precedents, JUL13 §S4):
//  * concurrent_triple_buffer (ork/util/triple_buffer.h): the host-side swap discipline.
//    TWO planes suffice here (not three) because a reader's copy-out runs entirely
//    UNDER the swap lock while the writer fills only the BACK plane outside it — the
//    plane being read is never the plane being written, by construction.
//  * radiance pointer-swap: publish is an O(1) index flip + generation bump under the
//    lock — a consumer either sees the complete previous plane or the complete new one,
//    NEVER a half-updated plane (the frame-coherence law).
//  * enqueueDelayedDestroy / deferred destroy: NOT NEEDED — deliberately. The planes are
//    host memory owned by the shared registry entry; no GPU buffer is ever published, so
//    nothing retires while in flight. The MoltenVK-class read-while-write risk on a GPU
//    double buffer is dodged STRUCTURALLY: the producer copies out of the cook SSBO only
//    right after that node's endDispatchPhase (submit + WAIT — the same point the
//    incremental capture flush already reads back at), so there is no mid-phase readback
//    and no GPU reader can be in flight against a plane being replaced.
//
// The buffer is owned at BakeEnv level OUTSIDE the register pool (BakeEnv::_live_out) —
// register-pool reuse never touches it, and it stays valid from the first publish
// through markFinal() and across bakes (generation is monotonic per artifact).
//
// Artifact lifecycle: absent -> live (beginBake) -> final (markFinal). The RENDERER may
// consume live generations; PHYSICS / SCATTER require final and HOLD their last final
// while isLive() (see BulletTerrainImpl::consumePendingReload).
//
// Kill-switch: ORKID_S4_DISABLE=1 reverts every S4 site to on_complete behavior.
//
////////////////////////////////////////////////////////////////
#pragma once

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ork::lev2 {

struct LiveFieldBuffer {

  // ---- producer side (the cook driver) --------------------------------------
  // absent -> live: a bake armed this artifact. Generation is NOT reset (monotonic
  // per artifact so consumers' last-seen cursors stay valid across bakes).
  void beginBake() {
    std::lock_guard<std::mutex> lk(_mutex);
    _final = false;
    _live  = true;
  }
  // whole-plane publish: copy `w*h` floats into the BACK plane, then flip + bump.
  // The copy happens OUTSIDE the lock (the back plane is never read — reads only
  // ever touch the front plane, under the lock); only the flip is locked.
  void publish(int w, int h, const float* src) {
    auto& back = _planes[_back];
    back.assign(src, src + size_t(w) * size_t(h));
    std::lock_guard<std::mutex> lk(_mutex);
    _w     = w;
    _h     = h;
    _front = _back;
    _back  = 1 - _back;
    _generation.fetch_add(1, std::memory_order_release);
  }
  // zero-extra-copy publish pair: beginPublish hands out the BACK plane (resized) for
  // the producer to fill DIRECTLY (e.g. an SSBO readback target); endPublish flips +
  // bumps and returns the new generation. Safe outside the lock — the back plane is
  // never read (reads only ever touch the front plane, under the lock).
  float* beginPublish(int w, int h) {
    auto& back = _planes[_back];
    back.resize(size_t(w) * size_t(h));
    return back.data();
  }
  uint64_t endPublish(int w, int h) {
    std::lock_guard<std::mutex> lk(_mutex);
    _w     = w;
    _h     = h;
    _front = _back;
    _back  = 1 - _back;
    return _generation.fetch_add(1, std::memory_order_release) + 1;
  }

  // live -> final: the bake's flush ran; the on-disk product is now the truth and
  // held-back consumers (physics) may rebind exactly once, now.
  void markFinal() {
    std::lock_guard<std::mutex> lk(_mutex);
    _final = true;
    _live  = false;
  }

  // ---- consumer side --------------------------------------------------------
  // cheap "anything new?" peek (no lock).
  uint64_t generation() const {
    return _generation.load(std::memory_order_acquire);
  }
  bool isLive() const { // an ACTIVE (armed, not yet final) bake exists for this artifact
    std::lock_guard<std::mutex> lk(_mutex);
    return _live;
  }
  bool isFinal() const {
    std::lock_guard<std::mutex> lk(_mutex);
    return _final;
  }
  // if a generation newer than `seen` is published, invoke fn(w, h, plane, gen)
  // UNDER the lock (copy out; keep it short) and return the consumed generation;
  // else return `seen`. The plane handed to fn is always a COMPLETE publish.
  uint64_t consume(uint64_t seen, const std::function<void(int, int, const float*, uint64_t)>& fn) const {
    std::lock_guard<std::mutex> lk(_mutex);
    uint64_t gen = _generation.load(std::memory_order_acquire);
    if (gen == seen or _front < 0)
      return seen;
    fn(_w, _h, _planes[_front].data(), gen);
    return gen;
  }

private:
  mutable std::mutex _mutex;
  std::vector<float> _planes[2];
  int _front = -1; // no publish yet ("valid from frame 0" = consumers hold last final until gen moves)
  int _back  = 0;
  int _w = 0, _h = 0;
  bool _live  = false;
  bool _final = false;
  std::atomic<uint64_t> _generation{0};
};

using live_field_buffer_ptr_t = std::shared_ptr<LiveFieldBuffer>;

///////////////////////////////////////////////////////////////////////////////
// registry — named live artifacts, keyed by the canonical product path.
///////////////////////////////////////////////////////////////////////////////

inline std::string liveFieldCanonicalKey(const std::string& path) {
  std::error_code ec;
  auto canon = std::filesystem::weakly_canonical(std::filesystem::path(path), ec);
  return ec ? std::filesystem::path(path).lexically_normal().string() : canon.string();
}

namespace live_field_detail {
struct Registry {
  std::mutex _mutex;
  std::unordered_map<std::string, live_field_buffer_ptr_t> _map;
  static Registry& instance() {
    static Registry g;
    return g;
  }
};
} // namespace live_field_detail

// get-or-create the named artifact (producer arm / consumer pre-arm both use this).
inline live_field_buffer_ptr_t liveFieldAcquire(const std::string& key) {
  auto& reg = live_field_detail::Registry::instance();
  std::lock_guard<std::mutex> lk(reg._mutex);
  auto& slot = reg._map[key];
  if (not slot)
    slot = std::make_shared<LiveFieldBuffer>();
  return slot;
}

// find-only (physics hold gate: an ABSENT artifact must behave like "not live").
inline live_field_buffer_ptr_t liveFieldFind(const std::string& key) {
  auto& reg = live_field_detail::Registry::instance();
  std::lock_guard<std::mutex> lk(reg._mutex);
  auto it = reg._map.find(key);
  return (it != reg._map.end()) ? it->second : nullptr;
}

// ORKID_S4_DISABLE=1 — the S4 kill-switch (every site checks this ONE predicate).
inline bool s4ProgressiveDisabled() {
  static const bool s_disabled = (std::getenv("ORKID_S4_DISABLE") != nullptr);
  return s_disabled;
}

} // namespace ork::lev2
