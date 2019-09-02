////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/lev2renderer.h>
#include <ork/math/misc_math.h>
#include <ork/math/audiomath.h>
#include <math.h>
#include <ork/lev2/gfx/util/grid.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>

using namespace ork::audiomath;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

Grid3d::Grid3d()
	: mVisGridBase( 0.3f )
	, mVisGridDiv( 10.0f )
	, mVisGridHiliteDiv( 100.0f )
{

}
///////////////////////////////////////////////////////////////////////////////
void Grid3d::Calc( const CCameraData& camdat )
{
	const CVector4& vn0 = camdat.GetFrustum().mNearCorners[0];
	const CVector4& vn1 = camdat.GetFrustum().mNearCorners[1];
	const CVector4& vn2 = camdat.GetFrustum().mNearCorners[2];
	const CVector4& vn3 = camdat.GetFrustum().mNearCorners[3];
	const CVector4& vf0 = camdat.GetFrustum().mFarCorners[0];
	const CVector4& vf1 = camdat.GetFrustum().mFarCorners[1];
	const CVector4& vf2 = camdat.GetFrustum().mFarCorners[2];
	const CVector4& vf3 = camdat.GetFrustum().mFarCorners[3];
	CVector3 vr0 = (vf0-vn0).Normal();
	CVector3 vr1 = (vf1-vn1).Normal();
	CVector3 vr2 = (vf2-vn2).Normal();
	CVector3 vr3 = (vf3-vn3).Normal();
	
	/////////////////////////////////////////////////////////////////
	// get center ray

	CVector3 vnc = (vn0+vn1+vn2+vn3)*float(0.25f);
	CVector3 vfc = (vf0+vf1+vf2+vf3)*float(0.25f);
	CVector3 vrc = (vfc-vnc).Normal();

	Ray3 centerray;
	centerray.mOrigin = vnc;
	centerray.mDirection = vrc;

	/////////////////////////////////////////////////////////////////

	float itx0 = 0.0f, itx1= 0.0f, ity0= 0.0f, ity1= 0.0f;

	/////////////////////////////////////////////////////////////////
	// calc intersection of frustum and XYPlane

	float DistToPlane(0.0f);

	if( meGridMode == EGRID_XY )
	{
		CPlane XYPlane( CVector3(float(0.0f),float(0.0f),float(1.0f)), CVector3::Zero() );
		CVector3 vc;
		bool bc = XYPlane.Intersect( centerray, DistToPlane );
		float isect_dist;
	    bc = XYPlane.Intersect( centerray, isect_dist, vc );
		float Restrict = DistToPlane*float(1.0f);
		// extend Grid to cover full viewport
		itx0 = vc.GetX() - Restrict;
		itx1 = vc.GetX() + Restrict;
		ity0 = vc.GetY() - Restrict;
		ity1 = vc.GetY() + Restrict;
	}
	else if( meGridMode == EGRID_XZ )
	{
		CPlane XZPlane( CVector3(float(0.0f),float(1.0f),float(0.0f)), CVector3::Zero() );
		CVector3 vc;
		bool bc = XZPlane.Intersect( centerray, DistToPlane );
		float isect_dist;
	    bc = XZPlane.Intersect( centerray, isect_dist, vc );
		float Restrict = DistToPlane*float(1.0f);
		// extend Grid to cover full viewport
		itx0 = vc.GetX() - Restrict;
		itx1 = vc.GetX() + Restrict;
		ity0 = vc.GetZ() - Restrict;
		ity1 = vc.GetZ() + Restrict;
	}

	/////////////////////////////////////////////////////////////////
	// get params for grid

	float fLEFT = float(itx0);
	float fRIGHT = float(itx1);
	float fTOP = float(ity0);
	float fBOTTOM = float(ity1);

	float fWIDTH = fRIGHT-fLEFT;
	float fHEIGHT = fBOTTOM-fTOP;
	float fASPECT = fHEIGHT/fWIDTH;

	float fLOG = float(log_base( mVisGridDiv, DistToPlane*mVisGridBase ));
	float fiLOG = float(pow_base( mVisGridDiv, float(CFloat::Floor(fLOG)) ));
	mVisGridSize = fiLOG/mVisGridDiv;

	if( mVisGridSize<10.0f ) mVisGridSize=10.0f;


	//if( mVisGridSize<float(0.5f) ) mVisGridSize = float(0.5f);
	//if( mVisGridSize>float(8.0f) ) mVisGridSize = float(8.0f);

	mGridDL = mVisGridSize*float(CFloat::Floor(fLEFT/mVisGridSize));
	mGridDR = mVisGridSize*float(CFloat::Ceil(fRIGHT/mVisGridSize));
	mGridDT = mVisGridSize*float(CFloat::Floor(fTOP/mVisGridSize));
	mGridDB = mVisGridSize*float(CFloat::Ceil(fBOTTOM/mVisGridSize));

}

