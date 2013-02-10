////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

//  Copyright 2001, 2004 Daryle Walker.  Use, modification, and distribution are
//  subject to the Boost Software License, Version 1.0.  (See accompanying file
//  LICENSE_1_0.txt or a copy at <http://www.boost.org/LICENSE_1_0.txt>.)

#ifndef BOOST_CRC_H
#define BOOST_CRC_H

//#define NO_LONG_LONG
#include <stdint.h>

namespace boost
{

// If we have a 64-bit integer type, then a 64-bit CRC looks just like the
// usual sort of implementation. (See Ross Williams' excellent introduction
// A PAINLESS GUIDE TO CRC ERROR DETECTION ALGORITHMS, available from
// ftp://ftp.rocksoft.com/papers/crc_v3.txt or several other net sites.)
// If we have no working 64-bit type, then fake it with two 32-bit registers.
//
// The present implementation is a normal (not "reflected", in Williams'
// terms) 64-bit CRC, using initial all-ones register contents and a final
// bit inversion. The chosen polynomial is borrowed from the DLT1 spec
// (ECMA-182, available from http://www.ecma.ch/ecma1/STAND/ECMA-182.HTM):
//
// x^64 + x^62 + x^57 + x^55 + x^54 + x^53 + x^52 + x^47 + x^46 + x^45 +
// x^40 + x^39 + x^38 + x^37 + x^35 + x^33 + x^32 + x^31 + x^29 + x^27 +
// x^24 + x^23 + x^22 + x^21 + x^19 + x^17 + x^13 + x^12 + x^10 + x^9 +
// x^7 + x^4 + x + 1

#ifdef NO_LONG_LONG

// Constant table for CRC calculation
extern const uint32_t crc_table0[];
extern const uint32_t crc_table1[];

// crc0 represents the LSBs of the 64-bit value, crc1 the MSBs. Note that
// with crc0 placed first, the output of 32-bit and 64-bit implementations
// will be bit-compatible only on little-endian architectures. If it were
// important to make the two possible implementations bit-compatible on
// all machines, we could do a configure test to decide how to order the
// two fields, but it seems not worth the trouble.

struct Crc64 { uint32_t crc0; uint32_t crc1; };

// Initialize a CRC accumulator
inline void
crc64_init(Crc64& crc) { crc.crc0 = crc.crc1 = 0xffffffff; }

// Finish a CRC calculation
inline void
crc64_fin(Crc64& crc) { crc.crc0 ^= 0xffffffff; crc.crc1 ^= 0xffffffff; }

// Accumulate some (more) bytes into a CRC

inline void
crc64_compute(Crc64& crc, void const* data, uint32_t len)
{
  uint32_t crc0 = crc.crc0;
  uint32_t crc1 = crc.crc1;
  unsigned char* cdata = (unsigned char *) data;
  while (len-- > 0)
  {
    int tab_index = ((int) (crc1 >> 24) ^ *cdata++) & 0xFF;
    crc1 = crc_table1[tab_index] ^ ((crc1 << 8) | (crc0 >> 24));
    crc0 = crc_table0[tab_index] ^ (crc0 << 8);
  }
  crc.crc0 = crc0;
  crc.crc1 = crc1;
}

inline bool
operator==(Crc64 const& c1, Crc64 const& c2)
  { return c1.crc0 == c2.crc0 && c1.crc1 == c2.crc1; }

inline bool
operator!=(Crc64 const& c1, Crc64 const& c2)
  { return c1.crc0 != c2.crc0 || c1.crc1 != c2.crc1; }

#else /* int64 works */

// Constant table for CRC calculation
extern const uint64_t crc_table[];

#define INT64CONST(x) uint64_t(x##LL)

struct Crc64 { uint64_t crc0; };

// Initialize a CRC accumulator
inline void
crc64_init(Crc64& crc) { crc.crc0 = uint64_t(0xffffffffffffffff); }

// Finish a CRC calculation
inline void
crc64_fin(Crc64& crc) { crc.crc0 ^= uint64_t(0xffffffffffffffff); }

// Accumulate some (more) bytes into a CRC
inline void
crc64_compute(Crc64& crc, void const* data, size_t len)
{
  uint64_t crc0 = crc.crc0;
  unsigned char* cdata = (unsigned char *) data;

  while (len-- > 0)
  {
    int tab_index = ((int) (crc0 >> 56) ^ *cdata++) & 0xFF;
    crc0 = crc_table[tab_index] ^ (crc0 << 8);
  }
  crc.crc0 = crc0;
}

inline bool
operator==(Crc64 const& c1, Crc64 const& c2)
  { return c1.crc0 == c2.crc0; }

inline bool
operator!=(Crc64 const& c1, Crc64 const& c2)
  { return c1.crc0 != c2.crc0; }

inline bool
operator<(Crc64 const& c1, Crc64 const& c2)
  { return c1.crc0 < c2.crc0; }

#endif /* NO_LONG_LONG */

Crc64 crc64(const void * block, size_t len);

} // namespace Objfs

#endif /* BOOST_CRC_H */

