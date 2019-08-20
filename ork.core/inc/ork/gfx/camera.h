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
	CMatrix4	mVMatrix;
	CMatrix4	mPMatrix;
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
	void GetPixelLengthVectors( const CVector3& Pos, const CVector2& vp, CVector3& OutX, CVector3& OutY ) const;
	////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////
	// generate direction vector/origin (from 2d normalized screen coordinate)

	void ProjectDepthRay( const CVector2& v2d, CVector3& vdir, CVector3& vori ) const;
    void ProjectDepthRay( const CVector2& v2d, fray3& ray_out ) const;

	void SetWidth(float fv) { mfWidth=fv; }
	void SetHeight(float fv) { mfHeight=fv; }
	void SetOverrideWidth(float fv) { mfOverrideWidth=fv; }
	void SetOverrideHeight(float fv) { mfOverrideHeight=fv; }
	void SetTimeStamp( float ft ) { mfTimeStamp=ft; }

	const CVector3& GetEye() const { return mEye; }
	const CVector3& GetTarget() const { return mTarget; }
	const CVector3& GetUp() const { return mUp; }
	void BindGfxTarget( ork::lev2::GfxTarget* ptarg ) { mpGfxTarget=ptarg; }

	float GetNear() const { return mNear; }
	float GetFar() const { return mFar; }
	float GetAperature() const { return mAper; }
	float GetAspect() const { return mfAspect; }
	float GetTimeStamp() const { return mfTimeStamp; }

	const CMatrix4& GetPMatrix() const { return mMatProj; }
	const CMatrix4& GetVMatrix() const { return mMatView; }
	const CMatrix4& GetIVPMatrix() const { return mIVPMatrix; }
	const CMatrix4& GetIVMatrix() const { return mIVMatrix; }
	const CMatrix4& GetVPMatrix() const { return mVPMatrix; }

	const CVector3& GetXNormal() const { return mCamXNormal; }
	const CVector3& GetYNormal() const { return mCamYNormal; }
	const CVector3& GetZNormal() const { return mCamZNormal; }
	void SetXNormal(CVector3&n) { mCamXNormal=n; }
	void SetYNormal(CVector3&n) { mCamYNormal=n; }
	void SetZNormal(CVector3&n) { mCamZNormal=n; }

	const Frustum& GetFrustum() const { return mFrustum; }
	Frustum& GetFrustum() { return mFrustum; }

	void Persp( float fnear, float ffar, float faper );
	void PerspH( float fnear, float ffar, float faperh );
	void Lookat( const CVector3& eye, const CVector3& tgt, const CVector3& up );
	void SetView( const ork::CMatrix4 &view);
	void setCustomProjection( const ork::CMatrix4& proj);

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

	CVector3	mCamXNormal;
	CVector3	mCamYNormal;
	CVector3	mCamZNormal;

	CMatrix4	mMatProj;
	CMatrix4	mMatProjID;
	CMatrix4	mMatView;
	CMatrix4	mIVMatrix;
	CMatrix4	mIVPMatrix;
	CMatrix4	mVPMatrix;

	float		mfWidth;
	float		mfHeight;
	float		mfOverrideWidth;
	float		mfOverrideHeight;
	float		mfAspect;
	float		mfTimeStamp;
	CVector3	mEye;
	CVector3	mTarget;
	CVector3	mUp;
	float		mAper;
	float		mHorizAper;
	float		mNear;
	float		mFar;



	ork::lev2::GfxTarget* mpGfxTarget;

};

////////////////////////////////////////////////////////////////////////////////

}

#endif
