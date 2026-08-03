////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/spike_diag.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <pthread.h>
#include <sys/resource.h>
#include <thread>

#if defined(__linux__) && defined(__x86_64__)
#define ORK_SPIKE_PMU 1
#include <linux/perf_event.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <x86intrin.h>
#endif

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

bool spikeDiagEnabledFlag = (getenv("ORKID_SPIKE_DIAG") != nullptr);
bool spikeDiagFaultsFlag  = (getenv("ORKID_SPIKE_FAULTS") != nullptr);

// trivially destructible pointer: no __cxa_thread_atexit registration on the
//  audio thread's path.
thread_local SpikeScope* spikeDiagCallbackScope = nullptr;

// ORKID_SPIKE_IRQS: also sample this core's /proc/interrupts column. that file
//  costs ~13ms to generate, which stretches the sampler period - off unless
//  device/IPI interference is the question being asked.
static const bool _sample_irqs = (getenv("ORKID_SPIKE_IRQS") != nullptr);

uint64_t spikeDiagThreadMinorFaults() {
#if defined(__linux__)
  rusage ru{};
  getrusage(RUSAGE_THREAD, &ru);
  return uint64_t(ru.ru_minflt);
#else
  return 0;
#endif
}

namespace {

///////////////////////////////////////////////////////////////////////////////
// self-monitoring counters. unprivileged: user-mode only (exclude_kernel),
//  task scoped, read with rdpmc through the event's mmap page. per-thread
//  because a perf event follows one task - a probe on a job-pool worker opens
//  its own set on first use.
///////////////////////////////////////////////////////////////////////////////

#if ORK_SPIKE_PMU

struct ThreadPmu {
  perf_event_mmap_page* _pages[4] = {nullptr, nullptr, nullptr, nullptr};
  bool _tried                     = false;
  bool _ok                        = false;
};

// trivially destructible on purpose: no __cxa_thread_atexit registration in a
//  path the audio thread runs. the mappings live until process exit.
thread_local ThreadPmu _tpmu;

perf_event_mmap_page* _openOne(uint64_t config, bool kernelmode = false) {
  perf_event_attr attr;
  memset(&attr, 0, sizeof(attr));
  attr.type           = PERF_TYPE_HARDWARE;
  attr.size           = sizeof(attr);
  attr.config         = config;
  attr.exclude_kernel = kernelmode ? 0 : 1;
  attr.exclude_user   = kernelmode ? 1 : 0;
  attr.exclude_hv     = 1;
  int fd              = syscall(__NR_perf_event_open, &attr, 0, -1, -1, 0);
  if (fd < 0)
    return nullptr;
  void* p = mmap(nullptr, 4096, PROT_READ, MAP_SHARED, fd, 0);
  if (p == MAP_FAILED)
    return nullptr;
  return (perf_event_mmap_page*)p;
}

inline uint64_t _readOne(perf_event_mmap_page* pc) {
  if (nullptr == pc)
    return 0;
  uint64_t count = 0;
  uint32_t seq   = 0;
  do {
    seq = pc->lock;
    std::atomic_thread_fence(std::memory_order_acquire);
    uint32_t idx = pc->index;
    if (0 == idx) { // counter not currently scheduled on this cpu
      count = pc->offset;
      break;
    }
    count = __rdpmc(idx - 1);
    count <<= 64 - pc->pmc_width; // sign extend the partial-width counter
    count = uint64_t(int64_t(count) >> (64 - pc->pmc_width));
    count += pc->offset;
    std::atomic_thread_fence(std::memory_order_acquire);
  } while (pc->lock != seq);
  return count;
}

#endif

///////////////////////////////////////////////////////////////////////////////

struct SpikeRec {
  uint64_t _t0     = 0;
  uint64_t _dur    = 0;
  uint64_t _gap    = 0;
  uint64_t _faults = 0;
  RtPmu _pmu;
  const char* _label = nullptr;
  int32_t _site      = 0;
  int32_t _coreIn    = -1;
  int32_t _coreOut   = -1;
  uint32_t _nmarks   = 0;
  uint64_t _marks[kSpikeMarks];
};

struct CbRec {
  uint64_t _t0     = 0;
  uint64_t _wall   = 0;
  uint64_t _cpu    = 0;
  uint64_t _faults = 0;
  RtPmu _pmu;
  int32_t _core = -1;
};

// BSS rings: written by the audio thread, drained by the reporter thread.
constexpr int kSpikeRing = 256;
constexpr int kCbRing    = 8192; // ~10s of 1.33ms callbacks

SpikeRec _spikering[kSpikeRing];
std::atomic<uint64_t> _spikehead{0};

CbRec _cbring[kCbRing];
std::atomic<uint64_t> _cbhead{0};

uint64_t _lastend[SPK_COUNT] = {0};

///////////////////////////////////////////////////////////////////////////////
// per-core kernel time buckets, sampled off the audio thread. with
//  CONFIG_IRQ_TIME_ACCOUNTING unset these come from the 1kHz tick sampler, so
//  a stall the kernel SAW lands in irq/softirq/system jiffies of the core that
//  took it - and a stall that accumulates NO jiffies anywhere while wall time
//  passes is time the kernel never ran either (SMM / firmware).
///////////////////////////////////////////////////////////////////////////////

struct StatRec {
  uint64_t _t    = 0;
  uint64_t _user = 0, _system = 0, _irq = 0, _softirq = 0, _idle = 0, _steal = 0;
  uint64_t _intr = 0; // system-wide interrupt count
  // this core's column out of /proc/interrupts: the cross-cpu classes that
  //  follow a THREAD (an mm's TLB shootdowns chase whichever cpus run it)
  //  rather than a core, plus everything a device raised on it.
  uint64_t _loc = 0, _cal = 0, _tlb = 0, _res = 0, _dev = 0;
  // automatic NUMA balancing state of the audio thread. the scan (task_numa_work)
  //  runs as the THREAD'S OWN task-work on return from a tick, walking up to
  //  numa_balancing_scan_size_mb of the address space - kernel instructions
  //  billed to the thread, at an arbitrary point in its user code.
  uint64_t _numascan = 0, _numamig = 0, _numaflt = 0;
  int32_t _core = -1;
};

constexpr int kStatRing = 4096; // 40s at 10ms
StatRec _statring[kStatRing];
std::atomic<uint64_t> _stathead{0};
std::atomic<int> _audiocore{-1};
std::atomic<long> _audiotid{0};

uint64_t _threshold_ns = 0;
double _tsc_ghz        = 0.0;

const char* const _sitenames[SPK_COUNT] = {"PANNER2D.compute", "blk.createInstance", "pa.callback"};

///////////////////////////////////////////////////////////////////////////////
// /proc + /sys reporter and sampler: linux-only forensics. no darwin analogue
//  (these read /proc/stat, /proc/interrupts, /proc/self/task/<tid>/sched), so
//  the whole block is a no-op on darwin - the SpikeDiagInit ctor below simply
//  does not spawn the reporter/sampler threads there.
///////////////////////////////////////////////////////////////////////////////

#if defined(__linux__)

void _printSysfs(const char* path, const char* tag) {
  FILE* f = fopen(path, "rb");
  if (nullptr == f) {
    printf("[SPIKE_CTX] %s<unreadable>\n", tag);
    return;
  }
  char buf[256];
  size_t n  = fread(buf, 1, sizeof(buf) - 1, f);
  buf[n]    = 0;
  for (size_t i = 0; i < n; i++)
    if (buf[i] == '\n')
      buf[i] = 0;
  fclose(f);
  printf("[SPIKE_CTX] %s<%s>\n", tag, buf);
}

// /proc/interrupts is ~500KB and costs milliseconds to generate - only the
//  sampler thread ever touches it, and only for one core's column.
void _readInterrupts(int core, StatRec& rec) {
  static char ibuf[1 << 20];
  int fd = open("/proc/interrupts", O_RDONLY);
  if (fd < 0)
    return;
  size_t got = 0;
  while (got + 1 < sizeof(ibuf)) {
    ssize_t n = read(fd, ibuf + got, sizeof(ibuf) - 1 - got);
    if (n <= 0)
      break;
    got += size_t(n);
  }
  close(fd);
  ibuf[got] = 0;
  char* p   = ibuf;
  p         = strchr(p, '\n'); // skip the cpu header
  while (p) {
    char* line = p + 1;
    p          = strchr(line, '\n');
    if (nullptr == p)
      break;
    *p = 0;
    while (*line == ' ')
      line++;
    char* colon = strchr(line, ':');
    if (nullptr == colon)
      continue;
    *colon    = 0;
    bool isdev = (line[0] >= '0' and line[0] <= '9');
    bool isloc = (0 == strcmp(line, "LOC"));
    bool iscal = (0 == strcmp(line, "CAL"));
    bool istlb = (0 == strcmp(line, "TLB"));
    bool isres = (0 == strcmp(line, "RES"));
    if (not(isdev or isloc or iscal or istlb or isres))
      continue;
    char* tok = colon + 1;
    for (int c = 0; c < core; c++) { // skip to this core's column
      while (*tok == ' ')
        tok++;
      while (*tok and *tok != ' ')
        tok++;
    }
    uint64_t v = strtoull(tok, nullptr, 10);
    if (isdev)
      rec._dev += v;
    else if (isloc)
      rec._loc = v;
    else if (iscal)
      rec._cal = v;
    else if (istlb)
      rec._tlb = v;
    else
      rec._res = v;
  }
}

void _readSchedNuma(long tid, StatRec& rec) {
  char path[128];
  snprintf(path, sizeof(path), "/proc/self/task/%ld/sched", tid);
  int fd = open(path, O_RDONLY);
  if (fd < 0)
    return;
  static char sbuf[1 << 15];
  ssize_t n = read(fd, sbuf, sizeof(sbuf) - 1);
  close(fd);
  if (n <= 0)
    return;
  sbuf[n]  = 0;
  auto pick = [&](const char* key, uint64_t& dst) {
    char* p = strstr(sbuf, key);
    if (nullptr == p)
      return;
    p = strchr(p, ':');
    if (p)
      dst = strtoull(p + 1, nullptr, 10);
  };
  pick("mm->numa_scan_seq", rec._numascan);
  pick("numa_pages_migrated", rec._numamig);
  pick("total_numa_faults", rec._numaflt);
}

void _statSamplerLoop() {
  pthread_setname_np(pthread_self(), "ork.spikestat");
  char buf[1 << 16];
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    int core = _audiocore.load(std::memory_order_relaxed);
    if (core < 0)
      continue;
    int fd = open("/proc/stat", O_RDONLY);
    if (fd < 0)
      return;
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
      continue;
    buf[n]      = 0;
    uint64_t ts = spikeDiagNanos();
    char key[32];
    int keylen = snprintf(key, sizeof(key), "cpu%d ", core);
    char* line = strstr(buf, key);
    char* intr = strstr(buf, "\nintr ");
    if (nullptr == line)
      continue;
    StatRec rec;
    rec._t    = ts;
    rec._core = core;
    unsigned long long u, ni, sy, id, io, hi, si, st;
    if (8 != sscanf(line + keylen, "%llu %llu %llu %llu %llu %llu %llu %llu", &u, &ni, &sy, &id, &io, &hi, &si, &st))
      continue;
    rec._user    = u + ni;
    rec._system  = sy;
    rec._idle    = id;
    rec._irq     = hi;
    rec._softirq = si;
    rec._steal   = st;
    if (intr) {
      unsigned long long it = 0;
      sscanf(intr + 6, "%llu", &it);
      rec._intr = it;
    }
    if (_sample_irqs)
      _readInterrupts(core, rec);
    long tid = _audiotid.load(std::memory_order_relaxed);
    if (tid)
      _readSchedNuma(tid, rec);
    uint64_t idx = _stathead.load(std::memory_order_relaxed);
    _statring[idx % kStatRing] = rec;
    _stathead.store(idx + 1, std::memory_order_release);
  }
}

