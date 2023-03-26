////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <memory.h>
#include <atomic>
#include <ork/kernel/opq.h>

#if defined(ORK_ARCHITECTURE_ARM_64)
namespace ork {
void memcpy_fast(void* dest, const void* src, size_t length) {
  ::memcpy(dest, src, length);
}
} // namespace ork
#elif defined(ORK_ARCHITECTURE_X86_64)
#include <immintrin.h>

namespace ork {

static void _memcpy_sse(uint8_t* dst, const uint8_t* src, size_t size) {
  size_t stride = 2 * sizeof(__m128);
  while (size) {
    __m128 a = _mm_load_ps((float*)(src + 0 * sizeof(__m128)));
    __m128 b = _mm_load_ps((float*)(src + 1 * sizeof(__m128)));
    _mm_store_ps((float*)(dst + 0 * sizeof(__m128)), a);
    _mm_store_ps((float*)(dst + 1 * sizeof(__m128)), b);

    size -= stride;
    src += stride;
    dst += stride;
  }
}

static void _memcpy_ssenocache( uint8_t* dst, //
                               const uint8_t* src, //
                               size_t size) { //
  size_t stride = 2 * sizeof(__m128);
  while (size) {
    __m128 a = _mm_load_ps((float*)(src + 0 * sizeof(__m128)));
    __m128 b = _mm_load_ps((float*)(src + 1 * sizeof(__m128)));
    _mm_stream_ps((float*)(dst + 0 * sizeof(__m128)), a);
    _mm_stream_ps((float*)(dst + 1 * sizeof(__m128)), b);

    size -= stride;
    src += stride;
    dst += stride;
  }
}

static void _memcpy_avxnocache( uint8_t* dst, //
                               uint8_t* src, //
                               size_t size) { //

  size_t stride = 2 * sizeof(__m256i);
  while (size) {
    __m256i a = _mm256_load_si256((__m256i*)src + 0);
    __m256i b = _mm256_load_si256((__m256i*)src + 1);
    _mm256_stream_si256((__m256i*)dst + 0, a);
    _mm256_stream_si256((__m256i*)dst + 1, b);

    size -= stride;
    src += stride;
    dst += stride;
  }
}

void memcpy_async(void* dest, //
                  const void* src, //
                  size_t length, //
                  std::atomic<int>& async_counter) { //

  auto write_base = (uint8_t*)dest;
  auto read_base  = (const uint8_t*)src;

  size_t nlen  = length;
  size_t index = 0;

  while (nlen > 0) {

    size_t this_iter = 512 << 10;
    if (this_iter > nlen)
      this_iter = nlen;

    async_counter.fetch_add(1);
    auto op = [=, &async_counter]() {
      memcpy((write_base + index), (read_base + index), this_iter);
      async_counter.fetch_add(-1);
    };
    nlen -= this_iter;
    index += this_iter;
    opq::concurrentQueue()->enqueue(op);
  }
}

void memcpy_parallel( void* dest, //
                      const void* src, //
                      size_t length) { //

  std::atomic<int> async_counter = 0;
  memcpy_async(dest, src, length, async_counter);
  while (async_counter.load()) {
    ork::usleep(0);
  }
}

void memcpy_fast( void* dest, //
                  const void* src, //
                  size_t length) { //

  //memcpy_parallel(dest, src, length);
  memcpy(dest, src, length);
}

} // namespace ork
#else
#error // architecture not supported yet..
#endif