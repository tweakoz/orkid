////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/aud/audio_numa.h>
#include <ork/kernel/string/string.h>
#include <ork/util/logger.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/mempolicy.h>
#include <sched.h>
#include <sys/syscall.h>
#endif

namespace ork::lev2 {

static logchannel_ptr_t logchan_numa = logger()->configureChannel("audio.NUMA", fvec3(0.45, 0.95, 0.75), true);

///////////////////////////////////////////////////////////////////////////////

static std::atomic<bool> _realtime_device = false;
static std::atomic<int> _home_node        = -1;

static bool _fixDisabled() {
  static const bool disabled = (nullptr != getenv("ORKID_AUDIO_NO_NUMAFIX"));
  return disabled;
}

void audioNumaSetRealtimeDevice(bool is_realtime) {
  _realtime_device.store(is_realtime);
  if (is_realtime and _fixDisabled()) {
    logerrchannel()->log(
        "AUDIO-NUMA: DISABLED by ORKID_AUDIO_NO_NUMAFIX - automatic NUMA balancing is free to "
        "fault and migrate audio pages under the dsp path (A/B baseline mode)");
  }
}

bool audioNumaActive() {
  return _realtime_device.load() and (not _fixDisabled());
}

int audioNumaHomeNode() {
  return _home_node.load();
}

///////////////////////////////////////////////////////////////////////////////
#if defined(__linux__)
///////////////////////////////////////////////////////////////////////////////
// glibc exports no wrapper for the mempolicy calls (they live in libnuma,
//  which the engine does not link) — so they are issued directly.
///////////////////////////////////////////////////////////////////////////////

static long _mbind(void* addr, unsigned long len, int mode, const unsigned long* nmask, unsigned long maxnode, unsigned flags) {
  return syscall(__NR_mbind, addr, len, mode, nmask, maxnode, flags);
}

static long _get_mempolicy(int* policy, unsigned long* nmask, unsigned long maxnode, void* addr, unsigned long flags) {
  return syscall(__NR_get_mempolicy, policy, nmask, maxnode, addr, flags);
}

// which node the page backing <addr> is resident on, or -1 if unknowable.
static int _nodeOfAddress(const void* addr) {
  int node = -1;
  if (0 != _get_mempolicy(&node, nullptr, 0, const_cast<void*>(addr), MPOL_F_NODE | MPOL_F_ADDR)) {
    return -1;
  }
  return node;
}

static size_t _pageSize() {
  static const size_t ps = size_t(sysconf(_SC_PAGESIZE));
  return ps;
}

static std::string _nodeCpuList(int node) {
  char path[256];
  snprintf(path, sizeof(path), "/sys/devices/system/node/node%d/cpulist", node);
  FILE* fil = fopen(path, "rb");
  if (nullptr == fil) {
    return "";
  }
  char buf[1024] = {0};
  size_t nread   = fread(buf, 1, sizeof(buf) - 1, fil);
  fclose(fil);
  buf[nread] = 0;
  std::string rval(buf);
  while ((not rval.empty()) and (rval.back() == '\n' or rval.back() == ' ')) {
    rval.pop_back();
  }
  return rval;
}

// "0-3,7-9,48" -> cpu_set_t. returns the number of cpus set.
static int _cpuSetFromList(const std::string& list, cpu_set_t& out) {
  CPU_ZERO(&out);
  int count   = 0;
  size_t pos  = 0;
  while (pos < list.size()) {
    size_t comma = list.find(',', pos);
    std::string tok = list.substr(pos, (comma == std::string::npos) ? std::string::npos : (comma - pos));
    pos             = (comma == std::string::npos) ? list.size() : (comma + 1);
    if (tok.empty()) {
      continue;
    }
    size_t dash = tok.find('-');
    int lo      = atoi(tok.c_str());
    int hi      = (dash == std::string::npos) ? lo : atoi(tok.c_str() + dash + 1);
    for (int c = lo; c <= hi; c++) {
      if (c >= 0 and c < CPU_SETSIZE) {
        CPU_SET(c, &out);
        count++;
      }
    }
  }
  return count;
}

///////////////////////////////////////////////////////////////////////////////

void AudioNumaBinder::add(const void* base, size_t length) {
  if ((nullptr == base) or (0 == length) or (not audioNumaActive())) {
    return;
  }
  const size_t ps = _pageSize();
  Range r;
  // outward alignment is mandatory (mbind rejects an unaligned start) and is
  //  why the coalescing pass below matters: neighbouring heap allocations share
  //  the edge pages, so unmerged ranges would bind the same page repeatedly and
  //  split a vma per allocation.
  r._begin = (size_t(base) / ps) * ps;
  r._end   = ((size_t(base) + length + ps - 1) / ps) * ps;
  _ranges.push_back(r);
}

void AudioNumaBinder::commit(const char* tag) {

  if (_ranges.empty() or (not audioNumaActive())) {
    _ranges.clear();
    return;
  }

  auto t0 = std::chrono::steady_clock::now();

  std::sort(_ranges.begin(), _ranges.end(), [](const Range& a, const Range& b) { return a._begin < b._begin; });

  std::vector<Range> merged;
  for (const auto& r : _ranges) {
    // touching counts as adjacent: both sides are mapped, so the union has no
    //  hole and mbind will not see EFAULT.
    if ((not merged.empty()) and (r._begin <= merged.back()._end)) {
      merged.back()._end = std::max(merged.back()._end, r._end);
    } else {
      merged.push_back(r);
    }
  }

  size_t total = 0;
  for (const auto& r : merged) {
    total += (r._end - r._begin);
  }

  // home node: majority vote over where the pages already are, so the common
  //  case migrates the minority rather than the whole pool.
  int node = _home_node.load();
  if (node < 0) {
    int votes[64] = {0};
    const size_t nprobe = std::min<size_t>(merged.size(), 64);
    for (size_t i = 0; i < nprobe; i++) {
      const auto& r = merged[(i * merged.size()) / nprobe];
      int n         = _nodeOfAddress((const void*)r._begin);
      if (n >= 0 and n < 64) {
        votes[n]++;
      }
    }
    int best = 0;
    for (int n = 1; n < 64; n++) {
      if (votes[n] > votes[best]) {
        best = n;
      }
    }
    node = best;
    _home_node.store(node);
  }

  const unsigned long nodemask = (1ul << unsigned(node));
  size_t nbound                = 0;
  size_t nfailed               = 0;
  int firsterr                 = 0;
  for (const auto& r : merged) {
    long rc = _mbind(
        (void*)r._begin,
        (unsigned long)(r._end - r._begin),
        MPOL_BIND,
        &nodemask,
        64,
        MPOL_MF_MOVE);
    if (0 == rc) {
      nbound++;
    } else {
      nfailed++;
      if (0 == firsterr) {
        firsterr = errno;
      }
    }
  }

  double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

  if (not _quiet) {
    logchan_numa->log(
        "AUDIO-NUMA: bound<%s> node<%d> spans<%zu/%zu> bytes<%.1fMiB> elapsed<%.0fms> - these pages "
        "are now outside the automatic balancer's scan set",
        tag,
        node,
        nbound,
        merged.size(),
        double(total) / (1024.0 * 1024.0),
        elapsed * 1000.0);
  }

  if (nfailed) {
    logerrchannel()->log(
        "AUDIO-NUMA: bind<%s> INCOMPLETE - %zu of %zu spans refused (first errno<%d:%s>); those "
        "pages remain migratable and can still stall the audio thread",
        tag,
        nfailed,
        merged.size(),
        firsterr,
        strerror(firsterr));
  }

  _ranges.clear();
}

///////////////////////////////////////////////////////////////////////////////

void audioNumaPinThreadToHomeNode(const char* thread_name) {

  if (not audioNumaActive()) {
    return;
  }

  const int node = _home_node.load();
  if (node < 0) {
    logerrchannel()->log(
        "AUDIO-NUMA: thread<%s> NOT pinned - no home node resolved (the pool bind never ran); "
        "the audio thread stays free to wander sockets",
        thread_name);
    return;
  }

  const auto cpulist = _nodeCpuList(node);
  cpu_set_t cpus;
  const int ncpus = _cpuSetFromList(cpulist, cpus);
  if (0 == ncpus) {
    logerrchannel()->log(
        "AUDIO-NUMA: thread<%s> NOT pinned - node<%d> cpulist unreadable or empty", thread_name, node);
    return;
  }

  const int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpus);
  if (0 == rc) {
    logchan_numa->log(
        "AUDIO-NUMA: thread<%s> pinned to node<%d> ncpus<%d> cpus<%s>", thread_name, node, ncpus, cpulist.c_str());
  } else {
    logerrchannel()->log(
        "AUDIO-NUMA: thread<%s> pin to node<%d> FAILED rc<%d:%s> - the audio thread stays free to "
        "wander sockets",
        thread_name,
        node,
        rc,
        strerror(rc));
  }
}

