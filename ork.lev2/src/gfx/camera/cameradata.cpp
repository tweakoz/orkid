////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/pch.h>

namespace ork::lev2 {

////////////////////////////////////////////////////////////////////////////////

CameraData::CameraData()
    : mAper(17.0f)
    , mHorizAper(0.0f)
    , mNear(100.0f)
    , mFar(750.0f)
    , mEye(0.0f, 0.0f, 0.0f)
    , mTarget(0.0f, 0.0f, 1.0f)
    , mUp(0.0f, 1.0f, 0.0f)
    , mpLev2Camera(nullptr) {}

void CameraData::SetLev2Camera(lev2::Camera* pcam) {
  // printf( "CameraData::SetLev2Camera() this<%p> pcam<%p>\n", this, pcam );
  mpLev2Camera = pcam;
}

////////////////////////////////////////////////////////////////////////////////

void CameraData::Persp(float fnear, float ffar, float faper) {
  mAper      = faper;
  mHorizAper = 0;
  mNear      = fnear;
  mFar       = ffar;
}

////////////////////////////////////////////////////////////////////////////////

void CameraData::PerspH(float fnear, float ffar, float faperh) {
  mHorizAper = faperh;
  mNear      = fnear;
  mFar       = ffar;
}

////////////////////////////////////////////////////////////////////////////////

void CameraData::Lookat(const fvec3& eye, const fvec3& tgt, const fvec3& up) {
  mEye    = eye;
  mTarget = tgt;
  mUp     = up;
}

////////////////////////////////////////////////////////////////////////////////

CameraMatrices CameraData::computeMatrices(float faspect) const {
  CameraMatrices rval;
  ///////////////////////////////
  // gameplay calculation
  ///////////////////////////////
  float faper = mAper;
  // float faspect = mfAspect;
  float fnear = mNear;
  float ffar  = mFar;
  ///////////////////////////////////////////////////
  if (faper < 1.0f)
    faper = 1.0f;
  if (fnear < 0.1f)
    fnear = 0.1f;
  if (ffar < 0.5f)
    ffar = 0.5f;
  ///////////////////////////////////////////////////
  fvec3 target = mTarget;
  ///////////////////////////////////////////////////
  float fmag2 = (target - mEye).MagSquared();
  if (fmag2 < 0.01f) {
    target = mEye + fvec3::Blue();
  }
  ///////////////////////////////////////////////////
  rval._pmatrix.Perspective(faper, faspect, fnear, ffar);
  rval._vmatrix.LookAt(mEye, mTarget, mUp);
  ///////////////////////////////////////////////////
  fmtx4 matgp_vp = rval._vmatrix * rval._pmatrix;
  fmtx4 matgp_ivp;
  matgp_ivp.inverseOf(matgp_vp);
  // matgp_iv.inverseOf(matgp_view);
  rval._frustum.Set(matgp_ivp);
  ///////////////////////////////////////////////////
  rval._camdat = *this;
  return rval;
}

////////////////////////////////////////////////////////////////////////////////

#if 0
CameraMatrices CameraData::computeMatrices(& calcctx) {

  // orkprintf( "ccd camEYE <%f %f %f>\n", mEye.GetX(), mEye.GetY(), mEye.GetZ() );
  // orkprintf( "ccd camTGT <%f %f %f>\n", mTarget.GetX(), mTarget.GetY(), mTarget.GetZ() );

  ///////////////////////////////
  // target calculation
  ///////////////////////////////

  //////////////////////////////////////////////
  // THIS 16x9 really means are you in "stretch" mode
  // like when the wii renders at 640x448 on a 16x9 screen
  //////////////////////////////////////////////

  float fhdaspect = 1.0f;

  if (false) // Is16x9() )
  {
    if (mpGfxTarget != 0) {
      float fw         = float(mpGfxTarget->GetW()); // float(render_rect.miW);
      float fh         = float(mpGfxTarget->GetH()); // float(render_rect.miH);
      const float rWH  = (fw / fh);
      const float r169 = (16.0f / 9.0f);
      fhdaspect        = (r169 / rWH);
    }
  }

  //////////////////////////////////////////////

  if (!_explicitViewMatrix)
    mMatView = fmtx4::Identity;

  if (mpGfxTarget != 0) {
    if (!_explicitViewMatrix) {
      // float fmag0 = mEye.MagSquared();
      // float fmag1 = mTarget.MagSquared();
      float fmag2 = (mTarget - mEye).MagSquared();
      if (fmag2 < 0.01f) {
        mTarget = mEye + fvec3::Blue();
      }

      mMatView = mpGfxTarget->MTXI()->LookAt(mEye, mTarget, mUp);
    }

		////////////////////////////////////////////////////
		// explicit: projection matrix was set by application
		// dont compute it (eg. when project matrix comes from HMD)
		////////////////////////////////////////////////////

    if (_explicitProjectionMatrix) {

    }

		////////////////////////////////////////////////////
		// implicit: compute projection matrix (using GfxTarget)
		////////////////////////////////////////////////////

		else if (mpGfxTarget->topRenderContextFrameData()) {
      mfAspect = fhdaspect * calcctx.mfAspectRatio;

      if (mHorizAper != 0)
        mAper = mHorizAper / calcctx.mfAspectRatio;

      if (mAper < 1.0f)
        mAper = 1.0f;
      if (mNear < 0.1f)
        mNear = 0.1f;
      if (mFar < 0.5f)
        mFar = 0.5f;

      mMatProj = mpGfxTarget->MTXI()->Persp(mAper, mfAspect, mNear, mFar);

    }

		////////////////////////////////////////////////////
		// not much else we can do here...
		////////////////////////////////////////////////////

		else {
      mMatProj = fmtx4::Identity;
    }

		////////////////////////////////////////////////////

  }

	///////////////////////////////

  calcctx.mVMatrix = mMatView;
  calcctx.mPMatrix = mMatProj;

  ///////////////////////////////

  mVPMatrix = mMatView * mMatProj;
  mIVMatrix.inverseOf(mMatView);
  mIVPMatrix.inverseOf(mVPMatrix);

  ///////////////////////////////
  // gameplay calculation
  ///////////////////////////////

  fmtx4 matgp_proj;
  fmtx4 matgp_view;
  fmtx4 matgp_ivp;
  fmtx4 matgp_iv;

  if (mpGfxTarget) {
    matgp_proj.Perspective(mAper, mfAspect, mNear, mFar);

    if (_explicitViewMatrix)
      matgp_view = mMatView;
    else
      matgp_view.LookAt(mEye, mTarget, mUp);

    fmtx4 matgp_vp = matgp_view * matgp_proj;
    matgp_ivp.inverseOf(matgp_vp);
    matgp_iv.inverseOf(matgp_view);
  }

  ///////////////////////////////
  // generate frustum (useful for many things, like billboarding, clipping, LOD, etc.. )
  // we generate the frustum points, we should also generate plane eqns
	///////////////////////////////

  mFrustum.Set(matgp_ivp);
  mCamXNormal = mFrustum.mXNormal;
  mCamYNormal = mFrustum.mYNormal;
  mCamZNormal = mFrustum.mZNormal;

  calcctx.mFrustum = mFrustum;
  ///////////////////////////////
}
#endif

///////////////////////////////////////////////////////////////////////////////

fmtx4 StereoCameraMatrices::VL() const {
  return _left->GetVMatrix();
}
fmtx4 StereoCameraMatrices::VR() const {
  return _right->GetVMatrix();
}
fmtx4 StereoCameraMatrices::PL() const {
  return _left->GetPMatrix();
}
fmtx4 StereoCameraMatrices::PR() const {
  return _right->GetPMatrix();
}
fmtx4 StereoCameraMatrices::VPL() const {
  return _left->GetVMatrix()*_left->GetPMatrix();
}
fmtx4 StereoCameraMatrices::VPR() const {
  return _right->GetVMatrix()*_right->GetPMatrix();
}
fmtx4 StereoCameraMatrices::VMONO() const {
  return _mono->GetVMatrix();
}
fmtx4 StereoCameraMatrices::PMONO() const {
  return _mono->GetPMatrix();
}
fmtx4 StereoCameraMatrices::VPMONO() const {
  return _mono->GetVMatrix()*_mono->GetPMatrix();
}

fmtx4 StereoCameraMatrices::MVPL(const fmtx4& M) const {
  return (M*VL())*PL();
}
fmtx4 StereoCameraMatrices::MVPR(const fmtx4& M) const {
  return (M*VR())*PR();
}
fmtx4 StereoCameraMatrices::MVPMONO(const fmtx4& M) const {
  return (M*VMONO())*PMONO();
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
