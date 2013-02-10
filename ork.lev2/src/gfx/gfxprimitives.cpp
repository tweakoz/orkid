////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/math/misc_math.h>
#include <math.h>

///////////////////////////////////////////////////////////////////////////////

#define GRIDDIVS			32
#define NUM_GROUND_LINES	80
#define CONEDIVS			63
//#define CONEDIVS			10000
#define CONESIZE			1.0f

#define CIRCSEGS			32

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static const int inumgroundverts = (NUM_GROUND_LINES*NUM_GROUND_LINES)*6;
static const int NUM_CYLINDER_FACES = 60;
static const int NUM_SPHERE_TRIANGLES = CIRCSEGS * (CIRCSEGS * 2);
static const int NUM_DOME_TRIANGLES = 10 * 50 * 6;



CGfxPrimitives::CGfxPrimitives()
	: NoRttiSingleton< CGfxPrimitives > ()
	, mVtxBuf_Axis( 6, 6, EPRIM_LINES )
	, mVtxBuf_GridX100( 1000, 400, EPRIM_LINES )
	, mVtxBuf_Cone( 1000, 400, EPRIM_TRIANGLES )
	, mVtxBuf_DirCone( 1000, 400, EPRIM_TRIANGLES )
	, mVtxBuf_TriCircle( 6*(CIRCSEGS+2), 6*(CIRCSEGS+2), EPRIM_LINES )
	, mVtxBuf_Diamond( 3*6, 3*6, EPRIM_TRIANGLES )
	, mVtxBuf_EQSphere( 1024, 1024, EPRIM_POINTS )
	, mVtxBuf_SkySphere( 32768, 32768, EPRIM_TRIANGLES ) // EPRIM_TRIANGLES
	, mVtxBuf_GroundPlane( 6, 6, EPRIM_TRIANGLES )
	, mVtxBuf_PerlinTerrain( inumgroundverts, inumgroundverts, EPRIM_TRIANGLES )
	, mVtxBuf_CircleStrip( 1000, 1000, EPRIM_TRIANGLES )
	, mVtxBuf_CircleStripUI( 1000, 1000, EPRIM_QUADS )
	, mVtxBuf_CircleUI( 1000, 1000, EPRIM_TRIANGLES )
	, mVtxBuf_Cylinder( 12*NUM_CYLINDER_FACES + 2 ,  12*NUM_CYLINDER_FACES + 2 , EPRIM_TRIANGLES)
	, mVtxBuf_Capsule( (6*NUM_CYLINDER_FACES + 2) , (6*NUM_CYLINDER_FACES + 2) , EPRIM_TRIANGLES)
	, mVtxBuf_Box( 6*2*3, 6*2*3, EPRIM_TRIANGLES)
	, mVtxBuf_AxisLine( 6*2*3, 6*2*3, EPRIM_TRIANGLES )
	, mVtxBuf_AxisCone(1000, 400, EPRIM_TRIANGLES)
	, mVtxBuf_AxisBox(36, 36, EPRIM_TRIANGLES)
	, mVtxBuf_Dome( NUM_DOME_TRIANGLES, NUM_DOME_TRIANGLES, EPRIM_TRIANGLES)
	, mVtxBuf_WireFrameCylinder( 4*NUM_CYLINDER_FACES + 2, 4*NUM_CYLINDER_FACES + 2, EPRIM_LINES)
	, mVtxBuf_WireFrameBox( 6*4*2, 6*4*2, EPRIM_LINES)
	, mVtxBuf_WireFrameCapsule( 6*4*2, 6*4*2, EPRIM_LINES)
	, mVtxBuf_WireFrameDome(  6*(CIRCSEGS+2), 6*(CIRCSEGS+2), EPRIM_LINES)
{

}

///////////////////////////////////////////////////////////////////////////////

