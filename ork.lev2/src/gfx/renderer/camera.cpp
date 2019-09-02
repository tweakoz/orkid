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
	, mpVisibilityCamDat(0)
	, mfOverrideWidth(0.0f)
	, mfOverrideHeight(0.0f)
	, mpLev2Camera(0)
	, _explicitProjectionMatrix(false)
	, _explicitViewMatrix(false)
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

void CCameraData::Lookat( const fvec3& eye, const fvec3& tgt, const fvec3& up )
{
	mEye = eye;
	mTarget = tgt;
	mUp = up;

	_explicitViewMatrix = false;
}

void CCameraData::SetView( const ork::fmtx4 &view)
{
	mMatView = view;
	//view.dump("setview");
	_explicitViewMatrix = true;
}

void CCameraData::setCustomProjection( const ork::fmtx4& proj){
	//proj.dump("setproj");
	mMatProj = proj;
	_explicitProjectionMatrix = true;
	mfAspect = 1.0f;
	mNear = .1f;
	mFar = 1000.f;
	mAper = 60;
}

////////////////////////////////////////////////////////////////////////////////

void CCameraData::projectDepthRay( const fvec2& v2d, fvec3& vdir, fvec3& vori ) const
{
	const Frustum& camfrus = mFrustum;
	fvec3 near_xt_lerp; near_xt_lerp.Lerp( camfrus.mNearCorners[0], camfrus.mNearCorners[1], v2d.GetX() );
	fvec3 near_xb_lerp; near_xb_lerp.Lerp( camfrus.mNearCorners[3], camfrus.mNearCorners[2], v2d.GetX() );
	fvec3 near_lerp; near_lerp.Lerp( near_xt_lerp, near_xb_lerp, v2d.GetY() );
	fvec3 far_xt_lerp; far_xt_lerp.Lerp( camfrus.mFarCorners[0], camfrus.mFarCorners[1], v2d.GetX() );
	fvec3 far_xb_lerp; far_xb_lerp.Lerp( camfrus.mFarCorners[3], camfrus.mFarCorners[2], v2d.GetX() );
	fvec3 far_lerp; far_lerp.Lerp( far_xt_lerp, far_xb_lerp, v2d.GetY() );
	vdir=(far_lerp-near_lerp).Normal();
	vori=near_lerp;
}

void CCameraData::projectDepthRay( const fvec2& v2d, fray3& ray_out ) const
{
    fvec3 dir, ori;
    projectDepthRay(v2d,dir,ori);
    ray_out = fray3(ori,dir);
}

////////////////////////////////////////////////////////////////////////////////

void CCameraData::GetPixelLengthVectors( const fvec3& Pos, const fvec2& vp, fvec3& OutX, fvec3& OutY ) const
{
	/////////////////////////////////////////////////////////////////
	int ivpw = int(vp.GetX());
	int ivph = int(vp.GetY());
	/////////////////////////////////////////////////////////////////
	fvec4 va = Pos;
	fvec4 va_xf = va.Transform(mVPMatrix);
	va_xf.PerspectiveDivide();
	va_xf = va_xf*fvec4(vp.GetX(),vp.GetY(),0.0f);
	/////////////////////////////////////////////////////////////////
	fvec4 vdx = Pos+mCamXNormal;
	fvec4 vdx_xf = vdx.Transform(mVPMatrix);
	vdx_xf.PerspectiveDivide();
	vdx_xf = vdx_xf*fvec4(vp.GetX(),vp.GetY(),0.0f);
	float MagX = (vdx_xf-va_xf).Mag(); // magnitude in pixels of mBillboardRight
	/////////////////////////////////////////////////////////////////
	fvec4 vdy = Pos+mCamYNormal;
	fvec4 vdy_xf = vdy.Transform(mVPMatrix);
	vdy_xf.PerspectiveDivide();
	vdy_xf = vdy_xf*fvec4(vp.GetX(),vp.GetY(),0.0f);
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
	fvec3 target = mTarget;
	///////////////////////////////////////////////////
	float fmag2 = (target-mEye).MagSquared();
	if( fmag2 < 0.01f )
	{
		target = mEye + fvec3::Blue();
	}
	///////////////////////////////////////////////////
	ctx.mPMatrix.Perspective( faper, faspect, fnear, ffar );
	ctx.mVMatrix.LookAt( mEye, mTarget, mUp );
	///////////////////////////////////////////////////
	fmtx4 matgp_vp = ctx.mVMatrix*ctx.mPMatrix;
	fmtx4 matgp_ivp;
	matgp_ivp.GEMSInverse(matgp_vp);
	//matgp_iv.GEMSInverse(matgp_view);
	ctx.mFrustum.Set( matgp_ivp );

}


////////////////////////////////////////////////////////////////////////////////

void CCameraData::CalcCameraData(CameraCalcContext& calcctx)
{

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

	if(!_explicitViewMatrix)
		mMatView = fmtx4::Identity;
	if( mpGfxTarget != 0 )
	{
		//mpGfxTarget->FBI()->ForceFlush();
		if(!_explicitViewMatrix)
		{
			//float fmag0 = mEye.MagSquared();
			//float fmag1 = mTarget.MagSquared();
			float fmag2 = (mTarget-mEye).MagSquared();
			if( fmag2 < 0.01f )
			{
				mTarget = mEye + fvec3::Blue();
			}

			mMatView = mpGfxTarget->MTXI()->LookAt( mEye, mTarget, mUp );
		}
		//mpGfxTarget->FBI()->ForceFlush();


		if( _explicitProjectionMatrix ){

		}
		else if( mpGfxTarget->GetRenderContextFrameData() )
		{
			mfAspect = fhdaspect*calcctx.mfAspectRatio;

			if (mHorizAper != 0)
				mAper = mHorizAper/calcctx.mfAspectRatio;

			if( mAper<1.0f ) mAper = 1.0f;
			if( mNear<0.1f ) mNear = 0.1f;
			if( mFar<0.5f ) mFar = 0.5f;

			mMatProj = mpGfxTarget->MTXI()->Persp( mAper, mfAspect, mNear, mFar );

		}
		else{
			mMatProj = fmtx4::Identity;
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

	fmtx4 matgp_proj;
	fmtx4 matgp_view;
	fmtx4 matgp_ivp;
	fmtx4 matgp_iv;

	if( mpGfxTarget )
	{
		//mpGfxTarget->FBI()->ForceFlush();
		matgp_proj.Perspective( mAper, mfAspect, mNear, mFar );

		if(_explicitViewMatrix)
			matgp_view = mMatView;
		else
			matgp_view.LookAt( mEye, mTarget, mUp );

		fmtx4 matgp_vp = matgp_view*matgp_proj;
		matgp_ivp.GEMSInverse(matgp_vp);
		matgp_iv.GEMSInverse(matgp_view);

		//mpGfxTarget->FBI()->ForceFlush();
	}

	//mMatViewSet = true;

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