void _reportOne(const SpikeRec& r, uint64_t t_origin) {
  double dur_us = double(r._dur) * 1e-3;
  double ref_us = _tsc_ghz > 0.0 ? double(r._pmu._ref) / (_tsc_ghz * 1e3) : 0.0;
  double corghz = r._pmu._ref ? _tsc_ghz * double(r._pmu._cyc) / double(r._pmu._ref) : 0.0;
  printf(
      "[SPIKE] %s label<%s> t<%.3fs> dur<%.1fus> gap<%.1fus> core<%d->%d> insn<%lu> cyc<%lu> ref<%lu> "
      "core_ghz<%.2f> user_us<%.1f> unaccounted_us<%.1f> ipc<%.2f> minflt<%lu> kinsn<%lu>\n",
      _sitenames[r._site],
      r._label ? r._label : "-",
      double(r._t0 - t_origin) * 1e-9,
      dur_us,
      double(r._gap) * 1e-3,
      r._coreIn,
      r._coreOut,
      (unsigned long)r._pmu._insn,
      (unsigned long)r._pmu._cyc,
      (unsigned long)r._pmu._ref,
      corghz,
      ref_us,
      dur_us - ref_us,
      r._pmu._cyc ? double(r._pmu._insn) / double(r._pmu._cyc) : 0.0,
      (unsigned long)r._faults,
      (unsigned long)r._pmu._kinsn);
  if (r._nmarks) {
    printf("[SPIKE_MARKS]");
    uint64_t prev = r._t0;
    for (uint32_t i = 0; i < r._nmarks; i++) {
      printf(" %.1f", double(r._marks[i] - prev) * 1e-3);
      prev = r._marks[i];
    }
    printf(" | tail %.1f (us)\n", double(r._t0 + r._dur - prev) * 1e-3);
  }
  // the callback trace bracketing the spike: was the core already slow, and
  //  for how long had this thread been idle ?
  uint64_t head = _cbhead.load(std::memory_order_acquire);
  uint64_t from = head > kCbRing ? head - kCbRing : 0;
  for (uint64_t i = from; i < head; i++) {
    const CbRec& c = _cbring[i % kCbRing];
    if (c._t0 + 12000000ull < r._t0)
      continue;
    if (c._t0 > r._t0 + 6000000ull)
      break;
    double cghz = c._pmu._ref ? _tsc_ghz * double(c._pmu._cyc) / double(c._pmu._ref) : 0.0;
    double cref = _tsc_ghz > 0.0 ? double(c._pmu._ref) / (_tsc_ghz * 1e3) : 0.0;
    printf(
        "[SPIKE_CB]   dt<%+.3fms> core<%d> wall<%.1fus> cpu<%.1fus> insn<%lu> core_ghz<%.2f> user_us<%.1f> minflt<%lu> kinsn<%lu>%s\n",
        double(int64_t(c._t0 - r._t0)) * 1e-6,
        c._core,
        double(c._wall) * 1e-3,
        double(c._cpu) * 1e-3,
        (unsigned long)c._pmu._insn,
        cghz,
        cref,
        (unsigned long)c._faults,
        (unsigned long)c._pmu._kinsn,
        (c._t0 <= r._t0 and (c._t0 + c._wall) >= r._t0) ? " <<< SPIKE INSIDE" : "");
  }
  // /proc/stat jiffy deltas for the spiking core across the stall window
  uint64_t shead = _stathead.load(std::memory_order_acquire);
  uint64_t sfrom = shead > kStatRing ? shead - kStatRing : 0;
  const StatRec* prev = nullptr;
  for (uint64_t i = sfrom; i < shead; i++) {
    const StatRec& s = _statring[i % kStatRing];
    if (s._t + 30000000ull < r._t0) {
      prev = &s;
      continue;
    }
    if (s._t > r._t0 + r._dur + 30000000ull)
      break;
    if (prev)
      printf(
          "[SPIKE_STAT] dt<%+.3fms> core<%d> d_user<%lu> d_sys<%lu> d_irq<%lu> d_softirq<%lu> d_idle<%lu> d_steal<%lu> "
          "d_intr<%lu> d_loc<%lu> d_cal<%lu> d_tlb<%lu> d_res<%lu> d_dev<%lu> "
          "d_numascan<%lu> d_numamig<%lu> d_numaflt<%lu> over<%.1fms>\n",
          double(int64_t(s._t - r._t0)) * 1e-6,
          s._core,
          (unsigned long)(s._user - prev->_user),
          (unsigned long)(s._system - prev->_system),
          (unsigned long)(s._irq - prev->_irq),
          (unsigned long)(s._softirq - prev->_softirq),
          (unsigned long)(s._idle - prev->_idle),
          (unsigned long)(s._steal - prev->_steal),
          (unsigned long)(s._intr - prev->_intr),
          (unsigned long)(s._loc - prev->_loc),
          (unsigned long)(s._cal - prev->_cal),
          (unsigned long)(s._tlb - prev->_tlb),
          (unsigned long)(s._res - prev->_res),
          (unsigned long)(s._dev - prev->_dev),
          (unsigned long)(s._numascan - prev->_numascan),
          (unsigned long)(s._numamig - prev->_numamig),
          (unsigned long)(s._numaflt - prev->_numaflt),
          double(s._t - prev->_t) * 1e-6);
    prev = &s;
  }
}

