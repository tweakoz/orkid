////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
// NVML telemetry poller — see nvmlstats.h for what this sink is for and why its
// absence is silent.
//
// ZERO BUILD DEPENDENCY is deliberate: NVML's headers and import library are part of
// the CUDA toolkit, which this engine does not require, and linking them would make a
// GPU vendor a build prerequisite. The five entry points we want have a stable C ABI, so
// they are declared locally and resolved out of libnvidia-ml.so.1 (shipped BY THE DRIVER,
// not the toolkit) at run time. The declarations below must match NVML's own; the
// enumerant values are pinned by NVML's ABI compatibility promise.
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/nvmlstats.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

#include <ork/kernel/thread.h>

#if defined(__linux__)
#include <dlfcn.h>
#define ORK_NVML_ENABLED 1
#endif

namespace ork::lev2 {

////////////////////////////////////////////////////////////////

#if defined(ORK_NVML_ENABLED)
namespace {

using nvmlDevice_t = void*; // opaque handle (NVML: struct nvmlDevice_st*)

struct nvmlUtilization_t {
  unsigned int gpu;    // percent of the sample period with any kernel resident
  unsigned int memory; // percent of it with memory read/write activity
};

enum {
  NVML_CLOCK_GRAPHICS  = 0,
  NVML_CLOCK_SM        = 1,
  NVML_CLOCK_MEM       = 2,
  NVML_TEMPERATURE_GPU = 0,
};

// nvmlClocksThrottleReason bits, paired with the short tags the HUD row shows. Order is
// the order a reader wants them in: what is limiting us first.
struct ThrottleBit {
  unsigned long long _bit;
  const char*        _tag;
};
static const ThrottleBit kThrottleBits[] = {
    {0x0000000000000008ULL, "hwslow"},  // hardware slowdown (any of the three below)
    {0x0000000000000040ULL, "hwtherm"}, // thermal, enforced in hardware
    {0x0000000000000080ULL, "hwbrake"}, // external power brake
    {0x0000000000000020ULL, "swtherm"}, // thermal, enforced by the driver
    {0x0000000000000004ULL, "swpwr"},   // driver power cap
    {0x0000000000000010ULL, "sync"},    // sync boost group
    {0x0000000000000002ULL, "appclk"},  // application clock setting
    {0x0000000000000100ULL, "dispclk"}, // display clock setting
    {0x0000000000000001ULL, "idle"},    // nothing to do (not a limit — but it explains a low clock)
};

using pfn_init_t        = int (*)(void);
using pfn_shutdown_t    = int (*)(void);
using pfn_handle_t      = int (*)(unsigned int, nvmlDevice_t*);
using pfn_clock_t       = int (*)(nvmlDevice_t, int, unsigned int*);
using pfn_util_t        = int (*)(nvmlDevice_t, nvmlUtilization_t*);
using pfn_power_t       = int (*)(nvmlDevice_t, unsigned int*);
using pfn_temp_t        = int (*)(nvmlDevice_t, int, unsigned int*);
using pfn_throttle_t    = int (*)(nvmlDevice_t, unsigned long long*);

} // namespace
#endif

////////////////////////////////////////////////////////////////

struct NvmlStats::Impl {
  std::mutex   _mtx;  // guards _snap
  NvmlSnapshot _snap; // last poll (default = unavailable)

  std::atomic<bool>       _started{false};
  std::atomic<bool>       _stop{false};
  std::thread             _thread;
  std::mutex              _cvmtx;
  std::condition_variable _cv; // stop signal; also the 1Hz cadence's sleep

#if defined(ORK_NVML_ENABLED)
  void*        _lib    = nullptr;
  nvmlDevice_t _dev    = nullptr;
  pfn_shutdown_t _shutdown = nullptr;
  pfn_clock_t    _clock    = nullptr;
  pfn_util_t     _util     = nullptr;
  pfn_power_t    _power    = nullptr;
  pfn_temp_t     _temp     = nullptr;
  pfn_throttle_t _throttle = nullptr;
#endif

  //////////////////////////////////////////////////////////////

