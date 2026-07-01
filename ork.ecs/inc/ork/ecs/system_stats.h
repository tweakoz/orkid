#pragma once
////////////////////////////////////////////////////////////////
// SystemStats — always-on per-ECS-system timing sink for the perf HUD.
//
// The Simulation ticks systems on TWO threads at different rates (update ~480/s,
// render/gpu ~120/s), so a per-frame commit (like RenderPhaseStats) doesn't fit
// both. Instead each system records a light EMA of its per-tick wall time, keyed
// by "<phase>:<SystemType>" where phase is 'u' (update), 'g' (gpuUpdate), 'r'
// (render). The HUD reads the smoothed latest value each repaint.
//
// Not profiler-gated (the default build is -DPROFILER=OFF). Lock is uncontended
// in practice (one writer thread per phase, one reader).
////////////////////////////////////////////////////////////////

#include <map>
#include <string>
#include <string_view>
#include <mutex>
#include <cstdint>
#include <ork/kernel/timer.h>

namespace ork::ecs {

class SystemStats {
public:
  static SystemStats& instance() {
    static SystemStats _i;
    return _i;
  }
  // phase: 'u' update-thread, 'g' gpuUpdate, 'r' render
  void record(char phase, std::string_view sys, double ms) {
    std::lock_guard<std::mutex> lk(_mtx);
    std::string key;
    key.reserve(sys.size() + 2);
    key.push_back(phase);
    key.push_back(':');
    key.append(sys);
    auto& v = _ms[key];
    v       = (v <= 0.0) ? ms : (v * 0.85 + ms * 0.15); // light EMA to de-jitter
  }
  std::map<std::string, double> snapshot() {
    std::lock_guard<std::mutex> lk(_mtx);
    return _ms;
  }

private:
  std::mutex                    _mtx;
  std::map<std::string, double> _ms;
};

// scoped wall-time timer feeding one (phase, system) cell
struct SystemStatScope {
  char             _phase;
  std::string_view _sys;
  uint64_t         _t0;
  SystemStatScope(char p, std::string_view s)
      : _phase(p)
      , _sys(s)
      , _t0(Timer::getSystemTick()) {
  }
  ~SystemStatScope() {
    SystemStats::instance().record(_phase, _sys, double(Timer::getSystemTick() - _t0) * 1.0e-6);
  }
};

} // namespace ork::ecs
