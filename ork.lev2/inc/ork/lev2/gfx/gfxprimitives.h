////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _GFX_PRIMITIVES_H
#define _GFX_PRIMITIVES_H

///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/core/singleton.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>

namespace ork { namespace lev2
{

class CGfxPrimitives : public NoRttiSingleton< CGfxPrimitives >
{
	public:

	CGfxPrimitives();
	//static void ClassInit() {}
	static void Init( GfxTarget *pTarg );

///////////////////////////////////////////////////////////////////////////////

	static void RenderGridX100( GfxTarget *pTarg );
	static void RenderCone( GfxTarget *pTarg );
	static void RenderAxis( GfxTarget *pTarg );
	static void RenderTriCircle( GfxTarget *pTarg );
	static void RenderDiamond( GfxTarget *pTarg );
	static void RenderRotManip( GfxTarget *pTarg );
	static void RenderEQSphere( GfxTarget *pTarg );
	static void RenderSkySphere( GfxTarget *pTarg );
	static void RenderGroundPlane( GfxTarget *pTarg );
	static void RenderPerlinTerrain( GfxTarget *pTarg );
	static void RenderCircleStrip( GfxTarget *pTarg );
	static void RenderCircleStripUI( GfxTarget *pTarg );
	static void RenderCircleUI( GfxTarget *pTarg );
	static void RenderDirCone( GfxTarget *pTarg );

	static void RenderCylinder( GfxTarget *pTarg ,bool drawoutline = false);
	static void RenderCapsule( GfxTarget *pTarg , float radius );
	static void RenderBox( GfxTarget *pTarg ,bool drawoutline = false);
	static void RenderAxisLineCone(GfxTarget* pTarg);
	static void RenderAxisBox(GfxTarget* pTarg);
	static void RenderDome( GfxTarget *pTarg )         { pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_Dome ); }
	static void RenderWireFrameBox( GfxTarget *pTarg ) { pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_WireFrameBox ); }
	static void RenderWireFrameDome( GfxTarget *pTarg ) { pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_WireFrameDome ); }


	static CVtxBuffer<SVtxV12C4T16>& GetAxisVB( void ) { return GetRef().mVtxBuf_Axis; }
	static CVtxBuffer<SVtxV12C4T16>& GetConeVB( void ) { return GetRef().mVtxBuf_Cone; }
	static CVtxBuffer<SVtxV12C4T16>& GetGridVB( void ) { return GetRef().mVtxBuf_GridX100; }
	static CVtxBuffer<SVtxV12C4T16>& GetGroundVB( void ) { return GetRef().mVtxBuf_GroundPlane; }
	static CVtxBuffer<SVtxV12N12B12T8C4>& GetPerlinVB( void ) { return GetRef().mVtxBuf_PerlinTerrain; }
	static CVtxBuffer<SVtxV12C4T16>& GetTriCircleVB( void ) { return GetRef().mVtxBuf_TriCircle; }
	static CVtxBuffer<SVtxV12C4T16>& GetDiamondVB( void ) { return GetRef().mVtxBuf_Diamond; }
	static CVtxBuffer<SVtxV12C4T16>& GetCircleStripVB( void ) { return GetRef().mVtxBuf_CircleStrip; }
	static CVtxBuffer<SVtxV12C4T16>& GetCircleStripUI( void ) { return GetRef().mVtxBuf_CircleStripUI; }
	static CVtxBuffer<SVtxV12C4T16>& GetCircleUI( void ) { return GetRef().mVtxBuf_CircleUI; }

	static CVtxBuffer<SVtxV12C4T16>& GetAxisLineVB( void ) { return GetRef().mVtxBuf_AxisLine; }
	static CVtxBuffer<SVtxV12C4T16>& GetAxisConeVB( void ) { return GetRef().mVtxBuf_AxisCone; }
	static CVtxBuffer<SVtxV12C4T16>& GetAxisBoxVB( void ) { return GetRef().mVtxBuf_AxisBox; }

	///////////////////////////////////////////////////////////////////////////////
	// other types of prims

	static void RenderOrthoQuad( GfxTarget *pTarg, f32 fX1, f32 fX2, f32 fY1, f32 fY2, f32 iminU, f32 imaxU, f32 iminV, f32 imaxV );
	static void RenderQuadAtX( GfxTarget *pTarg, f32 fY1, f32 fY2, f32 fZ1, f32 fZ2, f32 fX, f32 iminU, f32 imaxU, f32 iminV, f32 imaxV );
	static void RenderQuadAtY( GfxTarget *pTarg, f32 fX1, f32 fX2, f32 fZ1, f32 fZ2, f32 fY, f32 iminU, f32 imaxU, f32 iminV, f32 imaxV );
	static void RenderQuadAtZ( GfxTarget *pTarg, f32 fX1, f32 fX2, f32 fY1, f32 fY2, f32 fZ, f32 iminU, f32 imaxU, f32 iminV, f32 imaxV );

	static void RenderQuad( GfxTarget *pTarg, CVector4 &V0, CVector4 &V1, CVector4 &V2, CVector4 &V3 );

	///////////////////////////////////////////////////////////////////////////////

	private:

	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_GridX100;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_Cone;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_DirCone;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_Axis;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_TriCircle;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_Diamond;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_EQSphere;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_SkySphere;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_GroundPlane;
	StaticVertexBuffer<SVtxV12N12B12T8C4>	mVtxBuf_PerlinTerrain;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_CircleStrip;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_CircleStripUI;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_CircleUI;

	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_Cylinder;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_Capsule;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_Dome;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_Box;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_AxisLine;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_AxisCone;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_AxisBox;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_WireFrameCylinder;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_WireFrameBox;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_WireFrameCapsule;
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_WireFrameDome;

	GfxMaterial3DSolid							mMaterial;

};

///////////////////////////////////////////////////////////////////////////////

} }

#endif
