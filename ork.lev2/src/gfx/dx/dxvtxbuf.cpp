////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#if defined( ORK_CONFIG_DIRECT3D )
#include "dx.h"
#include <ork/kernel/timer.h>
#include <ork/kernel/string/string.h>
#include <ork/math/basicfilters.h>

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// temporary hack until bullet is using its own models

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

D3DVERTEXELEMENT9 gdwVertexDeclV16[] =
{	{ 0, 0,  D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	D3DDECL_END()
};
D3DVERTEXELEMENT9 gdwVertexDeclV12C4T16[] =
{	{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 12,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
	{ 0, 16,  D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	{ 0, 24,  D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
	D3DDECL_END()
};
D3DVERTEXELEMENT9 gdwVertexDeclV4T4[] =
{	{ 0, 0,  D3DDECLTYPE_SHORT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 4,  D3DDECLTYPE_SHORT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	D3DDECL_END()
};
D3DVERTEXELEMENT9 gdwVertexDeclV4C4[] =
{	{ 0, 0,  D3DDECLTYPE_SHORT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 4,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
	D3DDECL_END()
};
D3DVERTEXELEMENT9 gdwVertexDeclV4T4C4[] =
{	{ 0, 0,  D3DDECLTYPE_SHORT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 4,  D3DDECLTYPE_SHORT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	{ 0, 8,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
	D3DDECL_END()
};
D3DVERTEXELEMENT9 gdwVertexDeclV12C4N6I2T8[] =
{	{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },			// V12
	{ 0, 12,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },			// C4
	{ 0, 16,  D3DDECLTYPE_SHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 1 },			// N6 I2
	{ 0, 24,  D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	D3DDECL_END()
};
D3DVERTEXELEMENT9 gdwVertexDeclV12I4N12T8[] =
{	{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },			// V12
	{ 0, 12,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },			// I4
	{ 0, 16,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },			// N12
	{ 0, 28,  D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },		// T8
	D3DDECL_END()
};
///////////////////////////////////////////////////////////////////////////////

D3DVERTEXELEMENT9 gdwVertexDeclV12N12T8I4W4[] =
{	{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },			// V12
	{ 0, 12,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },			// N12
	{ 0, 24,  D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },		// T8
	{ 0, 32,  D3DDECLTYPE_UBYTE4 , D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },	// I4
	{ 0, 36,  D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0 },	// W4
	D3DDECL_END()
};
D3DVERTEXELEMENT9 gdwVertexDeclV12N12B12T8[] =
{	{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },			// V12
	{ 0, 12,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },			// N12
	{ 0, 24,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0 },		// B12
	{ 0, 36,  D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },		// T8
	D3DDECL_END()
};
D3DVERTEXELEMENT9 gdwVertexDeclV12N12B12T8C4[] =
{	{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },			// V12
	{ 0, 12,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },			// N12
	{ 0, 24,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0 },		// B12
	{ 0, 36,  D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },		// T8
	{ 0, 44,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },			// C4
	D3DDECL_END()
};
D3DVERTEXELEMENT9 gdwVertexDeclV12N12B12T16[] =
{	{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },			// V12
	{ 0, 12,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },			// N12
	{ 0, 24,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0 },		// B12
	{ 0, 36,  D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },		// T8
	{ 0, 44,  D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },		// T8
	D3DDECL_END()
};

D3DVERTEXELEMENT9 gdwVertexDeclV12N12T16C4[] =
{	{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },			// V12
	{ 0, 12,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },			// N12
	{ 0, 24,  D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },		// T8
	{ 0, 32,  D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },		// T8
	{ 0, 40,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },			// C4
	D3DDECL_END()
};

D3DVERTEXELEMENT9 gdwVertexDeclV12N12B12T8I4W4[] =
{	{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },			// V12
	{ 0, 12,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },			// N12
	{ 0, 24,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0 },		// B12
	{ 0, 36,  D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },		// T8
	{ 0, 44,  D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },	// I4
	{ 0, 48,  D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0 },		// W4
	D3DDECL_END()
};


///////////////////////////////////////////////////////////////////////////////

D3DVERTEXELEMENT9 gdwVertexDeclV12I4N6W4T4[] =
{	{ 0, 0,   D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },	// V12
	{ 0, 12,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },		// I4
	{ 0, 16,  D3DDECLTYPE_SHORT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },	// pad
	{ 0, 18,  D3DDECLTYPE_SHORT4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },		// N6
	{ 0, 24,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 1 },		// W4
	{ 0, 28,  D3DDECLTYPE_SHORT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },	// T4
	D3DDECL_END()
};

///////////////////////////////////////////////////////////////////////////////
// potential new vertex formats

D3DVERTEXELEMENT9 gdwVertexDeclModelerRigid[] =
{	{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 12,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 6 },
	{ 0, 16,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 1 },
	{ 0, 20,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 7 },
	{ 0, 24,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
	{ 0, 28,  D3DDECLTYPE_SHORT4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
	{ 0, 36,  D3DDECLTYPE_SHORT4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0 },
	{ 0, 44,  D3DDECLTYPE_SHORT2N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	{ 0, 48,  D3DDECLTYPE_SHORT2N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
	{ 0, 52,  D3DDECLTYPE_SHORT2N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2 },
	{ 0, 56,  D3DDECLTYPE_SHORT2N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 3 },
	{ 0, 60,  D3DDECLTYPE_SHORT2N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 4 },
	{ 0, 64,  D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 5 },
	D3DDECL_END()
};

///////////////////////////////////////////////////////////////////////////////
// potential new vertex formats

D3DVERTEXELEMENT9 gdwVertexDeclV12C4N4I4W4T4[] = // 32 bpv
{	{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 12,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
	{ 0, 16,  D3DDECLTYPE_DEC3N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
	{ 0, 20,  D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },
	{ 0, 24,  D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0 },
	{ 0, 28,  D3DDECLTYPE_SHORT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	D3DDECL_END()
};

D3DVERTEXELEMENT9 gdwVertexDeclV6C4N4I4T4[] = // 22 bpv
{	{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 6,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
	{ 0, 10,  D3DDECLTYPE_DEC3N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
	{ 0, 14,  D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },
	{ 0, 18,  D3DDECLTYPE_SHORT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	D3DDECL_END()
};

///////////////////////////////////////////////////////////////////////////////

D3DVERTEXELEMENT9 gdwVertexDeclV12N6I1T4[] = // 24 bpv
{	{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
//	{ 0, 6,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
//	{ 0, 12,  D3DDECLTYPE_DEC3N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
//{ 0, 18,  D3DDECLTYPE_SHORT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },
	{ 0, 20,  D3DDECLTYPE_SHORT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	D3DDECL_END()
};

D3DVERTEXELEMENT9 gdwVertexDeclV12N6C2T4[] = // 24 bpv
{	{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
//	{ 0, 6,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
//	{ 0, 12,  D3DDECLTYPE_DEC3N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
//	{ 0, 14,  D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },
	{ 0, 20,  D3DDECLTYPE_SHORT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	D3DDECL_END()
};

///////////////////////////////////////////////////////////////////////////////

//EVTXSTREAMFMT_V12C4N6I2T8

D3DVERTEXELEMENT9* gpwVertexDeclTable[EVTXSTREAMFMT_END] =
{
	gdwVertexDeclV16,
	gdwVertexDeclV4T4,				// EVTXSTREAMFMT_V4T4
	gdwVertexDeclV4C4,				// EVTXSTREAMFMT_V4C4
	gdwVertexDeclV4T4C4,			// EVTXSTREAMFMT_V4T4C4
	gdwVertexDeclV12C4T16,			// EVTXSTREAMFMT_V12C4T16

	gdwVertexDeclV12N6I1T4,
	gdwVertexDeclV12N6C2T4,

	0,								// EVTXSTREAMFMT_V16T16C16
	gdwVertexDeclV12I4N12T8,		// EVTXSTREAMFMT_V12I4N12T8
	gdwVertexDeclV12C4N6I2T8,		// EVTXSTREAMFMT_V12C4N6I2T8
	0,								// EVTXSTREAMFMT_V6I2C4N3T2
	gdwVertexDeclV12I4N6W4T4,		// EVTXSTREAMFMT_V12I4N6W4T4
	////////////////////////////////////////////////////////////
	gdwVertexDeclV12N12T8I4W4,		// EVTXSTREAMFMT_V12N12T8I4W4
	gdwVertexDeclV12N12B12T8,		// EVTXSTREAMFMT_V12N12B12T8
	gdwVertexDeclV12N12T16C4,		// EVTXSTREAMFMT_V12N12T16C4
	gdwVertexDeclV12N12B12T8C4,		// EVTXSTREAMFMT_V12N12B12T8C4
	gdwVertexDeclV12N12B12T16,		// EVTXSTREAMFMT_V12N12B12T16
	gdwVertexDeclV12N12B12T8I4W4,	// EVTXSTREAMFMT_V12N12B12T8I4W4
	////////////////////////////////////////////////////////////
	gdwVertexDeclModelerRigid,  // gdwVertexDeclV12C4T16 gdwVertexDeclModelerRigid

};

///////////////////////////////////////////////////////////////////////////////

D3DVERTEXELEMENT9* GetHardwareVertexDeclaration( EVtxStreamFormat eType )
{
	return gpwVertexDeclTable[ eType ];
}

///////////////////////////////////////////////////////////////////////////////

DxGeometryBufferInterface::DxGeometryBufferInterface( GfxTargetDX& target )
	: mImmIndexBuffer(1024)
	, mTargetDX( target )
	, mDrawIdxPrimEMLItem( CreateFormattedString( "<dxtarget:%p:drawidxprimeml>", this ) )
	, mLastVBDECL(0)
{
	for( int i=0; i<EVTXSTREAMFMT_END; i++ )
	{
		mpDXVtxDeclArray[i] = 0;
	}
	//mFramePerfItem.AddItem( mDrawIdxPrimEMLItem );
}

D3DGfxDevice DxGeometryBufferInterface::GetD3DDevice( void )
{
	return mTargetDX.GetD3DDevice();
}
void DxGeometryBufferInterface::BindVertexStreamSource( const VertexBufferBase& VBuf )
{
	BindVertexDeclaration(VBuf.GetStreamFormat());
	D3DVtxBufHandle hBuf = reinterpret_cast<D3DVtxBufHandle> (VBuf.GetHandle());
	int ivertexsize = VBuf.GetVtxSize();

	////////////////////////////////////////////////////////
#if defined(_XBOX)
	if( VBuf.IsStatic() )
	{
		HRESULT hr = GetD3DDevice()->SetStreamSource( 0, hBuf, 0, ivertexsize );
		OrkAssert( SUCCEEDED(hr) );
	}
	else
	{
		OrkAssert(false);
	}
#else
	HRESULT hr = GetD3DDevice()->SetStreamSource( 0, hBuf, 0, ivertexsize );
	OrkAssert( SUCCEEDED(hr) );
#endif
}

///////////////////////////////////////////////////////////////////////////////

void DxGeometryBufferInterface::BindIndexStreamSource( const IndexBufferBase& IBuf )
{
	LPDIRECT3DINDEXBUFFER9 IndexBuf = reinterpret_cast<LPDIRECT3DINDEXBUFFER9> (IBuf.GetHandle());
	if( 0 == IndexBuf ) // Upload to Hw
	{
		IndexBufferBase& nonconst = const_cast<IndexBufferBase&>( IBuf );
		//IdxBuf_Init( nonconst );
		IndexBuf = reinterpret_cast<LPDIRECT3DINDEXBUFFER9> (IBuf.GetHandle());
	}
	HRESULT hr = GetD3DDevice()->SetIndices( IndexBuf );
	OrkAssert( SUCCEEDED(hr) );
}

///////////////////////////////////////////////////////////////////////////////

void DxGeometryBufferInterface::BindVertexDeclaration( EVtxStreamFormat eStrFmt )
{
	D3DVERTEXELEMENT9* _vtxdecl = GetHardwareVertexDeclaration( eStrFmt );

	LPDIRECT3DVERTEXDECLARATION9 pDXVtxDec = mpDXVtxDeclArray[ eStrFmt ];

	if( 0 == pDXVtxDec )
	{
		HRESULT hr = GetD3DDevice()->CreateVertexDeclaration( _vtxdecl, &pDXVtxDec );

		mpDXVtxDeclArray[eStrFmt] = pDXVtxDec;

		OrkAssert( SUCCEEDED( hr ) );
	}

	if( mLastVBDECL != pDXVtxDec )
	{
		HRESULT hr = GetD3DDevice()->SetVertexDeclaration( pDXVtxDec );
		OrkAssert( SUCCEEDED( hr ) );
		mLastVBDECL = pDXVtxDec;
	}
}

////////////////////////////////////////////////////////////////////////////////
//	XBox Only Dynamic Vertex Buffer Implementation
///////////////////////////////////////////////////////////////////////////////

#if defined(_XBOX)
void* DxGeometryBufferInterface::BeginVertices( EPrimitiveType etype, int ivtxsize, int icount )
{
	OrkAssert( icount > 0 );
	OrkAssert( icount < 65536 );

	D3DPRIMITIVETYPE d3dpt = D3DPT_FORCE_DWORD;

	switch( etype )
	{
		case EPRIM_TRIANGLES:
			d3dpt = D3DPT_TRIANGLELIST;
			break;
		default:
		{
			OrkAssert(false);
			break;
		}
	};

	void* pcmdbuf = 0;
	HRESULT hr = GetD3DDevice()->BeginVertices(
		d3dpt,
		icount,
		ivtxsize,
		(void**) & pcmdbuf );
	OrkAssert( SUCCEEDED( hr ) );

	return pcmdbuf;
}

void DxGeometryBufferInterface::EndVertices()
{
	HRESULT hr = GetD3DDevice()->EndVertices();
	OrkAssert( SUCCEEDED( hr ) );
}
#endif

////////////////////////////////////////////////////////////////////////////////
//	DirectX Vertex Buffer Implementation
//
///////////////////////////////////////////////////////////////////////////////

void* DxGeometryBufferInterface::LockVB( VertexBufferBase& VBuf, int ibase, int icount )
{
	OrkAssert( false == VBuf.IsLocked() );

	void* rVal = 0;
	D3DVtxBufHandle hBuf = reinterpret_cast<D3DVtxBufHandle>(VBuf.GetHandle());

	if( 0 == hBuf )
	{
		HRESULT hr = 1;

		int iVBlen = VBuf.GetVtxSize()*VBuf.GetMax();

		if( VBuf.IsStatic() )
		{
			// D3DUSAGE_SOFTWAREPROCESSING
			hr = GetD3DDevice()->CreateVertexBuffer( iVBlen, D3dUsageWriteOnly, NULL, D3DPOOL_MANAGED, &hBuf, 0 );
			OrkAssert( SUCCEEDED( hr ) );
			VBuf.SetHandle( reinterpret_cast<void*> (hBuf) );
		}
		else
		{
			#if defined(_XBOX)
			void* pdata = new char[ iVBlen ];
			VBuf.SetHandle( reinterpret_cast<void*> (pdata) );
			#else
			hr = GetD3DDevice()->CreateVertexBuffer( iVBlen, D3dUsageDynamic, NULL, D3DPOOL_SYSTEMMEM, &hBuf, 0 );
			OrkAssert( SUCCEEDED( hr ) );
			VBuf.SetHandle( reinterpret_cast<void*> (hBuf) );
			#endif
		}
	}


	int iMax = VBuf.GetMax();

	HRESULT hr = 1;

	if( icount == 0 )
	{
		icount = VBuf.GetMax();
	}
	int ibasebytes = ibase*VBuf.GetVtxSize();
	int isizebytes = icount*VBuf.GetVtxSize();


	if( VBuf.IsStatic() )
	{
		if( isizebytes )
		{
			hr = hBuf->Lock( ibasebytes, isizebytes, (void**)&rVal, 0 );
			OrkAssert( SUCCEEDED( hr ) );
			OrkAssert( rVal );
		}
	}
	else
	{
#if defined(_XBOX)
		void* pdata = VBuf.GetHandle();
		char* pch = (char*) pdata;
		rVal = (void*)(pch+ibasebytes);
		int iendbytes = ibasebytes+isizebytes;
		int imaxbytes = VBuf.GetVtxSize()*VBuf.GetMax();
		OrkAssert( iendbytes <= imaxbytes );
#else
		DWORD dlock = D3DLOCK_NOOVERWRITE;
		if( 0 == ibase && 0 == icount ) dlock = D3dLockDiscard;
		if( isizebytes )
		{
			hr = hBuf->Lock( ibasebytes, isizebytes, (void**)&rVal, dlock );
			OrkAssert( SUCCEEDED( hr ) );
			OrkAssert( rVal );
		}
#endif
	}

	VBuf.Lock();
	//VBuf.SetVertexPointer( rVal );
	return rVal;
}

const void* DxGeometryBufferInterface::LockVB( const VertexBufferBase& VBuf, int ibase, int icount )
{
	OrkAssert( false == VBuf.IsLocked() );

	void* rVal = 0;
	D3DVtxBufHandle hBuf = reinterpret_cast<D3DVtxBufHandle>(VBuf.GetHandle());

	if( icount == 0 )
	{
		icount = VBuf.GetMax();
	}

	int ibasebytes = ibase*VBuf.GetVtxSize();
	int isizebytes = icount*VBuf.GetVtxSize();

	if( isizebytes )
	{
		HRESULT hr = hBuf->Lock( ibasebytes, isizebytes, &rVal, D3DLOCK_READONLY );
		OrkAssert( SUCCEEDED( hr ) );
		OrkAssert( rVal );
	}

	VBuf.Lock();

	return rVal;
}

///////////////////////////////////////////////////////////////////////////////

void DxGeometryBufferInterface::UnLockVB( VertexBufferBase& VBuf )
{
	OrkAssert( VBuf.IsLocked() );

#if defined(_XBOX)
	if( VBuf.IsStatic() )
	{
		D3DVtxBufHandle hBuf = reinterpret_cast<D3DVtxBufHandle>(VBuf.GetHandle());
		HRESULT hr = hBuf->Unlock();
		VBuf.Unlock();
		OrkAssert( SUCCEEDED( hr ) );
	}
	else
	{
		VBuf.Unlock();
	}
#else
	D3DVtxBufHandle hBuf = reinterpret_cast<D3DVtxBufHandle>(VBuf.GetHandle());
	HRESULT hr = hBuf->Unlock();
	VBuf.Unlock();
	OrkAssert( SUCCEEDED( hr ) );
#endif
}

///////////////////////////////////////////////////////////////////////////////

void DxGeometryBufferInterface::UnLockVB( const VertexBufferBase& VBuf )
{
	OrkAssert( VBuf.IsLocked() );
	D3DVtxBufHandle hBuf = reinterpret_cast<D3DVtxBufHandle>(VBuf.GetHandle());
	HRESULT hr = hBuf->Unlock();
	VBuf.Unlock();
	OrkAssert( SUCCEEDED( hr ) );
}

///////////////////////////////////////////////////////////////////////////////

void DxGeometryBufferInterface::ReleaseVB( VertexBufferBase& VBuf )
{
	D3DVtxBufHandle hBuf = reinterpret_cast<D3DVtxBufHandle> (VBuf.GetHandle());

	if( hBuf )
	{
		bool bdone = false;

		while( ! bdone )
		{
			ULONG rcount = hBuf->Release();
			bdone = (rcount==0);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void* DxGeometryBufferInterface::LockIB( IndexBufferBase& IdxBuf, int ibase, int icount )
{
	bool is_static = IdxBuf.IsStatic();

	int indexsize = IdxBuf.GetIndexSize();
	D3DFORMAT indexformat = D3DFMT_UNKNOWN;
	D3DPOOL pool = is_static ? D3DPOOL_MANAGED : D3DPOOL_SYSTEMMEM;
	U32 usage = is_static ? D3dUsageWriteOnly : D3dUsageDynamic|D3dUsageWriteOnly;

	switch( indexsize )
	{
		case 2:
			indexformat = D3DFMT_INDEX16;
			break;
		case 4:
			indexformat = D3DFMT_INDEX32;
			break;
		default:
			OrkAssert(false);
			break;
	}

	U16 *pDST = 0;
	LPDIRECT3DINDEXBUFFER9 IndexBuf = reinterpret_cast<LPDIRECT3DINDEXBUFFER9> (IdxBuf.GetHandle());

	if( 0 == IndexBuf )
	{
		HRESULT hr = GetD3DDevice()->CreateIndexBuffer( IdxBuf.GetNumIndices()*indexsize, usage, indexformat, pool, &IndexBuf ,NULL);
		IdxBuf.SetHandle( (void*) IndexBuf );
	}
	int ibasebytes = ibase*IdxBuf.GetIndexSize();
	int isizebytes = icount*IdxBuf.GetIndexSize();

	IndexBuf->Lock( ibasebytes, isizebytes, (void**) & pDST, 0 );
	return(pDST);
}

void DxGeometryBufferInterface::UnLockIB( IndexBufferBase& IdxBuf)
{
	LPDIRECT3DINDEXBUFFER9 IndexBuf = reinterpret_cast<LPDIRECT3DINDEXBUFFER9> (IdxBuf.GetHandle());
	HRESULT hr = IndexBuf->Unlock();
	OrkAssert( SUCCEEDED(hr) );
}

const void* DxGeometryBufferInterface::LockIB( const IndexBufferBase& IdxBuf, int ibase, int icount )
{
	LPDIRECT3DINDEXBUFFER9 IndexBuf = reinterpret_cast<LPDIRECT3DINDEXBUFFER9> (IdxBuf.GetHandle());

	OrkAssert( IndexBuf != 0 );

	int ibasebytes = ibase*IdxBuf.GetIndexSize();
	int isizebytes = icount*IdxBuf.GetIndexSize();
	void* pDST = 0;
	HRESULT hr = IndexBuf->Lock( ibasebytes, isizebytes, & pDST, D3DLOCK_READONLY  );
	OrkAssert( SUCCEEDED(hr) );
	return pDST;
}

void DxGeometryBufferInterface::UnLockIB( const IndexBufferBase& IdxBuf)
{
	LPDIRECT3DINDEXBUFFER9 IndexBuf = reinterpret_cast<LPDIRECT3DINDEXBUFFER9> (IdxBuf.GetHandle());
	HRESULT hr = IndexBuf->Unlock();
	OrkAssert( SUCCEEDED(hr) );
}


///////////////////////////////////////////////////////////////////////////////

void DxGeometryBufferInterface::ReleaseIB( IndexBufferBase& IdxBuf )
{
	LPDIRECT3DINDEXBUFFER9 IndexBuf = reinterpret_cast<LPDIRECT3DINDEXBUFFER9> (IdxBuf.GetHandle());
	bool bdone = false;

	while( ! bdone && IndexBuf )
	{
		ULONG rcount = IndexBuf->Release();
		bdone = (rcount==0);
	}
}

///////////////////////////////////////////////////////////////////////////////

void DxGeometryBufferInterface::DrawPrimitive( const VertexBufferBase& VBuf, EPrimitiveType eType, int ivbase, int ivcount)
{
	int iNum = VBuf.GetNumVertices();

	if( iNum )
	{
		////////////////////////////////////////////////////////

		OrkAssert( 0 != mTargetDX.GetCurMaterial() );

		int inumpasses = mTargetDX.GetCurMaterial()->BeginBlock(&mTargetDX);

		for( int ipass=0; ipass<inumpasses; ipass++ )
		{
			bool bDRAW = mTargetDX.GetCurMaterial()->BeginPass( &mTargetDX,ipass );

			if( bDRAW )
			{
				DrawPrimitiveEML( VBuf, eType, ivbase, ivcount );
			}

			mTargetDX.GetCurMaterial()->EndPass(&mTargetDX);

		}
		mTargetDX.GetCurMaterial()->EndBlock(&mTargetDX);
	}
}

///////////////////////////////////////////////////////////////////////////////

void DxGeometryBufferInterface::DrawIndexedPrimitive( const VertexBufferBase& VBuf, const IndexBufferBase& IBuf, EPrimitiveType eType, int ivbase, int ivcount)
{
	////////////////////////////////////////////////////////

	OrkAssert( 0 != mTargetDX.GetCurMaterial() );

	int inumpasses = mTargetDX.GetCurMaterial()->BeginBlock(&mTargetDX);

	for( int ipass=0; ipass<inumpasses; ipass++ )
	{
		bool bDRAW = mTargetDX.GetCurMaterial()->BeginPass( &mTargetDX,ipass );

		if( bDRAW )
		{
			DrawIndexedPrimitiveEML( VBuf, IBuf, eType, ivbase, ivcount );
		}

		mTargetDX.GetCurMaterial()->EndPass(&mTargetDX);

	}
	mTargetDX.GetCurMaterial()->EndBlock(&mTargetDX);
}

///////////////////////////////////////////////////////////////////////////////

void DxGeometryBufferInterface::DrawPrimitiveEML( const VertexBufferBase& VBuf, EPrimitiveType eType, int ivbase, int ivcount)
{
	if( 0 ) return;

	HRESULT hr;

	EPrimitiveType eRenderType = (eType==EPRIM_NONE) ? VBuf.GetPrimType() : eType;

	if( EPRIM_MULTI == eRenderType )
        eRenderType = eType;

	////////////////////////////////////////////////////////

	int inum = (ivcount==0) ? VBuf.GetNumVertices() : ivcount;

	switch( eRenderType )
	{
		case EPRIM_LINES:
		{	BindVertexStreamSource(VBuf);
			int inumprim = inum>>1;
			hr = GetD3DDevice()->DrawPrimitive( D3DPT_LINELIST, ivbase, inumprim );
			//miNumDrawTriangles += inumprim;
			OrkAssert( SUCCEEDED( hr ) );
			break;
		}
		case EPRIM_LINESTRIP:
		{	BindVertexStreamSource(VBuf);
			int inumprim = inum>>1;
			hr = GetD3DDevice()->DrawPrimitive( D3DPT_LINESTRIP, ivbase, inumprim );
			//miNumDrawTriangles += inumprim;
			OrkAssert( SUCCEEDED( hr ) );
			break;
		}
		case EPRIM_TRIANGLES:
		{	
			#if defined(_XBOX)
			if( VBuf.IsStatic() )
			{
				BindVertexStreamSource(VBuf);
				hr = GetD3DDevice()->DrawPrimitive( D3DPT_TRIANGLELIST, ivbase, (inum/3) );
				miTrianglesRendered += (inum/3);
				miNumDrawTriangles += (inum/3);
				OrkAssert( SUCCEEDED( hr ) );
			}
			else
			{
				BindVertexDeclaration(VBuf.GetStreamFormat());
				
				void* psrcbase = VBuf.GetHandle();
				char* psrcch = (char*)(psrcbase);
				int icopylen = VBuf.GetVtxSize()*inum;
				int isrcoffset = ivbase*VBuf.GetVtxSize();

				void* pdest = BeginVertices( EPRIM_TRIANGLES, VBuf.GetVtxSize(), inum );
				{
					memcpy( pdest, (void*)(psrcch+isrcoffset), icopylen );
				}
				EndVertices();
				
				miTrianglesRendered += (inum/3);
				miNumDrawTriangles += (inum/3);
			}
			break;
			#else
			BindVertexStreamSource(VBuf);
			hr = GetD3DDevice()->DrawPrimitive( D3DPT_TRIANGLELIST, ivbase, (inum/3) );
			miTrianglesRendered += (inum/3);
			miNumDrawTriangles += (inum/3);
			OrkAssert( SUCCEEDED( hr ) );
			#endif
			break;
		}
		case EPRIM_TRIANGLEFAN:
		{	BindVertexStreamSource(VBuf);
			hr = GetD3DDevice()->DrawPrimitive( D3DPT_TRIANGLEFAN, ivbase, inum-2 );
			miTrianglesRendered += (inum-2);
			miNumDrawTriangles += (inum-2);
			OrkAssert( SUCCEEDED( hr ) );
			break;
		}
		case EPRIM_TRIANGLESTRIP:
		{	BindVertexStreamSource(VBuf);
			hr = GetD3DDevice()->DrawPrimitive( D3DPT_TRIANGLESTRIP, ivbase, (inum-2) );
			miTrianglesRendered += (inum-2);
			miNumDrawTriangles += (inum-2);
			OrkAssert( SUCCEEDED( hr ) );
			break;
		}
		case EPRIM_POINTS:
		{	BindVertexStreamSource(VBuf);
			float pointsize=mTargetDX.RSI()->GetLastState().GetPointSize();
			hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSIZE, *((DWORD*)&pointsize) );
			hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSPRITEENABLE, FALSE );

#if ! defined(_XBOX)
			hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSCALEENABLE, FALSE );
#endif
			hr = GetD3DDevice()->DrawPrimitive( D3DPT_POINTLIST, ivbase, inum );
#if ! defined(_XBOX)
			hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSCALEENABLE, FALSE );
#endif
			miTrianglesRendered += inum;
			//miNumDrawTriangles += (inum*2);
			OrkAssert( SUCCEEDED( hr ) );
			break;
		}
		case EPRIM_POINTSPRITES:
		{
			float pointsize=mTargetDX.RSI()->GetLastState().GetPointSize();
			BindVertexStreamSource(VBuf);
			hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSPRITEENABLE, true );
#if ! defined(_XBOX)
			hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSCALEENABLE, true );
#endif
			hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSIZE, *((DWORD*)&pointsize) );
			hr = GetD3DDevice()->DrawPrimitive( D3DPT_POINTLIST, ivbase, inum );
			hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSPRITEENABLE, false );
			miTrianglesRendered += inum;
			//miNumDrawTriangles += (inum*2);
			OrkAssert( SUCCEEDED( hr ) );
			break;
		}
		case EPRIM_QUADS:
		{
			OrkAssert( 0 == (inum%4) );
			//U16 *pidx = (U16*) mImmIndexBuffer.GetDataPointer();
			//int ioutidx = 0;
			//for( int i=0; i<inum; i+=4 )
			//{
			//	u16 ii = u16(i+ivbase);
			//
				//pidx[ioutidx++] = ii;
				//pidx[ioutidx++] = ii+1;
				//pidx[ioutidx++] = ii+3;

				//pidx[ioutidx++] = ii+2;
				//pidx[ioutidx++] = ii+3;
				//pidx[ioutidx++] = ii+1;
			//}
			//UnLockIB( mImmIndexBuffer );
			//mImmIndexBuffer.SetNumIndices(ioutidx);
			//DrawIndexedPrimitiveEML( VBuf, mImmIndexBuffer, EPRIM_TRIANGLES );
			//miNumDrawTriangles += (inum>>1);
			break;
		}
		default:
			OrkAssert( false );
			break;
	}
	miNumDrawPrimCalls++;

}

///////////////////////////////////////////////////////////////////////////////

void DxGeometryBufferInterface::DrawIndexedPrimitiveEML( const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf , EPrimitiveType eType, int ivbase, int ivcount)
{
	if(0) return;

	mDrawIdxPrimEMLItem.Enter();
	HRESULT hr;


	EPrimitiveType eRenderType = VBuf.GetPrimType();

	if( EPRIM_MULTI == eRenderType )
        eRenderType = eType;

	////////////////////////////////////////////////////////
	// Bind Buffers

	BindVertexStreamSource( VBuf );
	BindIndexStreamSource( IdxBuf );

	////////////////////////////////////////////////////////

	int iNumIDX = IdxBuf.GetNumIndices();

	if( iNumIDX )
	{

		int iVBNumVtx = VBuf.GetMax();

		switch( eType )
		{
			case EPRIM_MULTI:
				break;
			case EPRIM_LINES:
			{	int inumprim = iNumIDX>>1;
				DWORD dwptsize = 10;
				hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSIZE, *((DWORD*)&dwptsize) );
				hr = GetD3DDevice()->DrawIndexedPrimitive( D3DPT_LINELIST, 0, 0, iVBNumVtx, 0, inumprim );
				OrkAssert( SUCCEEDED( hr ) );
				//miNumDrawTriangles += inumprim;
				break;
			}
			case EPRIM_TRIANGLESTRIP:
				hr = GetD3DDevice()->DrawIndexedPrimitive( D3DPT_TRIANGLESTRIP, 0, 0, iVBNumVtx, 0, (iNumIDX-2) );
				miTrianglesRendered += (iNumIDX-2);
				miNumDrawTriangles += (iNumIDX-2);
				OrkAssert( SUCCEEDED( hr ) );
				break;
			case EPRIM_TRIANGLES:
				hr = GetD3DDevice()->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, iVBNumVtx, 0, (iNumIDX/3) );
				miTrianglesRendered += (iNumIDX/3);
				miNumDrawTriangles += (iNumIDX/3);
				OrkAssert( SUCCEEDED( hr ) );
				break;
			case EPRIM_POINTS:
			{
				float fpointsize=mTargetDX.RSI()->GetLastState().GetPointSize();
				hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSIZE, *((DWORD*)&fpointsize) );
#if ! defined(_XBOX)
				hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSCALEENABLE, true );
#endif
				hr = GetD3DDevice()->DrawIndexedPrimitive( D3DPT_POINTLIST, 0, 0, iVBNumVtx, 0, (iNumIDX) );
#if ! defined(_XBOX)
				hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSCALEENABLE, false );
#endif
				//miNumDrawTriangles += (iNumIDX*2);
				//miTrianglesRendered += iNum;
				OrkAssert( SUCCEEDED( hr ) );
				break;
			}
			case EPRIM_POINTSPRITES:
			{
				float pointsize=mTargetDX.RSI()->GetLastState().GetPointSize();
				hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSPRITEENABLE, true );
#if ! defined(_XBOX)
				hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSCALEENABLE, false );
#endif
				hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSIZE, *((DWORD*)&pointsize) );
				hr = GetD3DDevice()->DrawIndexedPrimitive( D3DPT_POINTLIST, 0, 0, iVBNumVtx, 0, (iNumIDX) );
				hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSPRITEENABLE, false );
				miTrianglesRendered += (iNumIDX<<1);
				//miNumDrawTriangles += (iNumIDX*2);
				OrkAssert( SUCCEEDED( hr ) );
				break;
			}
			default:
				OrkAssert( false );
				break;
		}

	}
	mDrawIdxPrimEMLItem.Exit();
	miNumDrawPrimCalls++;

}

void DxGeometryBufferInterface::DoBeginFrame()
{
	miNumDrawPrimCalls = 0;
	miNumDrawTriangles = 0;
	mLastVBDECL = 0;
}

void DxGeometryBufferInterface::DoEndFrame()
{
	static const int knframes = 60;

	static avg_filter<knframes> dndpfilter;
	static avg_filter<knframes> dtrifilter;
	dtrifilter.mbEnable = true;
	dtrifilter.SetWindow(1.0f);
	dndpfilter.mbEnable = true;
	dndpfilter.SetWindow(1.0f);

	float favgnumtri = dtrifilter.compute( float(miNumDrawTriangles), 1.0f/float(knframes) );
	float favgnumdpc = dndpfilter.compute( float(miNumDrawPrimCalls), 1.0f/float(knframes) );

	static int counter = 0;

	if( counter==knframes )
	{
		//orkprintf( "targetDX NumDrawPrimCalls<%d> numTriangles<%d>\n", int(favgnumdpc), int(favgnumtri) );
		counter = 0;
	}

	counter++;

	mLastVBDECL = 0;
}



} } //namespace ork::lev2

#endif

