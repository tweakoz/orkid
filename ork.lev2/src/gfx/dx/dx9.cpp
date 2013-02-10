////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/ui/ui.h>
#if defined( ORK_CONFIG_DIRECT3D )
#include "dx.h"

#if( DIRECT3D_VERSION >= 0x0900 )

#if defined(_XBOX) && defined(_DEBUG)
#pragma comment(lib,"d3d9d.lib" )
#pragma comment(lib,"d3dx9d.lib" )
#pragma comment(lib,"xapilibd.lib" )
#pragma comment(lib,"xgraphicsd.lib" )
#elif defined(_XBOX) 
#pragma comment(lib,"d3d9i.lib" )
#pragma comment(lib,"d3dx9i.lib" )
#pragma comment(lib,"xapilibi.lib" )
#pragma comment(lib,"xgraphics.lib" )
#else
#pragma comment(lib,"d3d9.lib" )
#pragma comment(lib,"d3dx9.lib" )
#endif


///////////////////////////////////////////////////////////////////////////////
// Dx8 Vertex Buffer Init
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

LPDIRECT3DDEVICE9 GetGlobalD3D9Device()
{
	return GfxTargetDX::GetD3DDevice();
}
void GfxTargetDX::InitVB()
{
	//GBI()->LockVB( mVtxBufSharedV12C4T8, 0, 0 );
	//GBI()->UnLockVB( mVtxBufSharedV12C4T8 );
}

#if 1

///////////////////////////////////////////////////////////////////////////////

void DxFrameBufferInterface::SetViewport( int iX, int iY, int iW, int iH )
{
	//orkprintf( "setvp w<%f> h<%f>\n", float(iW), float(iH) );

	iX = OrkSTXClampToRange( iX, 0, 4096 );
	iY = OrkSTXClampToRange( iY, 0, 4096 );
	iW = OrkSTXClampToRange( iW, 8, 4096 );
	iH = OrkSTXClampToRange( iH, 8, 4096 );

	miCurVPX = iX;
	miCurVPY = iY;
	miCurVPW = iW;
	miCurVPH = iH;

	D3DVIEWPORT9 view_port;
	memset( &view_port, 0, sizeof(D3DVIEWPORT9) );

	view_port.X = iX;
	view_port.Y = iY;
    view_port.Width = iW;
    view_port.Height = iH;
    view_port.MinZ = 0.0f;
    view_port.MaxZ = 1.0f;

	HRESULT hr = GetD3DDevice()->SetViewport( &view_port );

	OrkAssert( SUCCEEDED(hr) );

}

///////////////////////////////////////////////////////////////////////////////

void DxFrameBufferInterface::ClearViewport( CUIViewport *pVP )
{
	F32 fDepth = pVP->GetClearDepth();
	CColor3 &rCol = pVP->GetClearColorRef();

	U32 r = (U32)(rCol.GetX()*CReal(255.0f));
	U32 g = (U32)(rCol.GetY()*CReal(255.0f));
	U32 b = (U32)(rCol.GetZ()*CReal(255.0f));
	U32 a = (U32)CReal(255.0f);

	U32 uColor = (a<<24)|(r<<16)|(g<<8)|b;


	D3DRECT MyRect;
	int ictxh = mTarget.GetH();

	int itx = OrkSTXClampToRange( pVP->GetX(), 0, 4096 );
	int itw = OrkSTXClampToRange( pVP->GetW(), 0, 4096 );
	int ith = OrkSTXClampToRange( pVP->GetH(), 0, 4096 );
	int ity = OrkSTXClampToRange( ictxh-(pVP->GetY()+ith), 0, 4096 );

	//int iOX = (int) pVP->GetX();
	//int iOY = (int) pVP->GetY();
	MyRect.x1 = itx;
	MyRect.x2 = itx+itw;
	MyRect.y1 = ity;
	MyRect.y2 = ity+ith;

	static bool bFirstClear = true; // clear to black initially for debugging purposes

	U32 ucolor = pVP->GetClearColorRef().GetARGBU32();

	HRESULT hr = GetD3DDevice()->Clear( 1, &MyRect, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, uColor, fDepth, 0L );
	//GetD3DDevice()->Clear( 0, 0, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, ucolor, fDepth, 0L );
	OrkAssert( SUCCEEDED(hr) );

}

#endif

} }

#endif

#endif
