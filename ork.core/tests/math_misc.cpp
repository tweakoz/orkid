#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <ork/math/misc_math.h>

using namespace ork;

TEST(npot_dynamic)
{
    for( int i=0; i<16; i++ ){

        size_t x = (size_t(rand())&0xffff);
        size_t y = nextPowerOfTwo(x);
        printf( "npot1: x<0x%zx> y<0x%zx>\n", x,y);
    }

    for( int i=0; i<16; i++ ){

        size_t x = size_t(rand());

        size_t y = nextPowerOfTwo(x);
        printf( "npot1: x<0x%zx> y<0x%zx>\n", x,y);
    }

    for( int i=0; i<16; i++ ){

        size_t x = (size_t(rand())<<32)
                 | size_t(rand());

        size_t y = nextPowerOfTwo(x);
        printf( "npot1: x<0x%zx> y<0x%zx>\n", x,y);
    }

    CHECK_EQUAL(1,nextPowerOfTwo(1));
    CHECK_EQUAL(4,nextPowerOfTwo(3));
    CHECK_EQUAL(8,nextPowerOfTwo(5));
    CHECK_EQUAL(16,nextPowerOfTwo(16));
    CHECK_EQUAL(32,nextPowerOfTwo(17));
    CHECK_EQUAL(0x8000,nextPowerOfTwo(0x7fff));
    CHECK_EQUAL(0x800000,nextPowerOfTwo(0x7fffff));
}

TEST(npot_static)
{
    printf( "npotstatic<0x%zx:0x%zx>\n", 1L, nextPowerOfTwo<1>());
    printf( "npotstatic<0x%zx:0x%zx>\n", 3L, nextPowerOfTwo<3>());
    printf( "npotstatic<0x%zx:0x%zx>\n", 5L, nextPowerOfTwo<5>());
    printf( "npotstatic<0x%zx:0x%zx>\n", 16L, nextPowerOfTwo<16>());
    printf( "npotstatic<0x%zx:0x%zx>\n", 17L, nextPowerOfTwo<17>());
    printf( "npotstatic<0x%zx:0x%zx>\n", 0x7fffL, nextPowerOfTwo<0x7fff>());
    printf( "npotstatic<0x%zx:0x%zx>\n", 0x7fffffL, nextPowerOfTwo<0x7fffff>());
    printf( "npotstatic<0x%zx:0x%zx>\n", 0x7fffffffL, nextPowerOfTwo<0x7fffffff>());
    printf( "npotstatic<0x%zx:0x%zx>\n", 0x7fffffffffL, nextPowerOfTwo<0x7fffffffff>());

    CHECK_EQUAL(1,nextPowerOfTwo<1>());
    CHECK_EQUAL(4,nextPowerOfTwo<3>());
    CHECK_EQUAL(8,nextPowerOfTwo<5>());
    CHECK_EQUAL(16,nextPowerOfTwo<16>());
    CHECK_EQUAL(32,nextPowerOfTwo<17>());
    CHECK_EQUAL(0x8000,nextPowerOfTwo<0x7fff>());
    CHECK_EQUAL(0x800000,nextPowerOfTwo<0x7fffff>());
}

TEST(clz_dynamic)
{
    printf( "clz_dynamic<0x%zx:%zu>\n", 1L, clz(1));
    printf( "clz_dynamic<0x%zx:%zu>\n", 3L, clz(3));
    printf( "clz_dynamic<0x%zx:%zu>\n", 5L, clz(5));
    printf( "clz_dynamic<0x%zx:%zu>\n", 16L, clz(16));
    printf( "clz_dynamic<0x%zx:%zu>\n", 17L, clz(17));
    printf( "clz_dynamic<0x%zx:%zu>\n", 0xfffffffffffffL, clz(0xfffffffffffff));
    printf( "clz_dynamic<0x%zx:%zu>\n", 0xffffffffffffffL, clz(0xffffffffffffff));
    printf( "clz_dynamic<0x%zx:%zu>\n", 0xfffffffffffffffL, clz(0xfffffffffffffff));
    printf( "clz_dynamic<0x%zx:%zu>\n", 0xffffffffffffffffL, clz(0xffffffffffffffff));
}

