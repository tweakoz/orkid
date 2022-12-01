////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <ork/math/misc_math.h>
#include <ork/orkstd.h>

using namespace ork;


/*struct Aligner {

    using addr_int_t = uint64_t;

    uint8_t* _dst;
    const uint8_t* _src;
    size_t _size;

    addr_int_t _dst_unaligned;
    addr_int_t _dst_aligned;
    addr_int_t _src_unaligned;
    addr_int_t _src_aligned;
    addr_int_t _alignment;

    Aligner(uint8_t* dst, const uint8_t* src, size_t size, size_t alignment)
        : _dst(dst)
        , _src(src)
        , _size(size) {

        _src = src;
        _dst = dst;
        _size = size;
        _alignment = alignment;

        addr_int_t alignm1 = addr_int_t(alignment-1);
        addr_int_t align_mask = addr_int_t(~alignm1);

        _src_aligned = (addr_int_t(_src)+alignm1)&align_mask;
        _src_unaligned = addr_int_t(_src);

        _dst_aligned = (addr_int_t(_dst)+alignm1)&align_mask;
        _dst_unaligned = addr_int_t(_dst);

        bool src_is_aligned = (src_aligned&alignm1)==0;
        bool dst_is_aligned = (dst_unaligned&alignm1)==0;

        printf( "align_mask<%p> dst_unaligned<%p> dst_aligned<%p> dst_is_aligned<%d> dst_size_is_aligned<%d> dst_size_unaligned<%d> dst_size_aligned<%d> _size<%d>\n", 
                align_mask, 
                dst_unaligned, 
                dst_aligned, 
                int(dst_is_aligned), 
                int(dst_size_is_aligned), 
                dst_size_unaligned, 
                dst_size_aligned, 
                _size);

        int iter_count =0;
        while(_size>0){

            //printf( "src<%p> align_mask<%p> src_aligned<%p> is_aligned<%d>\n", _src, align_mask, src_aligned, int(src_is_aligned));

            uint64_t dst_unaligned = uint64_t(_dst);
            uint64_t dst_aligned = (dst_unaligned+alignm1)&align_mask;
            size_t dst_size_unaligned = (dst_aligned-dst_unaligned);
            uint64_t dst_size_aligned = (dst_size_unaligned+alignm1)&align_mask;
            bool dst_size_is_aligned = (dst_size_unaligned&alignm1)==0;


            if(dst_is_aligned and dst_size_is_aligned){
                printf( "path1\n");
                _dst += dst_size_aligned;
                _size -= dst_size_aligned;
            }
            else if (dst_is_aligned and not dst_size_is_aligned){
                printf( "path2\n");
                _dst += dst_size_unaligned;
                _size -= dst_size_unaligned;
            }
            else if (not dst_is_aligned and dst_size_is_aligned){
                printf( "path3\n");
                _dst += dst_size_unaligned;
                _size -= dst_size_unaligned;
            }
            else{
                printf( "path4(slowpath)\n");
                OrkAssert(not dst_is_aligned);
                OrkAssert(not dst_size_is_aligned);
                _dst += dst_size_unaligned;
                _size -= dst_size_unaligned;
            }

            if( iter_count>4){
                OrkAssert(false);
            }
            iter_count++;
        }

    }
};

TEST(aligner){

    Aligner a((uint8_t*)0xff07,(const uint8_t*)0x8000,256,16);

    OrkAssert(false);
}
*/

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

TEST(bitsToHold_dynamic)
{
    printf( "bitsToHold_dynamic<0x%zx:%zu>\n", 1L, bitsToHold(1));
    printf( "bitsToHold_dynamic<0x%zx:%zu>\n", 3L, bitsToHold(3));
    printf( "bitsToHold_dynamic<0x%zx:%zu>\n", 5L, bitsToHold(5));
    printf( "bitsToHold_dynamic<0x%zx:%zu>\n", 16L, bitsToHold(16));
    printf( "bitsToHold_dynamic<0x%zx:%zu>\n", 17L, bitsToHold(17));
    printf( "bitsToHold_dynamic<0x%zx:%zu>\n", 255L, bitsToHold(255));
    printf( "bitsToHold_dynamic<0x%zx:%zu>\n", 256L, bitsToHold(256));
    printf( "bitsToHold_dynamic<0x%zx:%zu>\n", 257L, bitsToHold(257));
    printf( "bitsToHold_dynamic<0x%zx:%zu>\n", 0xfffffffffffffL, bitsToHold(0xfffffffffffff));
    printf( "bitsToHold_dynamic<0x%zx:%zu>\n", 0xffffffffffffffL, bitsToHold(0xffffffffffffff));
    printf( "bitsToHold_dynamic<0x%zx:%zu>\n", 0xfffffffffffffffL, bitsToHold(0xfffffffffffffff));
    printf( "bitsToHold_dynamic<0x%zx:%zu>\n", 0xffffffffffffffffL, bitsToHold(0xffffffffffffffff));
}

TEST(bitsToHold_static)
{
    printf( "bitsToHold_static<0x%zx:%zu>\n", 1L, bitsToHold<1>());
    printf( "bitsToHold_static<0x%zx:%zu>\n", 3L, bitsToHold<3>());
    printf( "bitsToHold_static<0x%zx:%zu>\n", 5L, bitsToHold<5>());
    printf( "bitsToHold_static<0x%zx:%zu>\n", 16L, bitsToHold<16>());
    printf( "bitsToHold_static<0x%zx:%zu>\n", 17L, bitsToHold<17>());
    printf( "bitsToHold_static<0x%zx:%zu>\n", 255L, bitsToHold<255>());
    printf( "bitsToHold_static<0x%zx:%zu>\n", 256L, bitsToHold<256>());
    printf( "bitsToHold_static<0x%zx:%zu>\n", 257L, bitsToHold<257>());
    printf( "bitsToHold_static<0x%zx:%zu>\n", 0xfffffffffffffL, bitsToHold<0xfffffffffffff>());
    printf( "bitsToHold_static<0x%zx:%zu>\n", 0xffffffffffffffL, bitsToHold<0xffffffffffffff>());
    printf( "bitsToHold_static<0x%zx:%zu>\n", 0xfffffffffffffffL, bitsToHold<0xfffffffffffffff>());
    printf( "bitsToHold_static<0x%zx:%zu>\n", 0xffffffffffffffffL, bitsToHold<0xffffffffffffffff>());
}
