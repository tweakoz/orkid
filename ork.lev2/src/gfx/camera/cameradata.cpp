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

void CameraData::SetLev2Camera(lev2::UiCamera* pcam) {
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
  rval._vmatrix.LookAt(mEye, mTarget, mUp);
  rval._ivmatrix.inverseOf(rval._vmatrix);
  rval._pmatrix.Perspective(faper, faspect, fnear, ffar);
  rval._ipmatrix.inverseOf(rval._pmatrix);
  rval._vpmatrix = rval._vmatrix * rval._pmatrix;
  rval._ivpmatrix.inverseOf(rval._vpmatrix);
  rval._frustum.Set(rval._ivpmatrix);
  ///////////////////////////////////////////////////
  rval._camdat = *this;
  return rval;
}

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
