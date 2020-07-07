////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <ork/config/config.h>
#include <ork/orktypes.h>

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
      , miElem0(0)
      , miElem1(0)
      , miElem2(0)
      , miwsign(0) {
  }
};

///////////////////////////////////////////////////////////////////////////////
template <typename T> class Matrix44;
template <typename T> class Matrix33;
template <typename T> class Vector4;
template <typename T> class Vector3;

template <typename T> struct Quaternion {
  /////////

  Quaternion(void) {
    Identity();
  }

  Quaternion(T _x, T _y, T _z, T _w);
  Quaternion(const Vector3<T>& axis, float angle);

  Quaternion(const Matrix44<T>& matrix);
  Quaternion(const Matrix33<T>& matrix);

  ~Quaternion() {
  }

  Quaternion<T> operator*(const Quaternion<T>& rhs) const;

  /////////

  const T& GetX() const {
    return x;
  }
  const T& GetY() const {
    return y;
  }
  const T& GetZ() const {
    return z;
  }
  const T& width() const {
    return w;
  }

  /////////

  void FromMatrix(const Matrix44<T>& matrix);
  Matrix44<T> ToMatrix(void) const;

  void FromMatrix3(const Matrix33<T>& matrix);
  Matrix33<T> ToMatrix3(void) const;

  void Scale(T scalar);
  Quaternion Multiply(const Quaternion& q) const;
  void Divide(Quaternion& a);
  void Sub(Quaternion& a);
  void Add(Quaternion& a);

  Vector4<T> toAxisAngle(void) const;
  void fromAxisAngle(const Vector4<T>& v);

  T Magnitude(void);
  Quaternion Conjugate(Quaternion& a);
  Quaternion Square(void);
  Quaternion Negate(void);

  void Normalize();

  static Quaternion Lerp(const Quaternion& a, const Quaternion& b, T alpha);
  static Quaternion Slerp(const Quaternion& a, const Quaternion& b, T alpha);

  void Identity(void);
  void ShortestRotationArc(Vector4<T> v0, Vector4<T> v1);

  Vector3<T> toEuler() const;

  /////////

  void dump(void);

  QuatCodec Compress(void) const;
  void DeCompress(QuatCodec qc);

  T x, y, z, w;
};

using fquat       = Quaternion<float>;
using fquat_ptr_t = std::shared_ptr<fquat>;

template <>                       //
struct use_custom_serdes<fquat> { //
  static constexpr bool enable = true;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork
