#pragma once
////////////////////////////////////////////////////////////////
// GpuPassStats — always-on, process-wide sink for PER-PASS GPU time, measured on
// the device with timestamp queries (MT1 subdivision of the MT0 whole-frame span,
// see gpumicrotask.h). RenderPhaseStats is the CPU-wall sibling of this file; the
// two answer different questions and neither substitutes for the other (a phase
// whose CPU wall is 0.2ms can own 6ms of GPU).
//
// Written by the backend's timestamp readback (VkGpuSliceTimer::readbackSlices),
// read by the ork.ecs.player perf HUD's GPU page. NOT profiler-gated: the
// compiled-out ork::Profiler is unavailable in the default build.
//
// FRAME LAG is part of the reading, not an implementation detail: the readback
// never waits on the GPU (a waiting read wedged RADV/amdgpu — see the ring
// rationale at VkGpuSliceTimer), so the published numbers are always a couple of
// frames old. _lag_frames says how old, and the HUD shows it.
//
// SEGMENT SUMMING: a pass name may bracket several device-side segments in one
// frame (a render pass instance that is suspended and resumed, an rtgroup pushed
// twice, two XR eye blits). Every segment adds into its name, so a row is the
// frame's TOTAL GPU time under that name — which is the number a budget is spent
// against.
////////////////////////////////////////////////////////////////

#include <chrono>
#include <cstdlib>
#include <deque>
#include <map>
#include <mutex>
#include <string>

