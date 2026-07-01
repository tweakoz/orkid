#pragma once
////////////////////////////////////////////////////////////////
// RenderPhaseStats — always-on, process-wide per-frame timing sink for the
// render thread. Engine phases (scene gpuUpdate / preRender / compositor assemble
// / composite, dynamic hypermesh + terrain computes, swapchain present-idle)
// record their wall time here; a consumer (the ork.ecs.player perf HUD) reads the
// last committed frame each repaint.
//
// Why a sink and not ork::Profiler: the profiler is compiled out in the default
// build (-DPROFILER=OFF). This is always on. Mirrors the HmPerf idiom but commits
// PER FRAME (HmPerf reports on a 5s window) so the HUD shows live values.
//
// Frame model: phases add() during the render; _renderIMPL calls commit() at the
// end of the frame, swapping the accumulator into the published snapshot. Phases
// that run outside that bracket (present-idle, which is paid after the frame) land
// in the next frame's snapshot — a harmless one-frame offset.
////////////////////////////////////////////////////////////////

#include <map>
#include <string>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <ork/kernel/timer.h>

namespace ork::lev2 {

struct RenderPhaseAccum {
  double ms    = 0.0;
  int    calls = 0;
};

class RenderPhaseStats {
public:
  static RenderPhaseStats& instance() {
    static RenderPhaseStats _i;
    return _i;
  }
  void add(const char* name, double ms) {
    std::lock_guard<std::mutex> lk(_mtx);
    auto& a = _accum[name];
    a.ms += ms;
    a.calls++;
  }
  // publish the accumulated frame and start a fresh one (call once per rendered frame)
  void commit() {
    std::lock_guard<std::mutex> lk(_mtx);
    _published = _accum;
    _accum.clear();
  }
  std::map<std::string, RenderPhaseAccum> snapshot() {
    std::lock_guard<std::mutex> lk(_mtx);
    return _published;
  }

private:
  std::mutex                              _mtx;
  std::map<std::string, RenderPhaseAccum> _accum;
  std::map<std::string, RenderPhaseAccum> _published;
};

////////////////////////////////////////////////////////////////
// CullStats — per-frame GPU-cull result counts (terrain + hypermesh) for the
// perf HUD. Both cull sites read their result buffers back ONLY when enabled
// (the HUD sets it while a stats mode is active) — off = zero readback cost,
// preserving the no-readback cull path. commit() publishes once per frame,
// alongside RenderPhaseStats::commit().
//
// Field semantics mirror the retired [terraincull]/[hmcull] log lines:
//   frustum fail = total - frustum;  occlusion fail (terrain) = frustum - visible;
//   occlusion fail (hyper) = occluded (stored directly). visible = occlusion-pass.
////////////////////////////////////////////////////////////////
struct CullCounts {
  bool     terrain_valid = false;
  uint32_t t_total = 0, t_frustum = 0, t_visible = 0; // visible = occlusion-pass
  bool     hyper_valid = false;
  int      h_variants = 0;
  uint64_t h_total = 0, h_frustum = 0, h_visible = 0, h_occluded = 0;
};

class CullStats {
public:
  static CullStats& instance() {
    static CullStats _i;
    return _i;
  }
  bool enabled() const { return _enabled.load(std::memory_order_relaxed); }
  void setEnabled(bool e) { _enabled.store(e, std::memory_order_relaxed); }
  void addTerrain(uint32_t total, uint32_t frustum, uint32_t visible) {
    std::lock_guard<std::mutex> lk(_mtx);
    _accum.terrain_valid = true;
    _accum.t_total += total;
    _accum.t_frustum += frustum;
    _accum.t_visible += visible;
  }
  void addHyperVariant(uint64_t total, uint64_t frustum, uint64_t visible, uint64_t occluded) {
    std::lock_guard<std::mutex> lk(_mtx);
    _accum.hyper_valid = true;
    _accum.h_variants++;
    _accum.h_total += total;
    _accum.h_frustum += frustum;
    _accum.h_visible += visible;
    _accum.h_occluded += occluded;
  }
  void commit() {
    std::lock_guard<std::mutex> lk(_mtx);
    _published = _accum;
    _accum     = CullCounts{};
  }
  CullCounts snapshot() {
    std::lock_guard<std::mutex> lk(_mtx);
    return _published;
  }

private:
  std::atomic<bool> _enabled{false};
  std::mutex        _mtx;
  CullCounts        _accum, _published;
};

// scoped wall-time timer that feeds a named phase
struct RenderPhaseScope {
  const char* _name;
  uint64_t    _t0;
  RenderPhaseScope(const char* n)
      : _name(n)
      , _t0(Timer::getSystemTick()) {
  }
  ~RenderPhaseScope() {
    double ms = double(Timer::getSystemTick() - _t0) * 1.0e-6; // ns -> ms
    RenderPhaseStats::instance().add(_name, ms);
  }
};

} // namespace ork::lev2
