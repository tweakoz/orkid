////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
//////////////////////////////////////////////////////////////// 

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <cstdarg>
#include <ork/orkconfig.h>

#define _IN_TOOLCHAIN 1

///////////////////////////////////////////////////////////////////////////////

void OrkNonFatalAssertFunction( const char *fmtstr, ... );
void OrkAssertFunction( const char *fmtstr, ... );

#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

#if (defined(_DEBUG) || ! defined(_XBOX)) && ! defined(RETAIL)
//# define OrkAssert( x ) ((void *)0)
# define OrkAssert( x ) { if( (x) == 0 ) { char buffer[1024]; snprintf( buffer, sizeof(buffer), "Assert At: [File %s] [Line %d] [Reason: Assertion %s failed]", __FILE__, __LINE__, #x ); OrkAssertFunction(&buffer[0]); } }
# define OrkAssertI( x, i ) { if( (x) == 0 ) OrkAssertFunction( "Assert At: [File %s] [Line %d] [Reason: %s]", __FILE__, __LINE__, i  ); }
# define OrkAssertEqual( x, y ) OrkAssert( x == y )
# define OrkAssertEqualI( x, y, i ) OrkAssertI( x == y, i )
# define OrkAssertNotEqual( x, y ) OrkAssert( x != y )
# define OrkAssertNotEqualI( x, y, i ) OrkAssertI( x != y, i )
# define OrkAssertNotImpl() OrkAssert( false )
# define OrkAssertNotImplI( i ) OrkAssertI( false, i )
# define OrkAssertNull( x ) OrkAssert( !x )
# define OrkAssertNullI( x, i ) OrkAssertI( !x, i )
# define OrkAssertNotNull( x ) OrkAssert( x )
# define OrkAssertNotNullI( x, i ) OrkAssertI( x, i )
# define OrkAssertLower( x, y ) OrkAssert( x <= y )
# define OrkAssertLowerI( x, y, i ) OrkAssertI( x <= y, i )
# define OrkAssertUpper( x, y ) OrkAssert( x >= y )
# define OrkAssertUpperI( x, y, i ) OrkAssertI( x >= y, i )
# define OrkAssertFwdRange( x, y, z ) OrkAssert( x <= z && y <= x )
# define OrkAssertFwdRangeI( x, y, z, i ) OrkAssertI( x <= z && y <= x, i )
# define OrkAssertBwdRange( x, y, z ) OrkAssert( x >= z && y >= x )
# define OrkAssertBwdRangeI( x, y, z, i ) OrkAssert( x >= z && y >= x, i )
# define OrkAssertArrayIndex( i, asize ) OrkAssertI( i >= 0 && i < asize, "NOOB (iNdex Out Of Bounds)" )
# define OrkNonFatalAssertI( x, i ) { if( (x) == 0 ) OrkNonFatalAssertFunction( "Assert [File %s] [Line %d] [Reason: %s]", __FILE__, __LINE__, i  ); }
#else
# define OrkAssert( x )
# define OrkAssertI( x, y )
# define OrkAssertEqual( x, y )
# define OrkAssertEqualI( x, y, i )
# define OrkAssertNotEqual( x, y )
# define OrkAssertNotEqualI( x, y, i )
# define OrkAssertNotImpl()
# define OrkAssertNotImplI( y )
# define OrkAssertNull( x )
# define OrkAssertNullI( x, i )
# define OrkAssertNotNull( x )
# define OrkAssertNotNullI( x, i )
# define OrkAssertLower( x, y )
# define OrkAssertLowerI( x, y, i )
# define OrkAssertUpper( x, y )
# define OrkAssertUpperI( x, y, i )
# define OrkAssertFwdRange( x, y, z )
# define OrkAssertFwdRangeI( x, y, z, i )
# define OrkAssertBwdRange( x, y, z )
# define OrkAssertBwdRangeI( x, y, z, i )
# define OrkAssertArrayIndex( i, asize )
# define OrkNonFatalAssertI( x, i )
#endif


///////////////////////////////////////////////////////////////////////////////
// FIX MAYA

#if defined( _MAYA_PLUGIN )

#define _BOOL
#define TRUE_AND_FALSE_DEFINED

#endif

#if defined (WII)
	using std::snprintf;
	using std::vsnprintf;
#endif
///////////////////////////////////////////////////////////////////////////////


