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

class GfxPrimitives : public NoRttiSingleton< GfxPrimitives >
{
	public:

	GfxPrimitives();
	//static void ClassInit() {}
	static void Init( Context *pTarg );

///////////////////////////////////////////////////////////////////////////////

	static void RenderGridX100( Context *pTarg );
	static void RenderCone( Context *pTarg );
	static void RenderAxis( Context *pTarg );
	static void RenderTriCircle( Context *pTarg );
	static void RenderDiamond( Context *pTarg );
	static void RenderRotManip( Context *pTarg );
	static void RenderEQSphere( Context *pTarg );
	static void RenderSkySphere( Context *pTarg );
	static void RenderGroundPlane( Context *pTarg );
	static void RenderPerlinTerrain( Context *pTarg );
	static void RenderCircleStrip( Context *pTarg );
	static void RenderCircleStripUI( Context *pTarg );
	static void RenderCircleUI( Context *pTarg );
	static void RenderDirCone( Context *pTarg );

	static void RenderCylinder( Context *pTarg ,bool drawoutline = false);
	static void RenderCapsule( Context *pTarg , float radius );
	static void RenderBox( Context *pTarg ,bool drawoutline = false);
	static void RenderAxisLineCone(Context* pTarg);
	static void RenderAxisBox(Context* pTarg);
	static void RenderDome( Context *pTarg )         { pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_Dome ); }
	static void RenderWireFrameBox( Context *pTarg ) { pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_WireFrameBox ); }
	static void RenderWireFrameDome( Context *pTarg ) { pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_WireFrameDome ); }


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
	static CVtxBuffer<SVtxV12C4T16>& GetEQSphere( void ) { return GetRef().mVtxBuf_EQSphere; }
	static CVtxBuffer<SVtxV12C4T16>& GetFullSphere( void ) { return GetRef().mVtxBuf_FullSphere; }

	///////////////////////////////////////////////////////////////////////////////
	// other types of prims

	static void RenderOrthoQuad( Context *pTarg, f32 fX1, f32 fX2, f32 fY1, f32 fY2, f32 iminU, f32 imaxU, f32 iminV, f32 imaxV );
	static void RenderQuadAtX( Context *pTarg, f32 fY1, f32 fY2, f32 fZ1, f32 fZ2, f32 fX, f32 iminU, f32 imaxU, f32 iminV, f32 imaxV );
	static void RenderQuadAtY( Context *pTarg, f32 fX1, f32 fX2, f32 fZ1, f32 fZ2, f32 fY, f32 iminU, f32 imaxU, f32 iminV, f32 imaxV );
	static void RenderQuadAtZ( Context *pTarg, f32 fX1, f32 fX2, f32 fY1, f32 fY2, f32 fZ, f32 iminU, f32 imaxU, f32 iminV, f32 imaxV );
	static void RenderQuadAtZV16T16C16( Context *pTarg, f32 fX1, f32 fX2, f32 fY1, f32 fY2, f32 fZ, f32 iminU, f32 imaxU, f32 iminV, f32 imaxV );

	static void RenderQuad( Context *pTarg, fvec4 &V0, fvec4 &V1, fvec4 &V2, fvec4 &V3 );

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
	StaticVertexBuffer<SVtxV12C4T16>			mVtxBuf_FullSphere;
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
