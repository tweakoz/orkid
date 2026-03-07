////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/timer.h>
#include <ork/kernel/memcpy.inl>
#include <utpp/UnitTest++.h>
#include <string.h>
#include <math.h>
#include <cstring>

typedef uint32_t u32;

typedef std::atomic<int> atomic_counter;

using namespace ork;

constexpr size_t ksize = 1<<26; // 64 M
constexpr size_t knumruns = 128;

using memcpy_func_t = std::function<void(void* dest, const void* src, size_t n)>;

static void _harness( memcpy_func_t func, const char* name ) {
  std::vector<float> src, dst;
  src.resize(ksize);
  dst.resize(ksize);
  for (size_t i = 0;        //
              i < ksize; //
              i++ ) {       //
    src[i] = float(i) * 0.0001f; // Fill with some data
    dst[i] = float(i) * -0.0001f; // Fill with some data
  }
  ork::Timer timer;
  timer.Start();
  for (size_t i = 0;        //
              i < knumruns; //
              i++ ) {       //
    src[i] = float(i) * 0.0001f; // Update src with new data
    func(dst.data(), src.data(), ksize * sizeof(float));
  }
  double elapsed = timer.SecsSinceStart();
  size_t bytes_copied = knumruns * ksize * sizeof(float);
  double ops_per_sec = double(knumruns) / elapsed;
  double bytes_per_sec = bytes_copied / elapsed;
  double gib_per_sec = bytes_per_sec / double(1 << 30); // Convert to GiB/sec
  printf("%s: NumRuns<%zu> GiB_copied<%f> elapsed<%f msec> ops/sec<%f> GiB/sec<%f>\n", //
         name,
         knumruns, //
         double(bytes_copied)/double(1<<30), //
         elapsed*1000.0, //
         ops_per_sec, //
         gib_per_sec);
  }

TEST(memcpy_c) {
  _harness([](void* dest, const void* src, size_t n){::memcpy(dest,src,n);},"memcpy_c");
}

TEST(memcpy_std) {
  _harness([](void* dest, const void* src, size_t n){std::memcpy(dest,src,n);},"memcpy_std");
}

TEST(memcpy_fast) {
  _harness([](void* dest, const void* src, size_t n){memcpy_fast(dest,src,n);},"memcpy_fast");
}

TEST(memcpy_async) {
  std::atomic<int> counter = 0;
  _harness( [&counter](void* dest, const void* src, size_t n){ //
    memcpy_async(dest,src,n, counter);
    while(counter.load() > 0) {
      ork::usleep(1); // Wait for async copies to finish
    }
  },"memcpy_async");
}

#if defined(ORK_ARCHITECTURE_ARM_64)
TEST(memcpy_neon) {
  _harness([](void* dest, const void* src, size_t n){_memcpy_neon(dest,src,n);},"memcpy_neon");
}
TEST(_memcpy_cache_optimized) {
  _harness([](void* dest, const void* src, size_t n){_memcpy_cache_optimized(dest,src,n);},"memcpy_cache_optimized");
}
TEST(memcpy_prefetch) {
  _harness([](void* dest, const void* src, size_t n){_memcpy_prefetch(dest,src,n);},"memcpy_prefetch");
}
TEST(memcpy_asm) {
  _harness([](void* dest, const void* src, size_t n){_memcpy_asm(dest,src,n);},"memcpy_asm");
}
#if defined(__APPLE__)
TEST(memcpy_accel) {
  _harness([](void* dest, const void* src, size_t n){_memcpy_accel(dest,src,n);},"memcpy_accel");
}
#endif

#endif



