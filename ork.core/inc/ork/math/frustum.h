////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <ork/math/line.h>
#include <ork/math/plane.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

template <typename T> struct TFrustum {
  typedef Ray3<T> ray_type;
  typedef Vector3<T> vec3_type;
  typedef Vector4<T> vec4_type;
  typedef Plane<T> plane_type;
  typedef Matrix44<T> mtx44_type;

  plane_type _nearPlane;
  plane_type _farPlane;
  plane_type _leftPlane;
  plane_type _rightPlane;
  plane_type _topPlane;
  plane_type _bottomPlane;

  vec3_type mNearCorners[4];
  vec3_type mFarCorners[4];
  vec3_type mCenter;
  vec3_type mXNormal;
  vec3_type mYNormal;
  vec3_type mZNormal;

  void SupportMapping(const vec3_type& v, vec3_type& result) const;

  void set(const mtx44_type& VMatrix, const mtx44_type& PMatrix);
  void set(const mtx44_type& IVPMatrix);

  void CalcCorners();

  bool contains(const vec3_type& v) const;
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TFrustum<T>::SupportMapping(const vec3_type& v, vec3_type& result) const {
  T num3;
  // const fvec3* pres = 0;
  num3 = T(0); // mNearCorners[0].dotWith(v); //fvec3.dotWith(this.corners[0], v, num3);
  for (int i = 0; i < 4; i++) {
    T num2;
    num2 = mNearCorners[i].dotWith(v); //, ref v, out num2);
    if (num2 > num3) {
      result = mNearCorners[i];
      num3   = num2;
    }
    num2 = mFarCorners[i].dotWith(v); //, ref v, out num2);
    if (num2 > num3) {
      result = mFarCorners[i];
      num3   = num2;
    }
  }
  // result = *pres;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TFrustum<T>::CalcCorners() {

  vec3_type ray_dir, ray_pos;

  bool bv;

  T planedist = T(0);

  // mNearCorners[4]; // tl tr br bl

  ray_type testray;

  _nearPlane.PlaneIntersect(_leftPlane, testray.mOrigin, testray.mDirection);
  bv = _topPlane.Intersect(testray, planedist, mNearCorners[0]);    // corner 0
  bv = _bottomPlane.Intersect(testray, planedist, mNearCorners[3]); // corner 3

  _rightPlane.PlaneIntersect(_nearPlane, testray.mOrigin, testray.mDirection);
  bv = _topPlane.Intersect(testray, planedist, mNearCorners[1]);    // corner 1
  bv = _bottomPlane.Intersect(testray, planedist, mNearCorners[2]); // corner 2

  _leftPlane.PlaneIntersect(_farPlane, testray.mOrigin, testray.mDirection);
  bv = _topPlane.Intersect(testray, planedist, mFarCorners[0]);    // corner 4
  bv = _bottomPlane.Intersect(testray, planedist, mFarCorners[3]); // corner 7

  _farPlane.PlaneIntersect(_rightPlane, testray.mOrigin, testray.mDirection);
  _topPlane.Intersect(testray, planedist, mFarCorners[1]);    // corner 5
  _bottomPlane.Intersect(testray, planedist, mFarCorners[2]); // corner 6
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TFrustum<T>::set(const mtx44_type& IVPMatrix) {
  T minv = T(-1);
  T maxv = T(1);
  T minz = T(-1);
  T maxz = T(1);

  vec4_type Vx0y0(minv, maxv, minz);
  vec4_type Vx1y0(maxv, maxv, minz);
  vec4_type Vx1y1(maxv, minv, minz);
  vec4_type Vx0y1(minv, minv, minz);

  // IVPMatrix.dump("IVPMatrix");
  // IVPMatrix.inverse().dump("PMatrix");

  mtx44_type::unProject(IVPMatrix, Vx0y0, mNearCorners[0]);
  mtx44_type::unProject(IVPMatrix, Vx1y0, mNearCorners[1]);
  mtx44_type::unProject(IVPMatrix, Vx1y1, mNearCorners[2]);
  mtx44_type::unProject(IVPMatrix, Vx0y1, mNearCorners[3]);

  // printf( "CalcCorners NC0<%g %g %g>\n", mNearCorners[0].x, mNearCorners[0].y, mNearCorners[0].z );
  // printf( "CalcCorners NC1<%g %g %g>\n", mNearCorners[1].x, mNearCorners[1].y, mNearCorners[1].z );
  // printf( "CalcCorners NC2<%g %g %g>\n", mNearCorners[2].x, mNearCorners[2].y, mNearCorners[2].z );
  // printf( "CalcCorners NC3<%g %g %g>\n", mNearCorners[3].x, mNearCorners[3].y, mNearCorners[3].z );

  Vx0y0.z = maxz;
  Vx1y0.z = maxz;
  Vx1y1.z = maxz;
  Vx0y1.z = maxz;

  mtx44_type::unProject(IVPMatrix, Vx0y0, mFarCorners[0]);
  mtx44_type::unProject(IVPMatrix, Vx1y0, mFarCorners[1]);
  mtx44_type::unProject(IVPMatrix, Vx1y1, mFarCorners[2]);
  mtx44_type::unProject(IVPMatrix, Vx0y1, mFarCorners[3]);

  vec3_type camrayN, camrayF;fmtx4 dmtx4_to_fmtx4(const dmtx4& dvec);
dmtx4 fmtx4_to_dmtx4(const fmtx4& dvec);


  mtx44_type::unProject(IVPMatrix, fvec4(0.0f, 0.0f, minz), camrayN);
  mtx44_type::unProject(IVPMatrix, fvec4(0.0f, 0.0f, maxz), camrayF);

  vec4_type camrayHALF = (camrayN + camrayF) * float(0.5f);

  mXNormal = mFarCorners[1] - mFarCorners[0];
  mYNormal = mFarCorners[3] - mFarCorners[0];
  mZNormal = (camrayF - camrayN);
  mXNormal.normalizeInPlace();
  mYNormal.normalizeInPlace();
  mZNormal.normalizeInPlace();

  vec3_type inNormal = mZNormal * float(-1.0f);
  _nearPlane.CalcFromNormalAndOrigin(mZNormal, camrayN);
  _farPlane.CalcFromNormalAndOrigin(inNormal, camrayF);

  _topPlane.CalcPlaneFromTriangle(mFarCorners[1], mFarCorners[0], mNearCorners[0], EPSILON);
  _bottomPlane.CalcPlaneFromTriangle(mNearCorners[3], mFarCorners[3], mFarCorners[2], EPSILON);
  _leftPlane.CalcPlaneFromTriangle(mNearCorners[0], mFarCorners[0], mFarCorners[3], EPSILON);
  _rightPlane.CalcPlaneFromTriangle(mNearCorners[2], mFarCorners[2], mFarCorners[1], EPSILON);
  // CalcCorners()l

  mCenter = (mFarCorners[0] + mFarCorners[1] + mFarCorners[2] + mFarCorners[3] + mNearCorners[0] + mNearCorners[1] +
             mNearCorners[2] + mNearCorners[3]) *
            0.125f;

#if 0 // test (camrayHALF should always be infront of planes, => all of these should return > 0
    F32 Dn = _nearPlane.pointDistance( camrayHALF );
    F32 Df = _farPlane.pointDistance( camrayHALF );
    F32 Dt = _topPlane.pointDistance( camrayHALF );
    F32 Db = _bottomPlane.pointDistance( camrayHALF );
    F32 Dl = _leftPlane.pointDistance( camrayHALF );
    F32 Dr = _rightPlane.pointDistance( camrayHALF );
    orkprintf( "Dn %f Df %f Dt %f Db %f Dl %f Dr %f\n", Dn, Df, Dt, Db, Dl, Dr );
#endif
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TFrustum<T>::set(const mtx44_type& VMatrix, const mtx44_type& PMatrix) {
  mtx44_type IVPMatrix;
  mtx44_type VPMatrix = mtx44_type::multiply_ltor(VMatrix, PMatrix);
  IVPMatrix.inverseOf(VPMatrix);
  set(IVPMatrix);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool TFrustum<T>::contains(const vec3_type& v) const {
  if (_topPlane.isPointBehind(v))
    return false;

  if (_bottomPlane.isPointBehind(v))
    return false;

  if (_leftPlane.isPointBehind(v))
    return false;

  if (_rightPlane.isPointBehind(v))
    return false;

  if (_nearPlane.isPointBehind(v))
    return false;

  if (_farPlane.isPointBehind(v))
    return false;

  return true;
}

///////////////////////////////////////////////////////////////////////////////

using Frustum  = TFrustum<float>;
using ffrustum = TFrustum<float>;
using dfrustum = TFrustum<double>;

using frustum_ptr_t = std::shared_ptr<Frustum>;
using dfrustum_ptr_t = std::shared_ptr<dfrustum>;

ffrustum dfrustum_to_ffrustum(const dfrustum& dvec);
dfrustum ffrustum_to_dfrustum(const ffrustum& dvec);

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
