////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _UTIL_CRC_H
#define _UTIL_CRC_H

///////////////////////////////////////////////////////////////////////////////

class CCRC
{

	static const int CRC_INIT = 0xFFFF;

	static const U16 crc_table[256]; 

public:

    static void Init( U16 & _v ) { _v = CRC_INIT; }
    static void Add( U16 & _v, U8 _d );
    static void Add( U16 & _v, S32 _d );
    static void Add( U16 & _v, S16 _d );
    static U32 HashStringCaseInsensitive( const char * _string );
    static U32 HashStringCaseSensitive( const char * _string );
	static U32 HashMemory( const void * _address, S32 _length );

	static bool DoesDataMatch( const void *pv0, const void *pv1, int ilen );

	//static CRCID StringUntil( const TCHAR * _string, TCHAR cStop );
    //static CRCID Strings( const TCHAR * _s1, const TCHAR * _s2 );
    //static CRCID String( TStr & _string );
    //static CRCID Raw( const void * _memory, uint32 _bytes );
    //static void RawCont( const void * pMemory, uint32 uBytes, CRC_ID & crc, tbool & aligned );

};

///////////////////////////////////////////////////////////////////////////////

#endif
