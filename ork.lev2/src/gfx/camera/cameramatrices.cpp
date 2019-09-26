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

void CameraMatrices::setCustomView(const ork::fmtx4& view) {
  _vmatrix = view;
  // view.dump("setview");
  _explicitViewMatrix = true;
}

void CameraMatrices::setCustomProjection(const ork::fmtx4& proj) {
  // proj.dump("setproj");
  _pmatrix                  = proj;
  _explicitProjectionMatrix = true;
  _aspectRatio              = 1.0f;
  // mNear                     = .1f;
  // mFar                      = 1000.f;
  // mAper                     = 60;
}

////////////////////////////////////////////////////////////////////////////////

void CameraMatrices::projectDepthRay(const fvec2& v2d, fvec3& vdir, fvec3& vori) const {
  fvec3 near_xt_lerp;
  near_xt_lerp.Lerp(_frustum.mNearCorners[0], _frustum.mNearCorners[1], v2d.GetX());
  fvec3 near_xb_lerp;
  near_xb_lerp.Lerp(_frustum.mNearCorners[3], _frustum.mNearCorners[2], v2d.GetX());
  fvec3 near_lerp;
  near_lerp.Lerp(near_xt_lerp, near_xb_lerp, v2d.GetY());
  fvec3 far_xt_lerp;
  far_xt_lerp.Lerp(_frustum.mFarCorners[0], _frustum.mFarCorners[1], v2d.GetX());
  fvec3 far_xb_lerp;
  far_xb_lerp.Lerp(_frustum.mFarCorners[3], _frustum.mFarCorners[2], v2d.GetX());
  fvec3 far_lerp;
  far_lerp.Lerp(far_xt_lerp, far_xb_lerp, v2d.GetY());
  vdir = (far_lerp - near_lerp).Normal();
  vori = near_lerp;
}

////////////////////////////////////////////////////////////////////////////////

void CameraMatrices::projectDepthRay(const fvec2& v2d, fray3& ray_out) const {
  fvec3 dir, ori;
  projectDepthRay(v2d, dir, ori);
  ray_out = fray3(ori, dir);
}

////////////////////////////////////////////////////////////////////////////////

void CameraMatrices::GetPixelLengthVectors(const fvec3& Pos, const fvec2& vp, fvec3& OutX, fvec3& OutY) const {
  /////////////////////////////////////////////////////////////////
  int ivpw = int(vp.x);
  int ivph = int(vp.y);
  /////////////////////////////////////////////////////////////////
  fvec4 va    = Pos;
  fvec4 va_xf = va.Transform(_vpmatrix);
  va_xf.PerspectiveDivide();
  va_xf = va_xf * fvec4(vp.GetX(), vp.GetY(), 0.0f);
  /////////////////////////////////////////////////////////////////
  fvec4 vdx    = Pos + _camdat.xNormal();
  fvec4 vdx_xf = vdx.Transform(_vpmatrix);
  vdx_xf.PerspectiveDivide();
  vdx_xf     = vdx_xf * fvec4(vp.GetX(), vp.GetY(), 0.0f);
  float MagX = (vdx_xf - va_xf).Mag(); // magnitude in pixels of mBillboardRight
  /////////////////////////////////////////////////////////////////
  fvec4 vdy    = Pos + _camdat.yNormal();
  fvec4 vdy_xf = vdy.Transform(_vpmatrix);
  vdy_xf.PerspectiveDivide();
  vdy_xf     = vdy_xf * fvec4(vp.GetX(), vp.GetY(), 0.0f);
  float MagY = (vdy_xf - va_xf).Mag(); // magnitude in pixels of mBillboardUp
  /////////////////////////////////////////////////////////////////
  OutX = _camdat.xNormal() * (2.0f / MagX);
  OutY = _camdat.yNormal() * (2.0f / MagY);
  /////////////////////////////////////////////////////////////////
}

} // namespace ork::lev2
