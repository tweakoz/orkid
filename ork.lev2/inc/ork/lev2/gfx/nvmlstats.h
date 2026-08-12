#pragma once
////////////////////////////////////////////////////////////////
// NvmlStats — NVIDIA driver telemetry (clocks / utilization / power / temperature /
// throttle reasons) as one more always-on sink beside GpuPassStats, for the perf HUD's
// GPU page. It answers the question the engine's own timers cannot: WHY a frame's
// device time moved. A frame that got slower at a constant workload while nv:clk fell
// and nv:throttle went from "none" to "hwtherm" was not a rendering regression.
//
// PLATFORM CAPABILITY, NOT A FEATURE. The library (libnvidia-ml.so.1) is dlopen'd at
// first use; on macOS or an AMD box there is nothing to open and the sink stays
// unavailable, which makes the HUD's nvml rows simply absent. That is the correct
// reading of a machine with no NVIDIA driver — there is no wanted feature failing, so
// nothing here is loud (contrast the engine's fail-loud law, which covers features the
// caller asked for).
//
// ONE POLLER, 1 Hz. Every NVML query is a driver round trip of a few hundred us, so
// they are never made from the render thread: a single background thread refreshes a
// mutex-guarded snapshot once a second (these quantities are thermal / power controlled
// and do not move meaningfully faster), and readers only ever copy the snapshot.
// nvmlDeviceGetPcieThroughput is deliberately NOT queried — it blocks ~20ms.
//
// The poller starts on the FIRST snapshot() and is joined at process exit, so a run
// that never reads the sink never spawns the thread (headless teardown law: no stray
// threads at exit).
////////////////////////////////////////////////////////////////

#include <string>

namespace ork::lev2 {

struct NvmlSnapshot {
  bool        _live     = false; // NVML loaded, initialized, and a device polled at least once
  int         _sm_mhz   = 0;
  int         _mem_mhz  = 0;
  int         _util_gpu = 0; // percent of the last sample period with any kernel resident
  int         _util_mem = 0; // percent of it with memory read/write activity
  double      _power_w  = 0.0;
  int         _temp_c   = 0;
  std::string _throttle; // decoded reasons, space separated; "none" when unthrottled
};

class NvmlStats {
public:
  static NvmlStats& instance();
  // Copy of the last poll. STARTS the poller (and loads NVML) the first time it is
  // called; an unavailable NVML answers a default snapshot (_live=false) forever after,
  // at the cost of one atomic load.
  NvmlSnapshot snapshot();
  // Stop + join the poller and shut NVML down. Idempotent; also wired to process exit.
  void shutdown();

private:
  NvmlStats();
  ~NvmlStats();
  NvmlStats(const NvmlStats&)            = delete;
  NvmlStats& operator=(const NvmlStats&) = delete;
  struct Impl;
  Impl* _impl = nullptr;
};

} // namespace ork::lev2