///////////////////////////////////////////////////////////////////////////////

void _reporterLoop() {
  pthread_setname_np(pthread_self(), "ork.spikediag");
  printf("[SPIKE_CTX] threshold<%.1fus> tsc_ghz<%.3f>\n", double(_threshold_ns) * 1e-3, _tsc_ghz);
  _printSysfs("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "governor");
  _printSysfs("/sys/devices/system/cpu/cpu0/cpufreq/scaling_driver", "driver");
  _printSysfs("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq", "scaling_min_khz");
  _printSysfs("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", "scaling_max_khz");
  uint64_t tail     = 0;
  uint64_t t_origin = spikeDiagNanos();
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    uint64_t head = _spikehead.load(std::memory_order_acquire);
    if (head > tail + kSpikeRing / 2)
      tail = head - kSpikeRing / 2;
    while (tail < head) {
      _reportOne(_spikering[tail % kSpikeRing], t_origin);
      tail++;
    }
    fflush(stdout);
  }
}

#endif // defined(__linux__)

// TSC rate: ref-cycles accumulated over a known wall interval on this thread.
//  ref counts at the invariant nominal rate, so this converts ref deltas back
//  into wall microseconds.
void _calibrate() {
#if ORK_SPIKE_PMU
  RtPmu a, b;
  spikeDiagReadPmu(a);
  uint64_t t0 = spikeDiagNanos();
  volatile double acc = 0;
  while (spikeDiagNanos() - t0 < 20000000ull)
    acc += 1.0;
  uint64_t t1 = spikeDiagNanos();
  spikeDiagReadPmu(b);
  if (b._ref > a._ref)
    _tsc_ghz = double(b._ref - a._ref) / double(t1 - t0);
#endif
}

