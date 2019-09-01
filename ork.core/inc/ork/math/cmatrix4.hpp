////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <cmath>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>

#if defined(_WIN32) && !defined(_XBOX)
#include <pmmintrin.h>
#endif

namespace ork {

template <typename T> const TMatrix4<T> TMatrix4<T>::Identity;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::SetToIdentity(void) {
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

template <typename T> void TMatrix4<T>::dump(const char* name) const {
  orkprintf("Matrix %p %s\n{	", this, name);

  for (int i = 0; i < 4; i++) {

    for (int j = 0; j < 4; j++) {
      orkprintf("%f ", elements[i][j]);
    }

    orkprintf("\n	");
  }

  orkprintf("\n}\n");
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::SetElemYX(int ix, int iy, T val) { elements[iy][ix] = val; }

///////////////////////////////////////////////////////////////////////////////

template <typename T> T TMatrix4<T>::GetElemYX(int ix, int iy) const { return elements[iy][ix]; }

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::SetElemXY(int ix, int iy, T val) { elements[ix][iy] = val; }

///////////////////////////////////////////////////////////////////////////////

template <typename T> T TMatrix4<T>::GetElemXY(int ix, int iy) const { return elements[ix][iy]; }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::Translate(const TVector4<T>& vec) {
  TMatrix4<T> temp, res;
  temp.SetTranslation(vec);
  res = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::Translate(T vx, T vy, T vz) {
  TMatrix4<T> temp, res;
  temp.SetTranslation(vx, vy, vz);
  res = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::SetTranslation(const TVector3<T>& vec) { SetColumn(3, TVector4<T>(vec, 1.0f)); }

///////////////////////////////////////////////////////////////////////////////

template <typename T> TVector3<T> TMatrix4<T>::GetTranslation(void) const { return GetColumn(3).xyz(); }

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::SetTranslation(T _x, T _y, T _z) { SetTranslation(TVector3<T>(_x, _y, _z)); }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//	set rotation explicitly

template <typename T> void TMatrix4<T>::SetRotateX(T rad) {
  T cosa, sina;

  cosa = CFloat::Cos(rad);
  sina = CFloat::Sin(rad);

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

template <typename T> void TMatrix4<T>::SetRotateY(T rad) {
  T cosa, sina;

  cosa = CFloat::Cos(rad);
  sina = CFloat::Sin(rad);

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

template <typename T> void TMatrix4<T>::SetRotateZ(T rad) {
  T cosa, sina;

  cosa = CFloat::Cos(rad);
  sina = CFloat::Sin(rad);

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

template <typename T> void TMatrix4<T>::RotateX(T rad) {
  TMatrix4<T> temp, res;
  temp.SetRotateX(rad);
  res = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////
//	rotate in place

template <typename T> void TMatrix4<T>::RotateY(T rad) {
  TMatrix4<T> temp, res;
  temp.SetRotateY(rad);
  res = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////
//	rotate in place

template <typename T> void TMatrix4<T>::RotateZ(T rad) {
  TMatrix4<T> temp, res;
  temp.SetRotateZ(rad);
  res = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::SetScale(const TVector4<T>& vec) { SetScale(vec.GetX(), vec.GetY(), vec.GetZ()); }

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::SetScale(T x, T y, T z) {
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

template <typename T> void TMatrix4<T>::SetScale(T s) { SetScale(s, s, s); }

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::Scale(const TVector4<T>& vec) {
  TMatrix4<T> temp, res;
  temp.SetScale(vec);
  res = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::Scale(T xscl, T yscl, T zscl) {
  TMatrix4<T> temp, res;
  temp.SetScale(xscl, yscl, zscl);
  res = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// sm - rotation matrix from quaternion
template <typename T> void TMatrix4<T>::FromQuaternion(TQuaternion<T> quat) {
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

template <typename T> void TMatrix4<T>::CreateBillboard(TVector3<T> objectPos, TVector3<T> viewPos, TVector3<T> upVec) {
  TVector3<T> dir;
  TVector3<T> res;
  TVector3<T> cross;

  dir.SetX(objectPos.GetX() - viewPos.GetX());
  dir.SetY(objectPos.GetY() - viewPos.GetY());
  dir.SetZ(objectPos.GetZ() - viewPos.GetZ());

  T slen = dir.MagSquared();
  dir = dir * (T(1.0f) / CFloat::Sqrt(slen));

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
  T* fc = &c[0][0];

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

  fc[8] = fa[8] * fb[0] + fa[9] * fb[4] + fa[10] * fb[8] + fa[11] * fb[12];
  fc[9] = fa[8] * fb[1] + fa[9] * fb[5] + fa[10] * fb[9] + fa[11] * fb[13];
  fc[10] = fa[8] * fb[2] + fa[9] * fb[6] + fa[10] * fb[10] + fa[11] * fb[14];
  fc[11] = fa[8] * fb[3] + fa[9] * fb[7] + fa[10] * fb[11] + fa[11] * fb[15];

  fc[12] = fa[12] * fb[0] + fa[13] * fb[4] + fa[14] * fb[8] + fa[15] * fb[12];
  fc[13] = fa[12] * fb[1] + fa[13] * fb[5] + fa[14] * fb[9] + fa[15] * fb[13];
  fc[14] = fa[12] * fb[2] + fa[13] * fb[6] + fa[14] * fb[10] + fa[15] * fb[14];
  fc[15] = fa[12] * fb[3] + fa[13] * fb[7] + fa[14] * fb[11] + fa[15] * fb[15];
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> TMatrix4<T> TMatrix4<T>::MatrixMult(const TMatrix4<T>& mat1) const {
  TMatrix4<T> result;

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

template <typename T> TMatrix4<T> TMatrix4<T>::Mult(T scalar) const {
  TMatrix4<T> res;

  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      res.elements[i][j] = elements[i][j] * scalar;
    }
  }

  return res;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> TMatrix4<T> TMatrix4<T>::Concat43(const TMatrix4<T>& mat1) const {
  TMatrix4<T> result;

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

template <typename T> TMatrix4<T> TMatrix4<T>::Concat43Transpose(const TMatrix4<T>& mat1) const {
  TMatrix4<T> result;

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

template <typename T> void TMatrix4<T>::CorrectionMatrix(const TMatrix4<T>& from, const TMatrix4<T>& to) {
  /////////////////////////
  //
  //	GENERATE CORRECTION	TO GET FROM A to C
  //
  //	A * B = C				(A and C are known we dont know B)
  //	(A * iA) * B = iA * C
  //	B = iA * C				we now know B
  //
  /////////////////////////

  TMatrix4<T> iFrom = from;

  // iFrom.Inverse();
  GEMSMatrixInverse(from, iFrom);

  *this = (iFrom * to); // B
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::SetRotation(const TMatrix4<T>& from) {
  TMatrix4<T> rval = from;
  rval.SetTranslation(T(0.0f), T(0.0f), T(0.0f));
  rval.Normalize();
  *this = rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::SetTranslation(const TMatrix4<T>& from) {
  SetToIdentity();
  TVector4<T> t = from.GetTranslation();
  SetTranslation(t);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void TMatrix4<T>::SetScale(const TMatrix4<T>& from) // assumes rot is zero!
{
  TMatrix4<T> RS = from;
  RS.SetTranslation(T(0.0f), T(0.0f), T(0.0f));

  TMatrix4<T> R = RS;
  R.Normalize();

  TMatrix4<T> S;
  S.CorrectionMatrix(R, RS);

  *this = S;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void TMatrix4<T>::Lerp(const TMatrix4<T>& from, const TMatrix4<T>& to, T par) // par 0.0f .. 1.0f
{
  //////////////////

  TVector4<T> vF = from.GetTranslation();
  TVector4<T> vT = to.GetTranslation();
  TVector4<T> vT2;
  vT2.Lerp(vF, vT, par);
  TMatrix4<T> matT;
  matT.SetTranslation(vT2);

  //////////////////

  TMatrix4<T> FromR;
  FromR.SetRotation(from); // froms ROTATION
  TMatrix4<T> ToR;
  ToR.SetRotation(to); // froms ROTATION

  TQuaternion<T> FromQ;
  FromQ.FromMatrix(FromR);
  TQuaternion<T> ToQ;
  ToQ.FromMatrix(ToR);

  TMatrix4<T> CORR;
  CORR.CorrectionMatrix(from, to); // CORR.Normalize();

  TQuaternion<T> Qidn;
  TQuaternion<T> Qrot;
  Qrot.FromMatrix(CORR);

  // TVector4<T>  rawaxisang = Qrot.ToAxisAngle();
  // T rawangle = rawaxisang.GetW();
  // T	newangle = rawangle*par;
  // TVector4<T> newaxisang = rawaxisang;
  // newaxisang.SetW( newangle );
  // TCQuaternion newQrot;	newQrot.FromAxisAngle( newaxisang );

#if 1

  TQuaternion<T> dQ = Qrot;
  dQ.Sub(Qidn);
  dQ.Scale(par);
  dQ.Add(Qidn);

  if (dQ.Magnitude() > T(0.0f))
    dQ.Negate();

  TQuaternion<T> newQrot = dQ;

#endif

  // TCQuaternion newQrot = FromQ.Slerp( ToQ, par );

  TMatrix4<T> matR;
  matR = newQrot.ToMatrix();
  // matR.Normalize();

  //////////////////

  *this = (FromR * matR * matT);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::DecomposeMatrix(TVector3<T>& pos, TQuaternion<T>& qrot, T& Scale) const {
  pos = GetTranslation();

  TMatrix4<T> rot = *this;

  rot.SetElemYX(3, 0, T(0.0f));
  rot.SetElemYX(3, 1, T(0.0f));
  rot.SetElemYX(3, 2, T(0.0f));
  rot.SetElemYX(0, 3, T(0.0f));
  rot.SetElemYX(1, 3, T(0.0f));
  rot.SetElemYX(2, 3, T(0.0f));

  rot.SetElemYX(3, 3, T(1.0f));

  TVector4<T> UnitVector(T(1.0f), T(0.0f), T(0.0f), T(1.0f));
  TVector4<T> XFVector = UnitVector.Transform(rot);

  Scale = XFVector.Mag();

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      rot.SetElemYX(i, j, rot.GetElemYX(i, j) / Scale);
    }
  }

  qrot.FromMatrix(rot);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::ComposeMatrix(const TVector3<T>& pos, const TQuaternion<T>& qrot, const T& Scale) {
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

template <typename T> TVector4<T> TMatrix4<T>::GetRow(int irow) const {
  TVector4<T> out;
  out.SetX(GetElemXY(0, irow));
  out.SetY(GetElemXY(1, irow));
  out.SetZ(GetElemXY(2, irow));
  out.SetW(GetElemXY(3, irow));
  return out;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> TVector4<T> TMatrix4<T>::GetColumn(int icol) const {
  TVector4<T> out;
  out.SetX(GetElemXY(icol, 0));
  out.SetY(GetElemXY(icol, 1));
  out.SetZ(GetElemXY(icol, 2));
  out.SetW(GetElemXY(icol, 3));
  return out;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::SetRow(int irow, const TVector4<T>& v) {
  SetElemXY(0, irow, v.GetX());
  SetElemXY(1, irow, v.GetY());
  SetElemXY(2, irow, v.GetZ());
  SetElemXY(3, irow, v.GetW());
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::SetColumn(int icol, const TVector4<T>& v) {
  SetElemXY(icol, 0, v.GetX());
  SetElemXY(icol, 1, v.GetY());
  SetElemXY(icol, 2, v.GetZ());
  SetElemXY(icol, 3, v.GetW());
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::NormalVectorsIn(const TVector3<T>& xv, const TVector3<T>& yv, const TVector3<T>& zv) {
  SetColumn(0, TVector4<T>(xv, T(0)));
  SetColumn(1, TVector4<T>(yv, T(0)));
  SetColumn(2, TVector4<T>(zv, T(0)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::NormalVectorsOut(TVector3<T>& xv, TVector3<T>& yv, TVector3<T>& zv) const {
  xv = GetColumn(0).xyz();
  yv = GetColumn(1).xyz();
  zv = GetColumn(2).xyz();
}

///////////////////////////////////////////////////////////////////////////////
// miniork IDEAL projection matrix ( use the gfxtarget for device specific projection matrices)
// this will project a point into a clip space box ranged from -1..1 on x/y and 0..1 on z
///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::Perspective(T fovy, T aspect, T fnear, T ffar) {
  OrkAssert(fnear >= 0.0f);
  OrkAssert(ffar > fnear);

  float xmin, xmax, ymin, ymax;
  ymax = fnear * CFloat::Tan(fovy * DTOR * 0.5f);
  ymin = -ymax;
  xmin = ymin * aspect;
  xmax = ymax * aspect;

  Frustum(xmin, xmax, ymax, ymin, fnear, ffar);
}

///////////////////////////////////////////////////////////////////////////////
// miniork IDEAL frustum matrix ( use the gfxtarget for device specific projection matrices)
// this will project a point into a clip space box ranged from -1..1 on x/y and 0..1 on z
///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::Frustum(T left, T right, T top, T bottom, T zn, T zf) {

  SetToIdentity();

  CReal width = right - left;
  CReal height = top - bottom;
  CReal depth = (zf - zn);

  /////////////////////////////////////////////

  SetElemYX(0, 0, CReal(2.0f * zn) / -width);
  SetElemYX(1, 1, CReal(2.0f * zn) / height);
  SetElemYX(2, 2, CReal(zf) / depth);
  SetElemYX(3, 3, CReal(0.0f));

  SetElemYX(2, 3, CReal(zn * zf) / CReal(zn - zf));
  SetElemYX(3, 2, CReal(1.0f));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::LookAt(const TVector3<T>& Eye, const TVector3<T>& Ctr, const TVector3<T>& Up) {
  SetToIdentity();

  TVector3<T> zaxis = (Ctr - Eye).Normal();
  TVector3<T> xaxis = (Up.Cross(zaxis)).Normal();
  TVector3<T> yaxis = zaxis.Cross(xaxis);

  SetRow(0, TVector4<T>(xaxis, 0.0f));
  SetRow(1, TVector4<T>(yaxis, 0.0f));
  SetRow(2, TVector4<T>(zaxis, 0.0f));

  SetElemXY(3, 0, -xaxis.Dot(Eye));
  SetElemXY(3, 1, -yaxis.Dot(Eye));
  SetElemXY(3, 2, -zaxis.Dot(Eye));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::LookAt(T eyex, T eyey, T eyez, T centerx, T centery, T centerz, T upx, T upy, T upz) {
  TVector3<T> Ctr(centerx, centery, centerz);
  TVector3<T> Eye(eyex, eyey, eyez);
  TVector3<T> Up(upx, upy, upz);
  LookAt(Eye, Ctr, Up);
}

///////////////////////////////////////////////////////////////////////////////
// abstract ortho (all axis -1 .. 1 )
// if you want device specific see the gfxtarget

template <typename T> void TMatrix4<T>::Ortho(T left, T right, T top, T bottom, T fnear, T ffar) {
  T invWidth = T(1.0f) / (right - left);
  T invHeight = T(1.0f) / (top - bottom);
  T invDepth = T(1.0f) / (ffar - fnear);
  T fScaleX = T(2.0f) * invWidth;
  T fScaleY = T(2.0f) * invHeight;
  T fScaleZ = T(-2.0f) * invDepth;
  T TransX = -(right + left) * invWidth;
  T TransY = -(top + bottom) * invHeight;
  T TransZ = -(ffar + fnear) * invDepth;

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
bool TMatrix4<T>::UnProject(const TVector4<T>& rVWin, const TMatrix4<T>& rIMVP, const SRect& rVP, TVector3<T>& rVObj) {
  T in[4];
  T _z = rVWin.GetZ();
  in[0] = (rVWin.GetX() - T(rVP.miX)) * T(2) / T(rVP.miW) - T(1.0f);
  in[1] = (T(rVP.miH) - rVWin.GetY() - T(rVP.miY)) * T(2) / T(rVP.miH) - T(1.0f);
  in[2] = _z;
  in[3] = T(1.0f);
  TVector4<T> rVDev(in[0], in[1], in[2], in[3]);
  TVector4<T> rval = rVDev.Transform(rIMVP);
  rval.PerspectiveDivide();
  rVObj = rval.xyz();
  return true;
}
template <typename T> bool TMatrix4<T>::UnProject(const TMatrix4<T>& rIMVP, const TVector3<T>& ClipCoord, TVector3<T>& rVObj) {
  TVector4<T> rval = ClipCoord.Transform(rIMVP);
  rval.PerspectiveDivide();
  rVObj = rval.xyz();
  return true;
}
///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::Transpose(void) {
  TMatrix4<T> temp = *this;

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      SetElemYX(i, j, temp.GetElemYX(j, i));
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::Inverse(void) {
  TMatrix4<T> result;

  /////////////
  // The rotational part of the matrix is simply the transpose of the original matrix.

  for (int i = 0; i <= 3; i++) {
    for (int j = 0; j <= 3; j++) {
      result.SetElemYX(i, j, GetElemYX(j, i));
    }
  }

  ////////////

  ////////////
  // The right column vector of the matrix should always be [ 0 0 0 1 ]
  // In most cases. . . you don't need this column at all because it'll
  // never be used in the program, but since this code is used with GL
  // and it does consider this column, it is here.

  result.SetElemYX(3, 0, T(0.0f));
  result.SetElemYX(3, 1, T(0.0f));
  result.SetElemYX(3, 2, T(0.0f));
  result.SetElemYX(3, 3, T(1.0f));

  T Tx = GetElemYX(0, 3);
  T Ty = GetElemYX(1, 3);
  T Tz = GetElemYX(2, 3);

  ////////////

  ////////////
  // Rrp = -(Tm * Rm) to get the translation part of the inverse

  T NTx = -(GetElemYX(0, 0) * Tx + GetElemYX(1, 0) * Ty + GetElemYX(2, 0) * Tz);
  T NTy = -(GetElemYX(0, 1) * Tx + GetElemYX(1, 1) * Ty + GetElemYX(2, 1) * Tz);
  T NTz = -(GetElemYX(0, 2) * Tx + GetElemYX(1, 2) * Ty + GetElemYX(2, 2) * Tz);
  result.SetElemYX(0, 3, NTx);
  result.SetElemYX(1, 3, NTy);
  result.SetElemYX(2, 3, NTz);

  ////////////

  *this = result;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::InverseTranspose(void) {
  TMatrix4<T> result;

  /////////////
  // The rotational part of the matrix is simply the transpose of the original matrix.

  for (int i = 0; i <= 3; i++) {
    for (int j = 0; j <= 3; j++) {
      result.SetElemXY(i, j, GetElemYX(j, i));
    }
  }

  ////////////

  ////////////
  // The right column vector of the matrix should always be [ 0 0 0 1 ]
  // In most cases. . . you don't need this column at all because it'll
  // never be used in the program, but since this code is used with GL
  // and it does consider this column, it is here.

  result.SetElemXY(3, 0, T(0.0f));
  result.SetElemXY(3, 1, T(0.0f));
  result.SetElemXY(3, 2, T(0.0f));
  result.SetElemXY(3, 3, T(1.0f));

  T Tx = GetElemYX(0, 3);
  T Ty = GetElemYX(1, 3);
  T Tz = GetElemYX(2, 3);

  ////////////

  ////////////
  // Rrp = -(Tm * Rm) to get the translation part of the inverse

  T NTx = -(GetElemYX(0, 0) * Tx + GetElemYX(1, 0) * Ty + GetElemYX(2, 0) * Tz);
  T NTy = -(GetElemYX(0, 1) * Tx + GetElemYX(1, 1) * Ty + GetElemYX(2, 1) * Tz);
  T NTz = -(GetElemYX(0, 2) * Tx + GetElemYX(1, 2) * Ty + GetElemYX(2, 2) * Tz);
  result.SetElemXY(0, 3, NTx);
  result.SetElemXY(1, 3, NTy);
  result.SetElemXY(2, 3, NTz);

  ////////////

  *this = result;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix4<T>::Normalize(void) {
  TMatrix4<T> result;

  T Xx = GetElemXY(0, 0);
  T Xy = GetElemXY(0, 1);
  T Xz = GetElemXY(0, 2);
  T Yx = GetElemXY(1, 0);
  T Yy = GetElemXY(1, 1);
  T Yz = GetElemXY(1, 2);
  T Zx = GetElemXY(2, 0);
  T Zy = GetElemXY(2, 1);
  T Zz = GetElemXY(2, 2);

  T Xi = T(1.0f) / CFloat::Sqrt((Xx * Xx) + (Xy * Xy) + (Xz * Xz));
  T Yi = T(1.0f) / CFloat::Sqrt((Yx * Yx) + (Yy * Yy) + (Yz * Yz));
  T Zi = T(1.0f) / CFloat::Sqrt((Zx * Zx) + (Zy * Zy) + (Zz * Zz));

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
