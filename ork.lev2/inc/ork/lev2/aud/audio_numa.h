////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <vector>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
// audio_numa — make the audio path immune to AUTOMATIC NUMA BALANCING.
//
// The mechanism this defuses (measured on the singularity dsp path, not
//  theorized): with kernel.numa_balancing on, the scanner — which runs from
//  the process's OWN threads, via task_work — marks anonymous pages PROT_NONE.
//  The next touch takes a hinting fault, and when the balancer rules the folio
//  misplaced it migrates it SYNCHRONOUSLY, in the faulting context. When that
//  context is the audio thread inside a dsp sample loop, the callback stalls
//  for milliseconds and the device underruns.
//
// Three legs, all unprivileged:
//  1 mbind(MPOL_BIND) over the audio-hot pools. This is decisive rather than
//    merely helpful: task_numa_work SKIPS any vma whose policy lacks
//    MPOL_F_MOF, and an explicitly mbind()'d vma carries no MOF (only the
//    implicit per-node default policy does) — so those pages are never marked
//    and the fault cannot happen at all. MPOL_MF_MOVE brings pages already
//    placed elsewhere home, so leg 3 is not a remote-memory sentence.
//  2 mlockall(MCL_CURRENT|MCL_FUTURE) — standard pro-audio hygiene, and an
//    mlocked folio additionally fails folio_isolate_lru, so it cannot be
//    batched for migration by anything. Degrades LOUDLY, never fatally:
//    RLIMIT_MEMLOCK is a per-login grant this process may simply not have.
//  3 confine the audio thread to the home node's cpus (the node mask, NOT a
//    single core), so fair-class load balancing cannot wander it onto a socket
//    its pools do not live on — that wandering is what feeds the balancer's
//    misplacement verdicts in the first place.
//
// ORKID_AUDIO_NO_NUMAFIX=1 disables all three — the A/B switch.
//
// Legs 1 and 3 are linux-only (no other supported platform has a task-driven
//  page balancer); leg 2 is POSIX and runs everywhere.
///////////////////////////////////////////////////////////////////////////////

// The deadline-bearing backends declare themselves here. The STREAM and NULL
//  devices never miss a deadline (they are pumped by their consumer), so they
//  must not pay the bind/lock cost — nor perturb an offline render with it.
void audioNumaSetRealtimeDevice(bool is_realtime);
bool audioNumaActive();

// The node the audio pools live on, or -1 while unresolved (nothing bound yet).
int audioNumaHomeNode();

///////////////////////////////////////////////////////////////////////////////
// leg 1. Ranges are collected, page-aligned outward and COALESCED before any
//  mbind is issued: the dsp pools are tens of thousands of adjacent heap
//  allocations, and one mbind each would split the heap into as many vmas
//  (vm.max_map_count is 65530). Coalescing also means the bind covers the
//  small objects interleaved between them — pool bookkeeping, Layer instances
//  — which could not be bound individually without that vma explosion.
///////////////////////////////////////////////////////////////////////////////

struct AudioNumaBinder {

  void add(const void* base, size_t length);
  void commit(const char* tag);

  struct Range {
    size_t _begin = 0;
    size_t _end   = 0;
  };

  std::vector<Range> _ranges;
  bool _quiet = false; // suppress the success line (failures always report)
};

// one-off range (a buffer reallocated after the pool pass) — best effort, quiet.
void audioNumaBindRange(const void* base, size_t length);

///////////////////////////////////////////////////////////////////////////////

void audioNumaLockMemory();                                 // leg 2, off-RT
void audioNumaPinThreadToHomeNode(const char* thread_name); // leg 3, on the audio thread

} // namespace ork::lev2
