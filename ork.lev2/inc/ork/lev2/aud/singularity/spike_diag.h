////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <cstdint>
#include <sched.h>
#include <time.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// spike forensics: WHERE a rare multi-millisecond dsp outlier goes, measured
//  with hardware counters the audio thread reads without a syscall.
//
// wall-vs-thread-cpu (see audiodevice_pa) cannot separate "the core executed
//  the work slowly" from "the core was taken away without a context switch":
//  this kernel is built with CONFIG_IRQ_TIME_ACCOUNTING unset, so interrupt
//  time is billed to whatever task was running. the three self-monitoring
//  counters below do separate them:
//    insn : retired user instructions - fixed work stays fixed
//    cyc  : core clock cycles (user) - the same work at a lower clock costs
//           the same cycles and more wall time
//    ref  : TSC-rate cycles (user)   - wall time that the counters did NOT
//           see is time spent outside user mode (kernel/irq) or with the
//           counters otherwise inactive
//  so core_ghz = tsc_ghz * cyc/ref, and (dur - ref/tsc_ghz) is the wall time
//  unaccounted to this thread's user-mode execution.
//
// RT contract: after a one-time per-thread setup (perf_event_open + mmap, on
//  the first probe that thread runs), every probe is rdpmc + clock_gettime -
//  no syscall, no allocation, no lock. records land in fixed BSS rings that a
//  reporter thread drains and prints, so the audio thread never formats.
// OFF unless ORKID_SPIKE_DIAG is set BEFORE the engine library loads.
///////////////////////////////////////////////////////////////////////////////

extern bool spikeDiagEnabledFlag;

// ORKID_SPIKE_FAULTS: adds a getrusage(RUSAGE_THREAD) pair around every probed
//  region - a SYSCALL on the audio thread, so it is its own opt-in mode. it
//  answers the one question the counters cannot: was the unaccounted wall time
//  a page-fault path (minflt advances) or something that faults never see
//  (interrupt / SMI).
extern bool spikeDiagFaultsFlag;

inline bool spikeDiagEnabled() {
  return spikeDiagEnabledFlag;
}

uint64_t spikeDiagThreadMinorFaults();

inline uint64_t spikeDiagNanos() {
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return uint64_t(ts.tv_sec) * 1000000000ull + uint64_t(ts.tv_nsec);
}

// which core the calling thread is on. sched_getcpu is linux-only; on darwin
//  there is no cheap equivalent, so report -1 (unknown) to match the NUMA
//  immunity no-op branches in the same subsystem.
inline int orkaud_getcpu() {
#if defined(__linux__)
  return sched_getcpu();
#else
  return -1;
#endif
}

struct RtPmu {
  uint64_t _insn  = 0; // user mode
  uint64_t _cyc   = 0; // user mode
  uint64_t _ref   = 0; // user mode, TSC rate
  uint64_t _kinsn = 0; // KERNEL mode instructions retired (paranoid<=1 allows
                       //  kernel counting; zero here while wall time passes
                       //  means nothing executed at all on this core)
};

// rdpmc read of this thread's own counters. all zero if the counters could
//  not be opened (permissions) - the report says so rather than lying.
void spikeDiagReadPmu(RtPmu& out);

// probe sites. one "last end" slot per site gives the idle gap ahead of a
//  spike (the DVFS-ramp question: was this the first pass after a quiet
//  stretch?).
enum SpikeSite : int {
  SPK_PANNER2D = 0,
  SPK_BLKCREATE,
  SPK_CALLBACK, // the whole device callback, marked region by region
  SPK_COUNT
};

// the callback timeline needs 4 marks per control pass: a 256-frame chunk at
//  the 32-frame control rate is 8 passes, plus the pre/post regions.
static constexpr int kSpikeMarks = 72;

// one per-callback trace entry, written by the audio thread every callback:
//  the frequency/occupancy history the spike is read against.
void spikeDiagCallback(uint64_t t0, uint64_t wall_ns, uint64_t cpu_ns, uint64_t faults, const RtPmu& delta, int core);

void spikeDiagPush(
    int site,
    const char* label,
    uint64_t t0,
    uint64_t dur,
    uint64_t gap,
    const RtPmu& delta,
    uint64_t faults,
    int core_in,
    int core_out,
    const uint64_t* marks,
    int nmarks);

uint64_t spikeDiagThresholdNanos();
uint64_t spikeDiagLastEnd(int site);
void spikeDiagSetLastEnd(int site, uint64_t t);

///////////////////////////////////////////////////////////////////////////////

struct SpikeScope {

  inline SpikeScope(int site, const char* label)
      : _site(site)
      , _label(label)
      , _enabled(spikeDiagEnabled()) {
    if (_enabled) {
      _coreIn = orkaud_getcpu();
      if (spikeDiagFaultsFlag)
        _flt0 = spikeDiagThreadMinorFaults();
      spikeDiagReadPmu(_pmu0);
      _t0 = spikeDiagNanos();
    }
  }

  // a checkpoint inside the measured region: the intra-block timeline that
  //  separates a uniform dilation (slow core) from a point stall (the core
  //  taken away at one instant).
  inline void mark() {
    if (_enabled and _nmarks < kSpikeMarks)
      _marks[_nmarks++] = spikeDiagNanos();
  }

  inline ~SpikeScope() {
    if (not _enabled)
      return;
    uint64_t t1  = spikeDiagNanos();
    uint64_t dur = t1 - _t0;
    uint64_t prv = spikeDiagLastEnd(_site);
    spikeDiagSetLastEnd(_site, t1);
    if (dur < spikeDiagThresholdNanos())
      return;
    RtPmu pmu1;
    spikeDiagReadPmu(pmu1);
    RtPmu d;
    d._insn       = pmu1._insn - _pmu0._insn;
    d._cyc        = pmu1._cyc - _pmu0._cyc;
    d._ref        = pmu1._ref - _pmu0._ref;
    d._kinsn      = pmu1._kinsn - _pmu0._kinsn;
    uint64_t flts = spikeDiagFaultsFlag ? (spikeDiagThreadMinorFaults() - _flt0) : 0;
    spikeDiagPush(_site, _label, _t0, dur, prv ? (_t0 - prv) : 0, d, flts, _coreIn, orkaud_getcpu(), _marks, _nmarks);
  }

  int _site;
  const char* _label;
  bool _enabled;
  int _coreIn      = -1;
  uint64_t _t0     = 0;
  uint64_t _flt0   = 0;
  uint32_t _nmarks = 0;
  RtPmu _pmu0;
  uint64_t _marks[kSpikeMarks];
};

///////////////////////////////////////////////////////////////////////////////
// the callback-wide scope, published per thread so regions deep inside
//  synth::compute can drop timeline marks into it without threading a scope
//  pointer through every call. the mark ORDER is the region ledger - see the
//  spikeDiagMark() call sites in synth.cpp / audiodevice_pa.cpp.
///////////////////////////////////////////////////////////////////////////////

extern thread_local SpikeScope* spikeDiagCallbackScope;

inline void spikeDiagMark() {
  if (spikeDiagEnabledFlag and spikeDiagCallbackScope)
    spikeDiagCallbackScope->mark();
}

struct SpikeCallbackScope {
  inline SpikeCallbackScope(const char* label)
      : _scope(SPK_CALLBACK, label) {
    if (spikeDiagEnabledFlag)
      spikeDiagCallbackScope = &_scope;
  }
  inline ~SpikeCallbackScope() {
    spikeDiagCallbackScope = nullptr;
  }
  SpikeScope _scope;
};

} // namespace ork::audio::singularity
