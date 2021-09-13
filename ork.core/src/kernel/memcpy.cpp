#include <stdint.h>
#include <memory.h>

#if defined(ORK_ARCHITECTURE_ARM_64)
namespace ork {
void memcpy_fast(void* dest, const void* src, size_t length){
  ::memcpy(dest,src,length);
}
}
#elif defined(ORK_ARCHITECTURE_X86_64)
#include <immintrin.h>

namespace ork {

static void memcpy_sse(uint8_t* dst, uint8_t* src, size_t size)
{
    size_t stride = 2 * sizeof(__m128);
    while (size)
    {
        __m128 a = _mm_load_ps((float*)(src + 0*sizeof(__m128)));
        __m128 b = _mm_load_ps((float*)(src + 1*sizeof(__m128)));
        _mm_store_ps((float*)(dst + 0*sizeof(__m128)), a);
        _mm_store_ps((float*)(dst + 1*sizeof(__m128)), b);

        size -= stride;
        src += stride;
        dst += stride;
    }
}

static void memcpy_ssenocache(uint8_t* dst, uint8_t* src, size_t size)
{
    size_t stride = 2 * sizeof(__m128);
    while (size)
    {
        __m128 a = _mm_load_ps((float*)(src + 0*sizeof(__m128)));
        __m128 b = _mm_load_ps((float*)(src + 1*sizeof(__m128)));
        _mm_stream_ps((float*)(dst + 0*sizeof(__m128)), a);
        _mm_stream_ps((float*)(dst + 1*sizeof(__m128)), b);

        size -= stride;
        src += stride;
        dst += stride;
    }
}

static void memcpy_avxnocache(uint8_t* dst, uint8_t* src, size_t size)
{
    size_t stride = 2 * sizeof(__m256i);
    while (size)
    {
        __m256i a = _mm256_load_si256((__m256i*)src + 0);
        __m256i b = _mm256_load_si256((__m256i*)src + 1);
        _mm256_stream_si256((__m256i*)dst + 0, a);
        _mm256_stream_si256((__m256i*)dst + 1, b);

        size -= stride;
        src += stride;
        dst += stride;
    }
}
void memcpy_fast(void* dest, const void* src, size_t length){
  //memcpy_sse((uint8_t*) dest, (uint8_t*) src, length );
  memcpy_ssenocache((uint8_t*) dest, (uint8_t*) src, length );
  //::memcpy(dest,src,length);
}



} // namespace ork
#else 
#error // architecture not supported yet..
#endif