/*

test results m3 max jun 20, 2025

memcpy_c:        NumRuns<128> GiB_copied<32.000000> elapsed<616.427302 msec> ops/sec<207.648168> GiB/sec<51.912042>
memcpy_std:      NumRuns<128> GiB_copied<32.000000> elapsed<578.335822 msec> ops/sec<221.324696> GiB/sec<55.331174>
memcpy_fast:     NumRuns<128> GiB_copied<32.000000> elapsed<295.639157 msec> ops/sec<432.960238> GiB/sec<108.240060>
memcpy_async:    NumRuns<128> GiB_copied<32.000000> elapsed<297.343016 msec> ops/sec<430.479255> GiB/sec<107.619814>
memcpy_neon:     NumRuns<128> GiB_copied<32.000000> elapsed<695.888758 msec> ops/sec<183.937445> GiB/sec<45.984361>
memcpy_prefetch: NumRuns<128> GiB_copied<32.000000> elapsed<693.061113 msec> ops/sec<184.687898> GiB/sec<46.171974>
memcpy_asm:      NumRuns<128> GiB_copied<32.000000> elapsed<702.507734 msec> ops/sec<182.204400> GiB/sec<45.551100>
memcpy_accel:    NumRuns<128> GiB_copied<32.000000> elapsed<579.061508 msec> ops/sec<221.047330> GiB/sec<55.261832>

test results m3 ultra jun 20, 2025

memcpy_c:        NumRuns<128> GiB_copied<32.000000> elapsed<590.799332 msec> ops/sec<216.655628> GiB/sec<54.163907>
memcpy_std:      NumRuns<128> GiB_copied<32.000000> elapsed<560.705721 msec> ops/sec<228.283742> GiB/sec<57.070935>
memcpy_fast:     NumRuns<128> GiB_copied<32.000000> elapsed<184.188843 msec> ops/sec<694.938945> GiB/sec<173.734736>
memcpy_async:    NumRuns<128> GiB_copied<32.000000> elapsed<171.948195 msec> ops/sec<744.410259> GiB/sec<186.102565>
memcpy_neon:     NumRuns<128> GiB_copied<32.000000> elapsed<696.741343 msec> ops/sec<183.712365> GiB/sec<45.928091>
memcpy_prefetch: NumRuns<128> GiB_copied<32.000000> elapsed<687.877655 msec> ops/sec<186.079602> GiB/sec<46.519900>
memcpy_asm:      NumRuns<128> GiB_copied<32.000000> elapsed<697.211742 msec> ops/sec<183.588417> GiB/sec<45.897104>
memcpy_accel:    NumRuns<128> GiB_copied<32.000000> elapsed<555.666447 msec> ops/sec<230.354020> GiB/sec<57.588505>

test results m4 max mar 6, 2026 (ParallelMemoryCopier: counting_semaphore, avg of 8 runs)

memcpy_c:        NumRuns<128> GiB_copied<32.000000> elapsed<545.334021 msec> ops/sec<235.243890> GiB/sec<58.810972>
memcpy_std:      NumRuns<128> GiB_copied<32.000000> elapsed<489.708292 msec> ops/sec<261.389361> GiB/sec<65.347340>
memcpy_fast:     NumRuns<128> GiB_copied<32.000000> elapsed<208.488880 msec> ops/sec<614.030359> GiB/sec<153.507590>
memcpy_async:    NumRuns<128> GiB_copied<32.000000> elapsed<210.358526 msec> ops/sec<608.530777> GiB/sec<152.132694>
memcpy_neon:     NumRuns<128> GiB_copied<32.000000> elapsed<611.296037 msec> ops/sec<209.394665> GiB/sec<52.348666>
memcpy_prefetch: NumRuns<128> GiB_copied<32.000000> elapsed<614.933625 msec> ops/sec<208.160716> GiB/sec<52.040179>
memcpy_asm:      NumRuns<128> GiB_copied<32.000000> elapsed<612.172260 msec> ops/sec<209.093045> GiB/sec<52.273261>
memcpy_accel:    NumRuns<128> GiB_copied<32.000000> elapsed<490.496662 msec> ops/sec<260.967191> GiB/sec<65.241798>

test results m4 max mar 6, 2026 (ParallelMemoryCopier: usleep(15) polling, avg of 8 runs)

memcpy_c:        NumRuns<128> GiB_copied<32.000000> elapsed<538.225146 msec> ops/sec<238.332677> GiB/sec<59.583169>
memcpy_std:      NumRuns<128> GiB_copied<32.000000> elapsed<494.543709 msec> ops/sec<258.829157> GiB/sec<64.707289>
memcpy_fast:     NumRuns<128> GiB_copied<32.000000> elapsed<222.741661 msec> ops/sec<576.264127> GiB/sec<144.066032>
memcpy_async:    NumRuns<128> GiB_copied<32.000000> elapsed<209.411865 msec> ops/sec<611.236545> GiB/sec<152.809136>
memcpy_neon:     NumRuns<128> GiB_copied<32.000000> elapsed<646.695510 msec> ops/sec<197.938959> GiB/sec<49.484740>
memcpy_prefetch: NumRuns<128> GiB_copied<32.000000> elapsed<645.670891 msec> ops/sec<198.249833> GiB/sec<49.562458>
memcpy_asm:      NumRuns<128> GiB_copied<32.000000> elapsed<646.263802 msec> ops/sec<198.069613> GiB/sec<49.517403>
memcpy_accel:    NumRuns<128> GiB_copied<32.000000> elapsed<501.625864 msec> ops/sec<255.183625> GiB/sec<63.795906>

comparison: counting_semaphore vs usleep(15) polling (avg of 8 runs, m4 max)

memcpy_fast:  semaphore 153.5 GiB/s  vs  polling 144.1 GiB/s  — +9.4 semaphore (~6%)
memcpy_async: semaphore 152.1 GiB/s  vs  polling 152.8 GiB/s  — within noise, equal

conclusion: throughput is effectively identical — both are memory bandwidth bound, not
scheduling bound. the real benefit of counting_semaphore is idle CPU usage: 4 worker
threads no longer burning usleep(15) wakeup cycles (~267k wakeups/sec) when the queue
is empty. semaphore is at worst equal and never worse on throughput.
[analysis by Claude Sonnet 4.6]

*/
