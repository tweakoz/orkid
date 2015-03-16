///////////////////////////////////////////////////////////////////////////////
// MD5.CC - source code for the C++/object oriented translation and 
//          modification of MD5.

// Translation and modification (c) 1995 by Mordechai T. Abzug 

// This translation/ modification is provided "as is," without express or 
// implied warranty of any kind.

// The translator/ modifier does not claim (1) that MD5 will do what you think 
// it does; (2) that this translation/ modification is accurate; or (3) that 
// this software is "merchantible."  (Language for this disclaimer partially 
// copied from the disclaimer below).

/* based on:

   MD5.H - header file for MD5C.C
   MDDRIVER.C - test driver for MD2, MD4 and MD5

   Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.

*/

#pragma once

#include <stdio.h>
#include <string>

class Md5Sum
{
public:

	uint8_t	muMD5[16];

	Md5Sum()
	{
    for( int i=0; i<16; i++ )
        muMD5[i]=0;
	}

	/*bool operator==( const Md5Sum & ref )
	{
		return ( (muMD5[0]==ref.muMD5[0]) && (muMD5[1]==ref.muMD5[1]) && (muMD5[2]==ref.muMD5[2]) && (muMD5[3]==ref.muMD5[3]) );
	}*/

  std::string hex_digest() const ;
};

class CMD5 {

public:
// methods for controlled operation:
  CMD5              ();  // simple initializer
  void  update     ( const unsigned char *input, unsigned int input_length );
  //void  update     (istream& stream);
  void  update     (FILE *file);
  //void  update     (ifstream& stream);
  void  finalize   ();

// constructors for special circumstances.  All these constructors finalize
// the MD5 context.
  CMD5              (unsigned char *string); // digest std::string, finalize
  //MD5              (istream& stream);       // digest stream, finalize
  CMD5              (FILE *file);            // digest file, close, finalize
  //MD5              (ifstream& stream);      // digest stream, close, finalize

// methods to acquire finalized result

  void					ResultU32( U32 & resa, U32 & resb, U32 & resc, U32 & resd ) const;
  Md5Sum				Result() const;

private:

// first, some types:
  typedef unsigned       int uint4; // assumes integer is 4 words long
  typedef unsigned short int uint2; // assumes short integer is 2 words long
  typedef unsigned      char uint1; // assumes char is 1 word long

// next, the private data:
  uint4 state[4];
  uint4 count[2];     // number of *bits*, mod 2^64
  uint1 buffer[64];   // input buffer
  uint1 digest[16];
  uint1 finalized;

// last, the private methods, mostly static:
  void init             ();               // called by all constructors
  void transform        ( const uint1 *buffer );  // does the real update work.  Note 
                                          // that length is implied to be 64.

  static void encode    (uint1 *dest, const uint4 *src, uint4 length);
  static void decode    (uint4 *dest, const uint1 *src, uint4 length);
  static void memcpy    (uint1 *dest, const uint1 *src, uint4 length);
  static void memset    (uint1 *start, uint1 val, uint4 length);

  static inline uint4  rotate_left (uint4 x, uint4 n);
  static inline uint4  F           (uint4 x, uint4 y, uint4 z);
  static inline uint4  G           (uint4 x, uint4 y, uint4 z);
  static inline uint4  H           (uint4 x, uint4 y, uint4 z);
  static inline uint4  I           (uint4 x, uint4 y, uint4 z);
  static inline void   FF  (uint4& a, uint4 b, uint4 c, uint4 d, uint4 x, 
			    uint4 s, uint4 ac);
  static inline void   GG  (uint4& a, uint4 b, uint4 c, uint4 d, uint4 x, 
			    uint4 s, uint4 ac);
  static inline void   HH  (uint4& a, uint4 b, uint4 c, uint4 d, uint4 x, 
			    uint4 s, uint4 ac);
  static inline void   II  (uint4& a, uint4 b, uint4 c, uint4 d, uint4 x, 
			    uint4 s, uint4 ac);

};