namespace ork::lev2 {

struct GpuPassSample {
  double _ms       = 0.0; // last committed frame's total for this name
  double _ms_ema   = 0.0; // de-jittered (same light EMA the ECS system rows use)
  int    _segments = 0;   // device-side brackets that summed into _ms
};

struct GpuPassSnapshot {
  std::map<std::string, GpuPassSample> _passes;
  float  _gpu_frame_ms  = -1.0f; // MT0 whole-frame span; -1 = no reading this frame
  int    _lag_frames    = 0;     // how many frames old _passes is (see header)
  int    _dropped       = 0;     // slices past the per-frame slot budget (loud, not silent)
  bool   _calibrated    = false; // VK_EXT_calibrated_timestamps active on this device
  double _cal_offset_ns = 0.0;   // gpu-domain -> CLOCK_MONOTONIC offset, when calibrated
};

class GpuPassStats {
public:
  static GpuPassStats& instance() {
    static GpuPassStats _i;
    return _i;
  }
  // one device-side segment, ms. Accumulates into the in-progress frame.
  void add(const std::string& name, double ms) {
    std::lock_guard<std::mutex> lk(_mtx);
    auto& a = _accum[name];
    a._ms += ms;
    a._segments++;
  }
  // publish the accumulated frame (called once per readback, from the backend).
  void commit(float gpu_frame_ms, int lag_frames, int dropped, bool calibrated, double cal_offset_ns) {
    std::lock_guard<std::mutex> lk(_mtx);
    for (auto& kv : _accum) {
      double prev  = _ema[kv.first];
      double ema   = (prev <= 0.0) ? kv.second._ms : (prev * 0.85 + kv.second._ms * 0.15);
      _ema[kv.first]      = ema;
      kv.second._ms_ema   = ema;
    }
    _published._passes        = _accum;
    _published._gpu_frame_ms  = gpu_frame_ms;
    _published._lag_frames    = lag_frames;
    _published._dropped       = dropped;
    _published._calibrated    = calibrated;
    _published._cal_offset_ns = cal_offset_ns;
    _accum.clear();
  }
  GpuPassSnapshot snapshot() {
    std::lock_guard<std::mutex> lk(_mtx);
    return _published;
  }
  // just the whole-frame span. The VR pacing sink needs it every XR frame to compute
  // headroom, and copying the whole pass map for one float is waste.
  float gpuFrameMs() {
    std::lock_guard<std::mutex> lk(_mtx);
    return _published._gpu_frame_ms;
  }

private:
  std::mutex                           _mtx;
  std::map<std::string, GpuPassSample> _accum;
  std::map<std::string, double>        _ema; // survives commit (the published copy is per-frame)
  GpuPassSnapshot                      _published;
};

////////////////////////////////////////////////////////////////
// VrPacingStats — the XR frame-deadline sibling of the two sinks above: what the
// RUNTIME's pacing looks like from inside our frame. Written once per XR frame by
// the OpenXR device's gpuUpdate instrumentation (which owns the xrWaitFrame block
// and the frame state), read by the perf HUD's GPU page.
//
// _live IS the VR-presence answer for a consumer: only the live XR frame path ever
// publishes here, so a non-VR run leaves this sink untouched and the HUD shows no
// pacing rows at all. That absence is correct — VR pacing is not a degraded reading
// of a desktop run, it is a reading that does not exist there.
//
// MISSED FRAMES are counted over a rolling ONE SECOND (a per-frame boolean is
// meaningless to a reader and a lifetime total hides whether it is happening NOW).
// The miss test itself belongs to the publisher: it is the runtime's own
// predictedDisplayTime discontinuity, not something derivable here.
//
// HEADROOM is the budget question — target minus the frame's DEVICE time (GpuPassStats
// MT0), where the target is the per-eye deadline the seat is aiming at
// (ORKID_VR_FRAME_TARGET_MS, default 8.0 = a 90Hz-class headset with the runtime's own
// compositor slice left unspent). It is only valid on frames the GPU timer read back;
// _headroom_valid says so rather than publishing a plausible lie.
////////////////////////////////////////////////////////////////

struct VrPacingSnapshot {
  bool   _live           = false; // an XR frame published at least once (see header)
  double _wait_ms        = 0.0;   // last frame's xrWaitFrame block
  double _wait_ms_ema    = 0.0;   // de-jittered (same light EMA as the pass rows)
  double _period_ms      = 0.0;   // XrFrameState::predictedDisplayPeriod
  int    _missed_1s      = 0;     // display-period discontinuities in the last second
  double _target_ms      = 0.0;   // ORKID_VR_FRAME_TARGET_MS
  double _headroom_ms    = 0.0;   // _target_ms - gpu frame ms
  bool   _headroom_valid = false; // false = the GPU timer had no reading this frame
};

class VrPacingStats {
public:
  static VrPacingStats& instance() {
    static VrPacingStats _i;
    return _i;
  }
  // one XR frame. `missed` = the runtime skipped a display period (the publisher's test).
  void publish(double wait_ms, double period_ms, bool missed) {
    auto                        now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lk(_mtx);
    if (missed)
      _missed.push_back(now);
    while (not _missed.empty() and (now - _missed.front()) > std::chrono::seconds(1))
      _missed.pop_front();
    double prev             = _published._wait_ms_ema;
    _published._live        = true;
    _published._wait_ms     = wait_ms;
    _published._wait_ms_ema = (prev <= 0.0) ? wait_ms : (prev * 0.85 + wait_ms * 0.15);
    _published._period_ms   = period_ms;
    _published._missed_1s   = int(_missed.size());
    _published._target_ms   = targetMs();
    float gpu_ms            = GpuPassStats::instance().gpuFrameMs();
    _published._headroom_valid = (gpu_ms >= 0.0f);
    _published._headroom_ms    = _published._headroom_valid ? (_published._target_ms - double(gpu_ms)) : 0.0;
  }
  VrPacingSnapshot snapshot() {
    std::lock_guard<std::mutex> lk(_mtx);
    return _published;
  }
  // per-eye GPU deadline the seat aims at, ms. Read once — a mid-run change of the
  // target would make the headroom column mean two different things down its history.
  static double targetMs() {
    static double _t = []() -> double {
      auto   e = getenv("ORKID_VR_FRAME_TARGET_MS");
      double v = e ? atof(e) : 0.0;
      return (v > 0.0) ? v : 8.0;
    }();
    return _t;
  }

private:
  std::mutex                                          _mtx;
  VrPacingSnapshot                                    _published;
  std::deque<std::chrono::steady_clock::time_point>   _missed; // rolling 1s window
};

} // namespace ork::lev2
