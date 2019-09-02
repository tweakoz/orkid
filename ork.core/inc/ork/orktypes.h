////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkconfig.h>
#include <stdint.h>
#include <functional>

typedef double f64;
typedef double F64;
typedef float f32;
typedef float F32;
typedef int32_t FX32, fx32;
typedef char* STRING;
typedef unsigned char* ADDRESS;

namespace ork
{

typedef std::function<void()> void_lambda_t;

template <typename T> T minimum( T a, T b ) { return (a<b) ? a : b; }
template <typename T> T maximum( T a, T b ) { return (a>b) ? a : b; }

}

static const int fx32_SHIFT = 12;
static const int fx64_SHIFT = 24;

#include <stdint.h>
typedef uint32_t u32, U32;
typedef int32_t  s32, S32, LONG;
typedef uint16_t u16, U16;
typedef int16_t  s16, S16, SHORT;
typedef uint8_t  u8,  U8;
typedef int8_t   s8,  S8;
typedef int32_t  FIX32;
typedef uint64_t u64, U64;
typedef int64_t  s64, S64;
typedef int64_t  fx64, FX64;

#include <ork/math/cfloat.h>

///////////////////////////////////////////////////////////////////////////////

#if defined( __cplusplus )
namespace ork {
#endif

///////////////////////////////////////////////////////////////////////////////

#if defined ( __cplusplus )

struct const_string
{
	const char * mpstr;

	const_string( const char *pstr=0 ) : mpstr( pstr ) {}

	bool operator==(const const_string &other) const;

	const char* c_str() const
	{
		return mpstr;
	}
};

#endif

class Object;

class CAssetHandle
{
	public: //
	ork::Object*	mpEngine;
	void*			mpAsset;

	CAssetHandle()
		: mpEngine( 0 )
		, mpAsset( 0 )
	{
	}

};

///////////////////////////////////////////////////////////////////////////////

typedef size_t FileH;
typedef size_t FileStampH; // (Y6M4D5:H5M6S6) (15:17) Base Year 2000 6 bits for year goes to 2063
typedef size_t LibraryH;
typedef size_t FunctionH;

#if defined( __cplusplus )
}
#endif

///////////////////////////////////////////////////////////////////////////////

#define FALSE 0
#define TRUE 1

#ifndef PI2
        #define PI2  6.283185307f
#endif

#ifndef PI
        #define PI   3.141592654f
#endif

#define PI1 PI

#define PI_DIV_2                        1.5707963267949f
#define NEG_PI_DIV_2                -PI_DIV2
#define INV_TWO_PI                        (1.0f / PI2)
#define THREE_PI_OVER_TWO        (PI*1.5f)
#define PI_DIV_3                        (PI / 3.0f)
#define PI_DIV_4                        (PI / 4.0f)
#define PI_DIV_5                        (PI / 5.0f)
#define PI_DIV_6                        (PI / 6.0f)

#define DTOR   0.017453f         // convert degrees to radians
#define RTOD   57.29578f         // convert radians to degrees

#define EPSILON 0.0001f

///////////////////////////////////////////////////////////////////////////////
// C++ Only Types
///////////////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)

inline bool AreBitsEnabled( U32 uval, U32 bittest ) { return (bittest==(uval&bittest)); }

struct SRect
{
	int miX, miY, miX2, miY2, miW, miH;

	SRect( int x = 0, int y = 0, int x2=0, int y2=0 ) :
		miX(x),
		miY(y),
		miX2(x2),
		miY2(y2),
		miW(x2-x),
		miH(y2-y)
	{
	}
};

#endif
