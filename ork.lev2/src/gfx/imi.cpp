////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
//#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/object/AutoConnector.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

ImmInterface::ImmInterface(GfxTarget& target)
	: mVtxBufUILine( 4<<10, 4096, EPRIM_LINES )
	, mVtxBufUIQuad( 4<<10, 8, EPRIM_TRIANGLES )
	, mVtxBufUITexQuad( 4<<10, 8, EPRIM_TRIANGLES )
	, mVtxBufText( 128<<10, 0, EPRIM_TRIANGLES )
	, mTarget(target)
{
}

///////////////////////////////////////////////////////////////////////////////
/*
void ImmInterface::DrawLine( int iX1, int iY1, int iX2, int iY2 )
{
	DrawLine( iX1, iY1, iX2, iY2, gGfxEnv.GetColor() );
}

///////////////////////////////////////////////////////////////////////////////

void ImmInterface::DrawLine( int iX1, int iY1, int iX2, int iY2, const CColor4& color )
{
	U32 uColor = mTarget.CColor4ToU32(color);

	mVtxBufUILine.AddVertex( SVtxV4C4( s16(iX1), s16(iY1), uColor ) );
	mVtxBufUILine.AddVertex( SVtxV4C4( s16(iX2), s16(iY2), uColor ) );
}

///////////////////////////////////////////////////////////////////////////////

void ImmInterface::DrawSolidBox( int iX1, int iY1, int iX2, int iY2, const CColor4& color )
{
	U32 ucolor = mTarget.CColor4ToU32(color);

	float maxuv = 1.0f;
	float minuv = 0.0f;

	mVtxBufUITexQuad.AddVertex( SVtxV12C4T16( float(iX1), float(iY1), 0.0f, minuv, minuv, ucolor ) );
	mVtxBufUITexQuad.AddVertex( SVtxV12C4T16( float(iX2), float(iY1), 0.0f, maxuv, minuv, ucolor ) );
	mVtxBufUITexQuad.AddVertex( SVtxV12C4T16( float(iX2), float(iY2), 0.0f, maxuv, maxuv, ucolor ) );

	mVtxBufUITexQuad.AddVertex( SVtxV12C4T16( float(iX2), float(iY2), 0.0f, maxuv, maxuv, ucolor ) );
	mVtxBufUITexQuad.AddVertex( SVtxV12C4T16( float(iX1), float(iY2), 0.0f, minuv, maxuv, ucolor ) );
	mVtxBufUITexQuad.AddVertex( SVtxV12C4T16( float(iX1), float(iY1), 0.0f, minuv, minuv, ucolor ) );

}

///////////////////////////////////////////////////////////////////////////////

void ImmInterface::DrawSolidBox( int iX1, int iY1, int iX2, int iY2 )
{
	DrawSolidBox( iX1, iY1, iX2, iY2, gGfxEnv.GetColor() );
}

///////////////////////////////////////////////////////////////////////////////

void ImmInterface::DrawTexturedBox( int iX1, int iY1, int iX2, int iY2 )
{
	U32 uColor = 0xffffffff;	//gGfxEnv.GetColor().GetABGRU32();

	float maxuv = 1.0f;
	float minuv = 0.0f;

	mVtxBufUITexQuad.AddVertex( SVtxV12C4T16( float(iX1), float(iY1), 0.0f, minuv, minuv, uColor ) );
	mVtxBufUITexQuad.AddVertex( SVtxV12C4T16( float(iX2), float(iY1), 0.0f, maxuv, minuv, uColor ) );
	mVtxBufUITexQuad.AddVertex( SVtxV12C4T16( float(iX2), float(iY2), 0.0f, maxuv, maxuv, uColor ) );

	mVtxBufUITexQuad.AddVertex( SVtxV12C4T16( float(iX1), float(iY1), 0.0f, minuv, minuv, uColor ) );
	mVtxBufUITexQuad.AddVertex( SVtxV12C4T16( float(iX2), float(iY2), 0.0f, maxuv, maxuv, uColor ) );
	mVtxBufUITexQuad.AddVertex( SVtxV12C4T16( float(iX1), float(iY2), 0.0f, minuv, maxuv, uColor ) );

}

///////////////////////////////////////////////////////////////////////////////

void ImmInterface::DrawBox( int iX1, int iY1, int iX2, int iY2 )
{
	DrawBox( iX1, iY1, iX2, iY2, gGfxEnv.GetColor() );
}

///////////////////////////////////////////////////////////////////////////////

void ImmInterface::DrawBox( int iX1, int iY1, int iX2, int iY2, const CColor4& color )
{
	U32 uColor = mTarget.CColor4ToU32(color);

	mVtxBufUILine.AddVertex( SVtxV4C4( s16(iX1), s16(iY1), uColor ) );
	mVtxBufUILine.AddVertex( SVtxV4C4( s16(iX2), s16(iY1), uColor ) );

	mVtxBufUILine.AddVertex( SVtxV4C4( s16(iX2), s16(iY1), uColor ) );
	mVtxBufUILine.AddVertex( SVtxV4C4( s16(iX2), s16(iY2), uColor ) );

	mVtxBufUILine.AddVertex( SVtxV4C4( s16(iX2), s16(iY2), uColor ) );
	mVtxBufUILine.AddVertex( SVtxV4C4( s16(iX1), s16(iY2), uColor ) );

	mVtxBufUILine.AddVertex( SVtxV4C4( s16(iX1), s16(iY2), uColor ) );
	mVtxBufUILine.AddVertex( SVtxV4C4( s16(iX1), s16(iY1), uColor ) );
}

///////////////////////////////////////////////////////////////////////////////

void ImmInterface::DrawGradBox( int iX1, int iY1, int iX2, int iY2, U32 uC1, U32 uC2, U32 uC3, U32 uC4 )
{
	U32 uColor = gGfxEnv.GetColor().GetABGRU32();

	mVtxBufUIQuad.AddVertex( SVtxV4C4( s16(iX1), s16(iY1), uC1 ) );
	mVtxBufUIQuad.AddVertex( SVtxV4C4( s16(iX2), s16(iY2), uC2 ) );
	mVtxBufUIQuad.AddVertex( SVtxV4C4( s16(iX1), s16(iY2), uC3 ) );

	mVtxBufUIQuad.AddVertex( SVtxV4C4( s16(iX1), s16(iY1), uC1 ) );
	mVtxBufUIQuad.AddVertex( SVtxV4C4( s16(iX2), s16(iY1), uC4 ) );
	mVtxBufUIQuad.AddVertex( SVtxV4C4( s16(iX2), s16(iY2), uC2 ) );
}

///////////////////////////////////////////////////////////////////////////////
void ImmInterface::QueBegin( VertexBufferBase& VBuf )
{	VBuf.Reset();
	mTarget.GBI()->LockVB( VBuf );
}

///////////////////////////////////////////////////////////////////////////////

void ImmInterface::QueEnd( VertexBufferBase& VBuf )
{	if( VBuf.IsLocked() )
	{	mTarget.GBI()->FlushVB( VBuf );
		mTarget.GBI()->UnLockVB( VBuf );
		VBuf.Reset();
	}
}

///////////////////////////////////////////////////////////////////////////////

void ImmInterface::QueFlush( bool bDefaultMaterial )
{
	mTarget.MTXI()->PushUIMatrix();
	mTarget.GBI()->FlushVB( mVtxBufUIQuad );
	mTarget.GBI()->FlushVB( mVtxBufUILine );
	mTarget.GBI()->FlushVB( mVtxBufUITexQuad );
	mTarget.MTXI()->PopUIMatrix();
}

///////////////////////////////////////////////////////////////////////////////

void ImmInterface::QueBeginFrame( void )
{	QueBegin( mVtxBufUIQuad );
	QueBegin( mVtxBufUILine );
	QueBegin( mVtxBufUITexQuad );
}

///////////////////////////////////////////////////////////////////////////////

void ImmInterface::QueEndFrame( void )
{	mTarget.BindMaterial( GfxEnv::GetDefaultUIMaterial() );
	QueEnd( mVtxBufUIQuad );
	QueEnd( mVtxBufUILine );
	QueEnd( mVtxBufUITexQuad );
}

///////////////////////////////////////////////////////////////////////////////

void ImmInterface::BeginFrame()
{
	if( mVtxBufUILine.IsLocked() )
	{
		mTarget.GBI()->UnLockVB( mVtxBufUILine );
	}
	if( mVtxBufUIQuad.IsLocked() )
	{
		mTarget.GBI()->UnLockVB( mVtxBufUIQuad );
	}
	if( mVtxBufUITexQuad.IsLocked() )
	{
		mTarget.GBI()->UnLockVB( mVtxBufUITexQuad );
	}
	if( mVtxBufText.IsLocked() )
	{
		mTarget.GBI()->UnLockVB( mVtxBufText );
	}

	QueBeginFrame();
	DoBeginFrame();

}

///////////////////////////////////////////////////////////////////////////////

void ImmInterface::EndFrame()
{
	DoEndFrame();

	QueEndFrame();

	if( mVtxBufUILine.IsLocked() )
	{
		mTarget.GBI()->UnLockVB( mVtxBufUILine );
	}
	if( mVtxBufUIQuad.IsLocked() )
	{
		mTarget.GBI()->UnLockVB( mVtxBufUIQuad );
	}
	if( mVtxBufUITexQuad.IsLocked() )
	{
		mTarget.GBI()->UnLockVB( mVtxBufUITexQuad );
	}
	if( mVtxBufText.IsLocked() )
	{
		mTarget.GBI()->UnLockVB( mVtxBufText );
	}
}
*/

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} }
