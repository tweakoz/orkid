////////////////////////////////////////////////////////////////////////////////
// Copyright 2007, Michael T. Mayers, all rights reserved.
////////////////////////////////////////////////////////////////////////////////

#pragma once

////////////////////////////////////////////////////////////////////////////////

#include <ork/math/frustum.h>
#include <ork/math/cmatrix4.h>

////////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
class GfxTarget;
class Camera;
struct CameraVpData;

////////////////////////////////////////////////////////////////////////////////

struct CameraData {

  CameraData();

  //CameraVpData computeViewData() const;
  CameraVpData computeViewData(float faspect = 1.0f) const;

  const fvec3& GetEye() const { return mEye; }
  const fvec3& GetTarget() const { return mTarget; }
  const fvec3& GetUp() const { return mUp; }

  float GetNear() const { return mNear; }
  float GetFar() const { return mFar; }
  float GetAperature() const { return mAper; }

  const fvec3& xNormal() const { return _xnormal; }
  const fvec3& yNormal() const { return _ynormal; }
  const fvec3& zNormal() const { return _znormal; }
  void setXNormal(fvec3& n) { _xnormal = n; }
  void setYNormal(fvec3& n) { _ynormal = n; }
  void setZNormal(fvec3& n) { _znormal = n; }

  void Lookat(const fvec3& eye, const fvec3& tgt, const fvec3& up);

  lev2::Camera* getEditorCamera() const { return mpLev2Camera; }
  void SetLev2Camera(lev2::Camera* pcam);

  void Persp(float fnear, float ffar, float faper);
  void PerspH(float fnear, float ffar, float faperh);

  fvec3 mEye;
  fvec3 mTarget;
  fvec3 mUp;

  fvec3 _xnormal;
  fvec3 _ynormal;
  fvec3 _znormal;

  lev2::Camera* mpLev2Camera = nullptr;
  float mAper = 0.0f;
  float mHorizAper = 0.0f;
  float mNear = 0.0f;
  float mFar = 0.0f;

};

////////////////////////////////////////////////////////////////////////////////

struct CameraVpData {
  CameraData _camdat;
  bool _explicitProjectionMatrix = false;
  bool _explicitViewMatrix = false;
  fmtx4 _vmatrix;
  fmtx4 _ivmatrix;
  fmtx4 _pmatrix;
  fmtx4 _vpmatrix;
  fmtx4 _ivpmatrix;
  Frustum _frustum;
  float _width = 1.0f;
  float _height = 1.0f;
  float _aspectRatio = 1.0f;
  ////////////////////////////////////////////////////////////////////
  void SetWidth(float fv) { _width = fv; }
  void SetHeight(float fv) { _height = fv; }
  ////////////////////////////////////////////////////////////////////
  const Frustum& GetFrustum() const { return _frustum; }
  Frustum& GetFrustum() { return _frustum; }
  ////////////////////////////////////////////////////////////////////
  // Get vectors whos length equals one pixel
  //  given a worldpos (hopefully within the camera's frustum)
  //  given viewport dimensions in
  ////////////////////////////////////////////////////////////////////
  void GetPixelLengthVectors(const fvec3& Pos, const fvec2& vp, fvec3& OutX, fvec3& OutY) const;
  ////////////////////////////////////////////////////////////////////
  // generate direction vector/origin (from 2d normalized screen coordinate)
  ////////////////////////////////////////////////////////////////////
  void projectDepthRay(const fvec2& v2d, fvec3& vdir, fvec3& vori) const;
  void projectDepthRay(const fvec2& v2d, fray3& ray_out) const;
  ////////////////////////////////////////////////////////////////////
  const fmtx4& GetVMatrix() const { return _vmatrix; }
  const fmtx4& GetPMatrix() const { return _pmatrix; }
  const fmtx4& GetIVPMatrix() const { return _ivpmatrix; }
  const fmtx4& GetVPMatrix() const { return _vpmatrix; }
  float GetAspect() const { return _aspectRatio; }
 //void (ork::lev2::GfxTarget* ptarg) { mpGfxTarget = ptarg; }
  ////////////////////////////////////////////////////////////////////
  void setCustomView(const ork::fmtx4& view);
  void setCustomProjection(const ork::fmtx4& proj);
  //ork::lev2::GfxTarget* mpGfxTarget;
};

////////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