TEST(clz_static)
{
    printf( "clz_static<0x%zx:%zu>\n", 1L, clz<1>());
    printf( "clz_static<0x%zx:%zu>\n", 3L, clz<3>());
    printf( "clz_static<0x%zx:%zu>\n", 5L, clz<5>());
    printf( "clz_static<0x%zx:%zu>\n", 16L, clz<16>());
    printf( "clz_static<0x%zx:%zu>\n", 17L, clz<17>());
    printf( "clz_static<0x%zx:%zu>\n", 0xfffffffffffffL, clz<0xfffffffffffff>());
    printf( "clz_static<0x%zx:%zu>\n", 0xffffffffffffffL, clz<0xffffffffffffff>());
    printf( "clz_static<0x%zx:%zu>\n", 0xfffffffffffffffL, clz<0xfffffffffffffff>());
    printf( "clz_static<0x%zx:%zu>\n", 0xffffffffffffffffL, clz<0xffffffffffffffff>());
}

TEST(numbits_dynamic)
{
    printf( "numbits_dynamic<0x%zx:%zu>\n", 1L, numbits(1));
    printf( "numbits_dynamic<0x%zx:%zu>\n", 3L, numbits(3));
    printf( "numbits_dynamic<0x%zx:%zu>\n", 5L, numbits(5));
    printf( "numbits_dynamic<0x%zx:%zu>\n", 16L, numbits(16));
    printf( "numbits_dynamic<0x%zx:%zu>\n", 17L, numbits(17));
    printf( "numbits_dynamic<0x%zx:%zu>\n", 255L, numbits(255));
    printf( "numbits_dynamic<0x%zx:%zu>\n", 256L, numbits(256));
    printf( "numbits_dynamic<0x%zx:%zu>\n", 257L, numbits(257));
    printf( "numbits_dynamic<0x%zx:%zu>\n", 0xfffffffffffffL, numbits(0xfffffffffffff));
    printf( "numbits_dynamic<0x%zx:%zu>\n", 0xffffffffffffffL, numbits(0xffffffffffffff));
    printf( "numbits_dynamic<0x%zx:%zu>\n", 0xfffffffffffffffL, numbits(0xfffffffffffffff));
    printf( "numbits_dynamic<0x%zx:%zu>\n", 0xffffffffffffffffL, numbits(0xffffffffffffffff));
}

TEST(numbits_static)
{
    printf( "numbits_static<0x%zx:%zu>\n", 1L, numbits<1>());
    printf( "numbits_static<0x%zx:%zu>\n", 3L, numbits<3>());
    printf( "numbits_static<0x%zx:%zu>\n", 5L, numbits<5>());
    printf( "numbits_static<0x%zx:%zu>\n", 16L, numbits<16>());
    printf( "numbits_static<0x%zx:%zu>\n", 17L, numbits<17>());
    printf( "numbits_static<0x%zx:%zu>\n", 255L, numbits<255>());
    printf( "numbits_static<0x%zx:%zu>\n", 256L, numbits<256>());
    printf( "numbits_static<0x%zx:%zu>\n", 257L, numbits<257>());
    printf( "numbits_static<0x%zx:%zu>\n", 0xfffffffffffffL, numbits<0xfffffffffffff>());
    printf( "numbits_static<0x%zx:%zu>\n", 0xffffffffffffffL, numbits<0xffffffffffffff>());
    printf( "numbits_static<0x%zx:%zu>\n", 0xfffffffffffffffL, numbits<0xfffffffffffffff>());
    printf( "numbits_static<0x%zx:%zu>\n", 0xffffffffffffffffL, numbits<0xffffffffffffffff>());
}
