////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#pragma once
////////////////////////////////////////////////////////////////
#include <cmath>
#include <ork/math/math_types.inl>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/string/deco.inl>
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>

#if defined(_WIN32) && !defined(_XBOX)
#include <pmmintrin.h>
#endif

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.inl>

namespace ork {

template <typename T> const Matrix44<T> Matrix44<T>::Identity() {
  return Matrix44<T>();
}

template <typename T> Matrix44<T>::Matrix44(const kln::translator& t) {
  const auto scalar = T(-2);
  setTranslation(t.e01() * scalar, t.e02() * scalar, t.e03() * scalar);
}

template <typename T> Matrix44<T>::Matrix44(const kln::rotor& r) {
  auto q = Quaternion<T>(r);
  this->fromQuaternion(q);
}

template <typename T> Matrix44<T>::Matrix44(const kln::motor& m) {
  const auto& M = m.as_mat4x4();
  auto dest     = asArray();
  for (int i = 0; i < 16; i++) {
    dest[i] = M.data[i];
  }
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> 
T Matrix44<T>::operator[](int i, int j) const {
  return elemXY(i, j);
}
template <typename T> 
T& Matrix44<T>::operator[](int i, int j) {
  base_t& as_base = *this;
  OrkAssert(0<=i and i<4);
  OrkAssert(0<=j and j<4);
  return as_base[i][j];
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Matrix44<T> Matrix44<T>::multiply_ltor(const Matrix44<T>& a, const Matrix44<T>& b) {
  return b.multiply_rtol(a);
}

template <typename T> Matrix44<T> Matrix44<T>::multiply_ltor(const Matrix44<T>& a, const Matrix44<T>& b, const Matrix44<T>& c) {
  return c.multiply_rtol(b.multiply_rtol(a));
}

template <typename T>
Matrix44<T> Matrix44<T>::multiply_ltor(const Matrix44<T>& a, const Matrix44<T>& b, const Matrix44<T>& c, const Matrix44<T>& d) {
  return d.multiply_rtol(c.multiply_rtol(b.multiply_rtol(a)));
}

template <typename T>
Matrix44<T> Matrix44<T>::multiply_ltor(
    const Matrix44<T>& a,
    const Matrix44<T>& b,
    const Matrix44<T>& c,
    const Matrix44<T>& d,
    const Matrix44<T>& e) {
  return e.multiply_rtol(d.multiply_rtol(c.multiply_rtol(b.multiply_rtol(a))));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Matrix44<T> Matrix44<T>::fromOuterProduct(const Vector4<T>& c, const Vector4<T>& r) {
  auto glm_op = glm::outerProduct(c.asGlmVec4(), r.asGlmVec4());
  return Matrix44<T>(glm_op);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::toEulerXYZ(T& ex, T& ey, T& ez) const {
  const base_t& as_base = *this;
  glm::extractEulerAngleXYZ(as_base, ex, ey, ez);
}
template <typename T> void Matrix44<T>::fromEulerXYZ(T ex, T ey, T ez) {
  base_t& as_base = *this;
  as_base         = glm::eulerAngleXYZ(ex, ey, ez);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Matrix44<T>::Matrix44(const Quaternion<T>& q) {
  this->fromQuaternion(q);
}

template <typename T> Matrix33<T> Matrix44<T>::rotMatrix33(void) const {
  Matrix33<T> rval;
  auto xnormal = xNormal().normalized();
  auto ynormal = yNormal().normalized();
  auto znormal = zNormal().normalized();
  rval.fromNormalVectors(xnormal, ynormal, znormal);
  return rval;
}
template <typename T> Matrix44<T> Matrix44<T>::rotMatrix44(void) const {
  Matrix44<T> rval;
  auto xnormal = xNormal().normalized();
  auto ynormal = yNormal().normalized();
  auto znormal = zNormal().normalized();
  rval.fromNormalVectors(xnormal, ynormal, znormal);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::setToIdentity() {
  base_t& as_base = *this;
  as_base         = base_t(T(1));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::dump(std::string name) const {
  orkprintf("Matrix %p %s\n{\n", this, name.c_str());

  Vector4<T> col0 = column(0);
  Vector4<T> col1 = column(1);
  Vector4<T> col2 = column(2);
  Vector4<T> col3 = column(3);

  orkprintf("   |   col0  |   col1  |   col2  |   col3  |\n");
  orkprintf("   | %+0.4f | %+0.4f | %+0.4f | %+0.4f |\n", col0.x, col1.x, col2.x, col3.x);
  orkprintf("   | %+0.4f | %+0.4f | %+0.4f | %+0.4f |\n", col0.y, col1.y, col2.y, col3.y);
  orkprintf("   | %+0.4f | %+0.4f | %+0.4f | %+0.4f |\n", col0.z, col1.z, col2.z, col3.z);
  orkprintf("   | %+0.4f | %+0.4f | %+0.4f | %+0.4f |\n", col0.w, col1.w, col2.w, col3.w);

  orkprintf("\n	");

  orkprintf("\n}\n");
}
template <typename T> std::string Matrix44<T>::dump(Vector3<T> color) const {
  std::string rval;
  bool use_color = color.length() > 0.0f;
  auto color2    = color * 0.6;
  auto color3    = color * 0.8;
  auto color4    = color * 0.3;
  for (int i = 0; i < 4; i++) {
    if (use_color) {
      rval += ork::deco::asciic_rgb(color4);
    }
    rval += "[";
    for (int j = 0; j < 4; j++) {
      if (use_color) {
        rval += j < 3 ? ork::deco::asciic_rgb(color2) : ork::deco::asciic_rgb(color4);
      }
      //////////////////////////////////
      // round down small numbers
      //////////////////////////////////
      float elem = elemXY(i, j);
      elem       = float(int(elem * 10000)) / 10000.0f;
      //////////////////////////////////
      rval += FormatString(" %+0.4g ", elem);
    }
    if (use_color) {
      rval += ork::deco::asciic_rgb(color4);
    }
    rval += "] ";
  }
  {
    Quaternion<T> q(*this);
    auto rot = q.toAxisAngle();
    if (rot.w < 0.0f) {
      rot *= -1.0f;
    }
    if (use_color) {
      rval += ork::deco::asciic_rgb(color);
    }
    // rval += FormatString("  quat<%0.1g %0.1g %0.1g %0.1g>", q.x, q.y, q.z, q.w);
    rval += FormatString("  axis<%0.1g %0.1g %0.1g> angle<%g>", rot.x, rot.y, rot.z, round(rot.w * RTOD));
  }
  if (use_color) {
    rval += ork::deco::asciic_reset();
  }
  return rval;
}
template <typename T> std::string Matrix44<T>::dump4x3(Vector3<T> color) const {
  std::string rval;
  bool use_color = color.length() > 0.0f;
  auto color2    = color * 0.6;
  auto color3    = color * 0.8;
  auto color4    = color * 0.3;
  for (int i = 0; i < 4; i++) {
    if (use_color) {
      rval += ork::deco::asciic_rgb(color4);
    }
    rval += "[";
    for (int j = 0; j < 3; j++) {
      if (use_color) {
        rval += ork::deco::asciic_rgb(color2);
      }
      //////////////////////////////////
      // round down small numbers
      //////////////////////////////////
      float elem = elemXY(i, j);
      elem       = float(int(elem * 10000)) / 10000.0f;
      //////////////////////////////////
      rval += FormatString(" %+0.4g ", elem);
    }
    if (use_color) {
      rval += ork::deco::asciic_rgb(color4);
    }
    rval += "] ";
  }
  {
    Quaternion<T> q(*this);
    auto rot = q.asEuler();
    if (use_color) {
      rval += ork::deco::asciic_rgb(color);
    }
    // rval += FormatString("  quat<%0.1g %0.1g %0.1g %0.1g>", q.x, q.y, q.z, q.w);
    rval += FormatString("  euler<%g %g %g>", round(rot.x * RTOD), round(rot.y * RTOD), round(rot.z * RTOD));
  }
  if (use_color) {
    rval += ork::deco::asciic_reset();
  }
  return rval;
}
template <typename T> std::string Matrix44<T>::dump4x3cn(bool do_axis_angle) const {
  std::string rval;
  auto brace_color = fvec3(0.7, 0.7, 0.7);
  for (int i = 0; i < 4; i++) {
    rval += ork::deco::decorate(brace_color, "[");
    fvec3 fcol;
    switch (i) {
      case 0:
        fcol = fvec3(1, .5, .5);
        break;
      case 1:
        fcol = fvec3(.5, 1, .5);
        break;
      case 2:
        fcol = fvec3(.5, .5, 1);
        break;
      case 3:
        fcol = fvec3(0.7, 0.7, 0.7);
        break;
    }
    for (int j = 0; j < 3; j++) {
      //////////////////////////////////
      // round down small numbers
      //////////////////////////////////
      float elem = elemXY(i, j);
      elem       = float(int(elem * 10000)) / 10000.0f;
      //////////////////////////////////
      rval += ork::deco::format(fcol, " %+0.4g ", elem);
    }
    rval += ork::deco::decorate(brace_color, "]");
  }
  if (do_axis_angle) {
    Quaternion<T> q(*this);
    // auto rot = q.toEuler();
    auto rot  = q.toAxisAngle();
    float ang = round(rot.w * RTOD);
    if (ang == 0.0f) {
    } else {
      rval += ork::deco::format(fvec3(0.8, 0.8, 0.8), "  axis<%g %g %g> ang<%g>", rot.x, rot.y, rot.z, ang);
    }
    // rval += ork::deco::format(fvec3(0.8, 0.8, 0.8), "  quat<%g %g %g %g>", q.x, q.y, q.z, q.w);
  }
  return rval;
}
template <typename T> std::string Matrix44<T>::dump4x4cn() const {
  std::string rval;
  auto brace_color = fvec3(0.7, 0.7, 0.7);
  for (int i = 0; i < 4; i++) {
    rval += ork::deco::decorate(brace_color, "[");
    fvec3 fcol;
    switch (i) {
      case 0:
        fcol = fvec3(1, .5, .5);
        break;
      case 1:
        fcol = fvec3(.5, 1, .5);
        break;
      case 2:
        fcol = fvec3(.5, .5, 1);
        break;
      case 3:
        fcol = fvec3(0.7, 0.7, 0.7);
        break;
    }
    for (int j = 0; j < 4; j++) {
      //////////////////////////////////
      // round down small numbers
      //////////////////////////////////
      float elem = elemXY(i, j);
      elem       = float(int(elem * 10000)) / 10000.0f;
      //////////////////////////////////
      rval += ork::deco::format(fcol, " %+0.4g ", elem);
    }
    rval += ork::deco::decorate(brace_color, "]");
  }
  return rval;
}
template <typename T> std::string Matrix44<T>::dump() const {
  std::string rval;
  for (int i = 0; i < 4; i++) {
    rval += "[";
    for (int j = 0; j < 4; j++) {
      rval += FormatString(" %+0.4g ", elemXY(i, j));
    }
    rval += "] ";
  }
  Quaternion<T> q(*this);
  auto rot = q.toAxisAngle();
  if (rot.w < 0.0f) {
    rot *= -1.0f;
  }
  // rval += FormatString("  quat<%0.1g %0.1g %0.1g %0.1g>", q.x, q.y, q.z, q.w);
  rval += FormatString("  axis<%0.1g %0.1g %0.1g> angle<%g>", rot.x, rot.y, rot.z, round(rot.w * RTOD));
  return rval;
}
template <typename T> std::string Matrix44<T>::dump4x3() const {
  std::string rval;
  for (int i = 0; i < 4; i++) {
    rval += "[";
    for (int j = 0; j < 3; j++) {
      rval += FormatString(" %+0.4g ", elemXY(i, j));
    }
    rval += "] ";
  }
  Quaternion<T> q(*this);
  auto rot = q.toAxisAngle();
  if (rot.w < 0.0f) {
    rot *= -1.0f;
  }
  // rval += FormatString("  quat<%0.1g %0.1g %0.1g %0.1g>", q.x, q.y, q.z, q.w);
  rval += FormatString("  axis<%0.1g %0.1g %0.1g> angle<%g>", rot.x, rot.y, rot.z, round(rot.w * RTOD));
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Matrix44<T> Matrix44<T>::translationOnly() const {
  Matrix44<T> rval = *this;
  rval.setColumn(0, Vector4<T>(1, 0, 0, 0));
  rval.setColumn(1, Vector4<T>(0, 1, 0, 0));
  rval.setColumn(2, Vector4<T>(0, 0, 1, 0));
  return rval;
}
template <typename T> Matrix44<T> Matrix44<T>::rotScaleOnly() const {
  Matrix44<T> rval = *this;
  rval.setColumn(3, Vector4<T>(0, 0, 0, 1));
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::setElemYX(int ix, int iy, T val) {
  base_t& as_base = *this;
  as_base[iy][ix] = val;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> T Matrix44<T>::elemYX(int ix, int iy) const {
  const base_t& as_base = *this;
  return as_base[iy][ix];
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::setElemXY(int ix, int iy, T val) {
  base_t& as_base = *this;
  OrkAssert(0<=ix and ix<4);
  OrkAssert(0<=iy and iy<4);
  as_base[ix][iy] = val;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> T Matrix44<T>::elemXY(int ix, int iy) const {
  const base_t& as_base = *this;
  OrkAssert(0<=ix and ix<4);
  OrkAssert(0<=iy and iy<4);
  return as_base[ix][iy];
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::translate(const Vector4<T>& vec) {
  Matrix44<T> temp, res;
  temp.setTranslation(vec);
  res   = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::translate(T vx, T vy, T vz) {
  Matrix44<T> temp, res;
  temp.setTranslation(vx, vy, vz);
  res   = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::setTranslation(const Vector3<T>& vec) {
  setColumn(3, Vector4<T>(vec, 1.0f));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::setTranslation(T _x, T _y, T _z) {
  setTranslation(Vector3<T>(_x, _y, _z));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T> Matrix44<T>::translation(void) const {
  return column(3).xyz();
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//	set rotation explicitly

template <typename T> void Matrix44<T>::setRotateX(T rad) {
  auto ident      = base_t(1);
  base_t& as_base = *this;
  as_base         = glm::rotate(ident, rad, Vector3<T>(1, 0, 0));
}

///////////////////////////////////////////////////////////////////////////////
//	set rotation explicitly

template <typename T> void Matrix44<T>::setRotateY(T rad) {
  auto ident      = base_t(1);
  base_t& as_base = *this;
  as_base         = glm::rotate(ident, rad, Vector3<T>(0, 1, 0));
}

///////////////////////////////////////////////////////////////////////////////
//	set rotation explicitly

template <typename T> void Matrix44<T>::setRotateZ(T rad) {
  auto ident      = base_t(1);
  base_t& as_base = *this;
  as_base         = glm::rotate(ident, rad, Vector3<T>(0, 0, 1));
}

///////////////////////////////////////////////////////////////////////////////
//	rotate in place

template <typename T> void Matrix44<T>::rotateOnX(T rad) {
  Matrix44<T> temp, res;
  temp.setRotateX(rad);
  res   = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////
//	rotate in place

template <typename T> void Matrix44<T>::rotateOnY(T rad) {
  Matrix44<T> temp, res;
  temp.setRotateY(rad);
  res   = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////
//	rotate in place

template <typename T> void Matrix44<T>::rotateOnZ(T rad) {
  Matrix44<T> temp, res;
  temp.setRotateZ(rad);
  res   = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::setScale(const Vector4<T>& vec) {
  setScale(vec.x, vec.y, vec.z);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::setScale(T x, T y, T z) {
  auto ident      = base_t(1);
  base_t& as_base = *this;
  as_base         = glm::scale(ident, Vector3<T>(x, y, z));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::setScale(T s) {
  setScale(s, s, s);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::scale(const Vector4<T>& vec) {
  Matrix44<T> temp, res;
  temp.setScale(vec);
  res   = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::scale(T xscl, T yscl, T zscl) {
  Matrix44<T> temp, res;
  temp.setScale(xscl, yscl, zscl);
  res   = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::setXY(const array44_t& val) {
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      setElemXY(i, j, val[i][j]);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::setYX(const array44_t& val) {
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      setElemXY(i, j, val[j][i]);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// sm - rotation matrix from quaternion
template <typename T> void Matrix44<T>::fromQuaternion(Quaternion<T> quat) {
  T xx = quat.x * quat.x;
  T yy = quat.y * quat.y;
  T zz = quat.z * quat.z;
  T xy = quat.x * quat.y;
  T zw = quat.z * quat.w;
  T zx = quat.z * quat.x;
  T yw = quat.y * quat.w;
  T yz = quat.y * quat.z;
  T xw = quat.x * quat.w;

  setElemXY(0, 0, T(1) - (T(2) * (yy + zz)));
  setElemXY(0, 1, T(2) * (xy + zw));
  setElemXY(0, 2, T(2) * (zx - yw));
  setElemXY(0, 3, T(0));
  setElemXY(1, 0, T(2) * (xy - zw));
  setElemXY(1, 1, T(1) - (T(2) * (zz + xx)));
  setElemXY(1, 2, T(2) * (yz + xw));
  setElemXY(1, 3, T(0));
  setElemXY(2, 0, T(2) * (zx + yw));
  setElemXY(2, 1, T(2) * (yz - xw));
  setElemXY(2, 2, T(1) - (T(2) * (yy + xx)));
  setElemXY(2, 3, T(0));
  setElemXY(3, 0, T(0));
  setElemXY(3, 1, T(0));
  setElemXY(3, 2, T(0));
  setElemXY(3, 3, T(1));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::createBillboard(Vector3<T> objectPos, Vector3<T> viewPos, Vector3<T> upVec) {
  Vector3<T> dir;
  Vector3<T> res;
  Vector3<T> cross;

  dir.x = (objectPos.x - viewPos.x);
  dir.y = (objectPos.y - viewPos.y);
  dir.z = (objectPos.z - viewPos.z);

  T slen = dir.magnitudeSquared();
  dir    = dir * (T(1) / sqrtf(slen));

  cross = upVec;
  cross = cross.crossWith(dir).normalized();

  res = dir;
  res = res.crossWith(cross);

  setElemXY(0, 0, cross.x);
  setElemXY(0, 1, cross.y);
  setElemXY(0, 2, cross.z);
  setElemXY(0, 3, T(0));
  setElemXY(1, 0, res.x);
  setElemXY(1, 1, res.y);
  setElemXY(1, 2, res.z);
  setElemXY(1, 3, T(0));
  setElemXY(2, 0, dir.x);
  setElemXY(2, 1, dir.y);
  setElemXY(2, 2, dir.z);
  setElemXY(2, 3, T(0));
  setElemXY(3, 0, objectPos.x);
  setElemXY(3, 1, objectPos.y);
  setElemXY(3, 2, objectPos.z);
  setElemXY(3, 3, T(1));
}

template <typename T> void Matrix44<T>::createBillboard2(Vector3<T> objectPos, Vector3<T> viewPos, Vector3<T> upVec) {
  Vector3<T> dir;
  Vector3<T> res;
  Vector3<T> cross;

  dir.x = (objectPos.x - viewPos.x);
  dir.y = (objectPos.y - viewPos.y);
  dir.z = (objectPos.z - viewPos.z);

  T slen = dir.magnitudeSquared();
  dir    = dir * (T(1) / sqrtf(slen));

  cross = -upVec;
  cross = cross.crossWith(dir).normalized();

  res = dir;
  res = res.crossWith(cross);

  setElemXY(0, 0, cross.x);
  setElemXY(0, 1, cross.y);
  setElemXY(0, 2, cross.z);
  setElemXY(0, 3, T(0));
  setElemXY(1, 0, res.x);
  setElemXY(1, 1, res.y);
  setElemXY(1, 2, res.z);
  setElemXY(1, 3, T(0));
  setElemXY(2, 0, dir.x);
  setElemXY(2, 1, dir.y);
  setElemXY(2, 2, dir.z);
  setElemXY(2, 3, T(0));
  setElemXY(3, 0, objectPos.x);
  setElemXY(3, 1, objectPos.y);
  setElemXY(3, 2, objectPos.z);
  setElemXY(3, 3, T(1));
}
///////////////////////////////////////////////////////////////////////////////

template <typename T> Matrix44<T> Matrix44<T>::multiply_rtol(const Matrix44<T>& rhs) const {
  Matrix44<T> result;
  base_t& result_as_base = result;
  const base_t& lhs_base = *this;
  const base_t& rhs_base = rhs;
  result_as_base         = lhs_base * rhs_base;
  return (result);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Matrix44<T> Matrix44<T>::multiply(T scalar) const {
  Matrix44<T> result;
  base_t& result_as_base  = result;
  const base_t& a_as_base = *this;
  result_as_base          = a_as_base * scalar;
  return (result);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::correctionMatrix(const Matrix44<T>& from, const Matrix44<T>& to) {
  /////////////////////////
  //
  //	GENERATE CORRECTION	TO GET FROM A to C
  //
  //	A * B = C				(A and C are known we dont know B)
  //	(A * iA) * B = iA * C
  //	B = iA * C				we now know B
  //
  /////////////////////////

  Matrix44<T> inv_from = from.inverse();
  inv_from.dump();
  *this = multiply_ltor(inv_from, to); // B
  this->dump();
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::setRotation(const Matrix44<T>& from) {
  Matrix44<T> rval = from;
  rval.setTranslation(T(0), T(0), T(0));
  rval.normalizeInPlace();
  *this = rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::setTranslation(const Matrix44<T>& from) {
  setToIdentity();
  Vector4<T> t = from.translation();
  setTranslation(t);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void Matrix44<T>::setScale(const Matrix44<T>& from) // assumes rot is zero!
{
  Matrix44<T> RS = from;
  RS.setTranslation(T(0), T(0), T(0));

  Matrix44<T> R = RS;
  R.normalizeInPlace();

  Matrix44<T> S;
  S.correctionMatrix(R, RS);

  *this = S;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void Matrix44<T>::lerp(const Matrix44<T>& from, const Matrix44<T>& to, T par) // par 0.0f .. 1.0f
{
  //////////////////

  Vector4<T> vF = from.translation();
  Vector4<T> vT = to.translation();
  Vector4<T> vT2;
  vT2.lerp(vF, vT, par);
  Matrix44<T> matT;
  matT.setTranslation(vT2);

  //////////////////

  Matrix44<T> FromR;
  FromR.setRotation(from); // froms ROTATION
  Matrix44<T> ToR;
  ToR.setRotation(to);     // froms ROTATION

  Quaternion<T> FromQ;
  FromQ.fromMatrix(FromR);
  Quaternion<T> ToQ;
  ToQ.fromMatrix(ToR);

  Matrix44<T> CORR;
  CORR.correctionMatrix(from, to); // CORR.Normalize();

  Quaternion<T> Qidn;
  Quaternion<T> Qrot;
  Qrot.fromMatrix(CORR);

  // Vector4<T>  rawaxisang = Qrot.toAxisAngle();
  // T rawangle = rawaxisang.width();
  // T	newangle = rawangle*par;
  // Vector4<T> newaxisang = rawaxisang;
  // newaxisang.setW( newangle );
  // Tfquat newQrot;	newQrot.fromAxisAngle( newaxisang );

#if 1

  Quaternion<T> dQ = Qrot;
  dQ.subtract(Qidn);
  dQ.scale(par);
  dQ.add(Qidn);

  if (dQ.norm() > T(0))
    dQ.negate();

  Quaternion<T> newQrot = dQ;

#endif

  // Tfquat newQrot = FromQ.Slerp( ToQ, par );

  Matrix44<T> matR;
  matR = newQrot.toMatrix();
  // matR.Normalize();

  //////////////////

  *this = multiply_ltor(FromR, matR, matT);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool Matrix44<T>::decompose(Vector3<T>& pos, Quaternion<T>& qrot, T& Scale) const {

  if (0) {
    using vec3_t = typename Vector3<T>::base_t;
    using vec4_t = typename Vector4<T>::base_t;
    using quat_t = typename Quaternion<T>::base_t;

    vec3_t scale, trans, skew;
    vec4_t perspective;
    quat_t orientation;

    bool OK = glm::decompose(asGlmMat4(), scale, orientation, trans, skew, perspective);

    if (OK) {
      pos   = trans;
      qrot  = orientation;
      Scale = scale.x;
    }
    return OK;
  } else {
    pos = translation();

    Matrix44<T> rot = *this;

    T zero = T(0);
    T one  = T(1);

    rot.setRow(3, zero, zero, zero, one);    // set bottom row to 0,0,0,1
    rot.setColumn(3, zero, zero, zero, one); // set right column to 0,0,0,1

    Vector4<T> UnitVectorX(one, zero, zero, one);
    Vector4<T> XFVectorX = UnitVectorX.transform(rot);
    Vector4<T> UnitVectorY(zero, one, zero, one);
    Vector4<T> XFVectorY = UnitVectorY.transform(rot);
    Vector4<T> UnitVectorZ(zero, zero, one, one);
    Vector4<T> XFVectorZ = UnitVectorZ.transform(rot);

    T magx = XFVectorX.magnitude();
    T magy = XFVectorY.magnitude();
    T magz = XFVectorZ.magnitude();

    T scale = T(0);
    if (magx > scale)
      scale = magx;
    if (magy > scale)
      scale = magy;
    if (magz > scale)
      scale = magz;

    Scale = scale;

    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        rot.setElemXY(i, j, rot.elemXY(i, j) / Scale);
      }
    }

    qrot.fromMatrix(rot);
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::compose(const Vector3<T>& pos, const Quaternion<T>& qrot, const Vector3<T>& scale) {
  compose(pos, qrot, scale.x, scale.y, scale.z);
}
template <typename T> void Matrix44<T>::compose(const Vector3<T>& pos, const Quaternion<T>& qrot, const T& scale) {
  compose(pos, qrot, scale, scale, scale);
}
template <typename T> void Matrix44<T>::compose2(const Vector3<T>& pos, const Quaternion<T>& qrot, const T& scale) {
  compose2(pos, qrot, scale, scale, scale);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void Matrix44<T>::compose(const Vector3<T>& pos, const Quaternion<T>& qrot, const T& scalex, const T& scaley, const T& scalez) {

  T one = T(1);
  T two = T(2);

  T l = qrot.norm();

  // should this be T::Epsilon() ?
  T s = (fabs(l) < T(EPSILON)) ? one : (two / l);

  T xs = qrot.x * s;
  T ys = qrot.y * s;
  T zs = qrot.z * s;

  T wx = qrot.w * xs;
  T wy = qrot.w * ys;
  T wz = qrot.w * zs;

  T xx = qrot.x * xs;
  T xy = qrot.x * ys;
  T xz = qrot.x * zs;

  T yy = qrot.y * ys;
  T yz = qrot.y * zs;
  T zz = qrot.z * zs;

  setColumn(0, scalex * (one - (yy + zz)), scalex * (xy - wz), scalex * (xz + wy), 0);

  setColumn(1, scaley * (xy + wz), scaley * (one - (xx + zz)), scaley * (yz - wx), 0);

  setColumn(2, scalez * (xz - wy), scalez * (yz + wx), scalez * (one - (xx + yy)), 0);

  setColumn(3, pos.x, pos.y, pos.z, 1);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void Matrix44<T>::compose(const Vector3<T>& pos, const Quaternion<T>& qrot ) {

  T one = T(1);
  T two = T(2);

  T l = qrot.norm();

  // should this be T::Epsilon() ?
  T s = (two / l);

  T xs = qrot.x * s;
  T ys = qrot.y * s;
  T zs = qrot.z * s;

  T wx = qrot.w * xs;
  T wy = qrot.w * ys;
  T wz = qrot.w * zs;

  T xx = qrot.x * xs;
  T xy = qrot.x * ys;
  T xz = qrot.x * zs;

  T yy = qrot.y * ys;
  T yz = qrot.y * zs;
  T zz = qrot.z * zs;

  setColumn(0, (one - (yy + zz)), (xy - wz), (xz + wy), 0);

  setColumn(1, (xy + wz), (one - (xx + zz)), (yz - wx), 0);

  setColumn(2, (xz - wy), (yz + wx), (one - (xx + yy)), 0);

  setColumn(3, pos.x, pos.y, pos.z, 1);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void Matrix44<T>::compose2(const Vector3<T>& pos, const Quaternion<T>& qrot, const T& scalex, const T& scaley, const T& scalez) {
  Matrix44<T> matT, matR, matS;

  matT.setTranslation(pos);
  matS.setScale(scalex, scaley, scalez);
  matR = qrot.toMatrix();

  auto mtxout = multiply_ltor(matR, matS, matT);
  *this       = Matrix44<T>(mtxout);
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector4<T> Matrix44<T>::row(int irow) const {
  Vector4<T> out;
  out.x = elemXY(0, irow);
  out.y = elemXY(1, irow);
  out.z = elemXY(2, irow);
  out.w = elemXY(3, irow);
  return out;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector4<T> Matrix44<T>::column(int icol) const {
  Vector4<T> out;
  out.x = elemXY(icol, 0);
  out.y = elemXY(icol, 1);
  out.z = elemXY(icol, 2);
  out.w = elemXY(icol, 3);
  return out;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::setRow(int irow, const Vector4<T>& v) {
  setElemXY(0, irow, v.x);
  setElemXY(1, irow, v.y);
  setElemXY(2, irow, v.z);
  setElemXY(3, irow, v.w);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::setColumn(int icol, const Vector4<T>& v) {
  setElemXY(icol, 0, v.x);
  setElemXY(icol, 1, v.y);
  setElemXY(icol, 2, v.z);
  setElemXY(icol, 3, v.w);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::setRow(int irow, float a, float b, float c, float d) {
  setElemXY(0, irow, a);
  setElemXY(1, irow, b);
  setElemXY(2, irow, c);
  setElemXY(3, irow, d);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::setColumn(int icol, float a, float b, float c, float d) {
  setElemXY(icol, 0, a);
  setElemXY(icol, 1, b);
  setElemXY(icol, 2, c);
  setElemXY(icol, 3, d);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::fromNormalVectors(const Vector3<T>& xv, const Vector3<T>& yv, const Vector3<T>& zv) {
  setRow(0, Vector4<T>(xv, T(0)));
  setRow(1, Vector4<T>(yv, T(0)));
  setRow(2, Vector4<T>(zv, T(0)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::toNormalVectors(Vector3<T>& xv, Vector3<T>& yv, Vector3<T>& zv) const {
  xv = row(0).xyz().normalized();
  yv = row(1).xyz().normalized();
  zv = row(2).xyz().normalized();
}

///////////////////////////////////////////////////////////////////////////////
// miniork IDEAL frustum matrix ( use the gfxtarget for device specific projection matrices)
// this will project a point into a clip space box ranged from -1..1 on x/y and 0..1 on z
///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::frustum(T left, T right, T top, T bottom, T zn, T zf) {

  setToIdentity();

  float width  = right - left;
  float height = top - bottom;
  float depth  = (zf - zn);

  /////////////////////////////////////////////

  setElemYX(0, 0, float(2.0f * zn) / -width);
  setElemYX(1, 1, float(2.0f * zn) / height);
  setElemYX(2, 2, float(zf) / depth);
  setElemYX(3, 3, float(0.0f));

  setElemYX(2, 3, float(zn * zf) / float(zn - zf));
  setElemYX(3, 2, float(1.0f));
}

///////////////////////////////////////////////////////////////////////////////
// miniork IDEAL projection matrix ( use the gfxtarget for device specific projection matrices)
// this will project a point into a clip space box ranged from -1..1 on x/y and 0..1 on z
///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::perspective(T fovy, T aspect, T fnear, T ffar) {
  OrkAssert(fnear >= 0.0f);
  OrkAssert(ffar > fnear);

  Matrix44<T> out;
  out   = glm::perspectiveRH(fovy, aspect, fnear, ffar);
  *this = out;

  // auto b = dump4x3(fvec3(1,0,1));

  // printf( "c<%s>\n", a.c_str() );
  // printf( "d<%s>\n", b.c_str() );
}
///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::lookAt(const Vector3<T>& Eye, const Vector3<T>& Ctr, const Vector3<T>& Up) {
  setToIdentity();

  Matrix44<T> out;
  out   = glm::lookAtRH(Eye, Ctr, Up);
  *this = out;

  // auto b = dump4x3(fvec3(1,1,0));
  // printf( "a<%s>\n", a.c_str() );
  // printf( "b<%s>\n", b.c_str() );
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::lookAt(T eyex, T eyey, T eyez, T centerx, T centery, T centerz, T upx, T upy, T upz) {
  Vector3<T> Ctr(centerx, centery, centerz);
  Vector3<T> Eye(eyex, eyey, eyez);
  Vector3<T> Up(upx, upy, upz);
  lookAt(Eye, Ctr, Up);
}

///////////////////////////////////////////////////////////////////////////////
// abstract ortho (all axis -1 .. 1 )
// if you want device specific see the gfxtarget

template <typename T> void Matrix44<T>::ortho(T left, T right, T top, T bottom, T fnear, T ffar) {
  /*T invWidth  = T(1) / (right - left);
  T invHeight = T(1) / (top - bottom);
  T invDepth  = T(1) / (ffar - fnear);
  T fScaleX   = T(2) * invWidth;
  T fScaleY   = T(2) * invHeight;
  T fScaleZ   = T(-2.0f) * invDepth;
  T TransX    = -(right + left) * invWidth;
  T TransY    = -(top + bottom) * invHeight;
  T TransZ    = -(ffar + fnear) * invDepth;

  T zero = T(0);
  T one = T(1);

  setColumn(0, fScaleX, zero,    zero,    zero);
  setColumn(1, zero,    fScaleY, zero,    zero);
  setColumn(2, zero,    zero,    fScaleZ, zero);
  setColumn(0, TransX,  TransY,  TransZ,  one);*/
  Matrix44<T> out;
  out   = glm::ortho(left, right, top, bottom);
  *this = out;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
bool Matrix44<T>::unProject(
    const Vector4<T>& rVWin,  //
    const Matrix44<T>& rIMVP, //
    const SRect& rVP,         //
    Vector3<T>& rVObj) {      //
  T in[4];
  T _z  = rVWin.z;
  in[0] = (rVWin.x - T(rVP.miX)) * T(2) / T(rVP.miW) - T(1);
  in[1] = (T(rVP.miH) - rVWin.y - T(rVP.miY)) * T(2) / T(rVP.miH) - T(1);
  in[2] = _z;
  in[3] = T(1);
  Vector4<T> rVDev(in[0], in[1], in[2], in[3]);
  Vector4<T> rval = rVDev.transform(rIMVP);
  rval.perspectiveDivideInPlace();
  rVObj = rval.xyz();
  return true;
}
template <typename T>
bool Matrix44<T>::unProject(
    const Matrix44<T>& rIMVP,    //
    const Vector3<T>& ClipCoord, //
    Vector3<T>& rVObj) {         //
  Vector4<T> rval = ClipCoord.transform(rIMVP);
  rval.perspectiveDivideInPlace();
  rVObj = rval.xyz();
  return true;
}
///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::transpose() {
  Matrix44<T> temp = *this;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      setElemYX(i, j, temp.elemYX(j, i));
    }
  }
}
template <typename T> Matrix44<T> Matrix44<T>::transposed() const {
  Matrix44<T> temp = *this;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      temp.setElemYX(i, j, elemYX(j, i));
    }
  }
  return temp;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::normalizeInPlace() {
  Matrix44<T> result;

  T Xx = elemXY(0, 0);
  T Xy = elemXY(0, 1);
  T Xz = elemXY(0, 2);
  T Yx = elemXY(1, 0);
  T Yy = elemXY(1, 1);
  T Yz = elemXY(1, 2);
  T Zx = elemXY(2, 0);
  T Zy = elemXY(2, 1);
  T Zz = elemXY(2, 2);

  T Xi = T(1) / sqrtf((Xx * Xx) + (Xy * Xy) + (Xz * Xz));
  T Yi = T(1) / sqrtf((Yx * Yx) + (Yy * Yy) + (Yz * Yz));
  T Zi = T(1) / sqrtf((Zx * Zx) + (Zy * Zy) + (Zz * Zz));

  Xx *= Xi;
  Xy *= Xi;
  Xz *= Xi;

  Yx *= Yi;
  Yy *= Yi;
  Yz *= Yi;

  Zx *= Zi;
  Zy *= Zi;
  Zz *= Zi;

  result.setColumn(0, Xx, Xy, Xz, 0);
  result.setColumn(1, Yx, Yy, Yz, 0);
  result.setColumn(2, Zx, Zy, Zz, 0);

  *this = result;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::inverseOf(const Matrix44<T>& in) {
  (*this) = in.inverse();
}

template <typename T> Matrix44<T> Matrix44<T>::inverse() const {
  Matrix44<T> out;
  out = glm::inverse(this->asGlmMat4());
  return out;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Matrix44<T> Matrix44<T>::composeFrom(const Vector3<T>& pos, const Quaternion<T>& rot, const Vector3<T>& scale) {
  Matrix44<T> rval;
  rval.compose(pos, rot, scale.x, scale.y, scale.z);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> T Matrix44<T>::determinant() const {
  return glm::determinant(this->asGlmMat4());
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> T Matrix44<T>::determinant3x3() const {
  Matrix33<T> m33;
  m33.setColumn(0, column(0).xyz());
  m33.setColumn(1, column(1).xyz());
  m33.setColumn(2, column(2).xyz());
  return glm::determinant(m33.asGlmMat3());
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////

namespace ork::reflect {
template <> //
inline void ::ork::reflect::ITyped<fmtx4>::serialize(serdes::node_ptr_t sernode) const {
  using namespace serdes;
  auto serializer        = sernode->_serializer;
  auto instance          = sernode->_ser_instance;
  auto arynode           = serializer->pushNode(_name, serdes::NodeType::ARRAY);
  arynode->_parent       = sernode;
  arynode->_ser_instance = instance;
  fmtx4 value;
  get(value, instance);
  for (int i = 0; i < 16; i++)
    serializeArraySubLeaf(arynode, value.asArray()[i], i);
  serializer->popNode(); // pop arraynode
}
template <>              //
inline void ::ork::reflect::ITyped<fmtx4>::deserialize(serdes::node_ptr_t arynode) const {
  using namespace serdes;
  auto deserializer  = arynode->_deserializer;
  auto instance      = arynode->_deser_instance;
  size_t numelements = arynode->_numchildren;
  OrkAssert(numelements == 16);

  fmtx4 value;
  for (int i = 0; i < 16; i++)
    value.asArray()[i] = deserializeArraySubLeaf<float>(arynode, i);

  set(value, instance);
}

} // namespace ork::reflect
