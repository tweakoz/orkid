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

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

DxImiInterface::DxImiInterface( GfxTargetDX& target )
	: ImmInterface( target )
	, mTargetDX( target )
{
}

D3DGfxDevice DxImiInterface::GetD3DDevice( void )
{
	return mTargetDX.GetD3DDevice();
}

void DxImiInterface::DrawPoint( F32 x, F32 y, F32 z )
{
	HRESULT hr;

	int inumpasses = mTarget.GetCurMaterial()->BeginBlock(&mTarget);
	for( int ipass=0; ipass<inumpasses; ipass++ )
	{
		bool bDRAW = mTarget.GetCurMaterial()->BeginPass( &mTarget, ipass );
		if( bDRAW )
		{
			hr = GetD3DDevice()->SetFVF( D3DFVF_XYZ );
			static CVector4 Vertex;
			
			Vertex = CVector4( x, y, z );

			hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSPRITEENABLE, true );
#if ! defined(_XBOX)
			hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSCALEENABLE, true );
#endif
			hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSIZE, 4 ) ; //*((DWORD*)&mpCurMaterial->mfParticleSize) );
			hr = GetD3DDevice()->DrawPrimitiveUP( D3DPT_POINTLIST, 1, Vertex.GetArray(), 16 );
			hr = GetD3DDevice()->SetRenderState( D3DRS_POINTSPRITEENABLE, false );
		}
		mTarget.GetCurMaterial()->EndPass(&mTarget);
	}
	mTarget.GetCurMaterial()->EndBlock(&mTarget);
}


///////////////////////////////////////////////////////////////////////////////

void DxImiInterface::DrawLine( const CVector4&frm, const CVector4&to )
{
	int inumpasses = mTarget.GetCurMaterial()->BeginBlock(&mTarget);

	for( int ipass=0; ipass<inumpasses; ipass++ )
	{
		bool bDRAW = mTarget.GetCurMaterial()->BeginPass( &mTarget, ipass );

		int ibase = GfxEnv::GetSharedDynamicVB().GetNumVertices();

		CVector2 uv;
		U32 uc = 0xffffffff;

		if( bDRAW )
		{
			lev2::VtxWriter<SVtxV12C4T16> vw;
			vw.Lock( &mTarget, &GfxEnv::GetSharedDynamicVB(), 2 );
			vw.AddVertex( SVtxV12C4T16( frm, uv, uc ) );
			vw.AddVertex( SVtxV12C4T16( to, uv, uc ) );
			vw.UnLock(&mTarget);		

			mTarget.GBI()->DrawPrimitiveEML( GfxEnv::GetSharedDynamicVB(), lev2::EPRIM_LINES, ibase, 2 );

		}
		mTarget.GetCurMaterial()->EndPass(&mTarget);
	}
	mTarget.GetCurMaterial()->EndBlock(&mTarget);
}

///////////////////////////////////////////////////////////////////////////////

void DxImiInterface::DrawPrim( const CVector4 *Points, int inumpoints, EPrimitiveType eType )
{
	#if 1

	HRESULT hr;


	int inumpasses = mTarget.GetCurMaterial()->BeginBlock(&mTarget);
	for( int ipass=0; ipass<inumpasses; ipass++ )
	{
		hr = GetD3DDevice()->SetFVF( D3DFVF_XYZ ); //|D3DFVF_DIFFUSE );

		bool bDRAW = mTarget.GetCurMaterial()->BeginPass( &mTarget,ipass );
		if( bDRAW )
		{
			//HRESULT DrawPrimitiveUP(
			//D3DPRIMITIVETYPE PrimitiveType,
			//UINT PrimitiveCount,
			//const void *pVertexStreamZeroData,
			//UINT VertexStreamZeroStride
			switch( eType )
			{
				case EPRIM_TRIANGLESTRIP:
				case EPRIM_QUADSTRIP:
				case EPRIM_QUADS:
				default:
					break;
				case EPRIM_LINES:
					OrkAssert((inumpoints%2)==0);
					hr = GetD3DDevice()->DrawPrimitiveUP( D3DPT_LINELIST, inumpoints/2, Points, 16 );
					break;
				case EPRIM_TRIANGLES:
					OrkAssert((inumpoints%3)==0);
					hr = GetD3DDevice()->DrawPrimitiveUP( D3DPT_TRIANGLELIST, inumpoints/3, Points, 16 );
					break;
				case EPRIM_POINTS:
					hr = GetD3DDevice()->DrawPrimitiveUP( D3DPT_POINTLIST, inumpoints, Points, 16 );
					break;
			}

			mTarget.GetCurMaterial()->EndPass(&mTarget);

		}
	}
	mTarget.GetCurMaterial()->EndBlock(&mTarget);

	#endif
}

///////////////////////////////////////////////////////////////////////////////

} }

#endif

