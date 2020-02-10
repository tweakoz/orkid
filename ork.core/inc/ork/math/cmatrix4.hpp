////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <cmath>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/cmatrix3.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/string/deco.inl>

#if defined(_WIN32) && !defined(_XBOX)
#include <pmmintrin.h>
#endif

namespace ork {

template <typename T> const Matrix44<T> Matrix44<T>::Identity;

///////////////////////////////////////////////////////////////////////////////

template <typename T> Matrix44<T>::Matrix44(const Quaternion<T>& q) {
  this->FromQuaternion(q);
}

template <typename T> Matrix33<T> Matrix44<T>::rotMatrix33(void) const {
  Matrix33<T> rval;
  rval.fromNormalVectors(GetXNormal().Normal(), GetYNormal().Normal(), GetZNormal().Normal());
  return rval;
}
template <typename T> Matrix44<T> Matrix44<T>::rotMatrix44(void) const {
  Matrix44<T> rval;
  rval.fromNormalVectors(GetXNormal().Normal(), GetYNormal().Normal(), GetZNormal().Normal());
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::SetToIdentity(void) {
  /*	xbox wasn't optimizing this.
      int	j,k;

      for (j=0;j<4;j++)
      {
          for (k=0;k<4;k++)
          {
              elements[j][k] = T( (j==k) );
          }
      }*/
  elements[0][0] = T(1);
  elements[0][1] = T(0);
  elements[0][2] = T(0);
  elements[0][3] = T(0);
  elements[1][0] = T(0);
  elements[1][1] = T(1);
  elements[1][2] = T(0);
  elements[1][3] = T(0);
  elements[2][0] = T(0);
  elements[2][1] = T(0);
  elements[2][2] = T(1);
  elements[2][3] = T(0);
  elements[3][0] = T(0);
  elements[3][1] = T(0);
  elements[3][2] = T(0);
  elements[3][3] = T(1);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::dump(std::string name) const {
  orkprintf("Matrix %p %s\n{	", this, name.c_str());

  for (int i = 0; i < 4; i++) {

    for (int j = 0; j < 4; j++) {
      orkprintf("%f ", elements[i][j]);
    }

    orkprintf("\n	");
  }

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
      float elem = elements[i][j];
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
      float elem = elements[i][j];
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
    auto rot = q.toEuler();
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
template <typename T> std::string Matrix44<T>::dump4x3cn() const {
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
      float elem = elements[i][j];
      elem       = float(int(elem * 10000)) / 10000.0f;
      //////////////////////////////////
      rval += ork::deco::format(fcol, " %+0.4g ", elem);
    }
    rval += ork::deco::decorate(brace_color, "]");
  }
  Quaternion<T> q(*this);
  auto rot = q.toEuler();
  rval +=
      ork::deco::format(fvec3(0.8, 0.8, 0.8), "  euler<%g %g %g>", round(rot.x * RTOD), round(rot.y * RTOD), round(rot.z * RTOD));
  return rval;
}
template <typename T> std::string Matrix44<T>::dump() const {
  std::string rval;
  for (int i = 0; i < 4; i++) {
    rval += "[";
    for (int j = 0; j < 4; j++) {
      rval += FormatString(" %+0.4g ", elements[i][j]);
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
      rval += FormatString(" %+0.4g ", elements[i][j]);
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
///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::SetElemYX(int ix, int iy, T val) {
  elements[iy][ix] = val;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> T Matrix44<T>::GetElemYX(int ix, int iy) const {
  return elements[iy][ix];
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::SetElemXY(int ix, int iy, T val) {
  elements[ix][iy] = val;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> T Matrix44<T>::GetElemXY(int ix, int iy) const {
  return elements[ix][iy];
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::Translate(const Vector4<T>& vec) {
  Matrix44<T> temp, res;
  temp.SetTranslation(vec);
  res   = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::Translate(T vx, T vy, T vz) {
  Matrix44<T> temp, res;
  temp.SetTranslation(vx, vy, vz);
  res   = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::SetTranslation(const Vector3<T>& vec) {
  SetColumn(3, Vector4<T>(vec, 1.0f));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T> Matrix44<T>::GetTranslation(void) const {
  return GetColumn(3).xyz();
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::SetTranslation(T _x, T _y, T _z) {
  SetTranslation(Vector3<T>(_x, _y, _z));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//	set rotation explicitly

template <typename T> void Matrix44<T>::SetRotateX(T rad) {
  T cosa, sina;

  cosa = cosf(rad);
  sina = sinf(rad);

  elements[0][0] = 1.0f;
  elements[0][1] = 0.0f;
  elements[0][2] = 0.0f;

  elements[1][0] = 0.0f;
  elements[1][1] = cosa;
  elements[1][2] = sina;

  elements[2][0] = 0.0f;
  elements[2][1] = -sina;
  elements[2][2] = cosa;
}

///////////////////////////////////////////////////////////////////////////////
//	set rotation explicitly

template <typename T> void Matrix44<T>::SetRotateY(T rad) {
  T cosa, sina;

  cosa = cosf(rad);
  sina = sinf(rad);

  elements[0][0] = cosa;
  elements[0][1] = 0.0f;
  elements[0][2] = -sina;

  elements[1][0] = 0.0f;
  elements[1][1] = 1.0f;
  elements[1][2] = 0.0f;

  elements[2][0] = sina;
  elements[2][1] = 0.0f;
  elements[2][2] = cosa;
}

///////////////////////////////////////////////////////////////////////////////
//	set rotation explicitly

template <typename T> void Matrix44<T>::SetRotateZ(T rad) {
  T cosa, sina;

  cosa = cosf(rad);
  sina = sinf(rad);

  elements[0][0] = cosa;
  elements[0][1] = sina;
  elements[0][2] = 0.0f;

  elements[1][0] = -sina;
  elements[1][1] = cosa;
  elements[1][2] = 0.0f;

  elements[2][0] = 0.0f;
  elements[2][1] = 0.0f;
  elements[2][2] = 1.0f;
}

///////////////////////////////////////////////////////////////////////////////
//	rotate in place

template <typename T> void Matrix44<T>::RotateX(T rad) {
  Matrix44<T> temp, res;
  temp.SetRotateX(rad);
  res   = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////
//	rotate in place

template <typename T> void Matrix44<T>::RotateY(T rad) {
  Matrix44<T> temp, res;
  temp.SetRotateY(rad);
  res   = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////
//	rotate in place

template <typename T> void Matrix44<T>::RotateZ(T rad) {
  Matrix44<T> temp, res;
  temp.SetRotateZ(rad);
  res   = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::SetScale(const Vector4<T>& vec) {
  SetScale(vec.GetX(), vec.GetY(), vec.GetZ());
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::SetScale(T x, T y, T z) {
  SetElemXY(0, 0, x);
  SetElemXY(1, 0, 0.0f);
  SetElemXY(2, 0, 0.0f);

  SetElemXY(0, 1, 0.0f);
  SetElemXY(1, 1, y);
  SetElemXY(2, 1, 0.0f);

  SetElemXY(0, 2, 0.0f);
  SetElemXY(1, 2, 0.0f);
  SetElemXY(2, 2, z);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::SetScale(T s) {
  SetScale(s, s, s);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::Scale(const Vector4<T>& vec) {
  Matrix44<T> temp, res;
  temp.SetScale(vec);
  res   = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::Scale(T xscl, T yscl, T zscl) {
  Matrix44<T> temp, res;
  temp.SetScale(xscl, yscl, zscl);
  res   = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// sm - rotation matrix from quaternion
template <typename T> void Matrix44<T>::FromQuaternion(Quaternion<T> quat) {
  T xx = quat.GetX() * quat.GetX();
  T yy = quat.GetY() * quat.GetY();
  T zz = quat.GetZ() * quat.GetZ();
  T xy = quat.GetX() * quat.GetY();
  T zw = quat.GetZ() * quat.GetW();
  T zx = quat.GetZ() * quat.GetX();
  T yw = quat.GetY() * quat.GetW();
  T yz = quat.GetY() * quat.GetZ();
  T xw = quat.GetX() * quat.GetW();

  elements[0][0] = T(1.0f) - (T(2.0f) * (yy + zz));
  elements[0][1] = T(2.0f) * (xy + zw);
  elements[0][2] = T(2.0f) * (zx - yw);
  elements[0][3] = T(0.0f);
  elements[1][0] = T(2.0f) * (xy - zw);
  elements[1][1] = T(1.0f) - (T(2.0f) * (zz + xx));
  elements[1][2] = T(2.0f) * (yz + xw);
  elements[1][3] = T(0.0f);
  elements[2][0] = T(2.0f) * (zx + yw);
  elements[2][1] = T(2.0f) * (yz - xw);
  elements[2][2] = T(1.0f) - (T(2.0f) * (yy + xx));
  elements[2][3] = T(0.0f);
  elements[3][0] = T(0.0f);
  elements[3][1] = T(0.0f);
  elements[3][2] = T(0.0f);
  elements[3][3] = T(1.0f);
}

///////////////////////////////////////////////////////////////////////////////
// sm - billboard matrix from object/view position

template <typename T> void Matrix44<T>::CreateBillboard(Vector3<T> objectPos, Vector3<T> viewPos, Vector3<T> upVec) {
  Vector3<T> dir;
  Vector3<T> res;
  Vector3<T> cross;

  dir.SetX(objectPos.GetX() - viewPos.GetX());
  dir.SetY(objectPos.GetY() - viewPos.GetY());
  dir.SetZ(objectPos.GetZ() - viewPos.GetZ());

  T slen = dir.MagSquared();
  dir    = dir * (T(1.0f) / sqrtf(slen));

  cross = upVec;
  cross = cross.Cross(dir);
  cross.Normalize();

  res = dir;
  res = res.Cross(cross);

  elements[0][0] = cross.GetX();
  elements[0][1] = cross.GetY();
  elements[0][2] = cross.GetZ();
  elements[0][3] = T(0.0f);
  elements[1][0] = res.GetX();
  elements[1][1] = res.GetY();
  elements[1][2] = res.GetZ();
  elements[1][3] = T(0.0f);
  elements[2][0] = dir.GetX();
  elements[2][1] = dir.GetY();
  elements[2][2] = dir.GetZ();
  elements[2][3] = T(0.0f);
  elements[3][0] = objectPos.GetX();
  elements[3][1] = objectPos.GetY();
  elements[3][2] = objectPos.GetZ();
  elements[3][3] = T(1.0f);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> void fpuM44xM44(const T a[4][4], const T b[4][4], T c[4][4]) {
  const T* fa = &a[0][0];
  const T* fb = &b[0][0];
  T* fc       = &c[0][0];

  //    y  x
  //    i  j      i  k    k  j      i  k    k  j      i  k    k  j      i  k    k  j

  fc[0] = fa[0] * fb[0] + fa[1] * fb[4] + fa[2] * fb[8] + fa[3] * fb[12];
  fc[1] = fa[0] * fb[1] + fa[1] * fb[5] + fa[2] * fb[9] + fa[3] * fb[13];
  fc[2] = fa[0] * fb[2] + fa[1] * fb[6] + fa[2] * fb[10] + fa[3] * fb[14];
  fc[3] = fa[0] * fb[3] + fa[1] * fb[7] + fa[2] * fb[11] + fa[3] * fb[15];

  fc[4] = fa[4] * fb[0] + fa[5] * fb[4] + fa[6] * fb[8] + fa[7] * fb[12];
  fc[5] = fa[4] * fb[1] + fa[5] * fb[5] + fa[6] * fb[9] + fa[7] * fb[13];
  fc[6] = fa[4] * fb[2] + fa[5] * fb[6] + fa[6] * fb[10] + fa[7] * fb[14];
  fc[7] = fa[4] * fb[3] + fa[5] * fb[7] + fa[6] * fb[11] + fa[7] * fb[15];

  fc[8]  = fa[8] * fb[0] + fa[9] * fb[4] + fa[10] * fb[8] + fa[11] * fb[12];
  fc[9]  = fa[8] * fb[1] + fa[9] * fb[5] + fa[10] * fb[9] + fa[11] * fb[13];
  fc[10] = fa[8] * fb[2] + fa[9] * fb[6] + fa[10] * fb[10] + fa[11] * fb[14];
  fc[11] = fa[8] * fb[3] + fa[9] * fb[7] + fa[10] * fb[11] + fa[11] * fb[15];

  fc[12] = fa[12] * fb[0] + fa[13] * fb[4] + fa[14] * fb[8] + fa[15] * fb[12];
  fc[13] = fa[12] * fb[1] + fa[13] * fb[5] + fa[14] * fb[9] + fa[15] * fb[13];
  fc[14] = fa[12] * fb[2] + fa[13] * fb[6] + fa[14] * fb[10] + fa[15] * fb[14];
  fc[15] = fa[12] * fb[3] + fa[13] * fb[7] + fa[14] * fb[11] + fa[15] * fb[15];
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Matrix44<T> Matrix44<T>::MatrixMult(const Matrix44<T>& mat1) const {
  Matrix44<T> result;

  ////////////////////////////////////////////////////////////////
  //              i  j                i  k                  k  j

  result.elements[0][0] = (elements[0][0] * mat1.elements[0][0]) + (elements[0][1] * mat1.elements[1][0]) +
                          (elements[0][2] * mat1.elements[2][0]) + (elements[0][3] * mat1.elements[3][0]);

  result.elements[0][1] = (elements[0][0] * mat1.elements[0][1]) + (elements[0][1] * mat1.elements[1][1]) +
                          (elements[0][2] * mat1.elements[2][1]) + (elements[0][3] * mat1.elements[3][1]);

  result.elements[0][2] = (elements[0][0] * mat1.elements[0][2]) + (elements[0][1] * mat1.elements[1][2]) +
                          (elements[0][2] * mat1.elements[2][2]) + (elements[0][3] * mat1.elements[3][2]);

  result.elements[0][3] = (elements[0][0] * mat1.elements[0][3]) + (elements[0][1] * mat1.elements[1][3]) +
                          (elements[0][2] * mat1.elements[2][3]) + (elements[0][3] * mat1.elements[3][3]);

  ////////////////////////////////////////////////////////////////
  //              i  j                i  k                  k  j

  result.elements[1][0] = (elements[1][0] * mat1.elements[0][0]) + (elements[1][1] * mat1.elements[1][0]) +
                          (elements[1][2] * mat1.elements[2][0]) + (elements[1][3] * mat1.elements[3][0]);

  result.elements[1][1] = (elements[1][0] * mat1.elements[0][1]) + (elements[1][1] * mat1.elements[1][1]) +
                          (elements[1][2] * mat1.elements[2][1]) + (elements[1][3] * mat1.elements[3][1]);

  result.elements[1][2] = (elements[1][0] * mat1.elements[0][2]) + (elements[1][1] * mat1.elements[1][2]) +
                          (elements[1][2] * mat1.elements[2][2]) + (elements[1][3] * mat1.elements[3][2]);

  result.elements[1][3] = (elements[1][0] * mat1.elements[0][3]) + (elements[1][1] * mat1.elements[1][3]) +
                          (elements[1][2] * mat1.elements[2][3]) + (elements[1][3] * mat1.elements[3][3]);
  ////////////////////////////////////////////////////////////////
  //              i  j                i  k                  k  j

  result.elements[2][0] = (elements[2][0] * mat1.elements[0][0]) + (elements[2][1] * mat1.elements[1][0]) +
                          (elements[2][2] * mat1.elements[2][0]) + (elements[2][3] * mat1.elements[3][0]);

  result.elements[2][1] = (elements[2][0] * mat1.elements[0][1]) + (elements[2][1] * mat1.elements[1][1]) +
                          (elements[2][2] * mat1.elements[2][1]) + (elements[2][3] * mat1.elements[3][1]);

  result.elements[2][2] = (elements[2][0] * mat1.elements[0][2]) + (elements[2][1] * mat1.elements[1][2]) +
                          (elements[2][2] * mat1.elements[2][2]) + (elements[2][3] * mat1.elements[3][2]);

  result.elements[2][3] = (elements[2][0] * mat1.elements[0][3]) + (elements[2][1] * mat1.elements[1][3]) +
                          (elements[2][2] * mat1.elements[2][3]) + (elements[2][3] * mat1.elements[3][3]);

  ////////////////////////////////////////////////////////////////
  //              i  j                i  k                  k  j

  result.elements[3][0] = (elements[3][0] * mat1.elements[0][0]) + (elements[3][1] * mat1.elements[1][0]) +
                          (elements[3][2] * mat1.elements[2][0]) + (elements[3][3] * mat1.elements[3][0]);

  result.elements[3][1] = (elements[3][0] * mat1.elements[0][1]) + (elements[3][1] * mat1.elements[1][1]) +
                          (elements[3][2] * mat1.elements[2][1]) + (elements[3][3] * mat1.elements[3][1]);

  result.elements[3][2] = (elements[3][0] * mat1.elements[0][2]) + (elements[3][1] * mat1.elements[1][2]) +
                          (elements[3][2] * mat1.elements[2][2]) + (elements[3][3] * mat1.elements[3][2]);

  result.elements[3][3] = (elements[3][0] * mat1.elements[0][3]) + (elements[3][1] * mat1.elements[1][3]) +
                          (elements[3][2] * mat1.elements[2][3]) + (elements[3][3] * mat1.elements[3][3]);

  ////////////////////////////////////////////////////////////////

  return (result);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Matrix44<T> Matrix44<T>::Mult(T scalar) const {
  Matrix44<T> res;

  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      res.elements[i][j] = elements[i][j] * scalar;
    }
  }

  return res;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Matrix44<T> Matrix44<T>::Concat43(const Matrix44<T>& mat1) const {
  Matrix44<T> result;

  ////////////////////////////////////////////////////////////////
  //              i  j                i  k                  k  j

  result.elements[0][0] =
      (elements[0][0] * mat1.elements[0][0]) + (elements[0][1] * mat1.elements[1][0]) + (elements[0][2] * mat1.elements[2][0]);

  result.elements[0][1] =
      (elements[0][0] * mat1.elements[0][1]) + (elements[0][1] * mat1.elements[1][1]) + (elements[0][2] * mat1.elements[2][1]);

  result.elements[0][2] =
      (elements[0][0] * mat1.elements[0][2]) + (elements[0][1] * mat1.elements[1][2]) + (elements[0][2] * mat1.elements[2][2]);

  ////////////////////////////////////////////////////////////////
  //              i  j                i  k                  k  j

  result.elements[1][0] =
      (elements[1][0] * mat1.elements[0][0]) + (elements[1][1] * mat1.elements[1][0]) + (elements[1][2] * mat1.elements[2][0]);

  result.elements[1][1] =
      (elements[1][0] * mat1.elements[0][1]) + (elements[1][1] * mat1.elements[1][1]) + (elements[1][2] * mat1.elements[2][1]);

  result.elements[1][2] =
      (elements[1][0] * mat1.elements[0][2]) + (elements[1][1] * mat1.elements[1][2]) + (elements[1][2] * mat1.elements[2][2]);

  ////////////////////////////////////////////////////////////////
  //              i  j                i  k                  k  j

  result.elements[2][0] =
      (elements[2][0] * mat1.elements[0][0]) + (elements[2][1] * mat1.elements[1][0]) + (elements[2][2] * mat1.elements[2][0]);

  result.elements[2][1] =
      (elements[2][0] * mat1.elements[0][1]) + (elements[2][1] * mat1.elements[1][1]) + (elements[2][2] * mat1.elements[2][1]);

  result.elements[2][2] =
      (elements[2][0] * mat1.elements[0][2]) + (elements[2][1] * mat1.elements[1][2]) + (elements[2][2] * mat1.elements[2][2]);

  ////////////////////////////////////////////////////////////////
  //              i  j                i  k                  k  j

  result.elements[3][0] = (elements[3][0] * mat1.elements[0][0]) + (elements[3][1] * mat1.elements[1][0]) +
                          (elements[3][2] * mat1.elements[2][0]) + mat1.elements[3][0];

  result.elements[3][1] = (elements[3][0] * mat1.elements[0][1]) + (elements[3][1] * mat1.elements[1][1]) +
                          (elements[3][2] * mat1.elements[2][1]) + mat1.elements[3][1];

  result.elements[3][2] = (elements[3][0] * mat1.elements[0][2]) + (elements[3][1] * mat1.elements[1][2]) +
                          (elements[3][2] * mat1.elements[2][2]) + mat1.elements[3][2];

  ////////////////////////////////////////////////////////////////

  return (result);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Matrix44<T> Matrix44<T>::Concat43Transpose(const Matrix44<T>& mat1) const {
  Matrix44<T> result;

  ////////////////////////////////////////////////////////////////
  //              i  j                i  k                  k  j

  result.elements[0][0] =
      (elements[0][0] * mat1.elements[0][0]) + (elements[0][1] * mat1.elements[1][0]) + (elements[0][2] * mat1.elements[2][0]);

  result.elements[1][0] =
      (elements[0][0] * mat1.elements[0][1]) + (elements[0][1] * mat1.elements[1][1]) + (elements[0][2] * mat1.elements[2][1]);

  result.elements[2][0] =
      (elements[0][0] * mat1.elements[0][2]) + (elements[0][1] * mat1.elements[1][2]) + (elements[0][2] * mat1.elements[2][2]);

  ////////////////////////////////////////////////////////////////
  //              i  j                i  k                  k  j

  result.elements[0][1] =
      (elements[1][0] * mat1.elements[0][0]) + (elements[1][1] * mat1.elements[1][0]) + (elements[1][2] * mat1.elements[2][0]);

  result.elements[1][1] =
      (elements[1][0] * mat1.elements[0][1]) + (elements[1][1] * mat1.elements[1][1]) + (elements[1][2] * mat1.elements[2][1]);

  result.elements[2][1] =
      (elements[1][0] * mat1.elements[0][2]) + (elements[1][1] * mat1.elements[1][2]) + (elements[1][2] * mat1.elements[2][2]);

  ////////////////////////////////////////////////////////////////
  //              i  j                i  k                  k  j

  result.elements[0][2] =
      (elements[2][0] * mat1.elements[0][0]) + (elements[2][1] * mat1.elements[1][0]) + (elements[2][2] * mat1.elements[2][0]);

  result.elements[1][2] =
      (elements[2][0] * mat1.elements[0][1]) + (elements[2][1] * mat1.elements[1][1]) + (elements[2][2] * mat1.elements[2][1]);

  result.elements[2][2] =
      (elements[2][0] * mat1.elements[0][2]) + (elements[2][1] * mat1.elements[1][2]) + (elements[2][2] * mat1.elements[2][2]);

  ////////////////////////////////////////////////////////////////
  //              i  j                i  k                  k  j

  result.elements[0][3] = (elements[3][0] * mat1.elements[0][0]) + (elements[3][1] * mat1.elements[1][0]) +
                          (elements[3][2] * mat1.elements[2][0]) + mat1.elements[3][0];

  result.elements[1][3] = (elements[3][0] * mat1.elements[0][1]) + (elements[3][1] * mat1.elements[1][1]) +
                          (elements[3][2] * mat1.elements[2][1]) + mat1.elements[3][1];

  result.elements[2][3] = (elements[3][0] * mat1.elements[0][2]) + (elements[3][1] * mat1.elements[1][2]) +
                          (elements[3][2] * mat1.elements[2][2]) + mat1.elements[3][2];

  ////////////////////////////////////////////////////////////////

  return (result);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::CorrectionMatrix(const Matrix44<T>& from, const Matrix44<T>& to) {
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
  *this = (inv_from * to); // B
  this->dump();
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::SetRotation(const Matrix44<T>& from) {
  Matrix44<T> rval = from;
  rval.SetTranslation(T(0.0f), T(0.0f), T(0.0f));
  rval.Normalize();
  *this = rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::SetTranslation(const Matrix44<T>& from) {
  SetToIdentity();
  Vector4<T> t = from.GetTranslation();
  SetTranslation(t);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void Matrix44<T>::SetScale(const Matrix44<T>& from) // assumes rot is zero!
{
  Matrix44<T> RS = from;
  RS.SetTranslation(T(0.0f), T(0.0f), T(0.0f));

  Matrix44<T> R = RS;
  R.Normalize();

  Matrix44<T> S;
  S.CorrectionMatrix(R, RS);

  *this = S;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void Matrix44<T>::Lerp(const Matrix44<T>& from, const Matrix44<T>& to, T par) // par 0.0f .. 1.0f
{
  //////////////////

  Vector4<T> vF = from.GetTranslation();
  Vector4<T> vT = to.GetTranslation();
  Vector4<T> vT2;
  vT2.Lerp(vF, vT, par);
  Matrix44<T> matT;
  matT.SetTranslation(vT2);

  //////////////////

  Matrix44<T> FromR;
  FromR.SetRotation(from); // froms ROTATION
  Matrix44<T> ToR;
  ToR.SetRotation(to); // froms ROTATION

  Quaternion<T> FromQ;
  FromQ.FromMatrix(FromR);
  Quaternion<T> ToQ;
  ToQ.FromMatrix(ToR);

  Matrix44<T> CORR;
  CORR.CorrectionMatrix(from, to); // CORR.Normalize();

  Quaternion<T> Qidn;
  Quaternion<T> Qrot;
  Qrot.FromMatrix(CORR);

  // Vector4<T>  rawaxisang = Qrot.toAxisAngle();
  // T rawangle = rawaxisang.GetW();
  // T	newangle = rawangle*par;
  // Vector4<T> newaxisang = rawaxisang;
  // newaxisang.SetW( newangle );
  // Tfquat newQrot;	newQrot.fromAxisAngle( newaxisang );

#if 1

  Quaternion<T> dQ = Qrot;
  dQ.Sub(Qidn);
  dQ.Scale(par);
  dQ.Add(Qidn);

  if (dQ.Magnitude() > T(0.0f))
    dQ.Negate();

  Quaternion<T> newQrot = dQ;

#endif

  // Tfquat newQrot = FromQ.Slerp( ToQ, par );

  Matrix44<T> matR;
  matR = newQrot.ToMatrix();
  // matR.Normalize();

  //////////////////

  *this = (FromR * matR * matT);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::decompose(Vector3<T>& pos, Quaternion<T>& qrot, T& Scale) const {
  pos = GetTranslation();

  Matrix44<T> rot = *this;

  rot.SetElemYX(3, 0, T(0.0f));
  rot.SetElemYX(3, 1, T(0.0f));
  rot.SetElemYX(3, 2, T(0.0f));
  rot.SetElemYX(0, 3, T(0.0f));
  rot.SetElemYX(1, 3, T(0.0f));
  rot.SetElemYX(2, 3, T(0.0f));

  rot.SetElemYX(3, 3, T(1.0f));

  Vector4<T> UnitVector(T(1.0f), T(0.0f), T(0.0f), T(1.0f));
  Vector4<T> XFVector = UnitVector.Transform(rot);

  Scale = XFVector.Mag();

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      rot.SetElemYX(i, j, rot.GetElemYX(i, j) / Scale);
    }
  }

  qrot.FromMatrix(rot);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::compose(const Vector3<T>& pos, const Quaternion<T>& qrot, const T& Scale) {
  *this = qrot.ToMatrix();
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      SetElemYX(i, j, GetElemYX(i, j) * Scale);
    }
  }
  SetTranslation(pos.GetX(), pos.GetY(), pos.GetZ());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector4<T> Matrix44<T>::GetRow(int irow) const {
  Vector4<T> out;
  out.SetX(GetElemXY(0, irow));
  out.SetY(GetElemXY(1, irow));
  out.SetZ(GetElemXY(2, irow));
  out.SetW(GetElemXY(3, irow));
  return out;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector4<T> Matrix44<T>::GetColumn(int icol) const {
  Vector4<T> out;
  out.SetX(GetElemXY(icol, 0));
  out.SetY(GetElemXY(icol, 1));
  out.SetZ(GetElemXY(icol, 2));
  out.SetW(GetElemXY(icol, 3));
  return out;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::SetRow(int irow, const Vector4<T>& v) {
  SetElemXY(0, irow, v.GetX());
  SetElemXY(1, irow, v.GetY());
  SetElemXY(2, irow, v.GetZ());
  SetElemXY(3, irow, v.GetW());
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::SetColumn(int icol, const Vector4<T>& v) {
  SetElemXY(icol, 0, v.GetX());
  SetElemXY(icol, 1, v.GetY());
  SetElemXY(icol, 2, v.GetZ());
  SetElemXY(icol, 3, v.GetW());
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::fromNormalVectors(const Vector3<T>& xv, const Vector3<T>& yv, const Vector3<T>& zv) {
  SetColumn(0, Vector4<T>(xv, T(0)));
  SetColumn(1, Vector4<T>(yv, T(0)));
  SetColumn(2, Vector4<T>(zv, T(0)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::toNormalVectors(Vector3<T>& xv, Vector3<T>& yv, Vector3<T>& zv) const {
  xv = GetColumn(0).xyz();
  yv = GetColumn(1).xyz();
  zv = GetColumn(2).xyz();
}

///////////////////////////////////////////////////////////////////////////////
// miniork IDEAL projection matrix ( use the gfxtarget for device specific projection matrices)
// this will project a point into a clip space box ranged from -1..1 on x/y and 0..1 on z
///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::Perspective(T fovy, T aspect, T fnear, T ffar) {
  OrkAssert(fnear >= 0.0f);
  OrkAssert(ffar > fnear);

  float xmin, xmax, ymin, ymax;
  ymax = fnear * tanf(fovy * DTOR * 0.5f);
  ymin = -ymax;
  xmin = ymin * aspect;
  xmax = ymax * aspect;

  Frustum(xmin, xmax, ymax, ymin, fnear, ffar);
}

///////////////////////////////////////////////////////////////////////////////
// miniork IDEAL frustum matrix ( use the gfxtarget for device specific projection matrices)
// this will project a point into a clip space box ranged from -1..1 on x/y and 0..1 on z
///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::Frustum(T left, T right, T top, T bottom, T zn, T zf) {

  SetToIdentity();

  float width  = right - left;
  float height = top - bottom;
  float depth  = (zf - zn);

  /////////////////////////////////////////////

  SetElemYX(0, 0, float(2.0f * zn) / -width);
  SetElemYX(1, 1, float(2.0f * zn) / height);
  SetElemYX(2, 2, float(zf) / depth);
  SetElemYX(3, 3, float(0.0f));

  SetElemYX(2, 3, float(zn * zf) / float(zn - zf));
  SetElemYX(3, 2, float(1.0f));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::LookAt(const Vector3<T>& Eye, const Vector3<T>& Ctr, const Vector3<T>& Up) {
  SetToIdentity();

  Vector3<T> zaxis = (Ctr - Eye).Normal();
  Vector3<T> xaxis = (Up.Cross(zaxis)).Normal();
  Vector3<T> yaxis = zaxis.Cross(xaxis);

  SetRow(0, Vector4<T>(xaxis, 0.0f));
  SetRow(1, Vector4<T>(yaxis, 0.0f));
  SetRow(2, Vector4<T>(zaxis, 0.0f));

  SetElemXY(3, 0, -xaxis.Dot(Eye));
  SetElemXY(3, 1, -yaxis.Dot(Eye));
  SetElemXY(3, 2, -zaxis.Dot(Eye));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::LookAt(T eyex, T eyey, T eyez, T centerx, T centery, T centerz, T upx, T upy, T upz) {
  Vector3<T> Ctr(centerx, centery, centerz);
  Vector3<T> Eye(eyex, eyey, eyez);
  Vector3<T> Up(upx, upy, upz);
  LookAt(Eye, Ctr, Up);
}

///////////////////////////////////////////////////////////////////////////////
// abstract ortho (all axis -1 .. 1 )
// if you want device specific see the gfxtarget

template <typename T> void Matrix44<T>::Ortho(T left, T right, T top, T bottom, T fnear, T ffar) {
  T invWidth  = T(1.0f) / (right - left);
  T invHeight = T(1.0f) / (top - bottom);
  T invDepth  = T(1.0f) / (ffar - fnear);
  T fScaleX   = T(2.0f) * invWidth;
  T fScaleY   = T(2.0f) * invHeight;
  T fScaleZ   = T(-2.0f) * invDepth;
  T TransX    = -(right + left) * invWidth;
  T TransY    = -(top + bottom) * invHeight;
  T TransZ    = -(ffar + fnear) * invDepth;

  SetElemYX(0, 0, fScaleX);
  SetElemYX(1, 0, T(0.0f));
  SetElemYX(2, 0, T(0.0f));
  SetElemYX(3, 0, T(0.0f));

  SetElemYX(0, 1, T(0.0f));
  SetElemYX(1, 1, fScaleY);
  SetElemYX(2, 1, T(0.0f));
  SetElemYX(3, 1, T(0.0f));

  SetElemYX(0, 2, T(0.0f));
  SetElemYX(1, 2, T(0.0f));
  SetElemYX(2, 2, fScaleZ);
  SetElemYX(3, 2, T(0.0f));

  SetElemYX(0, 3, TransX);
  SetElemYX(1, 3, TransY);
  SetElemYX(2, 3, TransZ);
  SetElemYX(3, 3, T(1.0f));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
bool Matrix44<T>::UnProject(const Vector4<T>& rVWin, const Matrix44<T>& rIMVP, const SRect& rVP, Vector3<T>& rVObj) {
  T in[4];
  T _z  = rVWin.GetZ();
  in[0] = (rVWin.GetX() - T(rVP.miX)) * T(2) / T(rVP.miW) - T(1.0f);
  in[1] = (T(rVP.miH) - rVWin.GetY() - T(rVP.miY)) * T(2) / T(rVP.miH) - T(1.0f);
  in[2] = _z;
  in[3] = T(1.0f);
  Vector4<T> rVDev(in[0], in[1], in[2], in[3]);
  Vector4<T> rval = rVDev.Transform(rIMVP);
  rval.PerspectiveDivide();
  rVObj = rval.xyz();
  return true;
}
template <typename T> bool Matrix44<T>::UnProject(const Matrix44<T>& rIMVP, const Vector3<T>& ClipCoord, Vector3<T>& rVObj) {
  Vector4<T> rval = ClipCoord.Transform(rIMVP);
  rval.PerspectiveDivide();
  rVObj = rval.xyz();
  return true;
}
///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::Transpose(void) {
  Matrix44<T> temp = *this;

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      SetElemYX(i, j, temp.GetElemYX(j, i));
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix44<T>::Normalize(void) {
  Matrix44<T> result;

  T Xx = GetElemXY(0, 0);
  T Xy = GetElemXY(0, 1);
  T Xz = GetElemXY(0, 2);
  T Yx = GetElemXY(1, 0);
  T Yy = GetElemXY(1, 1);
  T Yz = GetElemXY(1, 2);
  T Zx = GetElemXY(2, 0);
  T Zy = GetElemXY(2, 1);
  T Zz = GetElemXY(2, 2);

  T Xi = T(1.0f) / sqrtf((Xx * Xx) + (Xy * Xy) + (Xz * Xz));
  T Yi = T(1.0f) / sqrtf((Yx * Yx) + (Yy * Yy) + (Yz * Yz));
  T Zi = T(1.0f) / sqrtf((Zx * Zx) + (Zy * Zy) + (Zz * Zz));

  Xx *= Xi;
  Xy *= Xi;
  Xz *= Xi;

  Yx *= Yi;
  Yy *= Yi;
  Yz *= Yi;

  Zx *= Zi;
  Zy *= Zi;
  Zz *= Zi;

  result.SetElemXY(0, 0, Xx);
  result.SetElemXY(0, 1, Xy);
  result.SetElemXY(0, 2, Xz);

  result.SetElemXY(1, 0, Yx);
  result.SetElemXY(1, 1, Yy);
  result.SetElemXY(1, 2, Yz);

  result.SetElemXY(2, 0, Zx);
  result.SetElemXY(2, 1, Zy);
  result.SetElemXY(2, 2, Zz);

  *this = result;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
