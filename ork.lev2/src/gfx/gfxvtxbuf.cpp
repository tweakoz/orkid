////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>

/////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
/////////////////////////////////////////////////////////////////////////

IndexBufferBase::IndexBufferBase()
	: miNumIndices( 0 )
	, mhIndexBuf( 0 )
	, mpIndices( 0 )
{
}

IndexBufferBase::~IndexBufferBase()
{
	Context* pTARG = GfxEnv::GetRef().GetLoaderTarget();
	//pTARG->GBI()->ReleaseIB( *this );
	mpIndices = 0;
}

/////////////////////////////////////////////////////////////////////////
template< typename T > IdxBuffer<T>::~IdxBuffer()
{
}
/////////////////////////////////////////////////////////////////////////

template< typename T > StaticIndexBuffer<T>::StaticIndexBuffer( int inumidx, const T *src )
	: IdxBuffer<T>()
{
	if( inumidx )
	{
		this->miNumIndices = inumidx;

		if( src )
		{
			this->mpIndices = const_cast<T*>(src);
		}
	}
}


template< typename T > StaticIndexBuffer<T>::~StaticIndexBuffer()
{
	this->mpIndices = 0;
}

/////////////////////////////////////////////////////////////////////////

template< typename T > DynamicIndexBuffer<T>::DynamicIndexBuffer( int inumidx )
	: IdxBuffer<T>()
{
	this->miNumIndices = inumidx;
}


template< typename T > DynamicIndexBuffer<T>::~DynamicIndexBuffer()
{
	this->mpIndices = 0;
}

/////////////////////////////////////////////////////////////////////////

template class StaticIndexBuffer<U16>;
template class DynamicIndexBuffer<U16>;
template class StaticIndexBuffer<U32>;
template class DynamicIndexBuffer<U32>;

/////////////////////////////////////////////////////////////////////////

#define HandleVtxFormat(FORMAT)\
case EVTXSTREAMFMT_##FORMAT:\
{\
	if( bstatic )\
		pvb = new StaticVertexBuffer<SVtx##FORMAT>( inumverts, 0, ork::lev2::EPRIM_MULTI );\
	else\
		pvb = new DynamicVertexBuffer<SVtx##FORMAT>( inumverts, 0, ork::lev2::EPRIM_MULTI );\
	break;\
}


VertexBufferBase* VertexBufferBase::CreateVertexBuffer( EVtxStreamFormat eformat, int inumverts, bool bstatic )
{
	VertexBufferBase*pvb = 0;

	switch( eformat )
	{
		HandleVtxFormat(V12N6C2T4);
		HandleVtxFormat(V12N6I1T4);
		HandleVtxFormat(V12I4N12T8);
		HandleVtxFormat(V12N12T8I4W4);
		HandleVtxFormat(V12N12B12T8I4W4);
		HandleVtxFormat(V12N12B12T8C4);
		HandleVtxFormat(V12N12B12T16);
		HandleVtxFormat(V12N12T16C4);
		HandleVtxFormat(MODELERRIGID);
		default:
			OrkAssert( false );
	}
	return pvb;
}

/////////////////////////////////////////////////////////////////////////

VertexBufferBase::VertexBufferBase( int iMax, int iFlush, int iSize, EPrimitiveType eType, EVtxStreamFormat eFmt )
	: miNumVerts(0)
	, miMaxVerts( iMax )
	, miVtxSize( iSize )
	, miFlushSize(iFlush)
	, mePrimType(eType)
	, meStreamFormat( eFmt )
	, mhHandle(0)
	, mhPBHandle(0)
	, mbLocked(false)
	, miLockWriteIndex( 0 )
	, mbRingLock(false)
{
}

VertexBufferBase::~VertexBufferBase()
{
	Context* pTARG = GfxEnv::GetRef().GetLoaderTarget();
	//pTARG->GBI()->ReleaseVB( *this );
}

/////////////////////////////////////////////////////////////////////////

void VtxWriterBase::Lock( Context* pT, VertexBufferBase* pVB, int icount )
{
	OrkAssert(pVB!=0);
	bool bringlock = pVB->GetRingLock();
	int ivbase = pVB->GetNumVertices();
	int imax = pVB->GetMax();
	////////////////////////////////////////////
	OrkAssert( icount!=0 );
	int inewbase = ivbase+icount;
	if( bringlock )
	{
		if( inewbase > imax )
		{
			ivbase = 1;
			inewbase = icount;
			//printf( "ringcyc\n" );
		}
	}
	else 
	{
		OrkAssert( (ivbase+icount) <= imax );
	}
	////////////////////////////////////////////
	void* pdata = pT->GBI()->LockVB( *pVB, ivbase, icount );
	OrkAssert(pdata!=0);
	////////////////////////////////////////////
	mpBase = (char*) pdata;
	miWriteBase = ivbase;
	miWriteCounter = 0;
	miWriteMax = icount;
	mpVB = pVB;
	////////////////////////////////////////////
	pVB->SetNumVertices( inewbase );
}
void VtxWriterBase::UnLock( Context* pT, u32 ulflgs )
{
	OrkAssert(mpVB!=0);
	pT->GBI()->UnLockVB( *mpVB);

	if( (ulflgs&EULFLG_ASSIGNVBLEN)!=0 )
	{
		mpVB->SetNumVertices(miWriteCounter);
	}
}

} }



