////////////////////////////////////////////////////////////////////////////////
// Copyright 2007, Michael T. Mayers, all rights reserved.
////////////////////////////////////////////////////////////////////////////////

#ifndef _ORK_GFX_CAMERA_H
#define _ORK_GFX_CAMERA_H

////////////////////////////////////////////////////////////////////////////////

#include <ork/math/plane.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/frustum.h>

////////////////////////////////////////////////////////////////////////////////

namespace ork {

namespace lev2 {
class GfxTarget;
class CCamera;
}

////////////////////////////////////////////////////////////////////////////////

class CameraCalcContext
{
public:
	float		mfAspectRatio;
	fmtx4	mVMatrix;
	fmtx4	mPMatrix;
	Frustum		mFrustum;

	CameraCalcContext()
		: mfAspectRatio(1.0f)
	{
	}
};

////////////////////////////////////////////////////////////////////////////////

class CCameraData
{
public:

	CCameraData(); // : mAper(17.0f), mNear(100.0f), mFar(750.0f), mfWidth(1.0f), mfHeight(1.0f), mfAspect(1.0f), mpGfxTarget( 0 ) {}

	void CalcCameraData(CameraCalcContext& ctx);
	void CalcCameraMatrices(CameraCalcContext& ctx, float faspect=1.0f) const;

	////////////////////////////////////////////////////////////////////
	// Get vectors whos length equals one pixel
	//  given a worldpos (hopefully within the camera's frustum)
	//  given viewport dimensions in
	void GetPixelLengthVectors( const fvec3& Pos, const fvec2& vp, fvec3& OutX, fvec3& OutY ) const;
	////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////
	// generate direction vector/origin (from 2d normalized screen coordinate)

	void projectDepthRay( const fvec2& v2d, fvec3& vdir, fvec3& vori ) const;
    void projectDepthRay( const fvec2& v2d, fray3& ray_out ) const;

	void SetWidth(float fv) { mfWidth=fv; }
	void SetHeight(float fv) { mfHeight=fv; }
	void SetOverrideWidth(float fv) { mfOverrideWidth=fv; }
	void SetOverrideHeight(float fv) { mfOverrideHeight=fv; }
	void SetTimeStamp( float ft ) { mfTimeStamp=ft; }

	const fvec3& GetEye() const { return mEye; }
	const fvec3& GetTarget() const { return mTarget; }
	const fvec3& GetUp() const { return mUp; }
	void BindGfxTarget( ork::lev2::GfxTarget* ptarg ) { mpGfxTarget=ptarg; }

	float GetNear() const { return mNear; }
	float GetFar() const { return mFar; }
	float GetAperature() const { return mAper; }
	float GetAspect() const { return mfAspect; }
	float GetTimeStamp() const { return mfTimeStamp; }

	const fmtx4& GetPMatrix() const { return mMatProj; }
	const fmtx4& GetVMatrix() const { return mMatView; }
	const fmtx4& GetIVPMatrix() const { return mIVPMatrix; }
	const fmtx4& GetIVMatrix() const { return mIVMatrix; }
	const fmtx4& GetVPMatrix() const { return mVPMatrix; }

	const fvec3& GetXNormal() const { return mCamXNormal; }
	const fvec3& GetYNormal() const { return mCamYNormal; }
	const fvec3& GetZNormal() const { return mCamZNormal; }
	void SetXNormal(fvec3&n) { mCamXNormal=n; }
	void SetYNormal(fvec3&n) { mCamYNormal=n; }
	void SetZNormal(fvec3&n) { mCamZNormal=n; }

	const Frustum& GetFrustum() const { return mFrustum; }
	Frustum& GetFrustum() { return mFrustum; }

	void Persp( float fnear, float ffar, float faper );
	void PerspH( float fnear, float ffar, float faperh );
	void Lookat( const fvec3& eye, const fvec3& tgt, const fvec3& up );
	void SetView( const ork::fmtx4 &view);
	void setCustomProjection( const ork::fmtx4& proj);

	void SetVisibilityCamDat( const CCameraData* pvcd ) { mpVisibilityCamDat=pvcd; }
	const CCameraData* GetVisibilityCamDat() const { return mpVisibilityCamDat; }

	lev2::CCamera* getEditorCamera() const { return mpLev2Camera; }
	void SetLev2Camera(lev2::CCamera*pcam); // { mpLev2Camera=pcam; }

private:

	const CCameraData* mpVisibilityCamDat;
	lev2::CCamera	*mpLev2Camera;

	bool _explicitViewMatrix;
	bool _explicitProjectionMatrix;

	Frustum		mFrustum;

	fvec3	mCamXNormal;
	fvec3	mCamYNormal;
	fvec3	mCamZNormal;

	fmtx4	mMatProj;
	fmtx4	mMatProjID;
	fmtx4	mMatView;
	fmtx4	mIVMatrix;
	fmtx4	mIVPMatrix;
	fmtx4	mVPMatrix;

	float		mfWidth;
	float		mfHeight;
	float		mfOverrideWidth;
	float		mfOverrideHeight;
	float		mfAspect;
	float		mfTimeStamp;
	fvec3	mEye;
	fvec3	mTarget;
	fvec3	mUp;
	float		mAper;
	float		mHorizAper;
	float		mNear;
	float		mFar;



	ork::lev2::GfxTarget* mpGfxTarget;

};

////////////////////////////////////////////////////////////////////////////////

}

#endif
