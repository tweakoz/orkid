////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
//////////////////////////////////////////////////////////////// 


#ifndef _ORK_UTIL_ENDIAN_H
#define _ORK_UTIL_ENDIAN_H

#include <ork/util/Context.h>


namespace ork {

template <typename T>
void swapbytes( T& item ) { // inplace endian swap

	int isize = sizeof(T);

	T temp = item;

	U8 *src = reinterpret_cast<U8 *>( & item );
	U8 *tmp = reinterpret_cast<U8 *>( & temp );

	for( int i=0, j=isize-1; i<isize; i++, j-- )
	{
		tmp[j] = src[i];
	}

	for( int i=0; i<isize; i++ )
	{
		src[i] = tmp[i];
	}
}

enum EEndian
{
	EENDIAN_LITTLE = 0,
	EENDIAN_BIG
};

#if defined( __BIG_ENDIAN__ )
static const EEndian kHostEndian = EENDIAN_BIG;
# define swapbytes_from_little(item) swapbytes(item)
# define swapbytes_from_big(item)    ((void)0)
# define swapbytes_to_little(item)   swapbytes(item)
# define swapbytes_to_big(item)      ((void)0)
#else
static const EEndian kHostEndian = EENDIAN_LITTLE;
# define swapbytes_from_big(item)    swapbytes(item)
# define swapbytes_from_little(item) ((void)0)
# define swapbytes_to_big(item)      swapbytes(item)
# define swapbytes_to_little(item)   ((void)0)
#endif

EEndian GetCurrentEndian();

struct EndianContext : public ork::util::Context<EndianContext>
{
	ork::EEndian mendian;
	EndianContext()	: mendian( ork::kHostEndian ) {	}
};

inline EEndian GetCurrentEndian()
{
	EndianContext* pctx = ork::EndianContext::context();
	EEndian rval = (pctx!=0) ? pctx->mendian : ork::kHostEndian;
	return rval;
}

template <typename T>
void swapbytes_dynamic( T& item ) // inplace endian swap
{
	if( kHostEndian != GetCurrentEndian() )
	{
		swapbytes<T>( item );
	}
}

}

#endif
