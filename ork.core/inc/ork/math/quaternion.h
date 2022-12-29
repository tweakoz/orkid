////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <ork/config/config.h>
#include <ork/orktypes.h>
#include <ork/math/math_types.h>

#define GLM_FORCE_PURE
#define GLM_FORCE_XYZW_ONLY
#define GLM_FORCE_UNRESTRICTED_GENTYPE

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

struct QuatCodec {
  unsigned int milargest : 2;
  unsigned int miwsign : 1;
  int miElem0 : 16;
  int miElem1 : 16;
  int miElem2 : 16;

  QuatCodec()
      : milargest(0)
      , miwsign(0)
      , miElem0(0)
      , miElem1(0)
      , miElem2(0) {
  }
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> struct Matrix44;
template <typename T> struct Matrix33;
template <typename T> struct Vector4;
template <typename T> struct Vector3;

///////////////////////////////////////////////////////////////////////////////

template <typename T> struct Quaternion final 
  : public glm::qua<T, glm::defaultp> {

  using base_t = glm::qua<T, glm::defaultp>;

  //////////////////////////////////////////////////////
  // constructors/destructors
  //////////////////////////////////////////////////////

  Quaternion();
  Quaternion(T _x, T _y, T _z, T _w);
  Quaternion(const base_t& base);
  Quaternion(const Vector3<T>& axis, float angle);

  Quaternion(const Matrix44<T>& matrix);
  Quaternion(const Matrix33<T>& matrix);
  Quaternion(const kln::rotor& rotor);

  ~Quaternion();

  //////////////////////////////////////////////////////
  // converters
  //////////////////////////////////////////////////////

  const base_t& asGlmQuat() const;

  void fromMatrix(const Matrix44<T>& matrix);
  Matrix44<T> toMatrix() const;

  void fromMatrix3(const Matrix33<T>& matrix);
  Matrix33<T> toMatrix3() const;

  QuatCodec compress() const;
  void deCompress(QuatCodec qc);

  Vector3<T> asEuler() const;

  kln::rotor asKleinRotor() const;

  //////////////////////////////////////////////////////
  // operators
  //////////////////////////////////////////////////////

  Quaternion<T> operator*(const Quaternion<T>& rhs) const;
  Quaternion<T> operator+(const Quaternion<T>& rhs) const;
  bool operator==(const Quaternion<T>& rhs) const;

  //////////////////////////////////////////////////////
  // dumps
  //////////////////////////////////////////////////////

  std::string formatcn(const std::string named) const;
  void dump();

  //////////////////////////////////////////////////////
  // math operations
  //////////////////////////////////////////////////////

  void scale(T scalar);
  Quaternion multiply(const Quaternion& q) const;
  void divide(Quaternion& a);
  void subtract(Quaternion& a);
  void add(Quaternion& a);

  Vector4<T> toAxisAngle() const;
  void fromAxisAngle(const Vector4<T>& v);

  T norm() const;
  Quaternion conjugate() const;
  Quaternion inverse() const;
  void inverseOf( const Quaternion& of);

  Quaternion square() const;
  Quaternion negate() const;

  Vector3<T> transform(const Vector3<T>& point) const;

  void normalizeInPlace();
  Quaternion normalized() const;

  static Quaternion lerp(const Quaternion& a, const Quaternion& b, T alpha);
  static Quaternion slerp(const Quaternion& a, const Quaternion& b, T alpha);

  void setToIdentity();
  static Quaternion shortestRotationArc(Vector4<T> v0, Vector4<T> v1);

  void correctionQuaternion(const Quaternion& from, const Quaternion& to);

  /////////

  #if defined(GLM_FORCE_QUAT_DATA_XYZW)
  T* asArray() { return & this->x; }
  #else
  T* asArray() { return & this->w; }
  #endif

};

using fquat       = Quaternion<float>;
using fquat_ptr_t = std::shared_ptr<fquat>;
using dquat       = Quaternion<double>;

template <>                       //
struct use_custom_serdes<fquat> { //
  static constexpr bool enable = true;
};


template <typename T> struct DualQuaternion final 
  : public glm::tdualquat<T, glm::defaultp> {

  using base_t = glm::tdualquat<T, glm::defaultp>;

  DualQuaternion();
  DualQuaternion(const base_t& base);
  DualQuaternion(const kln::motor& motor);

  const base_t& asGlmDualQuaternion() const;
  kln::motor asKleinMotor() const;

};

using fdualquat       = DualQuaternion<float>;
using fdualquat_ptr_t = std::shared_ptr<fdualquat>;
using ddualquat       = DualQuaternion<double>;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork
