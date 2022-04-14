////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
#pragma once
////////////////////////////////////////////////////////////////
#include <cmath>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/kernel/string/deco.inl>
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>

#include <glm/gtc/matrix_transform.hpp>

namespace ork {

template <typename T> const Matrix33<T> Matrix33<T>::Identity;

template <typename T> Matrix33<T>::Matrix33(const Quaternion<T>& q) {
  fromQuaternion(q);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::setToIdentity() {
  base_t& as_base = *this;
  as_base = base_t(T(1));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::dump(STRING name) {
  orkprintf("Matrix %p %s\n{	", this, name);

  for (int i = 0; i < 4; i++) {

    for (int j = 0; j < 4; j++) {
      orkprintf("%f ", elemXY(i,j));
    }

    orkprintf("\n	");
  }

  orkprintf("\n}\n");
}

template <typename T> std::string Matrix33<T>::dumpcn() const {
  std::string rval;
  auto brace_color = Vector3<T>(0.7, 0.7, 0.7);
  for (int i = 0; i < 3; i++) {
    rval += ork::deco::decorate(brace_color, "[");
    fvec3 fcol;
    switch (i) {
      case 0:
        fcol = Vector3<T>(1, .5, .5);
        break;
      case 1:
        fcol = Vector3<T>(.5, 1, .5);
        break;
      case 2:
        fcol = Vector3<T>(.5, .5, 1);
        break;
    }
    for (int j = 0; j < 3; j++) {
      //////////////////////////////////
      // round down small numbers
      //////////////////////////////////
      float elem = elemXY(i,j);
      elem       = float(int(elem * 10000)) / 10000.0f;
      //////////////////////////////////
      rval += ork::deco::format(fcol, " %+0.4g ", elem);
    }
    rval += ork::deco::decorate(brace_color, "]");
  }
  // Quaternion<T> q(*this);
  // auto rot = q.toEuler();
  // rval +=
  //  ork::deco::format(fvec3(0.8, 0.8, 0.8), "  euler<%g %g %g>", round(rot.x * RTOD), round(rot.y * RTOD), round(rot.z * RTOD));
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::setElemYX(int ix, int iy, T val) {
  base_t& as_base = *this;
  as_base[iy][ix] = val;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> T Matrix33<T>::elemYX(int ix, int iy) const {
  const base_t& as_base = *this;
  return as_base[iy][ix];
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::setElemXY(int ix, int iy, T val) {
  base_t& as_base = *this;
  as_base[ix][iy] = val;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> T Matrix33<T>::elemXY(int ix, int iy) const {
  const base_t& as_base = *this;
  return as_base[ix][iy];
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//	set rotation explicitly

template <typename T> void Matrix33<T>::setRotateX(T rad) {
  auto ident = base44_t(1);
  base_t& as_base = *this;
  as_base = glm::rotate(ident,rad,Vector3<T>(1,0,0));
}

///////////////////////////////////////////////////////////////////////////////
//	set rotation explicitly

template <typename T> void Matrix33<T>::setRotateY(T rad) {
  auto ident = base44_t(1);
  base_t& as_base = *this;
  as_base = glm::rotate(ident,rad,Vector3<T>(0,1,0));
}

///////////////////////////////////////////////////////////////////////////////
//	set rotation explicitly

template <typename T> void Matrix33<T>::setRotateZ(T rad) {
  auto ident = base44_t(1);
  base_t& as_base = *this;
  as_base = glm::rotate(ident,rad,Vector3<T>(0,0,1));
}

///////////////////////////////////////////////////////////////////////////////
//	rotate in place

template <typename T> void Matrix33<T>::rotateOnX(T rad) {
  Matrix33<T> temp, res;
  temp.setRotateX(rad);
  res   = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////
//	rotate in place

template <typename T> void Matrix33<T>::rotateOnY(T rad) {
  Matrix33<T> temp, res;
  temp.setRotateY(rad);
  res   = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////
//	rotate in place

template <typename T> void Matrix33<T>::rotateOnZ(T rad) {
  Matrix33<T> temp, res;
  temp.setRotateZ(rad);
  res   = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::setScale(const Vector4<T>& vec) {
  auto ident = base44_t(1);
  base_t& as_base = *this;
  as_base = glm::scale(ident,vec.xyz());
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::setScale(T x, T y, T z) {
  auto ident = base44_t(1);
  base_t& as_base = *this;
  as_base = glm::scale(ident,Vector3<T>(x,y,z));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::setScale(T s) {
  setScale(s, s, s);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::scale(const Vector4<T>& vec) {
  Matrix33<T> temp, res;
  temp.setScale(vec);
  res   = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::scale(T xscl, T yscl, T zscl) {
  Matrix33<T> temp, res;
  temp.setScale(xscl, yscl, zscl);
  res   = temp * *this;
  *this = res;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// sm - rotation matrix from quaternion
template <typename T> void Matrix33<T>::fromQuaternion(Quaternion<T> quat) {
  
  T xx = quat.x * quat.x;
  T yy = quat.y * quat.y;
  T zz = quat.z * quat.z;
  T xy = quat.x * quat.y;
  T zw = quat.z * quat.w;
  T zx = quat.z * quat.x;
  T yw = quat.y * quat.w;
  T yz = quat.y * quat.z;
  T xw = quat.x * quat.w;

  setElemXY(0,0, T(1.0f) - (T(2.0f) * (yy + zz)));
  setElemXY(0,1, T(2.0f) * (xy + zw));
  setElemXY(0,2, T(2.0f) * (zx - yw));
  setElemXY(1,0, T(2.0f) * (xy - zw));
  setElemXY(1,1, T(1.0f) - (T(2.0f) * (zz + xx)));
  setElemXY(1,2, T(2.0f) * (yz + xw));
  setElemXY(2,0, T(2.0f) * (zx + yw));
  setElemXY(2,1, T(2.0f) * (yz - xw));
  setElemXY(2,2, T(1.0f) - (T(2.0f) * (yy + xx)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Matrix33<T> Matrix33<T>::multiply(const Matrix33<T>& mat1) const {
  Matrix33<T> result;
  base_t& result_as_base = result;
  const base_t& a_as_base = *this;
  const base_t& b_as_base = mat1;
  result_as_base = b_as_base*a_as_base; // TODO b*a -> a*b
  return (result);
}

template <typename T> Matrix33<T> Matrix33<T>::multiply(T scalar) const {
  Matrix33<T> result;
  base_t& result_as_base = result;
  const base_t& a_as_base = *this;
  result_as_base = a_as_base*scalar;
  return (result);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::correctionMatrix(const Matrix33<T>& from, const Matrix33<T>& to) {
  /////////////////////////
  //
  //	GENERATE CORRECTION	TO GET FROM A to C
  //
  //	A * B = C				(A and C are known we dont know B)
  //	(A * iA) * B = iA * C
  //	B = iA * C				we now know B
  //
  /////////////////////////

  Matrix33<T> iFrom = from;

  iFrom.inverse();

  *this = (iFrom * to); // B
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::setRotation(const Matrix33<T>& from) {
  Matrix33<T> rval = from;
  rval.normalizeInPlace();
  *this = rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void Matrix33<T>::setScale(const Matrix33<T>& from) // assumes rot is zero!
{
  Matrix33<T> RS = from;

  Matrix33<T> R = RS;
  R.normalizeInPlace();

  Matrix33<T> S;
  S.correctionMatrix(R, RS);

  *this = S;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void Matrix33<T>::lerp(const Matrix33<T>& from, const Matrix33<T>& to, T par) // par 0.0f .. 1.0f
{
  Matrix33<T> FromR, ToR, CORR, matR;
  Quaternion<T> FromQ, ToQ, Qidn, Qrot;

  //////////////////

  FromR.setRotation(from); // froms ROTATION
  ToR.setRotation(to);     // froms ROTATION

  FromQ.fromMatrix3(FromR);
  ToQ.fromMatrix3(ToR);

  CORR.correctionMatrix(from, to); // CORR.Normalize();

  Qrot.fromMatrix3(CORR);

  Quaternion<T> dQ = Qrot;
  dQ.subtract(Qidn);
  dQ.scale(par);
  dQ.add(Qidn);

  if (dQ.norm() > T(0.0f))
    dQ.negate();

  Quaternion<T> newQrot = dQ;

  matR = newQrot.toMatrix3();

  //////////////////

  *this = (FromR * matR);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::decompose(Quaternion<T>& qrot, T& Scale) const {
  Matrix33<T> rot = *this;

  Vector3<T> UnitVector(T(1.0f), T(0.0f), T(0.0f));
  Vector3<T> XFVector = UnitVector.transform(rot);

  Scale = XFVector.magnitude();

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      rot.setElemXY(i, j, rot.elemXY(i, j) / Scale);
    }
  }

  qrot.fromMatrix3(rot);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::compose(const Quaternion<T>& qrot, const T& Scale) {
  *this = qrot.toMatrix3();
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      setElemYX(i, j, elemYX(i, j) * Scale);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T> Matrix33<T>::row(int irow) const {
  Vector3<T> out;
  out.x = (elemXY(0, irow));
  out.y = (elemXY(1, irow));
  out.z = (elemXY(2, irow));
  return out;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T> Matrix33<T>::column(int icol) const {
  Vector3<T> out;
  out.x = (elemXY(icol, 0));
  out.y = (elemXY(icol, 1));
  out.z = (elemXY(icol, 2));
  return out;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::setRow(int irow, const Vector3<T>& v) {
  setElemXY(0, irow, v.x);
  setElemXY(1, irow, v.y);
  setElemXY(2, irow, v.z);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::setColumn(int icol, const Vector3<T>& v) {
  setElemXY(icol, 0, v.x);
  setElemXY(icol, 1, v.y);
  setElemXY(icol, 2, v.z);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::fromNormalVectors(const Vector3<T>& xv, const Vector3<T>& yv, const Vector3<T>& zv) {
  setColumn(0, xv);
  setColumn(1, yv);
  setColumn(2, zv);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::toNormalVectors(Vector3<T>& xv, Vector3<T>& yv, Vector3<T>& zv) const {
  xv = column(0);
  yv = column(1);
  zv = column(2);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::transpose() {
  Matrix33<T> temp = *this;

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      setElemYX(i, j, temp.elemYX(j, i));
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::inverse() {
  Matrix33<T> result;

  /////////////
  // The rotational part of the matrix is simply the transpose of the original matrix.

  for (int i = 0; i <= 2; i++) {
    for (int j = 0; j <= 2; j++) {
      result.setElemYX(i, j, elemYX(j, i));
    }
  }

  ////////////

  *this = result;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::inverseTranspose() {
  Matrix33<T> result;

  /////////////
  // The rotational part of the matrix is simply the transpose of the original matrix.

  for (int i = 0; i <= 3; i++) {
    for (int j = 0; j <= 3; j++) {
      result.setElemXY(i, j, elemYX(j, i));
    }
  }

  ////////////

  *this = result;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Matrix33<T>::normalizeInPlace() {
  Matrix33<T> result;

  T Xx = elemXY(0, 0);
  T Xy = elemXY(0, 1);
  T Xz = elemXY(0, 2);
  T Yx = elemXY(1, 0);
  T Yy = elemXY(1, 1);
  T Yz = elemXY(1, 2);
  T Zx = elemXY(2, 0);
  T Zy = elemXY(2, 1);
  T Zz = elemXY(2, 2);

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

  result.setElemXY(0, 0, Xx);
  result.setElemXY(0, 1, Xy);
  result.setElemXY(0, 2, Xz);

  result.setElemXY(1, 0, Yx);
  result.setElemXY(1, 1, Yy);
  result.setElemXY(1, 2, Yz);

  result.setElemXY(2, 0, Zx);
  result.setElemXY(2, 1, Zy);
  result.setElemXY(2, 2, Zz);

  *this = result;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////

namespace ork::reflect {
template <> //
inline void ::ork::reflect::ITyped<fmtx3>::serialize(serdes::node_ptr_t sernode) const {
  using namespace serdes;
  auto serializer        = sernode->_serializer;
  auto instance          = sernode->_ser_instance;
  auto arynode           = serializer->pushNode(_name, serdes::NodeType::ARRAY);
  arynode->_parent       = sernode;
  arynode->_ser_instance = instance;
  fmtx3 value;
  get(value, instance);
  for (int i = 0; i < 9; i++)
    serializeArraySubLeaf(arynode, value.asArray()[i], i);
  serializer->popNode(); // pop arraynode
}
template <> //
inline void ::ork::reflect::ITyped<fmtx3>::deserialize(serdes::node_ptr_t arynode) const {
  using namespace serdes;
  auto deserializer  = arynode->_deserializer;
  auto instance      = arynode->_deser_instance;
  size_t numelements = arynode->_numchildren;
  OrkAssert(numelements == 9);

  fmtx3 value;
  for (int i = 0; i < 9; i++)
    value.asArray()[i] = deserializeArraySubLeaf<float>(arynode, i);

  set(value, instance);
}

} // namespace ork::reflect
