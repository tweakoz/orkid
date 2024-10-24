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
#include <ork/kernel/thread.h>
#include <cstddef>

#if defined(__APPLE__)
#include <Accelerate/Accelerate.h>
#endif

#if defined(ORK_ARCHITECTURE_ARM_64)
#include <arm_neon.h>
namespace ork {
inline void _memcpy_neon(void* dest, const void* src, size_t n) { // arm neon
    uint8_t* d = static_cast<uint8_t*>(dest);
    const uint8_t* s = static_cast<const uint8_t*>(src);
    size_t i = 0;

    for (; i + 15 < n; i += 16) {
        vst1q_u8(d + i, vld1q_u8(s + i));
    }
    for (; i < n; ++i) {
        d[i] = s[i];
    }
}

inline void _memcpy_cache_optimized(void* dest, const void* src, size_t n) {
    constexpr size_t cache_line_size = 64;
    uint8_t* d = static_cast<uint8_t*>(dest);
    const uint8_t* s = static_cast<const uint8_t*>(src);
    
    size_t i = 0;
    for (; i + cache_line_size <= n; i += cache_line_size) {
        __builtin_prefetch(s + i + cache_line_size);
        __builtin_prefetch(d + i + cache_line_size);
        std::copy_n(s + i, cache_line_size, d + i);
    }
    std::copy_n(s + i, n - i, d + i);
}

inline void _memcpy_prefetch(void* dest, const void* src, size_t n) {
    uint8_t* d = static_cast<uint8_t*>(dest);
    const uint8_t* s = static_cast<const uint8_t*>(src);

    size_t i = 0;
    for (; i + 64 <= n; i += 64) {
        __builtin_prefetch(s + i + 64);
        std::copy_n(s + i, 64, d + i);
    }
    for (; i < n; ++i) {
        d[i] = s[i];
    }
}

inline void _memcpy_asm(void* dest, const void* src, size_t n) {
    asm volatile (
        "mov x0, %[src] \n"
        "mov x1, %[dest] \n"
        "mov x2, %[n] \n"
        "1: \n"
        "ldr q0, [x0], #16 \n"
        "subs x2, x2, #16 \n"
        "str q0, [x1], #16 \n"
        "bhi 1b \n"
        :
        : [dest] "r" (dest), [src] "r" (src), [n] "r" (n)
        : "x0", "x1", "x2", "memory"
    );
}

#if defined(__APPLE__)

inline void _memcpy_accel(void* dest, const void* src, size_t n) { // accelerate
    vDSP_mmov((const float*)src, (float*)dest, n / sizeof(float), 1, n / sizeof(float), n / sizeof(float));
}

struct AsyncCopyOperation{
  AsyncCopyOperation(int n) : _async_counter(n) {}
  void finish(){
    while (_async_counter.load()) {
      ork::usleep(0);
      //std::this_thread::yield();
    }
  }
  std::atomic<int> _async_counter;
};
using async_copy_op_ptr_t = std::shared_ptr<AsyncCopyOperation>;

struct copy_rec {
    async_copy_op_ptr_t _async_op = nullptr;
    void* _dest = nullptr;
    const void* _src = nullptr;
    size_t _n = 0;
};


using mpmc_queue_t = ork::MpMcBoundedQueue<copy_rec,4096>;

struct ParallelMemoryCopier {

  static constexpr size_t NUM_THREADS = 3;
  mpmc_queue_t _mem_op_q;
  std::vector<thread_ptr_t> _threads;
  std::atomic<int> _run_state = -1;
  //////////////////////////////////
  void enqueue(copy_rec op) {
    _mem_op_q.push(op);
  }
  //////////////////////////////////
  static void _thread_impl(ParallelMemoryCopier* pmc) {

    while (pmc->_run_state<1) {
      copy_rec op;
      if (pmc->_mem_op_q.try_pop(op)) {
        memcpy(op._dest, op._src, op._n);
        op._async_op->_async_counter.fetch_add(-1);
      }
      else{
        std::this_thread::yield();
        //::usleep(0);
      }
    }
    pmc->_run_state++;
  }
  //////////////////////////////////
  ParallelMemoryCopier(){
    _run_state = 0;
    auto L = [this](anyp data) {
      ork::SetCurrentThreadName("ParallelMemoryCopier");
      _thread_impl(this);
    };
    for (int i = 0; i < NUM_THREADS; ++i) {
      thread_ptr_t thr = std::make_shared<Thread>();
      thr->start(L);
      _threads.push_back(thr);
    }
  }
  //////////////////////////////////
  ~ParallelMemoryCopier(){
    _run_state = 1;
    for (auto thr : _threads) {
      thr->join();
    }
  }
  //////////////////////////////////
};
using pmemcpy_ptr_t = std::shared_ptr<ParallelMemoryCopier>;

static pmemcpy_ptr_t _gmemopq = std::make_shared<ParallelMemoryCopier>();

void memcpy_fast(void* dest, const void* src, size_t length) {
  if(length>(8<<20)){
    size_t part_size = 4<<20;
    // split into N parts
    size_t num_parts = (length + part_size - 1) / part_size;
    copy_rec crec;
    async_copy_op_ptr_t aop = std::make_shared<AsyncCopyOperation>(num_parts-1);
    for (size_t i = 0; i < num_parts; ++i) {
      size_t offset = i * part_size;
      size_t this_part_size = (i == num_parts - 1) ? length - offset : part_size;
      auto doff = (uint8_t*) dest + offset;
      auto soff = (uint8_t*) src + offset;
      if(i==(num_parts-1)){
        memcpy(doff, soff, this_part_size);
      }
      else{
        crec._async_op = aop;
        crec._dest = doff;
        crec._src = soff;
        crec._n = this_part_size;
        _gmemopq->enqueue(crec);
      }
    }
    aop->finish();
  }
  else{
    memcpy(dest, src, length);
    //_memcpy_accel(dest, src, length);
  }
}


#else
void memcpy_fast(void* dest, const void* src, size_t length) {
  ::memcpy(dest, src, length);
}
#endif
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