  void start();
  void stop();
  void poll();
};

////////////////////////////////////////////////////////////////

namespace {
void nvmlAtExit() {
  NvmlStats::instance().shutdown();
}
} // namespace

////////////////////////////////////////////////////////////////

void NvmlStats::Impl::start() {
  bool expected = false;
  if (not _started.compare_exchange_strong(expected, true))
    return; // another reader already brought it up (or found it unavailable)

#if defined(ORK_NVML_ENABLED)
  // RTLD_LOCAL: nothing else in the process should inherit these symbols. Absent
  // library = an NVIDIA-less machine; nothing to report and nothing to say about it.
  _lib = dlopen("libnvidia-ml.so.1", RTLD_NOW | RTLD_LOCAL);
  if (not _lib)
    return;
  auto sym = [this](const char* n) { return dlsym(_lib, n); };
  auto init     = (pfn_init_t)sym("nvmlInit_v2");
  auto handle   = (pfn_handle_t)sym("nvmlDeviceGetHandleByIndex_v2");
  _shutdown     = (pfn_shutdown_t)sym("nvmlShutdown");
  _clock        = (pfn_clock_t)sym("nvmlDeviceGetClockInfo");
  _util         = (pfn_util_t)sym("nvmlDeviceGetUtilizationRates");
  _power        = (pfn_power_t)sym("nvmlDeviceGetPowerUsage");
  _temp         = (pfn_temp_t)sym("nvmlDeviceGetTemperature");
  _throttle     = (pfn_throttle_t)sym("nvmlDeviceGetCurrentClocksThrottleReasons");
  // A driver library that loads but does not carry the v2 init / device lookup is not
  // an NVML we know how to drive. The per-metric pointers are allowed to be null
  // individually (poll() skips what it cannot ask for).
  if (not init or not handle or init() != 0 or handle(0, &_dev) != 0 or not _dev) {
    dlclose(_lib);
    _lib = nullptr;
    return;
  }
  // Registered HERE — after the dlopen — so glibc's LIFO exit ordering runs it BEFORE
  // the driver library unwinds (the same ordering constraint the lev2 loader thread's
  // atexit is placed for). The singleton's dtor is a second net for a teardown that
  // never reaches the exit handlers.
  std::atexit(nvmlAtExit);

  poll(); // one synchronous poll so the first HUD frame already has real numbers

  _thread = std::thread([this]() {
    ork::SetCurrentThreadName("nvml");
    while (not _stop.load(std::memory_order_acquire)) {
      std::unique_lock<std::mutex> lk(_cvmtx);
      // wait_for, not sleep_for: a shutdown joins in microseconds instead of
      // blocking a headless teardown for up to a second.
      _cv.wait_for(lk, std::chrono::seconds(1), [this]() { return _stop.load(std::memory_order_acquire); });
      lk.unlock();
      if (_stop.load(std::memory_order_acquire))
        break;
      poll();
    }
  });
#endif
}

////////////////////////////////////////////////////////////////

void NvmlStats::Impl::poll() {
#if defined(ORK_NVML_ENABLED)
  NvmlSnapshot s;
  s._live = true;
  unsigned int u32 = 0;
  if (_clock and _clock(_dev, NVML_CLOCK_SM, &u32) == 0)
    s._sm_mhz = int(u32);
  if (_clock and _clock(_dev, NVML_CLOCK_MEM, &u32) == 0)
    s._mem_mhz = int(u32);
  nvmlUtilization_t util{0, 0};
  if (_util and _util(_dev, &util) == 0) {
    s._util_gpu = int(util.gpu);
    s._util_mem = int(util.memory);
  }
  if (_power and _power(_dev, &u32) == 0)
    s._power_w = double(u32) * 1.0e-3; // NVML reports milliwatts
  if (_temp and _temp(_dev, NVML_TEMPERATURE_GPU, &u32) == 0)
    s._temp_c = int(u32);
  unsigned long long reasons = 0;
  if (_throttle and _throttle(_dev, &reasons) == 0) {
    for (const auto& tb : kThrottleBits)
      if (reasons & tb._bit) {
        if (not s._throttle.empty())
          s._throttle += " ";
        s._throttle += tb._tag;
      }
  }
  if (s._throttle.empty())
    s._throttle = "none";
  {
    std::lock_guard<std::mutex> lk(_mtx);
    _snap = s;
  }
#endif
}

////////////////////////////////////////////////////////////////

void NvmlStats::Impl::stop() {
  _stop.store(true, std::memory_order_release);
  _cv.notify_all();
  if (_thread.joinable())
    _thread.join();
#if defined(ORK_NVML_ENABLED)
  if (_lib) {
    if (_shutdown)
      _shutdown();
    dlclose(_lib);
    _lib = nullptr;
  }
  _dev = nullptr;
#endif
  {
    std::lock_guard<std::mutex> lk(_mtx);
    _snap = NvmlSnapshot{}; // a reader after shutdown gets "unavailable", not stale numbers
  }
}

////////////////////////////////////////////////////////////////

NvmlStats::NvmlStats()
    : _impl(new Impl) {
}

NvmlStats::~NvmlStats() {
  shutdown();
  delete _impl;
  _impl = nullptr;
}

NvmlStats& NvmlStats::instance() {
  static NvmlStats _i;
  return _i;
}

NvmlSnapshot NvmlStats::snapshot() {
  auto& I = *_impl;
  if (not I._started.load(std::memory_order_acquire))
    I.start();
  std::lock_guard<std::mutex> lk(I._mtx);
  return I._snap;
}

void NvmlStats::shutdown() {
  if (_impl)
    _impl->stop();
}

////////////////////////////////////////////////////////////////
} // namespace ork::lev2
