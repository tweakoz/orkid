////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
#pragma once
////////////////////////////////////////////////////////////////
#include <cmath>
#include <ork/math/math_types.inl>
#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/cvector4.h>
#include <ork/kernel/string/deco.inl>
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>


///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T>::Quaternion() {
    setToIdentity();
}

template <typename T> Quaternion<T>::~Quaternion() {
}

template <typename T> const typename Quaternion<T>::base_t& Quaternion<T>::asGlmQuat() const {
  return *this;
}

template <typename T> Quaternion<T>::Quaternion(T _x, T _y, T _z, T _w) {
  this->x = (_x);
  this->y = (_y);
  this->z = (_z);
  this->w = (_w);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T>::Quaternion(const base_t& base) {
  this->x = base.x;
  this->y = base.y;
  this->z = base.z;
  this->w = base.w;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T>::Quaternion(const Vector3<T>& axis, float angle) {
  this->fromAxisAngle(Vector4<T>(axis, angle));
}

template <typename T> Quaternion<T>::Quaternion(const kln::rotor& rotor) {
  auto rn = rotor;
  rn.normalize();
  this->w = rn.scalar();
  this->x = rn.e23();
  this->y = rn.e31(); // or e13()?
  this->z = rn.e12();
}

template <typename T> kln::rotor Quaternion<T>::asKleinRotor() const{
  // TODO : does this handle double-coverage?
  auto aa = normalized().toAxisAngle();
  return kln::rotor(aa.w,aa.x,aa.y,aa.z);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> 
Vector3<T> Quaternion<T>::transform(const Vector3<T>& point) const {
  Vector4<T> p4(point);
  Vector4<T> result = p4*(*this);
  return result.xyz();
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T> Quaternion<T>::toEuler() const {
  Vector3<T> angles;

  // roll (x-axis rotation)
  double sinr_cosp = 2 * (this->w * this->x + this->y * this->z);
  double cosr_cosp = 1 - 2 * (this->x * this->x + this->y * this->y);
  angles.x         = std::atan2(sinr_cosp, cosr_cosp);

  // pitch (y-axis rotation)
  double sinp = 2 * (this->w * this->y - this->z * this->x);
  if (std::abs(sinp) >= 1)
    angles.y = std::copysign(M_PI / 2, sinp); // use 90 degrees if out of range
  else
    angles.y = std::asin(sinp);

  // yaw (z-axis rotation)
  double siny_cosp = 2 * (this->w * this->z + this->x * this->y);
  double cosy_cosp = 1 - 2 * (this->y * this->y + this->z * this->z);
  angles.z         = std::atan2(siny_cosp, cosy_cosp);

  return angles;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T> Quaternion<T>::lerp(const Quaternion<T>& a, const Quaternion<T>& b, T alpha) {

  T cos_t = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

  /* if B is on opposite hemisphere from A, use -B instead */
  bool bflip = (cos_t < T(0));

  T beta   = T(1) - alpha;
  T alpha2 = alpha;

  if (bflip) {
    alpha2 = -alpha2;
  }

  Quaternion q;
  q.x = (beta * a.x + alpha2 * b.x);
  q.y = (beta * a.y + alpha2 * b.y);
  q.z = (beta * a.z + alpha2 * b.z);
  q.w = (beta * a.w + alpha2 * b.w);

  return q;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T>::Quaternion(const Matrix44<T>& matrix) {
  fromMatrix(matrix);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T>::Quaternion(const Matrix33<T>& matrix) {
  fromMatrix3(matrix);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T> Quaternion<T>::operator*(const Quaternion<T>& rhs) const {
  return this->multiply(rhs);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T> Quaternion<T>::operator+(const Quaternion<T>& rhs) const {
  return Quaternion<T>(this->x+rhs.x,this->y+rhs.y,this->z+rhs.z,this->w+rhs.w);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
bool Quaternion<T>::operator==(const Quaternion<T>& rhs) const {
  bool match = true;
  match &= (this->x==rhs.x);
  match &= (this->y==rhs.y);
  match &= (this->z==rhs.z);
  match &= (this->w==rhs.w);
  return match;
}

///////////////////////////////////////////////////////////////////////////////

template <typename MatrixType> inline Quaternion<typename MatrixType::value_type> quaternionFromMatrix(const MatrixType& M) {

  Quaternion<typename MatrixType::value_type> qout;
  qout = glm::quat_cast(M.asGlmMat4());
  return qout;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::fromMatrix(const Matrix44<T>& M) {
  *this = quaternionFromMatrix(M);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::fromMatrix3(const Matrix33<T>& M) {
  *this = quaternionFromMatrix(M);
}

///////////////////////////////////////////////////////////////////////////////

template <typename MatrixType> inline MatrixType QuaterniontoMatrix(const Quaternion<typename MatrixType::value_type>& Q) {
  return MatrixType(glm::mat4_cast(Q.asGlmQuat()));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Matrix44<T> Quaternion<T>::toMatrix(void) const {
  return QuaterniontoMatrix<Matrix44<T>>(*this);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Matrix33<T> Quaternion<T>::toMatrix3(void) const {
  return QuaterniontoMatrix<Matrix33<T>>(*this);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::scale(T scalar) {
  this->x *= scalar;
  this->y *= scalar;
  this->z *= scalar;
  this->w *= scalar;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T> Quaternion<T>::slerp(const Quaternion<T>& a, const Quaternion<T>& b, T alpha) {
  // Original code from Graphics Gems III - (Morrison, quaternion interpolation with extra spins).

  T beta;         /* complementary interp parameter */
  T theta;        /* angle between A and B */
  T sin_t, cos_t; /* sine, cosine of theta */
  T phi;          /* theta plus spins */
  bool bflip;     /* use negation of B? */
  int spin = 0;
  // const F32 EPSILON = 0.0001f;
  // const F32 PI=3.14159254f;
  Quaternion<T> q;

  if ((a.x == b.x) && (a.y == b.y) && (a.z == b.z) && (a.w == b.w)) {
    q.x = (a.x);
    q.y = (a.y);
    q.z = (a.z);
    q.w = (a.w);
    return (q);
  }

  /* cosine theta = dot product of A and B */
  cos_t = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

  /* if B is on opposite hemisphere from A, use -B instead */
  if (cos_t < T(0)) {
    cos_t = -cos_t;
    bflip = true;
  } else
    bflip = false;

  /* if B is (within precision limits) the same as A,
   * just linear interpolate between A and B.
   * Can't do spins, since we don't know what direction to spin.
   */
  if (T(1) - cos_t < Float::Epsilon()) {
    beta = T(1) - alpha;
  } else { /* normal case */
    theta = acosf(cos_t);
    phi   = theta + T(spin) * Float::Pi();
    sin_t = sinf(theta);
    beta  = sinf(theta - alpha * phi) / sin_t;
    alpha = sinf(alpha * phi) / sin_t;
  }

  if (bflip)
    alpha = -alpha;

  /* interpolate */
  q.x = (beta * a.x + alpha * b.x);
  q.y = (beta * a.y + alpha * b.y);
  q.z = (beta * a.z + alpha * b.z);
  q.w = (beta * a.w + alpha * b.w);

  return (q);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T> Quaternion<T>::negate() const {
  Quaternion<T> result;

  result.x = (-this->x);
  result.y = (-this->y);
  result.z = (-this->z);
  result.w = (-this->w);

  return (result);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T> Quaternion<T>::square() const {
  T temp = T(2) * this->w;
  Quaternion<T> result;

  result.x = (this->x * temp);
  result.y = (this->y * temp);
  result.z = (this->z * temp);
  result.w = (this->w * this->w - (this->x * this->x + this->y * this->y + this->z * this->z));

  return (result);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T> Quaternion<T>::conjugate() const {
  return Quaternion<T>(glm::conjugate(asGlmQuat()));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T> Quaternion<T>::inverse() const {
  return Quaternion<T>(glm::inverse(asGlmQuat()));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::inverseOf(const Quaternion<T>& of) {
  *this = Quaternion<T>(glm::inverse(of.asGlmQuat()));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> T Quaternion<T>::norm() const {
  return (this->w * this->w + this->x * this->x + this->y * this->y + this->z * this->z);
}
 
///////////////////////////////////////////////////////////////////////////////
//	DESC: Converts a normalized axis and angle to a unit quaternion.

template <typename T> void Quaternion<T>::fromAxisAngle(const Vector4<T>& v) {
  auto axis = v.xyz().asGlmVec3();
  T angle = v.w;
  base_t& as_base = *this;
  as_base = glm::angleAxis(angle,axis);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector4<T> Quaternion<T>::toAxisAngle(void) const {
  T tr = acosf(this->w);
  bool is_nan = isnan(tr);
  T wsq = this->w*this->w;
  T invwsq = T(1)/sqrt(1-wsq);
  T vx  = is_nan ? T(0) : this->x * invwsq;
  T vy  = is_nan ? T(0) : this->y * invwsq;
  T vz  = is_nan ? T(0) : this->z * invwsq;
  T ang = is_nan ? T(0) : 2.0*tr;
  return Vector4<T>(vx, vy, vz, ang);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::add(Quaternion<T>& a) {
  *this = Quaternion<T>(asGlmQuat()+a.asGlmQuat());
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::subtract(Quaternion<T>& a) {
  *this = Quaternion<T>(asGlmQuat()-a.asGlmQuat());
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T> Quaternion<T>::multiply(const Quaternion<T>& a) const {
  return Quaternion<T>(asGlmQuat()*a.asGlmQuat());
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::divide(Quaternion<T>& a) {
  this->x /= a.x;
  this->y /= a.y;
  this->z /= a.z;
  this->w /= a.w;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::setToIdentity(void) {
  this->w = (T(1));
  this->x = (T(0));
  this->y = (T(0));
  this->z = (T(0));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::correctionQuaternion(const Quaternion<T>& from,const Quaternion<T>& to) {
  /////////////////////////
  //
  //  GENERATE CORRECTION TO GET FROM A to C
  //
  //  A * B = C       (A and C are known we dont know B)
  //  (A * iA) * B = iA * C
  //  B = iA * C        we now know B
  //
  /////////////////////////

  Quaternion<T> inv_from = from.inverse();
  *this = (inv_from * to); // B
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T> Quaternion<T>::shortestRotationArc(Vector4<T> v0, Vector4<T> v1) {
Quaternion q;

  v0.normalizeInPlace();
  v1.normalizeInPlace();

  Vector4<T> cross = v1.crossWith(v0); // Cross is non destructive
  T dot            = v1.dotWith(v0);
  T s              = sqrtf((T(1.0) + dot) * T(2.0));

  q.x = (cross.x / s);
  q.y = (cross.y / s);
  q.z = (cross.z / s);
  q.w = (s / T(2.0));

  return q;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::dump(void) {
  orkprintf("quat %f %f %f %f\n", this->x, this->y, this->z, this->w);
}

///////////////////////////////////////////////////////////////////////////////
// smallest 3 compression - 16 bytes -> 4 bytes  (game gems 3, page 189)
///////////////////////////////////////////////////////////////////////////////

template <typename T> QuatCodec Quaternion<T>::compress(void) const {
  static const T frange = T((1 << 9) - 1);

  QuatCodec uquat;

  T qf[4];
  T fqmax(-2.0f);
  qf[0] = this->x;
  qf[1] = this->y;
  qf[2] = this->z;
  qf[3] = this->w;

  ////////////////////////////////////////
  // normalize quaternion

  T fmag = sqrtf(qf[0] * qf[0] + qf[1] * qf[1] + qf[2] * qf[2] + qf[3] * qf[3]);

  // qf[0] /= fmag;
  // qf[1] /= fmag;
  // qf[2] /= fmag;
  // qf[3] /= fmag;

  ////////////////////////////////////////
  // find smallest 3

  int iqlargest = -1;

  for (int i = 0; i < 4; i++) {
    T fqi = fabs(qf[i]);

    if (fqi > fqmax) {
      fqmax     = fqi;
      iqlargest = i;
    }
  }

  uquat.milargest = iqlargest;

  ////////////////////////////////////////
  // scale quat component from -1 .. 1
  const T fsqr2d2 = T(1) / sqrtf(T(2));
  ////////////////////////////////////////

  int iq3[3];
  T ftq3[3];

  int iidx = 0;

  static T fmin(100);
  static T fmax(-100);

  for (int i = 0; i < 4; i++) {
    T fqi = fabs(qf[i]);

    if (i != iqlargest) {
      OrkAssert(fqi <= fsqr2d2);
      T fi = qf[i] * fsqr2d2 * T(2);

      if (fi > fmax)
        fmax = fi;
      if (fi < fmin)
        fmin = fi;

      iq3[iidx]  = int(fi * frange);
      ftq3[iidx] = qf[i];

      iidx++;
    }
  }

  uquat.miElem0 = iq3[0];
  uquat.miElem1 = iq3[1];
  uquat.miElem2 = iq3[2];

  OrkAssert(uquat.miElem0 == iq3[0]);
  OrkAssert(uquat.miElem1 == iq3[1]);
  OrkAssert(uquat.miElem2 == iq3[2]);

  uquat.miwsign = (qf[iqlargest] >= T(0));

  ////////////////////////////////

  iidx = 0;

  iidx   = (iidx == iqlargest) ? iidx + 1 : iidx;
  T ftq0 = qf[iidx++];
  iidx   = (iidx == iqlargest) ? iidx + 1 : iidx;
  T ftq1 = qf[iidx++];
  iidx   = (iidx == iqlargest) ? iidx + 1 : iidx;
  T ftq2 = qf[iidx++];

  T ftq012 = ((ftq0 * ftq0) + (ftq1 * ftq1) + (ftq2 * ftq2));

  // 1.0f = ftq0123 + (ftqD*ftqD)
  // ftqD*ftqD = 1.0f - ftq0123

  T ftqD = sqrtf(T(1) - ftq012);

  T ferr = fabs(ftqD - qf[iqlargest]);

  ////////////////////////////////

  return uquat;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::deCompress(QuatCodec uquat) {
  static const T frange  = T((1 << 9) - 1);
  static const T fsqr2d2 = sqrtf(T(2)) / T(2);
  static const T firange = fsqr2d2 / frange;

  int iqlargest = int(uquat.milargest);
  int iqlsign   = int(uquat.miwsign);

  T* pfq = (T*)&this->x;

  int iidx = 0;

  iidx      = (iidx == iqlargest) ? iidx + 1 : iidx;
  pfq[iidx] = T(int(uquat.miElem0)) * firange;
  T fq0     = pfq[iidx++];
  iidx      = (iidx == iqlargest) ? iidx + 1 : iidx;
  pfq[iidx] = T(int(uquat.miElem1)) * firange;
  T fq1     = pfq[iidx++];
  iidx      = (iidx == iqlargest) ? iidx + 1 : iidx;
  pfq[iidx] = T(int(uquat.miElem2)) * firange;
  T fq2     = pfq[iidx++];

  T fq012 = ((fq0 * fq0) + (fq1 * fq1) + (fq2 * fq2));

  pfq[iqlargest] = sqrtf(T(1) - fq012) * (iqlsign ? T(1) : T(-1.0f));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::normalizeInPlace() {
  T x2 = this->x * this->x;
  T y2 = this->y * this->y;
  T z2 = this->z * this->z;
  T w2 = this->w * this->w;

  float isq = T(1) / T(sqrtf(w2 + x2 + y2 + z2));

  this->x *= isq;
  this->y *= isq;
  this->z *= isq;
  this->w *= isq;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T> Quaternion<T>::normalized() const {
  T x2 = this->x * this->x;
  T y2 = this->y * this->y;
  T z2 = this->z * this->z;
  T w2 = this->w * this->w;

  float isq = T(1) / T(sqrtf(w2 + x2 + y2 + z2));

  return Quaternion<T>(this->x*isq,this->y*isq,this->z*isq,this->w*isq);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> std::string Quaternion<T>::formatcn(const std::string named) const {
  std::string rval;
 
  rval += ork::deco::format(fvec3(1,1,1), "%s< ", named.c_str() );

  rval += ork::deco::format(fvec3(1, .5, .5), "x:%g, ", this->x );
  rval += ork::deco::format(fvec3(.5, 1, .5), "y:%g, ", this->y );
  rval += ork::deco::format(fvec3(.5, .5, 1), "z:%g, ", this->z );
  rval += ork::deco::format(fvec3(.6, .6, .6), "w:%g", this->w );

  rval += ork::deco::format(fvec3(1,1,1), ">" );

  return rval;
}


///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////

namespace ork::reflect {

template <> //
inline void ::ork::reflect::ITyped<fquat>::serialize(serdes::node_ptr_t sernode) const {
  using namespace serdes;
  auto serializer        = sernode->_serializer;
  auto instance          = sernode->_ser_instance;
  auto arynode           = serializer->pushNode(_name, serdes::NodeType::ARRAY);
  arynode->_parent       = sernode;
  arynode->_ser_instance = instance;
  fquat value;
  get(value, instance);
  serializeArraySubLeaf(arynode, value.x, 0);
  serializeArraySubLeaf(arynode, value.y, 1);
  serializeArraySubLeaf(arynode, value.z, 2);
  serializeArraySubLeaf(arynode, value.w, 3);
  serializer->popNode(); // pop arraynode
}
template <> //
inline void ::ork::reflect::ITyped<fquat>::deserialize(serdes::node_ptr_t arynode) const {
  using namespace serdes;
  auto deserializer  = arynode->_deserializer;
  auto instance      = arynode->_deser_instance;
  size_t numelements = arynode->_numchildren;
  OrkAssert(numelements == 4);

  fquat outval;
  outval.x = deserializeArraySubLeaf<float>(arynode, 0);
  outval.y = deserializeArraySubLeaf<float>(arynode, 1);
  outval.z = deserializeArraySubLeaf<float>(arynode, 2);
  outval.w = deserializeArraySubLeaf<float>(arynode, 3);
  set(outval, instance);
}
} // namespace ork::reflect
