////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/lev2renderer.h>
namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

FrameBufferInterface::FrameBufferInterface( GfxTarget& tgt )
	: mTarget(tgt)
	, mbEnableVSync( false )
	, mbEnableFullScreen( GfxEnv::GetRef().GetCreationParams().mbFullScreen )
	, miScissorStackIndex(0)
	, mpBufferTex( 0 )
	, mbIsPbuffer( false )
	, mbAutoClear( true )
	, mcClearColor( CColor4::Black() )
	, mpThisBuffer( 0 )
	, miPickState(0)
	, miViewportStackIndex(0)
	, mCurrentRtGroup( 0 )
    , mpPickBuffer(0)
{
	//for( int i=0; i<kiVPStackMax; i++ )
	//	maViewportStack[i]

}

///////////////////////////////////////////////////////////////////////////////

FrameBufferInterface::~FrameBufferInterface()
{
	//if( mpBufferTex )
	//{
	//	delete mpBufferTex;
	//}
}

///////////////////////////////////////////////////////////////////////////////

void FrameBufferInterface::PushRtGroup( RtGroup* Base )
{
	mRtGroupStack.push(mCurrentRtGroup);
	SetRtGroup( Base );

	int iw = mTarget.GetW();
	int ih = mTarget.GetH();

	if( Base != nullptr )
	{
		iw = Base->GetW();
		ih = Base->GetH();
	}

	SRect r(0,0,iw,ih);

	PushScissor( r );
	PushViewport( r );
	//BeginFrame();	
}
void FrameBufferInterface::PopRtGroup()
{
	RtGroup* prev = mRtGroupStack.top();
	mRtGroupStack.pop();
	//EndFrame();
	SetRtGroup( prev );	// Enable Mrt
	PopViewport();
	PopScissor();
}

///////////////////////////////////////////////////////////////////////////////

SRect &FrameBufferInterface::GetViewport( void )
{
	static SRect VP;

	VP.miX = miCurVPX;
	VP.miY = miCurVPY;
	VP.miW = miCurVPW;
	VP.miH = miCurVPH;
	VP.miX2 = miCurVPW+miCurVPX;
	VP.miY2 = miCurVPH+miCurVPY;

	return VP;
}

///////////////////////////////////////////////////////////////////////////////

void FrameBufferInterface::BeginFrame( void )
{
	DoBeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

void FrameBufferInterface::EndFrame( void )
{
	DoEndFrame();
}

///////////////////////////////////////////////////////////////////////////////

void FrameBufferInterface::PushScissor( const SRect &rScissorRect )
{	
	maScissorStack[ miScissorStackIndex ] = rScissorRect;

	int X = rScissorRect.miX;
	int X2 = rScissorRect.miX2;

	int Y = rScissorRect.miY;
	int Y2 = rScissorRect.miY2;

	if( miScissorStackIndex > 0 )
	{
		SRect &rParentRect = maScissorStack[ miScissorStackIndex-1 ];	
		
		int PX = rParentRect.miX;
		int PX2 = rParentRect.miX2;
		int PY = rParentRect.miY;
		int PY2 = rParentRect.miY2;

		if(X < PX)
			X = PX;
		if(X > PX2)
			X = PX2;
		if(X2 < PX)
			X2 = PX;
		if(X2 > PX2)
			X2 = PX2;

		if(Y < PY)
			Y = PY;
		if(Y > PY2)
			Y = PY2;
		if(Y2 < PY)
			Y2 = PY;
		if(Y2 > PY2)
			Y2 = PY2;
	}

	SetScissor( X, Y, X2 - X, Y2 - Y );

	miScissorStackIndex++;
}

///////////////////////////////////////////////////////////////////////////////

SRect &FrameBufferInterface::GetScissor( void )
{
	OrkAssert( miScissorStackIndex>=0 );
	OrkAssert( miScissorStackIndex<kiVPStackMax );
	 return maScissorStack[ miScissorStackIndex ];
}

///////////////////////////////////////////////////////////////////////////////

SRect &FrameBufferInterface::PopScissor( void )
{
	OrkAssert( miScissorStackIndex>0 );
	miScissorStackIndex--;
	if(miScissorStackIndex > 0)
	{
		SRect &rRect = maScissorStack[ miScissorStackIndex-1 ];
		int W = rRect.miX2 - rRect.miX;
		int H = rRect.miY2 - rRect.miY;
		
		SetScissor( rRect.miX, rRect.miY, W, H );

		return rRect;
	}
	return maScissorStack[0];
}

///////////////////////////////////////////////////////////////////////////////

void FrameBufferInterface::PushViewport( const SRect &rViewportRect )
{	
	OrkAssert( miViewportStackIndex<(kiVPStackMax-1) );
    
	int icvpx1 = miCurVPX;
	int icvpy1 = miCurVPY;
	int icvpx2 = miCurVPX+miCurVPW;
	int icvpy2 = miCurVPY+miCurVPH;

	maViewportStack[ miViewportStackIndex ] = SRect( icvpx1, icvpy1, icvpx2, icvpy2 ); //rViewportRect;

	SetViewport( rViewportRect.miX, rViewportRect.miY, rViewportRect.miW, rViewportRect.miH );
	
	//miCurVPX = rViewportRect.miX;
	//miCurVPY = rViewportRect.miY;
	//miCurVPW = rViewportRect.miW;
	//miCurVPH = rViewportRect.miH;

	miViewportStackIndex++;
}

///////////////////////////////////////////////////////////////////////////////

SRect &FrameBufferInterface::PopViewport( void )
{	OrkAssert( miViewportStackIndex>0 );
	miViewportStackIndex--;
	SRect &rRect = maViewportStack[ miViewportStackIndex ];
	int W = rRect.miX2 - rRect.miX;
	int H = rRect.miY2 - rRect.miY;
	
	SetViewport( rRect.miX, rRect.miY, W, H );
	
	return rRect;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} }
