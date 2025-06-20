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
  _harness([](void* dest, const void* src, size_t n){_memcpy_neon(dest,src,n);},"memcpy_fast");
}
TEST(_memcpy_cache_optimized) {
  _harness([](void* dest, const void* src, size_t n){_memcpy_cache_optimized(dest,src,n);},"memcpy_fast");
}
TEST(memcpy_prefetch) {
  _harness([](void* dest, const void* src, size_t n){_memcpy_prefetch(dest,src,n);},"memcpy_fast");
}
TEST(memcpy_asm) {
  _harness([](void* dest, const void* src, size_t n){_memcpy_asm(dest,src,n);},"memcpy_fast");
}
#if defined(__APPLE__)
TEST(memcpy_accel) {
  _harness([](void* dest, const void* src, size_t n){_memcpy_accel(dest,src,n);},"memcpy_fast");
}
#endif

#endif



/*
test results m3 max jun 20, 2025

memcpy_c:     NumRuns<128> GiB_copied<32.000000> elapsed<614.298820 msec> ops/sec<208.367647> GiB/sec<52.091912>
memcpy_std:   NumRuns<128> GiB_copied<32.000000> elapsed<583.938658 msec> ops/sec<219.201106> GiB/sec<54.800277>
memcpy_fast:  NumRuns<128> GiB_copied<32.000000> elapsed<297.715187 msec> ops/sec<429.941117> GiB/sec<109.285409>
memcpy_async: NumRuns<128> GiB_copied<32.000000> elapsed<298.758864 msec> ops/sec<428.439170> GiB/sec<107.109793>
memcpy_fast:  NumRuns<128> GiB_copied<32.000000> elapsed<712.167978 msec> ops/sec<179.732877> GiB/sec<44.933219>
memcpy_fast:  NumRuns<128> GiB_copied<32.000000> elapsed<706.161022 msec> ops/sec<181.261775> GiB/sec<45.315444>
memcpy_fast:  NumRuns<128> GiB_copied<32.000000> elapsed<712.764502 msec> ops/sec<179.582456> GiB/sec<44.895614>
memcpy_fast:  NumRuns<128> GiB_copied<32.000000> elapsed<581.959248 msec> ops/sec<219.946672> GiB/sec<54.986668>


*/