struct SpikeDiagInit {
  SpikeDiagInit() {
    if (not spikeDiagEnabledFlag)
      return;
    const char* thr = getenv("ORKID_SPIKE_US");
    _threshold_ns   = uint64_t(thr ? atof(thr) : 300.0) * 1000ull;
    _calibrate();
#if defined(__linux__)
    std::thread(_reporterLoop).detach();
    std::thread(_statSamplerLoop).detach();
#endif
  }
};

SpikeDiagInit _spikediaginit;

} // namespace

///////////////////////////////////////////////////////////////////////////////

void spikeDiagReadPmu(RtPmu& out) {
#if ORK_SPIKE_PMU
  auto& t = _tpmu;
  if (not t._tried) {
    t._tried    = true;
    t._pages[0] = _openOne(PERF_COUNT_HW_INSTRUCTIONS);
    t._pages[1] = _openOne(PERF_COUNT_HW_CPU_CYCLES);
    t._pages[2] = _openOne(PERF_COUNT_HW_REF_CPU_CYCLES);
    t._pages[3] = _openOne(PERF_COUNT_HW_INSTRUCTIONS, true);
    t._ok       = t._pages[0] and t._pages[1] and t._pages[2];
    if (not t._ok)
      printf("[SPIKE_CTX] pmu counters UNAVAILABLE on this thread (perf_event_open denied)\n");
  }
  if (not t._ok) {
    out = RtPmu();
    return;
  }
  out._insn = _readOne(t._pages[0]);
  out._cyc  = _readOne(t._pages[1]);
  out._ref   = _readOne(t._pages[2]);
  out._kinsn = _readOne(t._pages[3]);
#else
  out = RtPmu();
#endif
}