///////////////////////////////////////////////////////////////////////////////

void Grid3d::Render( RenderContextFrameData& FrameData ) const
{
	GfxTarget* pTARG = FrameData.GetTarget();

	//pTARG->MTXI()->PushPMatrix( FrameData.GetCameraData()->GetPMatrix() );
	//pTARG->MTXI()->PushVMatrix( FrameData.GetCameraData()->GetVMatrix() );
	pTARG->MTXI()->PushMMatrix( CMatrix4::Identity );
	{
		static GfxMaterial3DSolid gridmat( pTARG );
		gridmat.SetColorMode( GfxMaterial3DSolid::EMODE_MOD_COLOR );
		gridmat.mRasterState.SetBlending( EBLENDING_ADDITIVE );

		pTARG->BindMaterial( & gridmat );
	
		////////////////////////////////
		// Grid

		float GridGrey(0.10f);
		float GridHili(0.20f);
		CVector4 BaseGridColor( GridGrey,GridGrey,GridGrey );
		CVector4 HiliGridColor( GridHili,GridHili,GridHili );

		pTARG->PushModColor( BaseGridColor );
		
		if( meGridMode == EGRID_XY
			)
		{
			for( float fX=mGridDL; fX<=mGridDR; fX+=mVisGridSize )
			{
				bool bORIGIN = (fX==0.0f);
				bool bhi = std::fmod( CFloat::Abs(fX), mVisGridHiliteDiv ) <CFloat::Epsilon();

				pTARG->PushModColor( bORIGIN ? CColor4::Green() : (bhi ? HiliGridColor : BaseGridColor) );
				//pTARG->IMI()->DrawLine( CVector4( fX, mGridDT, 0.0f, 1.0f ), CVector4( fX, mGridDB, 0.0f, 1.0f ) );
				//pTARG->IMI()->QueFlush( false );
				pTARG->PopModColor();
			}
			for( float fY=mGridDT; fY<=mGridDB; fY+=mVisGridSize )
			{
				bool bORIGIN = (fY==0.0f);
				bool bhi = std::fmod( CFloat::Abs(fY),mVisGridHiliteDiv ) < CFloat::Epsilon();

				pTARG->PushModColor( bORIGIN ? CColor4::Red() : (bhi ? HiliGridColor : BaseGridColor) );
				//pTARG->IMI()->DrawLine( CVector4( mGridDL, fY, 0.0f, 1.0f ), CVector4( mGridDR, fY, 0.0f, 1.0f ) );
				//pTARG->IMI()->QueFlush( false );
				pTARG->PopModColor();
			}
		}
		else if( meGridMode == EGRID_XZ )
		{
			for( float fX=mGridDL; fX<=mGridDR; fX+=mVisGridSize )
			{
				bool bORIGIN = (fX==0.0f);
				bool bhi = std::fmod( CFloat::Abs(fX), mVisGridHiliteDiv ) < CFloat::Epsilon();

				pTARG->PushModColor( bORIGIN ? CColor4::Blue() : (bhi ? HiliGridColor : BaseGridColor) );
				//pTARG->IMI()->DrawLine( CVector4( fX, 0.0f, mGridDT, 1.0f ), CVector4( fX, 0.0f, mGridDB, 1.0f ) );
				//pTARG->IMI()->QueFlush( false );
				pTARG->PopModColor();
			}
			for( float fY=mGridDT; fY<=mGridDB; fY+=mVisGridSize )
			{
				bool bORIGIN = (fY==0.0f);
				bool bhi = std::fmod( CFloat::Abs(fY), mVisGridHiliteDiv ) < CFloat::Epsilon();

				pTARG->PushModColor( bORIGIN ? CColor4::Red() : (bhi ? HiliGridColor : BaseGridColor) );
				//pTARG->IMI()->DrawLine( CVector4( mGridDL, 0.0f, fY, 1.0f ), CVector4( mGridDR, 0.0f, fY, 1.0f ) );
				//pTARG->IMI()->QueFlush( false );
				pTARG->PopModColor();
			}
		}
		//CFontMan::FlushQue(pTARG);
		pTARG->PopModColor();

	}
	//pTARG->MTXI()->PopPMatrix();
	//pTARG->MTXI()->PopVMatrix();
	pTARG->MTXI()->PopMMatrix();

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

Grid2d::Grid2d()
	: mVisGridBase( 0.3f )
	, mVisGridDiv( 10.0f )
	, mVisGridHiliteDiv( 100.0f )
    , mVisGridSize(10)
	, mCenter(0.0f,0.0f)
	, mExtent(100.0f)
	, mZoom(1.0f)
{

}

///////////////////////////////////////////////////////////////////////////////
/*
static const int kdiv_base = 240 * 4;
static const int kfreqmult = 5;
static const float klogbias = 0.3f; // adjust to change threshold of lod switch

struct grid_calculator
{
    int ispace;
    int il;
    int ir;
    int it;
    int ib;
    int ic;
    float flog;
    float filog;
    float flogb;

    grid_calculator( const Rectangle& devviewrect, float scale_factor )
    {
        static const float div_base = kdiv_base; // base distance between gridlines at unity scale

        auto log_base = []( float base, float inp ) ->float
        {
            float rval = std::log( inp ) / std::log( base );
            return rval;
        };
        auto pow_base = []( float base, float inp ) ->float
        {
            float rval = std::pow( base, inp );
            return rval;
        };

        const float kfbase = kfreqmult;
        flogb = log_base( kfbase, div_base );
        flog = log_base( kfbase, scale_factor );
        flog = klogbias+std::round(flog*10.0f)/10.0f;
        flog = (flog<1.0f) ? 1.0f : flog;
        filog = pow_base( kfbase, flogb+std::floor(flog) );
        filog = std::round(filog/kfbase)*kfbase;

        ispace = int(filog);
        il = (int(devviewrect.x-filog)/ispace)*ispace;
        ir = (int(devviewrect.x2+filog)/ispace)*ispace;
        it = (int(devviewrect.y-filog)/ispace)*ispace;
        ib = (int(devviewrect.y2+filog)/ispace)*ispace;
        ic = (ir-il)/ispace;
        printf( "scale_factor<%f> flog<%f> filog<%f> il<%d> ir<%d> ispace<%d> ic<%d>\n", scale_factor, flog, filog, il, ir, ispace, ic );
    }


};
*/
////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
void Grid2d::ReCalc( int iw, int ih )
{
	//float scale_factor = (dev_view_rect.y2 - dev_view_rect.y) / gmastercfg.ideviceH;
	//grid_calculator the_grid_calc(dev_view_rect,scale_factor);
	float ftW = float(iw);
	float ftH = float(ih);
	float fASPECT = ftH/ftW;

	float fnext = mExtent/mZoom;

	float itx0 = mCenter.GetX() - fnext*0.5f;
	float itx1 = mCenter.GetX() + fnext*0.5f;
	float ity0 = mCenter.GetY() - fnext*0.5f*fASPECT;
	float ity1 = mCenter.GetY() + fnext*0.5f*fASPECT;

	/////////////////////////////////////////////////////////////////
	// get params for grid

	float fLEFT = itx0;
	float fRIGHT = itx1;
	float fTOP = ity0;
	float fBOTTOM = ity1;

	float fWIDTH = fRIGHT-fLEFT;
	float fHEIGHT = fBOTTOM-fTOP;

	float fLOG = log_base( mVisGridDiv, fnext*mVisGridBase );
	float fiLOG = pow_base( mVisGridDiv, CFloat::Floor(fLOG) );
	mVisGridSize = fiLOG/mVisGridDiv;

	if( mVisGridSize<10.0f ) mVisGridSize=10.0f;
    if( mVisGridSize>100.0f ) mVisGridSize=100.0f;

	//if( mVisGridSize<float(0.5f) ) mVisGridSize = float(0.5f);
	//if( mVisGridSize>float(8.0f) ) mVisGridSize = float(8.0f);

	mTopLeft.SetX( mVisGridSize*float(CFloat::Floor(fLEFT/mVisGridSize)) );
	mTopLeft.SetY( mVisGridSize*float(CFloat::Floor(fTOP/mVisGridSize)) );

	mBotRight.SetX( mVisGridSize*float(CFloat::Ceil(fRIGHT/mVisGridSize)) );
	mBotRight.SetY( mVisGridSize*float(CFloat::Ceil(fBOTTOM/mVisGridSize)) );

}

void Grid2d::SetExtent( float fv )
{
	mExtent = fv;
}
void Grid2d::SetZoom( float fv )
{
	mZoom = fv;
}
void Grid2d::SetCenter( const CVector2& ctr)
{
	mCenter = ctr;
}
///////////////////////////////////////////////////////////////////////////////

CVector2 Grid2d::Snap( CVector2 inp ) const
{
	float ffx = inp.GetX();
	float ffy = inp.GetY();
	float ffxm = std::fmod(ffx,mVisGridSize);
	float ffym = std::fmod(ffy,mVisGridSize);
	
	bool blx = (ffxm<(mVisGridSize*0.5f));
	bool bly = (ffym<(mVisGridSize*0.5f));

	float fnx = blx ? (ffx-ffxm) : (ffx-ffxm)+mVisGridSize;
	float fny = bly ? (ffy-ffym) : (ffy-ffym)+mVisGridSize;
	
	return CVector2(fnx,fny);
}

///////////////////////////////////////////////////////////////////////////////

void Grid2d::Render( GfxTarget* pTARG, int iw, int ih )
{
	auto mtxi = pTARG->MTXI();

	lev2::DynamicVertexBuffer<lev2::SVtxV12C4T16>& VB = lev2::GfxEnv::GetSharedDynamicVB();

	ReCalc( iw,ih );

	mMtxOrtho = mtxi->Ortho( mTopLeft.GetX(), mBotRight.GetX(), mTopLeft.GetY(), mBotRight.GetY(), 0.0f, 1.0f );

	mtxi->PushPMatrix( mMtxOrtho );
	mtxi->PushVMatrix( CMatrix4::Identity );
	mtxi->PushMMatrix( CMatrix4::Identity );
	{
		static GfxMaterial3DSolid gridmat( pTARG );
		gridmat.SetColorMode( GfxMaterial3DSolid::EMODE_VERTEX_COLOR );
		gridmat.mRasterState.SetBlending( EBLENDING_ADDITIVE );

		pTARG->PushMaterial( & gridmat );
	
		////////////////////////////////
		// Grid

		float GridGrey(0.10f);
		float GridHili(0.20f);
		CVector4 BaseGridColor( GridGrey,GridGrey,GridGrey );
		CVector4 HiliGridColor( GridHili,GridHili,GridHili );

		float x1 = mTopLeft.GetX();
		float x2 = mBotRight.GetX();
		float y1 = mTopLeft.GetY();
		float y2 = mBotRight.GetY();

		int inumx = 0;
		int inumy = 0;
		for( float fx=x1; fx<=x2; fx+=mVisGridSize ) inumx++;
		for( float fy=y1; fy<=y2; fy+=mVisGridSize ) inumy++;

        int count = (inumx+inumy)*2;

        if( count )
        {
    		lev2::VtxWriter<lev2::SVtxV12C4T16> vw;
    		vw.Lock( pTARG, &VB, (inumx+inumy)*2 );

    		CVector2 uv0(0.0f,0.0f);
    		for( float fx=x1; fx<=x2; fx+=mVisGridSize )
    		{
    			bool bORIGIN = (fx==0.0f);
    			bool bhi = std::fmod( CFloat::Abs(fx), mVisGridHiliteDiv ) <CFloat::Epsilon();

    			auto color = bORIGIN ? CColor4::Green() : (bhi ? HiliGridColor : BaseGridColor);
    			u32 ucolor = color.GetVtxColorAsU32();
    			ork::lev2::SVtxV12C4T16 v0( CVector3(fx,y1,0.0f), uv0, ucolor );
    			ork::lev2::SVtxV12C4T16 v1( CVector3(fx,y2,0.0f), uv0, ucolor );
    			vw.AddVertex(v0);
    			vw.AddVertex(v1);
    		}
    		for( float fy=y1; fy<=y2; fy+=mVisGridSize )
    		{
    			bool bORIGIN = (fy==0.0f);
    			bool bhi = std::fmod( CFloat::Abs(fy),mVisGridHiliteDiv ) < CFloat::Epsilon();

    			auto color = bORIGIN ? CColor4::Red() : (bhi ? HiliGridColor : BaseGridColor);
    			u32 ucolor = color.GetVtxColorAsU32();
    			ork::lev2::SVtxV12C4T16 v0( CVector3(x1,fy,0.0f), uv0, ucolor );
    			ork::lev2::SVtxV12C4T16 v1( CVector3(x2,fy,0.0f), uv0, ucolor );
    			vw.AddVertex(v0);
    			vw.AddVertex(v1);

    		}
    		vw.UnLock(pTARG);
    		pTARG->GBI()->DrawPrimitive( vw, ork::lev2::EPRIM_LINES );
        }
		pTARG->PopMaterial();
	}
	mtxi->PopPMatrix();
	mtxi->PopVMatrix();
	mtxi->PopMMatrix();

}

}}
