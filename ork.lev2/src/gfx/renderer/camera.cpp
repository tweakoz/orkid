////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/gfx/camera.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer.h>

#if defined(WII)
#include <revolution/sc.h>
bool Is16x9()
{
	return ORKetAspectRatio()==SC_ASPECT_RATIO_16x9;
}
#else
bool Is16x9()
{
	//return false;
	// this wont work until we have a physical screen dimension detection
	const ork::lev2::GfxTargetCreationParams& CreationParams = ork::lev2::GfxEnv::GetRef().GetCreationParams();
	return CreationParams.mbWideScreen;
}
#endif

namespace ork {

////////////////////////////////////////////////////////////////////////////////

CCameraData::CCameraData()
	: mAper(17.0f)
	, mHorizAper(0.0f)
	, mNear(100.0f)
	, mFar(750.0f)
	, mfWidth(1.0f)
	, mfHeight(1.0f)
	, mfAspect(1.0f)
	, mpGfxTarget( 0 )
	, mEye( 0.0f, 0.0f, 0.0f )
	, mTarget( 0.0f, 0.0f, 1.0f )
	, mUp( 0.0f, 1.0f, 0.0f )
	, mMatViewSet(false)
	, mpVisibilityCamDat(0)
	, mfOverrideWidth(0.0f)
	, mfOverrideHeight(0.0f)
	, mpLev2Camera(0)
{

}

void CCameraData::SetLev2Camera(lev2::CCamera*pcam)
{
	//printf( "CCameraData::SetLev2Camera() this<%p> pcam<%p>\n", this, pcam );
	mpLev2Camera=pcam;
}

////////////////////////////////////////////////////////////////////////////////

void CCameraData::Persp(float fnear, float ffar, float faper)
{
	mAper = faper;
	mHorizAper = 0;
	mNear = fnear;
	mFar = ffar;
}

////////////////////////////////////////////////////////////////////////////////

void CCameraData::PerspH(float fnear, float ffar, float faperh)
{
	mHorizAper = faperh;
	mNear = fnear;
	mFar = ffar;
}

////////////////////////////////////////////////////////////////////////////////

void CCameraData::Lookat( const CVector3& eye, const CVector3& tgt, const CVector3& up )
{
	mEye = eye;
	mTarget = tgt;
	mUp = up;

	mMatViewSet = false;
}

void CCameraData::SetView( const ork::CMatrix4 &view)
{
	mMatView = view;

	mMatViewSet = true;
}

////////////////////////////////////////////////////////////////////////////////

void CCameraData::ProjectDepthRay( const CVector2& v2d, CVector3& vdir, CVector3& vori ) const
{
	const Frustum& camfrus = mFrustum;
	CVector3 near_xt_lerp; near_xt_lerp.Lerp( camfrus.mNearCorners[0], camfrus.mNearCorners[1], v2d.GetX() );
	CVector3 near_xb_lerp; near_xb_lerp.Lerp( camfrus.mNearCorners[3], camfrus.mNearCorners[2], v2d.GetX() );
	CVector3 near_lerp; near_lerp.Lerp( near_xt_lerp, near_xb_lerp, v2d.GetY() );
	CVector3 far_xt_lerp; far_xt_lerp.Lerp( camfrus.mFarCorners[0], camfrus.mFarCorners[1], v2d.GetX() );
	CVector3 far_xb_lerp; far_xb_lerp.Lerp( camfrus.mFarCorners[3], camfrus.mFarCorners[2], v2d.GetX() );
	CVector3 far_lerp; far_lerp.Lerp( far_xt_lerp, far_xb_lerp, v2d.GetY() );
	vdir=(far_lerp-near_lerp).Normal();
	vori=near_lerp;
}

void CCameraData::ProjectDepthRay( const CVector2& v2d, fray3& ray_out ) const
{
    fvec3 dir, ori;
    ProjectDepthRay(v2d,dir,ori);
    ray_out = fray3(ori,dir);
}

////////////////////////////////////////////////////////////////////////////////

void CCameraData::GetPixelLengthVectors( const CVector3& Pos, const CVector2& vp, CVector3& OutX, CVector3& OutY ) const
{
	/////////////////////////////////////////////////////////////////
	int ivpw = int(vp.GetX());
	int ivph = int(vp.GetY());
	/////////////////////////////////////////////////////////////////
	CVector4 va = Pos;
	CVector4 va_xf = va.Transform(mVPMatrix);
	va_xf.PerspectiveDivide();
	va_xf = va_xf*CVector4(vp.GetX(),vp.GetY(),0.0f);
	/////////////////////////////////////////////////////////////////
	CVector4 vdx = Pos+mCamXNormal;
	CVector4 vdx_xf = vdx.Transform(mVPMatrix);
	vdx_xf.PerspectiveDivide();
	vdx_xf = vdx_xf*CVector4(vp.GetX(),vp.GetY(),0.0f);
	float MagX = (vdx_xf-va_xf).Mag(); // magnitude in pixels of mBillboardRight
	/////////////////////////////////////////////////////////////////
	CVector4 vdy = Pos+mCamYNormal;
	CVector4 vdy_xf = vdy.Transform(mVPMatrix);
	vdy_xf.PerspectiveDivide();
	vdy_xf = vdy_xf*CVector4(vp.GetX(),vp.GetY(),0.0f);
	float MagY = (vdy_xf-va_xf).Mag(); // magnitude in pixels of mBillboardUp
	/////////////////////////////////////////////////////////////////
	OutX = mCamXNormal*(2.0f/MagX);
	OutY = mCamYNormal*(2.0f/MagY);
	/////////////////////////////////////////////////////////////////
}

////////////////////////////////////////////////////////////////////////////////

void CCameraData::CalcCameraMatrices(CameraCalcContext& ctx, float faspect) const
{
	///////////////////////////////
	// gameplay calculation
	///////////////////////////////
	float faper = mAper;
	//float faspect = mfAspect;
	float fnear = mNear;
	float ffar = mFar;
	///////////////////////////////////////////////////	
	if( faper<1.0f ) faper = 1.0f;
	if( fnear<0.1f ) fnear = 0.1f;
	if( ffar<0.5f ) ffar = 0.5f;
	///////////////////////////////////////////////////
	CVector3 target = mTarget;
	///////////////////////////////////////////////////
	float fmag2 = (target-mEye).MagSquared();
	if( fmag2 < 0.01f )
	{
		target = mEye + CVector3::Blue();
	}
	///////////////////////////////////////////////////			
	ctx.mPMatrix.Perspective( faper, faspect, fnear, ffar );
	ctx.mVMatrix.LookAt( mEye, mTarget, mUp );
	///////////////////////////////////////////////////
	CMatrix4 matgp_vp = ctx.mVMatrix*ctx.mPMatrix;
	CMatrix4 matgp_ivp;
	matgp_ivp.GEMSInverse(matgp_vp);
	//matgp_iv.GEMSInverse(matgp_view);
	ctx.mFrustum.Set( matgp_ivp );

}

////////////////////////////////////////////////////////////////////////////////

void CCameraData::CalcCameraData(CameraCalcContext& calcctx)
{
	mMatViewSet = false;
	
	//orkprintf( "ccd camEYE <%f %f %f>\n", mEye.GetX(), mEye.GetY(), mEye.GetZ() );
	//orkprintf( "ccd camTGT <%f %f %f>\n", mTarget.GetX(), mTarget.GetY(), mTarget.GetZ() );

	///////////////////////////////
	// target calculation
	///////////////////////////////

	//////////////////////////////////////////////
	// THIS 16x9 really means are you in "stretch" mode
	// like when the wii renders at 640x448 on a 16x9 screen
	//////////////////////////////////////////////

	float fhdaspect = 1.0f;
	
	if( Is16x9() )
	{
		if( mpGfxTarget != 0 )
		{
			float fw = float(mpGfxTarget->GetW());//float(render_rect.miW);
			float fh = float(mpGfxTarget->GetH());//float(render_rect.miH);
			const float rWH = (fw/fh);
			const float r169 = (16.0f/9.0f);
			fhdaspect = (r169/rWH);
		}
	}

	//////////////////////////////////////////////
	// THIS 16x9 really means are you in "stretch" mode
	// like when the wii renders at 640x448 on a 16x9 screen
	//////////////////////////////////////////////

	if(!mMatViewSet)
		mMatView = CMatrix4::Identity;
	mMatProj = CMatrix4::Identity;
	if( mpGfxTarget != 0 )
	{
		//mpGfxTarget->FBI()->ForceFlush();
		if(!mMatViewSet)
		{
			//float fmag0 = mEye.MagSquared();
			//float fmag1 = mTarget.MagSquared();
			float fmag2 = (mTarget-mEye).MagSquared();
			if( fmag2 < 0.01f )
			{
				mTarget = mEye + CVector3::Blue();
			}
			
			mMatView = mpGfxTarget->MTXI()->LookAt( mEye, mTarget, mUp );
		}
		//mpGfxTarget->FBI()->ForceFlush();
			
		if( mpGfxTarget->GetRenderContextFrameData() )
		{			
			mfAspect = fhdaspect*calcctx.mfAspectRatio;
			
			if (mHorizAper != 0)
				mAper = mHorizAper/calcctx.mfAspectRatio;

			if( mAper<1.0f ) mAper = 1.0f;
			if( mNear<0.1f ) mNear = 0.1f;
			if( mFar<0.5f ) mFar = 0.5f;

			mMatProj = mpGfxTarget->MTXI()->Persp( mAper, mfAspect, mNear, mFar );

		}
	//	mpGfxTarget->FBI()->ForceFlush();
	}
	
	calcctx.mVMatrix = mMatView;
	calcctx.mPMatrix = mMatProj;
	
	///////////////////////////////

	mVPMatrix = mMatView*mMatProj;
	mIVMatrix.GEMSInverse(mMatView);
	mIVPMatrix.GEMSInverse(mVPMatrix);

	///////////////////////////////
	// gameplay calculation
	///////////////////////////////

	CMatrix4 matgp_proj;
	CMatrix4 matgp_view;
	CMatrix4 matgp_ivp;
	CMatrix4 matgp_iv;

	if( mpGfxTarget )
	{
		//mpGfxTarget->FBI()->ForceFlush();
		matgp_proj.Perspective( mAper, mfAspect, mNear, mFar );
		
		if(mMatViewSet)
			matgp_view = mMatView;
		else
			matgp_view.LookAt( mEye, mTarget, mUp );
		
		CMatrix4 matgp_vp = matgp_view*matgp_proj;
		matgp_ivp.GEMSInverse(matgp_vp);
		matgp_iv.GEMSInverse(matgp_view);

		//mpGfxTarget->FBI()->ForceFlush();
	}

	mMatViewSet = true;

	///////////////////////////////
    // generate frustum (useful for many things, like billboarding, clipping, LOD, etc.. )
    // we generate the frustum points, we should also generate plane eqns

	mFrustum.Set( matgp_ivp );
	mCamXNormal = mFrustum.mXNormal;
	mCamYNormal = mFrustum.mYNormal;
    mCamZNormal = mFrustum.mZNormal;

	calcctx.mFrustum = mFrustum;
	///////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////

}