///////////////////////////////////////////////////////////////////////////////
#else // not linux
///////////////////////////////////////////////////////////////////////////////

void AudioNumaBinder::add(const void* base, size_t length) {
}
void AudioNumaBinder::commit(const char* tag) {
  _ranges.clear();
}
void audioNumaPinThreadToHomeNode(const char* thread_name) {
}

///////////////////////////////////////////////////////////////////////////////
#endif
///////////////////////////////////////////////////////////////////////////////

// the few-page buffers reallocated after the pool pass — quiet, because this
//  is reachable from the audio thread (outputBuffer::resize, first callback).
void audioNumaBindRange(const void* base, size_t length) {
  AudioNumaBinder binder;
  binder._quiet = true;
  binder.add(base, length);
  binder.commit("straggler");
}

///////////////////////////////////////////////////////////////////////////////
// leg 2. MCL_FUTURE matters as much as MCL_CURRENT here: the dsp pools that
//  matter most are allocated after the device comes up.
///////////////////////////////////////////////////////////////////////////////

void audioNumaLockMemory() {

  if (not audioNumaActive()) {
    return;
  }

  rlimit lim{};
  getrlimit(RLIMIT_MEMLOCK, &lim);
  std::string limstr = (lim.rlim_cur == RLIM_INFINITY) //
                           ? std::string("unlimited")
                           : FormatString("%lluKiB", (unsigned long long)(lim.rlim_cur / 1024));

  if (0 == mlockall(MCL_CURRENT | MCL_FUTURE)) {
    logchan_numa->log("AUDIO-MLOCK: locked resident (MCL_CURRENT|MCL_FUTURE) rlimit<%s>", limstr.c_str());
    return;
  }

  // NOT fatal: an unlocked audio path still runs, it is just exposed to reclaim
  //  and to migration batching. Loud, once, with the grant that would fix it.
  const int err = errno;
  logerrchannel()->log(
      "AUDIO-MLOCK: DENIED rlimit<%s> errno<%d:%s> - running unlocked; audio pages stay reclaimable "
      "and migratable (grant memlock to this user, e.g. '@audio - memlock unlimited' in "
      "/etc/security/limits.d, then log in fresh)",
      limstr.c_str(),
      err,
      strerror(err));
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