///////////////////////////////////////////////////////////////////////////////

uint64_t spikeDiagThresholdNanos() {
  return _threshold_ns;
}
uint64_t spikeDiagLastEnd(int site) {
  return _lastend[site];
}
void spikeDiagSetLastEnd(int site, uint64_t t) {
  _lastend[site] = t;
}

///////////////////////////////////////////////////////////////////////////////

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
    int nmarks) {
  uint64_t idx = _spikehead.load(std::memory_order_relaxed);
  auto& r      = _spikering[idx % kSpikeRing];
  r._t0        = t0;
  r._dur       = dur;
  r._gap       = gap;
  r._faults    = faults;
  r._pmu       = delta;
  r._label     = label;
  r._site      = site;
  r._coreIn    = core_in;
  r._coreOut   = core_out;
  r._nmarks    = uint32_t(nmarks > kSpikeMarks ? kSpikeMarks : nmarks);
  for (uint32_t i = 0; i < r._nmarks; i++)
    r._marks[i] = marks[i];
  _spikehead.store(idx + 1, std::memory_order_release);
}

///////////////////////////////////////////////////////////////////////////////

void spikeDiagCallback(uint64_t t0, uint64_t wall_ns, uint64_t cpu_ns, uint64_t faults, const RtPmu& delta, int core) {
  uint64_t idx = _cbhead.load(std::memory_order_relaxed);
  auto& c      = _cbring[idx % kCbRing];
  c._t0        = t0;
  c._wall      = wall_ns;
  c._cpu       = cpu_ns;
  c._faults    = faults;
  c._pmu       = delta;
  c._core      = core;
  _audiocore.store(core, std::memory_order_relaxed);
#if defined(__linux__)
  if (0 == _audiotid.load(std::memory_order_relaxed))
    _audiotid.store(syscall(SYS_gettid), std::memory_order_relaxed);
#endif
  _cbhead.store(idx + 1, std::memory_order_release);
}

} // namespace ork::audio::singularity