void CGfxPrimitives::Init( GfxTarget *pTarg )
{
	static bool binit = true;

	if( binit )
	{
		binit=false;
	}
	else return;

	GetRef().mMaterial.Init(pTarg);
	GetRef().mMaterial.SetColorMode( ork::lev2::GfxMaterial3DSolid::EMODE_VERTEXMOD_COLOR );

	//orkprintf( "Inititializing Primitives\n" );

	CColor4 Color0, Color1, Color2, Color3;

	////////////////////////////////////////////////////
	// Axis

	F32 fLineSize = 1.0f;
	
	lev2::VtxWriter<SVtxV12C4T16> vw;

	vw.Lock( pTarg, & GetRef().mVtxBuf_Axis, 6 );

	vw.AddVertex( SVtxV12C4T16( 0.0f, 0.0f, 0.0f, 0, 0, CColor4::Red().GetARGBU32() ) );
	vw.AddVertex( SVtxV12C4T16( fLineSize, 0.0f, 0.0f, 0, 0, CColor4::Red().GetARGBU32() ) );
	vw.AddVertex( SVtxV12C4T16( 0.0f, 0.0f, 0.0f, 0, 0, CColor4::Green().GetARGBU32() ) );
	vw.AddVertex( SVtxV12C4T16( 0.0f, fLineSize, 0.0f, 0, 0, CColor4::Green().GetARGBU32() ) );
	vw.AddVertex( SVtxV12C4T16( 0.0f, 0.0f, 0.0f, 0, 0, CColor4::Blue().GetARGBU32() ) );
	vw.AddVertex( SVtxV12C4T16( 0.0f, 0.0f, fLineSize, 0, 0, CColor4::Blue().GetARGBU32() ) );

	vw.UnLock( pTarg,EULFLG_ASSIGNVBLEN );

	////////////////////////////////////////////////////
	// Grid

	int iNumGridLines = GRIDDIVS;

	int icount = (iNumGridLines*4)+2;
	vw.Lock( pTarg, & GetRef().mVtxBuf_GridX100, icount );

	fLineSize = 100.0f;

	f32 fBas0 = -fLineSize;
	f32 fSca0 = (2.0f*fLineSize)/(F32)iNumGridLines;

	for( int i=0; i<iNumGridLines; i++ )
	{
		F32 fVal = fBas0+(i*fSca0);

		U32 uColorX = CVector4( 0.5f, 0.5f, 0.5f, 1.0f ).GetABGRU32();
		U32 uColorZ = CVector4( 0.5f, 0.5f, 0.5f, 1.0f ).GetABGRU32();

		if( i==(GRIDDIVS>>1) )
		{
			uColorX = CVector4( 1.0f, 0.0f, 0.0f, 1.0f ).GetABGRU32();
			uColorZ = CVector4( 0.0f, 0.0f, 1.0f, 1.0f ).GetABGRU32();
		}
		vw.AddVertex( SVtxV12C4T16( -fLineSize, 0.0f, fVal, 0, 0, uColorX ) );
		vw.AddVertex( SVtxV12C4T16( fLineSize, 0.0f, fVal, 0, 0, uColorX ) );
		vw.AddVertex( SVtxV12C4T16( fVal, 0.0f, -fLineSize, 0, 0, uColorZ ) );
		vw.AddVertex( SVtxV12C4T16( fVal, 0.0f, fLineSize, 0, 0, uColorZ ) );
	}

	vw.AddVertex( SVtxV12C4T16( 0.0f, -fLineSize, 0.0f, 0, 0, CVector4( 0.0f, 1.0f, 0.0f, 1.0f ).GetABGRU32() ) );
	vw.AddVertex( SVtxV12C4T16( 0.0f, fLineSize, 0.0f, 0, 0, CVector4( 0.0f, 1.0f, 0.0f, 1.0f ).GetABGRU32() ) );

	//SVtxV12C4T16 *pVTX = (SVtxV12C4T16*) GetRef().mVtxBuf_GridX100.GetVertexPointer();
	//SVtxV12C4T16 *pVTX0 = pVTX;

	vw.UnLock( pTarg,EULFLG_ASSIGNVBLEN ); //pTarg->GBI()->UnLockVB( GetRef().mVtxBuf_GridX100 );

	////////////////////////////////////////////////////
	// CircleStrip

	icount = (CONEDIVS*6);
	vw.Lock( pTarg, & GetRef().mVtxBuf_CircleStrip,  icount );

	U32 uColor = CVector4( 0.5f, 0.5f, 0.5f, 1.0f ).GetARGBU32();

	f32 fOuterSize = 1.0f;
	f32 fInnerSize = 0.85f;

	for( int i=0; i<CONEDIVS; i++ )
	{
		F32 fi = i / (F32) CONEDIVS;
		F32 fi2 = (i+1) / (F32) CONEDIVS;

		F32 fPhase = PI2*fi;
		F32 fPhase2 = PI2*fi2;
		F32 fX0 = sinf( fPhase ) * fOuterSize;
		F32 fZ0 = cosf( fPhase ) * fOuterSize;
		F32 fX1 = sinf( fPhase2 ) * fOuterSize;
		F32 fZ1 = cosf( fPhase2 ) * fOuterSize;
		F32 fX2 = sinf( fPhase ) * fInnerSize;
		F32 fZ2 = cosf( fPhase ) * fInnerSize;
		F32 fX3 = sinf( fPhase2 ) * fInnerSize;
		F32 fZ3 = cosf( fPhase2 ) * fInnerSize;

		vw.AddVertex( SVtxV12C4T16( fX0, 0.0f, fZ0, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX2, 0.0f, fZ2, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX3, 0.0f, fZ3, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( fX1, 0.0f, fZ1, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX0, 0.0f, fZ0, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX3, 0.0f, fZ3, 0, 0, uColor ) );

	}

	vw.UnLock( pTarg,EULFLG_ASSIGNVBLEN ); //->GBI()->UnLockVB( GetRef().mVtxBuf_CircleStrip );

	////////////////////////////////////////////////////
	// CircleStripUI

	vw.Lock( pTarg, & GetRef().mVtxBuf_CircleStripUI, CONEDIVS*4 );

	fOuterSize = 1.0f;
	fInnerSize = 0.5f;

	f32 fcenterx = 0.0f;
	f32 fcentery = 0.0f;

	for( int i=0; i<CONEDIVS; i++ )
	{
		F32 fi = i / (F32) CONEDIVS;
		F32 fi2 = (i+1) / (F32) CONEDIVS;

		F32 fPhase = PI2*fi;
		F32 fPhase2 = PI2*fi2;
		F32 fX0 = fcenterx+sinf( fPhase ) * fOuterSize;
		F32 fZ0 = fcentery+cosf( fPhase ) * fOuterSize;
		F32 fX1 = fcenterx+sinf( fPhase2 ) * fOuterSize;
		F32 fZ1 = fcentery+cosf( fPhase2 ) * fOuterSize;
		F32 fX2 = fcenterx+sinf( fPhase ) * fInnerSize;
		F32 fZ2 = fcentery+cosf( fPhase ) * fInnerSize;
		F32 fX3 = fcenterx+sinf( fPhase2 ) * fInnerSize;
		F32 fZ3 = fcentery+cosf( fPhase2 ) * fInnerSize;

		f32 fC0 = 1.0f - (0.5f+cosf( fPhase )*0.5f);
		f32 fC1 = 1.0f - (0.5f+cosf( fPhase2 )*0.5f);

		fC0 = 0.5f + (0.5f*fC0);
		fC1 = 0.5f + (0.5f*fC1);

		Color0.SetXYZ( fC0, 0.0f, 0.0f );
		Color1.SetXYZ( fC1, 0.0f, 0.0f );
		Color2.SetXYZ( fC0, 0.0f, 1.0f );
		Color3.SetXYZ( fC1, 0.0f, 1.0f );

		vw.AddVertex( SVtxV12C4T16( fX0, fZ0, 0.0f, 0, 0, Color2.GetARGBU32() ) );
		vw.AddVertex( SVtxV12C4T16( fX2, fZ2, 0.0f, 0, 0, Color0.GetARGBU32() ) );
		vw.AddVertex( SVtxV12C4T16( fX1, fZ1, 0.0f, 0, 0, Color3.GetARGBU32() ) );
		vw.AddVertex( SVtxV12C4T16( fX3, fZ3, 0.0f, 0, 0, Color1.GetARGBU32() ) );

	}

	vw.UnLock(pTarg,EULFLG_ASSIGNVBLEN);
	
	////////////////////////////////////////////////////
	// CircleUI

	vw.Lock( pTarg, & GetRef().mVtxBuf_CircleUI, CONEDIVS*3 );

	fOuterSize = 1.0f;

	fcenterx = 0.0f;
	fcentery = 0.0f;

	for( int i=0; i<CONEDIVS; i++ )
	{
		F32 fi = i / (F32) CONEDIVS;
		F32 fi2 = (i+1) / (F32) CONEDIVS;

		F32 fPhase = PI2*fi;
		F32 fPhase2 = PI2*fi2;
		F32 fX0 = fcenterx+sinf( fPhase ) * fOuterSize;
		F32 fZ0 = fcentery+cosf( fPhase ) * fOuterSize;
		F32 fX1 = fcenterx+sinf( fPhase2 ) * fOuterSize;
		F32 fZ1 = fcentery+cosf( fPhase2 ) * fOuterSize;

		f32 fC0 = 0.5f+cosf( fPhase )*0.5f;
		f32 fC1 = 0.5f+cosf( fPhase2 )*0.5f;
		f32 fCC = 0.5f;

		Color0.SetXYZ( 0.5f+(0.5f*fC0),  0.5f+(0.5f*fC0),  0.5f+(0.5f*fC0) );
		Color1.SetXYZ(  0.5f+(0.5f*fC1),  0.5f+(0.5f*fC1),  0.5f+(0.5f*fC1) );
		Color2.SetXYZ(  0.5f+(0.5f*fCC),  0.5f+(0.5f*fCC),  0.5f+(0.5f*fCC) );

		vw.AddVertex( SVtxV12C4T16( fcenterx, fcentery, 0.0f, 0, 0, Color2.GetARGBU32() ) );
		vw.AddVertex( SVtxV12C4T16( fX1, fZ1, 0.0f, 0, 0, Color1.GetARGBU32() ) );
		vw.AddVertex( SVtxV12C4T16( fX0, fZ0, 0.0f, 0, 0, Color0.GetARGBU32() ) );
		//GetRef().mVtxBuf_CircleStripUI.AddVertex( SVtxV12C4T16( fX3, fZ3, 0.0f, 0, 0, uColor ) );

	}

	vw.UnLock(pTarg,EULFLG_ASSIGNVBLEN);
	
	////////////////////////////////////////////////////
	// Cone

	vw.Lock( pTarg, & GetRef().mVtxBuf_Cone, CONEDIVS*3 );

	for( int i=0; i<CONEDIVS; i++ )
	{	F32 fi = i / (F32) CONEDIVS;
		F32 fi2 = (i+1) / (F32) CONEDIVS;
		F32 fPhase = PI2*fi;
		F32 fPhase2 = PI2*fi2;
		F32 fX = sinf( fPhase ) * CONESIZE;
		F32 fZ = cosf( fPhase ) * CONESIZE;
		F32 fX2 = sinf( fPhase2 ) * CONESIZE;
		F32 fZ2 = cosf( fPhase2 ) * CONESIZE;
		U32 uColor = CVector4( 0.5f, 0.5f, 0.5f, 1.0f ).GetARGBU32();
		vw.AddVertex( SVtxV12C4T16( fX, 0.0f, fZ, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX2, 0.0f, fZ2, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( 0.0f, CONESIZE, 0.0f, 0, 0, uColor ) );
	}
	vw.UnLock(pTarg,EULFLG_ASSIGNVBLEN);
	
	////////////////////////////////////////////////////
	// DirCone

	vw.Lock( pTarg, & GetRef().mVtxBuf_DirCone, CONEDIVS*3 );
	for( int i=0; i<CONEDIVS; i++ )
	{	F32 fi = i / (F32) CONEDIVS;
		F32 fi2 = (i+1) / (F32) CONEDIVS;
		F32 fPhase = PI2*fi;
		F32 fPhase2 = PI2*fi2;
		F32 fX = sinf( fPhase ) * CONESIZE;
		F32 fZ = cosf( fPhase ) * CONESIZE;
		F32 fX2 = sinf( fPhase2 ) * CONESIZE;
		F32 fZ2 = cosf( fPhase2 ) * CONESIZE;
		U32 uColor = CVector4( 0.5f, 0.5f, 0.5f, 1.0f ).GetARGBU32();
		static const float dirscaleXZ = 1.5f;
		vw.AddVertex( SVtxV12C4T16( fX*dirscaleXZ, 0.0f, fZ*dirscaleXZ, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX2*dirscaleXZ, 0.0f, fZ2*dirscaleXZ, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( 0.0f, CONESIZE*20.0f, 0.0f, 0, 0, uColor ) );
	}
	vw.UnLock(pTarg,EULFLG_ASSIGNVBLEN);

	////////////////////////////////////////////////////
	// Cylinder

	vw.Lock( pTarg, & GetRef().mVtxBuf_Cylinder, NUM_CYLINDER_FACES*12 );
	float baseAngle = PI2 / NUM_CYLINDER_FACES;
	for( int i=0; i<NUM_CYLINDER_FACES; i++ )
	{
		float angle = baseAngle * i;
		float angle2 = baseAngle * (i + 1);

		F32 fX = cosf(angle);
		F32 fZ = sinf( angle );

		F32 fX2 = cosf(angle2);
		F32 fZ2 = sinf( angle2 );


		U32 uColor = CVector4( 0.5f, 0.5f, 0.5f, 1.0f ).GetARGBU32();

		vw.AddVertex( SVtxV12C4T16( fX, 0.0f, fZ, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX2, 0.0f, fZ2, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX, 1.0f, fZ, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( fX, 1.0f, fZ, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX2, 0.0f, fZ2, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX2, 1.0f, fZ2, 0, 0, uColor ) );

		//caps
		vw.AddVertex( SVtxV12C4T16( 0.0f, 0.0f, 0.0f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX, 0.0f, fZ, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX2, 0.0f, fZ2, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( 0.0f, 1.0f, 0.0f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX, 1.0f, fZ, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX2, 1.0f, fZ2, 0, 0, uColor ) );
	}
	vw.UnLock(pTarg,EULFLG_ASSIGNVBLEN);

	////////////////////////////////////////////////////
	// Capsule is just a cylinder with no caps may go away
	vw.Lock( pTarg, & GetRef().mVtxBuf_Capsule, NUM_CYLINDER_FACES*6 );
	baseAngle = PI2 / NUM_CYLINDER_FACES;
	for( int i=0; i<NUM_CYLINDER_FACES; i++ )
	{	float angle = baseAngle * i;
		float angle2 = baseAngle * (i + 1);
		F32 fX = cosf(angle);
		F32 fZ = sinf( angle );
		F32 fX2 = cosf(angle2);
		F32 fZ2 = sinf( angle2 );
		U32 uColor = CVector4( 0.5f, 0.5f, 0.5f, 1.0f ).GetARGBU32();
		vw.AddVertex( SVtxV12C4T16( fX, 0.0f, fZ, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX2,0.0f, fZ2, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX, 1.0f, fZ, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX, 1.0f, fZ, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX2, 0.0f, fZ2, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX2, 1.0f, fZ2, 0, 0, uColor ) );
	}
	vw.UnLock(pTarg,EULFLG_ASSIGNVBLEN);

	////////////////////////////////////////////////////
	// Wired Dome

	vw.Lock( pTarg, & GetRef().mVtxBuf_WireFrameDome, CIRCSEGS*6 );
	F32 fiCIRCSEGS = (F32) CIRCSEGS;
	for( int i=0; i<CIRCSEGS; i++ )
	{	F32 fPhase = PI2 * i / fiCIRCSEGS;
		F32 fX = sinf( fPhase );
		F32 fZ = cosf( fPhase );
		F32 fPhase2 = PI2 * (i+1) / fiCIRCSEGS;
		F32 fX2 = sinf( fPhase2 );
		F32 fZ2 = cosf( fPhase2 );
		vw.AddVertex( SVtxV12C4T16( fX, 0.0f, fZ, 0, 0, CColor4::Green().GetARGBU32() ) );
		vw.AddVertex( SVtxV12C4T16( fX2, 0.0f, fZ2, 0, 0, CColor4::Green().GetARGBU32() ) );
	}
	for( int i=0; i<CIRCSEGS; i++ )
	{	F32 fPhase = ((PI * i) + PI)  / fiCIRCSEGS;
		F32 fX = sinf( fPhase);
		F32 fZ = cosf( fPhase );
		F32 fPhase2 = ((PI * (i+1)) + PI)  / fiCIRCSEGS;
		F32 fX2 = sinf( fPhase2 );
		F32 fZ2 = cosf( fPhase2 );
		vw.AddVertex( SVtxV12C4T16( fX, fZ, 0.0f, 0, 0, CColor4::Blue().GetARGBU32() ) );
		vw.AddVertex( SVtxV12C4T16( fX2, fZ2, 0.0f, 0, 0, CColor4::Blue().GetARGBU32() ) );
	}
	for( int i=0; i<CIRCSEGS; i++ )
	{	F32 fPhase = PI * i / fiCIRCSEGS;
		F32 fX = sinf( fPhase );
		F32 fZ = cosf( fPhase );
		F32 fPhase2 = PI * (i+1) / fiCIRCSEGS;
		F32 fX2 = sinf( fPhase2 );
		F32 fZ2 = cosf( fPhase2 );
		vw.AddVertex( SVtxV12C4T16( 0.0f, fX, fZ, 0, 0, CColor4::Red().GetARGBU32() ) );
		vw.AddVertex( SVtxV12C4T16( 0.0f, fX2, fZ2, 0, 0, CColor4::Red().GetARGBU32() ) );
	}
	vw.UnLock( pTarg,EULFLG_ASSIGNVBLEN );

	////////////////////////////////////////////////////
	// Dome
		
	//caps
	const int kiUD = 50;
	const int kiVD = 10;
	//f32 fZ = 0.0f;

	const float kinvU = 1.0f / float(kiUD);
	const float kinvV = (0.5f / float(kiVD));
	const float kytopoff = 0.0f;
	const float kybotoff = -0.0f;

	vw.Lock( pTarg, & GetRef().mVtxBuf_Dome, kiUD*kiVD*6 );

	for( int iU=0; iU<kiUD; iU++ )
	{
		f32 fU1 = iU*kinvU;
		f32 fU2 = fU1+kinvU;

		float fsinU1 = sinf( fU1*PI2 );
		float fsinU2 = sinf( fU2*PI2 );
		float fcosU1 = cosf( fU1*PI2 );
		float fcosU2 = cosf( fU2*PI2 );

		for( int iV=0; iV<kiVD; iV++ )
		{
			f32 fV1 = iV*kinvV;
			f32 fV2 = fV1+kinvV;

			float fsinV1 = sinf( fV1*PI );
			float fsinV2 = sinf( fV2*PI );
			float fcosV1 = cosf( fV1*PI );
			float fcosV2 = cosf( fV2*PI );

			/////////////////////////
			// lower 2
			float fX1 = fsinU1*fcosV1;
			float fY1 = fsinV1 + kytopoff;
			float fbY1 = -fsinV1 - kytopoff;
			float fZ1 = fcosU1*fcosV1;

			float fX2 = fsinU2*fcosV1;
			float fY2 = fsinV1 + kytopoff;
			float fbY2 = -fsinV1 - kytopoff;
			float fZ2 = fcosU2*fcosV1;

			/////////////////////////
			// upper 2
			float fX3 = fsinU1*fcosV2;
			float fY3 = fsinV2 + kytopoff;
			float fbY3 = -fsinV2 - kytopoff;
			float fZ3 = fcosU1*fcosV2;

			float fX4 = fsinU2*fcosV2;
			float fY4 = fsinV2 + kytopoff;
			float fbY4 = -fsinV2 - kytopoff;
			float fZ4 = fcosU2*fcosV2;
			/////////////////////////
	#if 0
			GetRef().mVtxBuf_Capsule.AddVertex( SVtxV12C4T16( fX1, fbY1, fZ1, 0.0f, 0.0f, 0x11111111 ) );
			GetRef().mVtxBuf_Capsule.AddVertex( SVtxV12C4T16( fX2, fbY2, fZ2, 0.0f, 0.0f, 0x44444444 ) );
			GetRef().mVtxBuf_Capsule.AddVertex( SVtxV12C4T16( fX3, fbY3, fZ3, 0.0f, 0.0f, 0x77777777 ) );

			GetRef().mVtxBuf_Capsule.AddVertex( SVtxV12C4T16( fX4, fbY4, fZ4, 0.0f, 0.0f, 0xaaaaaaaa ) );
			GetRef().mVtxBuf_Capsule.AddVertex( SVtxV12C4T16( fX3, fbY3, fZ3, 0.0f, 0.0f, 0xcccccccc ) );
			GetRef().mVtxBuf_Capsule.AddVertex( SVtxV12C4T16( fX2, fbY2, fZ2, 0.0f, 0.0f, 0xffffffff ) );
	#else
			vw.AddVertex( SVtxV12C4T16( fX3, fY3, fZ3, 0.0f, 0.0f, 0x11111111 ) );
			vw.AddVertex( SVtxV12C4T16( fX2, fY2, fZ2, 0.0f, 0.0f, 0x44444444 ) );
			vw.AddVertex( SVtxV12C4T16( fX1, fY1, fZ1, 0.0f, 0.0f, 0x77777777 ) );

			vw.AddVertex( SVtxV12C4T16( fX2, fY2, fZ2, 0.0f, 0.0f, 0xaaaaaaaa ) );
			vw.AddVertex( SVtxV12C4T16( fX3, fY3, fZ3, 0.0f, 0.0f, 0xcccccccc ) );
			vw.AddVertex( SVtxV12C4T16( fX4, fY4, fZ4, 0.0f, 0.0f, 0xffffffff ) );
	#endif


		}

	}
	vw.UnLock(pTarg,EULFLG_ASSIGNVBLEN);
	
	////////////////////////////////////////////////////
	// Box

	vw.Lock( pTarg, & GetRef().mVtxBuf_Box, 12*3 );
	{
		U32 uColor = CVector4( 0.5f, 0.5f, 0.5f, 1.0f ).GetARGBU32();

		vw.AddVertex( SVtxV12C4T16( 0.5f, 0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-0.5f, 0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( 0.5f,-0.5f, 0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16(-0.5f, 0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-0.5f,-0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( 0.5f,-0.5f, 0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( 0.5f, 0.5f,-0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-0.5f, 0.5f,-0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( 0.5f,-0.5f,-0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16(-0.5f, 0.5f,-0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-0.5f,-0.5f,-0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( 0.5f,-0.5f,-0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( 0.5f, 0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-0.5f, 0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( 0.5f, 0.5f,-0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16(-0.5f, 0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-0.5f, 0.5f,-0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( 0.5f, 0.5f,-0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( 0.5f,-0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-0.5f,-0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( 0.5f,-0.5f,-0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16(-0.5f,-0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-0.5f,-0.5f,-0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( 0.5f,-0.5f,-0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( 0.5f, 0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( 0.5f,-0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( 0.5f, 0.5f,-0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( 0.5f,-0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( 0.5f,-0.5f,-0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( 0.5f, 0.5f,-0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16(-0.5f, 0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-0.5f,-0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-0.5f, 0.5f,-0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16(-0.5f,-0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-0.5f,-0.5f,-0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-0.5f, 0.5f,-0.5f, 0, 0, uColor ) );

	}
	vw.UnLock(pTarg,EULFLG_ASSIGNVBLEN);
	
	////////////////////////////////////////////////////
	// Axis Line

	vw.Lock( pTarg, & GetRef().mVtxBuf_AxisLine, 36 );
	{
		U32 uColor = CVector4( 0.5f, 0.5f, 0.5f, 1.0f ).GetARGBU32();

		float width = 0.05f;
		float length = 0.8f;

		vw.AddVertex( SVtxV12C4T16( width, length, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, length, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, 0, width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16(-width, length, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, 0, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, 0, width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( width, 0, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, length, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, length, -width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( width, 0, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, 0, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, length, -width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( width, length, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, length, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, length, width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( width, length, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, length, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, length, width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( width, 0, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, 0, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, 0, -width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16(-width, 0, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, 0, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, 0, -width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( width, length, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, 0, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, length, -width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( width, 0, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, 0, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, length, -width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16(-width, length, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, 0, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, length, width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16(-width, length, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, 0, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, 0, width, 0, 0, uColor ) );
	}
	vw.UnLock(pTarg,EULFLG_ASSIGNVBLEN);

	////////////////////////////////////////////////////
	// Axis Cone
	vw.Lock( pTarg, & GetRef().mVtxBuf_AxisCone, CONEDIVS*3 );
	for(int i = 0; i < CONEDIVS; i++)
	{
		F32 fi = i / (F32)CONEDIVS;
		F32 fi2 = (i+1) / (F32)CONEDIVS;

		F32 fPhase = PI2 * fi;
		F32 fPhase2 = PI2 * fi2;
		F32 fX = sinf( fPhase )/20.0f;
		F32 fZ = cosf( fPhase )/20.0f;
		F32 fX2 = sinf( fPhase2 )/20.0f;
		F32 fZ2 = cosf( fPhase2 )/20.0f;

		U32 uColor = CVector4( 0.5f, 0.5f, 0.5f, 1.0f ).GetARGBU32();

		vw.AddVertex( SVtxV12C4T16( fX, 15.0f/20.0f, fZ, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( fX2, 15.0f/20.0f, fZ2, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( 0.0f, 20.0f/20.0f, 0.0f, 0, 0, uColor ) );
	}
	vw.UnLock(pTarg,EULFLG_ASSIGNVBLEN);

	// sm - for trans manip pick buffer
	////////////////////////////////////////////////////
	// Axis Box

	vw.Lock( pTarg, & GetRef().mVtxBuf_AxisBox, 36 );
	{
		U32 uColor = CVector4( 0.5f, 0.5f, 0.5f, 1.0f ).GetARGBU32();

		float width = 0.1f;
		float length = 1.0f;

		vw.AddVertex( SVtxV12C4T16( width, length, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, length, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, 0, width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16(-width, length, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, 0, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, 0, width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( width, length, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, length, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, 0, -width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16(-width, length, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, 0, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, 0, -width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( width, length, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, length, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, length, -width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16(-width, length, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, length, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, length, -width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( width, 0, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, 0, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, 0, -width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16(-width, 0, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, 0, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, 0, -width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( width, length, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, 0, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, length, -width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( width, 0, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, 0, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( width, length, -width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16(-width, length, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, 0, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, length, -width, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16(-width, 0, width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, 0, -width, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-width, length, -width, 0, 0, uColor ) );
	}
	vw.UnLock(pTarg,EULFLG_ASSIGNVBLEN);

	vw.Lock( pTarg, & GetRef().mVtxBuf_WireFrameBox, 24 );
	{
		U32 uColor = CVector4( 0.5f, 0.5f, 0.5f, 1.0f ).GetARGBU32();

		vw.AddVertex( SVtxV12C4T16( 0.5f, 0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-0.5f, 0.5f, 0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( 0.5f, 0.5f, -0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-0.5f, 0.5f, -0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( 0.5f, 0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( 0.5f, 0.5f, -0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16(-0.5f, 0.5f, 0.5f, 0, 0, uColor ) );
	  	vw.AddVertex( SVtxV12C4T16(-0.5f, 0.5f, -0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( 0.5f, 0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( 0.5f, -0.5f, 0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( -0.5f, 0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( -0.5f, -0.5f, 0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( 0.5f, 0.5f, -0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( 0.5f, -0.5f, -0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( -0.5f, 0.5f,-0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( -0.5f,-0.5f,-0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( 0.5f, -0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-0.5f, -0.5f, 0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( 0.5f, -0.5f, -0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16(-0.5f, -0.5f, -0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16( 0.5f, -0.5f, 0.5f, 0, 0, uColor ) );
		vw.AddVertex( SVtxV12C4T16( 0.5f, -0.5f, -0.5f, 0, 0, uColor ) );

		vw.AddVertex( SVtxV12C4T16(-0.5f, -0.5f, 0.5f, 0, 0, uColor ) );
	  	vw.AddVertex( SVtxV12C4T16(-0.5f, -0.5f, -0.5f, 0, 0, uColor ) );
	}
	vw.UnLock(pTarg,EULFLG_ASSIGNVBLEN);

	////////////////////////////////////////////////////
	// TriCircle
	{
		vw.Lock( pTarg, & GetRef().mVtxBuf_TriCircle, CIRCSEGS*6 );

		F32 fiCIRCSEGS = (F32) CIRCSEGS;

		for( int i=0; i<CIRCSEGS; i++ )
		{	F32 fPhase = PI2 * i / fiCIRCSEGS;
			F32 fX = sinf( fPhase );
			F32 fZ = cosf( fPhase );
			F32 fPhase2 = PI2 * (i+1) / fiCIRCSEGS;
			F32 fX2 = sinf( fPhase2 );
			F32 fZ2 = cosf( fPhase2 );
			vw.AddVertex( SVtxV12C4T16( fX, 0.0f, fZ, 0, 0, CColor4::Green().GetARGBU32() ) );
			vw.AddVertex( SVtxV12C4T16( fX2, 0.0f, fZ2, 0, 0, CColor4::Green().GetARGBU32() ) );
		}
		for( int i=0; i<CIRCSEGS; i++ )
		{	F32 fPhase = PI2 * i / fiCIRCSEGS;
			F32 fX = sinf( fPhase );
			F32 fZ = cosf( fPhase );
			F32 fPhase2 = PI2 * (i+1) / fiCIRCSEGS;
			F32 fX2 = sinf( fPhase2 );
			F32 fZ2 = cosf( fPhase2 );
			vw.AddVertex( SVtxV12C4T16( fX, fZ, 0.0f, 0, 0, CColor4::Blue().GetARGBU32() ) );
			vw.AddVertex( SVtxV12C4T16( fX2, fZ2, 0.0f, 0, 0, CColor4::Blue().GetARGBU32() ) );
		}
		for( int i=0; i<CIRCSEGS; i++ )
		{	F32 fPhase = PI2 * i / fiCIRCSEGS;
			F32 fX = sinf( fPhase );
			F32 fZ = cosf( fPhase );
			F32 fPhase2 = PI2 * (i+1) / fiCIRCSEGS;
			F32 fX2 = sinf( fPhase2 );
			F32 fZ2 = cosf( fPhase2 );
			vw.AddVertex( SVtxV12C4T16( 0.0f, fX, fZ, 0, 0, CColor4::Red().GetARGBU32() ) );
			vw.AddVertex( SVtxV12C4T16( 0.0f, fX2, fZ2, 0, 0, CColor4::Red().GetARGBU32() ) );
		}
		vw.UnLock(pTarg,EULFLG_ASSIGNVBLEN);
	}
	////////////////////////////////////////////////////
	// GroundPlane
	{
		vw.Lock( pTarg, & GetRef().mVtxBuf_GroundPlane, 6 );

		///////////////////////////////////////////

		const U32 color = 0xffffffff;

		vw.AddVertex( SVtxV12C4T16( -1.0f, 0.0f, -1.0f, 0, 0, color ) );
		vw.AddVertex( SVtxV12C4T16(  1.0f, 0.0f, -1.0f, 0, 0, color ) );
		vw.AddVertex( SVtxV12C4T16(  1.0f, 0.0f,  1.0f, 0, 0, color ) );

		vw.AddVertex( SVtxV12C4T16( -1.0f, 0.0f, -1.0f, 0, 0, color ) );
		vw.AddVertex( SVtxV12C4T16(  1.0f, 0.0f,  1.0f, 0, 0, color ) );
		vw.AddVertex( SVtxV12C4T16( -1.0f, 0.0f,  1.0f, 0, 0, color ) );

		vw.UnLock(pTarg,EULFLG_ASSIGNVBLEN);

	}	////////////////////////////////////////////////////
	// Diamond
	{

		vw.Lock( pTarg, & GetRef().mVtxBuf_Diamond, 18 );

		f32 fscale = 1.0f;

		f32 p0X = sinf( PI2*0.0f )*fscale;
		f32 p0Z = cosf( PI2*0.0f )*fscale;
		f32 p1X = sinf( PI2*0.33f )*fscale;
		f32 p1Z = cosf( PI2*0.33f )*fscale;
		f32 p2X = sinf( PI2*0.66f )*fscale;
		f32 p2Z = cosf( PI2*0.66f )*fscale;

		///////////////////////////////////////////
		// Top Faces

		U32 ucolor = 0xffffffff;

		vw.AddVertex( SVtxV12C4T16( p0X, 0.0f, p0Z, 0, 0, ucolor ) );
		vw.AddVertex( SVtxV12C4T16( p1X, 0.0f, p1Z, 0, 0, ucolor ) );
		vw.AddVertex( SVtxV12C4T16( 0.0f, fscale, 0.0f, 0, 0, ucolor ) );

		vw.AddVertex( SVtxV12C4T16( p1X, 0.0f, p1Z, 0, 0, ucolor ) );
		vw.AddVertex( SVtxV12C4T16( p2X, 0.0f, p2Z, 0, 0, ucolor ) );
		vw.AddVertex( SVtxV12C4T16( 0.0f, fscale, 0.0f, 0, 0, ucolor ) );

		vw.AddVertex( SVtxV12C4T16( p2X, 0.0f, p2Z, 0, 0, ucolor ) );
		vw.AddVertex( SVtxV12C4T16( p0X, 0.0f, p0Z, 0, 0, ucolor ) );
		vw.AddVertex( SVtxV12C4T16( 0.0f, fscale, 0.0f, 0, 0, ucolor ) );

		///////////////////////////////////////////
		// Bottom Faces

		vw.AddVertex( SVtxV12C4T16( p0X, 0.0f, p0Z, 0, 0, ucolor ) );
		vw.AddVertex( SVtxV12C4T16( p1X, 0.0f, p1Z, 0, 0, ucolor ) );
		vw.AddVertex( SVtxV12C4T16( 0.0f, -fscale, 0.0f, 0, 0, ucolor ) );

		vw.AddVertex( SVtxV12C4T16( p1X, 0.0f, p1Z, 0, 0, ucolor ) );
		vw.AddVertex( SVtxV12C4T16( p2X, 0.0f, p2Z, 0, 0, ucolor ) );
		vw.AddVertex( SVtxV12C4T16( 0.0f, -fscale, 0.0f, 0, 0, ucolor ) );

		vw.AddVertex( SVtxV12C4T16( p2X, 0.0f, p2Z, 0, 0, ucolor ) );
		vw.AddVertex( SVtxV12C4T16( p0X, 0.0f, p0Z, 0, 0, ucolor ) );
		vw.AddVertex( SVtxV12C4T16( 0.0f, -fscale, 0.0f, 0, 0, ucolor ) );
		
		vw.UnLock(pTarg,EULFLG_ASSIGNVBLEN);
	}
	////////////////////////////////////////////////////
	// EQSphere (even dispersion of points across sphere, for normal compression )

	{

		f32 fiCIRCSEGS = (F32) 256;
		f32 famp = 50.0f;

		int iringdiv = 1;
		int ipaccum = 0;
		int inumrings = 21;

		u16 normtabX[256];
		u16 normtabY[256];
		u16 normtabZ[256];

		vw.Lock( pTarg, & GetRef().mVtxBuf_EQSphere, inumrings*inumrings );

		for( int iring=0; iring<inumrings; iring++ )
		{
			f32 fringdivphase = ((f32)iring) / ((f32)inumrings-1);


			f32 fringdiv = (1.0f-(0.5f+0.5f*cosf( fringdivphase * PI2 )));

			fringdiv = sqrtf( fringdiv );

			iringdiv = (int) (fringdiv*20.5f);

			iringdiv = (iringdiv<1) ? 1 : iringdiv;

			//orkprintf( "[iring %03d] [iringdiv %02d] [ipaccum %02d]\n", iring, iringdiv, ipaccum );

			//f32 fY = (fringdivphase-0.5f)*famp*2.0f;
			f32 fY = cosf( fringdivphase*PI );
			f32 fmul = sinf( fringdivphase*PI );

			//orkprintf( "///////////////////////////////\n [ring %d] \n\n", iring );

			for( int iv=0; iv<iringdiv; iv++ )
			{
				f32 fphase = ((f32)iv)/((f32)iringdiv);

				F32 fX = sinf( fphase*PI2 )*fmul;
				F32 fZ = cosf( fphase*PI2 )*fmul;

				u32 ucolor = 0xff000000 | (((u32)ipaccum)<<8);

				//orkprintf( "[idx %03d] [%1.4f %1.4f %1.4f]\n", ipaccum, fX, fY, fZ );

				vw.AddVertex( SVtxV12C4T16( fX*famp, fY*famp, fZ*famp, 0, 0, ucolor ) );

				s16 sX = (s16) (fX*32767.0f);
				s16 sY = (s16) (fY*32767.0f);
				s16 sZ = (s16) (fZ*32767.0f);

				normtabX[ipaccum] = *reinterpret_cast<u16*>(&sX);
				normtabY[ipaccum] = *reinterpret_cast<u16*>(&sY);
				normtabZ[ipaccum] = *reinterpret_cast<u16*>(&sZ);

				ipaccum++;

			}

		}
		vw.UnLock(pTarg,EULFLG_ASSIGNVBLEN);
	}

	////////////////////////////////////////////////////
	// SkySphere (portion of sphere directly infront of camera)

	{

		const int kiUD = 100;
		const int kiVD = 20;
		//f32 fZ = 0.0f;

		vw.Lock( pTarg, & GetRef().mVtxBuf_SkySphere, kiUD*kiVD*6 );

		const float kinvU = 1.0f / float(kiUD);
		const float kinvV = (0.5f / float(kiVD));

		for( int iU=0; iU<kiUD; iU++ )
		{
			f32 fU1 = iU*kinvU;
			f32 fU2 = fU1+kinvU;

			float fsinU1 = sinf( fU1*PI2 );
			float fsinU2 = sinf( fU2*PI2 );
			float fcosU1 = cosf( fU1*PI2 );
			float fcosU2 = cosf( fU2*PI2 );

			for( int iV=0; iV<kiVD; iV++ )
			{
				f32 fV1 = iV*kinvV;
				f32 fV2 = fV1+kinvV;

				float fsinV1 = sinf( fV1*PI );
				float fsinV2 = sinf( fV2*PI );
				float fcosV1 = cosf( fV1*PI );
				float fcosV2 = cosf( fV2*PI );

				/////////////////////////
				// lower 2
				float fX1 = fsinU1*fcosV1;
				float fY1 = fsinV1;
				float fZ1 = fcosU1*fcosV1;
				float fuU1 = (fX1+1.0f)*0.5f;
				float fuV1 = (fZ1+1.0f)*0.5f;

				float fX2 = fsinU2*fcosV1;
				float fY2 = fsinV1;
				float fZ2 = fcosU2*fcosV1;
				float fuU2 = (fX2+1.0f)*0.5f;
				float fuV2 = (fZ2+1.0f)*0.5f;

				/////////////////////////
				// upper 2
				float fX3 = fsinU1*fcosV2;
				float fY3 = fsinV2;
				float fZ3 = fcosU1*fcosV2;
				float fuU3 = (fX3+1.0f)*0.5f;
				float fuV3 = (fZ3+1.0f)*0.5f;

				float fX4 = fsinU2*fcosV2;
				float fY4 = fsinV2;
				float fZ4 = fcosU2*fcosV2;
				float fuU4 = (fX4+1.0f)*0.5f;
				float fuV4 = (fZ4+1.0f)*0.5f;
				/////////////////////////

				vw.AddVertex( SVtxV12C4T16( fX1, fY1, fZ1, S16(fuU1*32767.0f), S16(fuV1*32767.0f), 0x11111111 ) );
				vw.AddVertex( SVtxV12C4T16( fX2, fY2, fZ2, S16(fuU2*32767.0f), S16(fuV2*32767.0f), 0x44444444 ) );
				vw.AddVertex( SVtxV12C4T16( fX3, fY3, fZ3, S16(fuU3*32767.0f), S16(fuV3*32767.0f), 0x77777777 ) );

				vw.AddVertex( SVtxV12C4T16( fX4, fY4, fZ4, S16(fuU4*32767.0f), S16(fuV4*32767.0f), 0xaaaaaaaa ) );
				vw.AddVertex( SVtxV12C4T16( fX3, fY3, fZ3, S16(fuU3*32767.0f), S16(fuV3*32767.0f), 0xcccccccc ) );
				vw.AddVertex( SVtxV12C4T16( fX2, fY2, fZ2, S16(fuU2*32767.0f), S16(fuV2*32767.0f), 0xffffffff ) );
			}
		}

		vw.UnLock(pTarg,EULFLG_ASSIGNVBLEN);

		GetRef().mVtxBuf_SkySphere.SetNumVertices( kiUD*kiVD*6 );

	}

	////////////////////////////////////////////////////
	// mVtxBuf_PerlinTerrain
	{

		const int iNumGroundLines = NUM_GROUND_LINES;

		f32 fLineSize = 100.0f;
		f32 fnoisespacescale = 10.0f;

		f32 fBas0 = -fLineSize;
		f32 fSca0 = (2.0f*fLineSize)/(F32)iNumGroundLines;

		f32 fBias = 1.1021f;
		f32 fAmp = 15.0f;
		f32 fFrq = 0.023f;
		f32 fAmpScale = 0.75f;
		f32 fFrqScale = 1.3f;

		int inumoctaves = 24;

		/////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////
		// Compute Heightfield

		CReal fmin =  CFloat::TypeMax();
		CReal fmax = -CFloat::TypeMax();

		struct sheightfield
		{
			const int   minumgl;
			CVector3*	mpxyz;
			CVector2*	mpuv;

			sheightfield( int igl ) : minumgl(igl)
			{
				mpxyz = new CVector3[ igl*igl ];
				mpuv = new CVector2[ igl*igl ];
			}

			CVector3& XYZ( int ix, int iy ) { return mpxyz[ix*minumgl+iy]; }
			CVector2& UV( int ix, int iy ) { return mpuv[ix*minumgl+iy]; }

			CVector3 Normal(int ix1,int iz1)
			{
				int ix0 = (ix1-1); if( ix0<0 ) ix0+=minumgl;
				int iz0 = (iz1-1); if( iz0<0 ) iz0+=minumgl;
				int ix2 = (ix1+1)%minumgl;
				int iz2 = (iz1+1)%minumgl;

				CVector3 d0 = XYZ(ix0,iz0)-XYZ(ix1,iz1);
				CVector3 d1 = XYZ(ix1,iz0)-XYZ(ix1,iz1);
				CVector3 d2 = XYZ(ix2,iz0)-XYZ(ix1,iz1);
				CVector3 d3 = XYZ(ix2,iz1)-XYZ(ix1,iz1);
				CVector3 d4 = XYZ(ix2,iz2)-XYZ(ix1,iz1);
				CVector3 d5 = XYZ(ix1,iz2)-XYZ(ix1,iz1);
				CVector3 d6 = XYZ(ix0,iz2)-XYZ(ix1,iz1);
				CVector3 d7 = XYZ(ix0,iz1)-XYZ(ix1,iz1);

				CVector3 c0 = d0.Cross(d1);
				CVector3 c1 = d1.Cross(d2);
				CVector3 c2 = d2.Cross(d3);
				CVector3 c3 = d3.Cross(d4);
				CVector3 c4 = d4.Cross(d5);
				CVector3 c5 = d5.Cross(d6);
				CVector3 c6 = d6.Cross(d7);
				CVector3 c7 = d7.Cross(d0);

				CVector3 vdx = (c0+c1+c2+c3+c4+c5+c6+c7).Normal();
				return vdx;
			}

		};

		sheightfield heightfield( iNumGroundLines );

		for( int iX=0; iX<iNumGroundLines; iX++ )
		{
			F32 fx = fBas0 + ((F32) iX * fSca0);

			F32 fu = (F32) iX / (F32) iNumGroundLines;

			for( int iZ=0; iZ<iNumGroundLines; iZ++ )
			{
				F32 fz = fBas0 + ((F32) iZ * fSca0);

				F32 fv = (F32) iZ / (F32) iNumGroundLines;

				f32 fy = 0.0f;

				for( int iOctave=0; iOctave<inumoctaves; iOctave++ )
				{
					f32 fascal = fAmp * powf( fAmpScale, (f32) (iOctave) );
					f32 ffscal = fFrq * powf( fFrqScale, (f32) (iOctave) );

					fy += CPerlin2D::PlaneNoiseFunc( fx, fz, 0.0f, 0.0f, fascal, ffscal );
				}

				if( CReal(fy)<fmin ) fmin=CReal(fy);
				if( CReal(fy)>fmax ) fmax=CReal(fy);

				heightfield.XYZ(iX,iZ).SetX( fx );
				heightfield.XYZ(iX,iZ).SetY( fy );
				heightfield.XYZ(iX,iZ).SetZ( fz );

				heightfield.UV(iX,iZ).SetX( fu );
				heightfield.UV(iX,iZ).SetY( fv );
			}
		}

		CReal frange = fmax-fmin;

		/////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////
		// Detail Heightfield

		CVector4 ColorA( 0.2f, 0.3f, 0.1f, 1.0f );
		CVector4 ColorB( 0.4f, 0.5f, 0.3f, 1.0f );
		CVector4 ColorC( 1.0f, 1.0f, 1.0f, 1.0f );

		lev2::VtxWriter<SVtxV12N12B12T8C4> vwp;
		vwp.Lock( pTarg, & GetRef().mVtxBuf_PerlinTerrain, (iNumGroundLines-1)*(iNumGroundLines-1)*6 );

		for( int iX1=0; iX1<iNumGroundLines-1; iX1++ )
		{
			int iX2 = (iX1+1)%iNumGroundLines;

			for( int iZ1=0; iZ1<iNumGroundLines-1; iZ1++ )
			{
				int iZ2 = (iZ1+1)%iNumGroundLines;

				const CVector3& pX1Z1 = heightfield.XYZ(iX1,iZ1);
				const CVector3& pX2Z1 = heightfield.XYZ(iX2,iZ1);
				const CVector3& pX1Z2 = heightfield.XYZ(iX1,iZ2);
				const CVector3& pX2Z2 = heightfield.XYZ(iX2,iZ2);

				const CVector2& tX1Z1 = heightfield.UV(iX1,iZ1);
				const CVector2& tX2Z1 = heightfield.UV(iX2,iZ1);
				const CVector2& tX1Z2 = heightfield.UV(iX1,iZ2);
				const CVector2& tX2Z2 = heightfield.UV(iX2,iZ2);

				/////////////////////////////////////////////////////

				float fX1 = pX1Z1.GetX();
				float fX2 = pX2Z1.GetX();
				float fZ1 = pX1Z1.GetZ();
				float fZ2 = pX1Z2.GetZ();

				float fTU1 = tX1Z1.GetX();
				float fTU2 = tX2Z1.GetX();
				float fTV1 = tX1Z1.GetY();
				float fTV2 = tX1Z2.GetY();

				/////////////////////////////////////////////////////
				// get heights

				F32 fYH_X1Z1 = pX1Z1.GetY();
				F32 fYH_X1Z2 = pX1Z2.GetY();
				F32 fYH_X2Z1 = pX2Z1.GetY();
				F32 fYH_X2Z2 = pX2Z2.GetY();

				/////////////////////////////////////////////////////
				// calc height lerps

				CReal YLerpX1Z1 = CFloat::Pow( (CReal(fYH_X1Z1)-fmin)/frange, 2.0f );
				CReal YLerpX2Z1 = CFloat::Pow( (CReal(fYH_X2Z1)-fmin)/frange, 2.0f );
				CReal YLerpX2Z2 = CFloat::Pow( (CReal(fYH_X2Z2)-fmin)/frange, 2.0f );
				CReal YLerpX1Z2 = CFloat::Pow( (CReal(fYH_X1Z2)-fmin)/frange, 2.0f );

				/////////////////////////////////////////////////////
				// calc normals

				CVector3 nX1Z1 = heightfield.Normal(iX1,iZ1);
				CVector3 nX2Z1 = heightfield.Normal(iX2,iZ1);
				CVector3 nX1Z2 = heightfield.Normal(iX1,iZ2);
				CVector3 nX2Z2 = heightfield.Normal(iX2,iZ2);

				CVector3 Up( 0.0f, 1.0f, 0.0f );
				CReal DotX1Z1 = CFloat::Pow( Up.Dot( nX1Z1 ), 6.0f );
				CReal DotX2Z1 = CFloat::Pow( Up.Dot( nX2Z1 ), 6.0f );
				CReal DotX1Z2 = CFloat::Pow( Up.Dot( nX1Z2 ), 6.0f );
				CReal DotX2Z2 = CFloat::Pow( Up.Dot( nX2Z2 ), 6.0f );

				/////////////////////////////////////////////////////

				CVector4 caX1Z1, caX2Z1, caX2Z2, caX1Z2;
				CVector4 cX1Z1, cX2Z1, cX2Z2, cX1Z2;
				caX1Z1.Lerp( ColorB, ColorA, DotX1Z1 );
				caX2Z1.Lerp( ColorB, ColorA, DotX2Z1 );
				caX2Z2.Lerp( ColorB, ColorA, DotX2Z2 );
				caX1Z2.Lerp( ColorB, ColorA, DotX1Z2 );

				cX1Z1.Lerp( caX1Z1, ColorC, YLerpX1Z1 );
				cX2Z1.Lerp( caX2Z1, ColorC, YLerpX2Z1 );
				cX2Z2.Lerp( caX2Z2, ColorC, YLerpX2Z2 );
				cX1Z2.Lerp( caX1Z2, ColorC, YLerpX1Z2 );

				/////////////////////////////////////////////////////

				vwp.AddVertex(
					SVtxV12N12B12T8C4(	CVector3(fX1, fYH_X1Z1, fZ1),
										nX1Z1, nX1Z1,
										CVector2(fTU1, fTV1),
										pTarg->CColor4ToU32( cX1Z1) ) );
				vwp.AddVertex(
					SVtxV12N12B12T8C4(  CVector3(fX1, fYH_X1Z2, fZ2),
										nX1Z2, nX1Z2,
										CVector2(fTU1, fTV2),
										pTarg->CColor4ToU32( cX1Z2) ) );
				vwp.AddVertex(
					SVtxV12N12B12T8C4(  CVector3(fX2, fYH_X2Z2, fZ2),
										nX2Z2, nX2Z2,
										CVector2(fTU2, fTV2),
										pTarg->CColor4ToU32( cX2Z2) ) );

				vwp.AddVertex(
					SVtxV12N12B12T8C4(  CVector3(fX1, fYH_X1Z1, fZ1),
										nX1Z1, nX1Z1,
										CVector2(fTU1, fTV1),
										pTarg->CColor4ToU32( cX1Z1) ) );
				vwp.AddVertex(
					SVtxV12N12B12T8C4(  CVector3(fX2, fYH_X2Z2, fZ2),
										nX2Z2, nX2Z2,
										CVector2(fTU2, fTV2),
										pTarg->CColor4ToU32( cX2Z2) ) );
				vwp.AddVertex(
					SVtxV12N12B12T8C4(  CVector3(fX2, fYH_X2Z1, fZ1),
										nX2Z1, nX2Z1,
										CVector2(fTU2, fTV1),
										pTarg->CColor4ToU32( cX2Z1) ) );
			}

		}
		vwp.UnLock( pTarg,EULFLG_ASSIGNVBLEN );

		//GetRef().mVtxBuf_GroundPlane.AddVertex( SVtxV12C4T16( 0.0f, -fLineSize, 0.0f, 0, 0, 0xff00ff00 ) );
		//GetRef().mVtxBuf_GroundPlane.AddVertex( SVtxV12C4T16( 0.0f, fLineSize, 0.0f, 0, 0, 0xff00ff00 ) );
	}
	////////////////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////

void CGfxPrimitives::RenderAxis( GfxTarget *pTarg )
{
	pTarg->BindMaterial( & GetRef().mMaterial );
	pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_Axis );
	pTarg->BindMaterial( 0 );
}

///////////////////////////////////////////////////////////////////////////////

void CGfxPrimitives::RenderGridX100( GfxTarget *pTarg )
{
	pTarg->BindMaterial( & GetRef().mMaterial );
	pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_GridX100 );
	pTarg->BindMaterial( 0 );
}

///////////////////////////////////////////////////////////////////////////////

void CGfxPrimitives::RenderCone( GfxTarget *pTarg )
{
	pTarg->BindMaterial( & GetRef().mMaterial );
	pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_Cone );
	pTarg->BindMaterial( 0 );
}

///////////////////////////////////////////////////////////////////////////////

void CGfxPrimitives::RenderDirCone( GfxTarget *pTarg )
{
	pTarg->BindMaterial( & GetRef().mMaterial );
	pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_DirCone );
	pTarg->BindMaterial( 0 );
}
///////////////////////////////////////////////////////////////////////////////

void CGfxPrimitives::RenderCircleStrip( GfxTarget *pTarg )
{
	pTarg->BindMaterial( & GetRef().mMaterial );
	pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_CircleStrip );
	pTarg->BindMaterial( 0 );
}

///////////////////////////////////////////////////////////////////////////////

void CGfxPrimitives::RenderCircleUI( GfxTarget *pTarg )
{
	pTarg->BindMaterial( & GetRef().mMaterial );
	pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_CircleUI );
	pTarg->BindMaterial( 0 );
}

///////////////////////////////////////////////////////////////////////////////

void CGfxPrimitives::RenderCircleStripUI( GfxTarget *pTarg )
{
	pTarg->BindMaterial( & GetRef().mMaterial );
	pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_CircleStripUI );
	pTarg->BindMaterial( 0 );
}

///////////////////////////////////////////////////////////////////////////////

void CGfxPrimitives::RenderTriCircle( GfxTarget *pTarg )
{
	pTarg->BindMaterial( & GetRef().mMaterial );
	pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_TriCircle );
	pTarg->BindMaterial( 0 );
}

///////////////////////////////////////////////////////////////////////////////

void CGfxPrimitives::RenderDiamond( GfxTarget *pTarg )
{
	pTarg->BindMaterial( & GetRef().mMaterial );
	pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_Diamond );
	pTarg->BindMaterial( 0 );
}

void CGfxPrimitives::RenderCylinder( GfxTarget *pTarg, bool drawoutline )
{
	pTarg->BindMaterial( & GetRef().mMaterial );
	pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_Cylinder );
	pTarg->BindMaterial( 0 );
}

void CGfxPrimitives::RenderCapsule( GfxTarget *pTarg, float radius )
{
	pTarg->BindMaterial( & GetRef().mMaterial );
	CMatrix4 MatScale, MatTrans, MatRotate;

	MatScale.Scale( radius, radius, radius );

	//Top dome
	CVector3 trans(0.0f, 1.0f, 0.0f);
	trans = trans.Transform( pTarg->MTXI()->RefMMatrix() );
	MatTrans.SetTranslation( trans );

	pTarg->MTXI()->PushMMatrix( MatScale * MatRotate * MatTrans );
		pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_Dome );
	pTarg->MTXI()->PopMMatrix();

	//Bottom dome
	MatRotate.RotateZ( PI );
	MatTrans.SetTranslation( pTarg->MTXI()->RefMMatrix().GetTranslation() );
	pTarg->MTXI()->PushMMatrix( MatScale * MatRotate * MatTrans );
		pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_Dome );
	pTarg->MTXI()->PopMMatrix();

	pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_Capsule );
	pTarg->BindMaterial( 0 );
}

void CGfxPrimitives::RenderBox( GfxTarget *pTarg, bool drawoutline )
{
	pTarg->BindMaterial( & GetRef().mMaterial );
	pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_Box );
	pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_WireFrameBox );
	pTarg->BindMaterial( 0 );
}

void CGfxPrimitives::RenderAxisLineCone(GfxTarget* pTarg)
{
	pTarg->BindMaterial( & GetRef().mMaterial );
	pTarg->GBI()->DrawPrimitive(GetRef().mVtxBuf_AxisCone);
	pTarg->GBI()->DrawPrimitive(GetRef().mVtxBuf_AxisLine);
	pTarg->BindMaterial( 0 );
}

void CGfxPrimitives::RenderAxisBox(GfxTarget* pTarg)
{
	pTarg->BindMaterial( & GetRef().mMaterial );
	pTarg->GBI()->DrawPrimitive(GetRef().mVtxBuf_AxisBox);
	pTarg->BindMaterial( 0 );
}

/*
void CGfxPrimitives::RenderHalfSphere( GfxTarget *pTarg )
{
	pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_Dome );
}
*/

///////////////////////////////////////////////////////////////////////////////

void CGfxPrimitives::RenderEQSphere( GfxTarget *pTarg )
{
	pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_EQSphere );
}

///////////////////////////////////////////////////////////////////////////////

void CGfxPrimitives::RenderSkySphere( GfxTarget *pTarg )
{
	pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_SkySphere );
}

///////////////////////////////////////////////////////////////////////////////

void CGfxPrimitives::RenderGroundPlane( GfxTarget *pTarg )
{
	pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_GroundPlane );
}

///////////////////////////////////////////////////////////////////////////////

void CGfxPrimitives::RenderPerlinTerrain( GfxTarget *pTarg )
{
	pTarg->GBI()->DrawPrimitive( GetRef().mVtxBuf_PerlinTerrain );
}

///////////////////////////////////////////////////////////////////////////////

void CGfxPrimitives::RenderOrthoQuad( GfxTarget *pTarg, f32 fX1, f32 fX2, f32 fY1, f32 fY2, f32 iminU, f32 imaxU, f32 iminV, f32 imaxV )
{
	static DynamicVertexBuffer<SVtxV12C4T16> gpQuadBuffer( 6, 0, EPRIM_TRIANGLES );

	///////////////////////////////////////////
	// SET VERTICES (range 0..1)

	gpQuadBuffer.Reset();
	lev2::VtxWriter<SVtxV12C4T16> vw;
	vw.Lock( pTarg, & gpQuadBuffer, 0 );

	vw.AddVertex( SVtxV12C4T16( fX1, fY1, 0.0f, iminU, iminV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( fX2, fY1, 0.0f, imaxU, iminV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( fX2, fY2, 0.0f, imaxU, imaxV, 0xffffffff ) );

	vw.AddVertex( SVtxV12C4T16( fX1, fY1, 0.0f, iminU, iminV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( fX2, fY2, 0.0f, imaxU, imaxV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( fX1, fY2, 0.0f, iminU, imaxV, 0xffffffff ) );

	vw.UnLock(pTarg);
	
	///////////////////////////////////////////
	// Render Using Ortho Matrix

	CMatrix4 MatTrans, MatScale;
	MatTrans.Translate( -1.0f, 1.0f, 0.0f );
	MatScale.Scale( 2.0f, -2.0f, 0.0f );
	//MatTrans.SetTranslation( -1.0f, 1.0f, 0.0f );
	//MatScale.Scale( 2.0f, -2.0f, 0.0f );

	CMatrix4 OrthoMat = pTarg->MTXI()->GetUIOrthoProjectionMatrix();
	pTarg->MTXI()->PushPMatrix( OrthoMat );
	pTarg->MTXI()->PushVMatrix( MatTrans*MatScale );
	pTarg->MTXI()->PushMMatrix( CMatrix4::Identity );
	{
		pTarg->GBI()->DrawPrimitive( vw, EPRIM_TRIANGLES );
	}
	pTarg->MTXI()->PopPMatrix(); // back to ortho
	pTarg->MTXI()->PopVMatrix(); // back to ortho
	pTarg->MTXI()->PopMMatrix(); // back to ortho

	///////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////

void CGfxPrimitives::RenderQuadAtX( GfxTarget *pTarg, f32 fY1, f32 fY2, f32 fZ1, f32 fZ2, f32 fX, f32 iminU, f32 imaxU, f32 iminV, f32 imaxV )
{
	static DynamicVertexBuffer<SVtxV12C4T16> gpQuadBuffer( 6, 0, EPRIM_TRIANGLES );

	///////////////////////////////////////////
	// SET VERTICES (range 0..1)

	gpQuadBuffer.Reset();
	lev2::VtxWriter<SVtxV12C4T16> vw;
	vw.Lock( pTarg, & gpQuadBuffer, 0 );

	vw.AddVertex( SVtxV12C4T16( fX, fY1, fZ1, iminU, iminV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( fX, fY2, fZ1, imaxU, iminV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( fX, fY2, fZ2, imaxU, imaxV, 0xffffffff ) );

	vw.AddVertex( SVtxV12C4T16( fX, fY1, fZ1, iminU, iminV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( fX, fY2, fZ2, imaxU, imaxV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( fX, fY1, fZ2, iminU, imaxV, 0xffffffff ) );

	vw.UnLock(pTarg);
	
	///////////////////////////////////////////

	pTarg->GBI()->DrawPrimitive( vw, EPRIM_TRIANGLES );

	///////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////

void CGfxPrimitives::RenderQuadAtY( GfxTarget *pTarg, f32 fX1, f32 fX2, f32 fZ1, f32 fZ2, f32 fY, f32 iminU, f32 imaxU, f32 iminV, f32 imaxV )
{
	static DynamicVertexBuffer<SVtxV12C4T16> gpQuadBuffer( 6, 0, EPRIM_TRIANGLES );

	///////////////////////////////////////////
	// SET VERTICES (range 0..1)

	gpQuadBuffer.Reset();
	lev2::VtxWriter<SVtxV12C4T16> vw;
	vw.Lock( pTarg, & gpQuadBuffer, 0 );

	vw.AddVertex( SVtxV12C4T16( fX2, fY, fZ2, imaxU, imaxV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( fX2, fY, fZ1, imaxU, iminV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( fX1, fY, fZ1, iminU, iminV, 0xffffffff ) );

	vw.AddVertex( SVtxV12C4T16( fX1, fY, fZ2, iminU, imaxV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( fX2, fY, fZ2, imaxU, imaxV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( fX1, fY, fZ1, iminU, iminV, 0xffffffff ) );

	vw.UnLock( pTarg );

	///////////////////////////////////////////

	pTarg->GBI()->DrawPrimitive( vw, EPRIM_TRIANGLES );

	///////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////

void CGfxPrimitives::RenderQuadAtZ( GfxTarget *pTarg, f32 fX1, f32 fX2, f32 fY1, f32 fY2, f32 fZ, f32 iminU, f32 imaxU, f32 iminV, f32 imaxV )
{
	static DynamicVertexBuffer<SVtxV12C4T16> gpQuadBuffer( 6, 0, EPRIM_TRIANGLES );

	///////////////////////////////////////////
	// SET VERTICES (range 0..1)

	gpQuadBuffer.Reset();
	lev2::VtxWriter<SVtxV12C4T16> vw;
	vw.Lock( pTarg, & gpQuadBuffer, 0 );

	vw.AddVertex( SVtxV12C4T16( fX1, fY1, fZ, iminU, iminV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( fX2, fY1, fZ, imaxU, iminV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( fX2, fY2, fZ, imaxU, imaxV, 0xffffffff ) );

	vw.AddVertex( SVtxV12C4T16( fX1, fY1, fZ, iminU, iminV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( fX2, fY2, fZ, imaxU, imaxV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( fX1, fY2, fZ, iminU, imaxV, 0xffffffff ) );

	vw.UnLock( pTarg );

	///////////////////////////////////////////

	pTarg->GBI()->DrawPrimitive( vw, EPRIM_TRIANGLES );

	///////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////

void CGfxPrimitives::RenderQuad( GfxTarget *pTarg, CVector4 &V0, CVector4 &V1, CVector4 &V2, CVector4 &V3 )
{
	static DynamicVertexBuffer<SVtxV12C4T16> gpQuadBuffer( 6, 0, EPRIM_TRIANGLES );

	///////////////////////////////////////////
	// SET VERTICES (range 0..1)

	gpQuadBuffer.Reset();
	lev2::VtxWriter<SVtxV12C4T16> vw;
	vw.Lock( pTarg, & gpQuadBuffer, 0 );

	f32 iminU = 0.0f;
	f32 iminV = 0.0f;
	f32 imaxU = 1.0f;
	f32 imaxV = 1.0f;

	vw.AddVertex( SVtxV12C4T16( V0.GetX(), V0.GetY(), V0.GetZ(), iminU, iminV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( V1.GetX(), V1.GetY(), V1.GetZ(), imaxU, iminV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( V2.GetX(), V2.GetY(), V2.GetZ(), iminU, imaxV, 0xffffffff ) );

	vw.AddVertex( SVtxV12C4T16( V1.GetX(), V1.GetY(), V1.GetZ(), imaxU, iminV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( V2.GetX(), V2.GetY(), V2.GetZ(), iminU, imaxV, 0xffffffff ) );
	vw.AddVertex( SVtxV12C4T16( V3.GetX(), V3.GetY(), V3.GetZ(), imaxU, imaxV, 0xffffffff ) );

	vw.UnLock(pTarg);
	
	///////////////////////////////////////////

	pTarg->GBI()->DrawPrimitive( vw, EPRIM_TRIANGLES );

	///////////////////////////////////////////

}

} }
