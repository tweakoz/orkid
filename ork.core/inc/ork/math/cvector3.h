////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/orkstd.h> // For OrkAssert
#include <ork/math/cvector2.h>

#include <ork/config/config.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

template <typename T> class Matrix44;
template <typename T> class Matrix33;
template <typename T> class Vector4;
template <typename T> class Vector2;

///////////////////////////////////////////////////////////////////////////////

template <typename T> class Vector3 {
  static T Sin(T);
  static T Cos(T);
  static T Sqrt(T);
  static T Epsilon();
  static T Abs(T);

public:
  Vector3();
  explicit Vector3(T _x, T _y, T _z); // constructor from 3 floats
  Vector3(const Vector3& vec);        // constructor from a vector
  Vector3(const Vector4<T>& vec);
  Vector3(const Vector2<T>& vec);
  Vector3(U32 uval) {
    SetRGBAU32(uval);
  }
  ~Vector3(){}; // default destructor, does nothing

  void RotateX(T rad);
  void RotateY(T rad);
  void RotateZ(T rad);

  Vector3 saturated() const;
  Vector3 clamped(float min, float max) const;

  T Dot(const Vector3& vec) const;         // dot product of two vectors
  Vector3 Cross(const Vector3& vec) const; // cross product of two vectors

  void Normalize(void); // normalize this vector
  Vector3 Normal() const;

  inline T length() const {
    return Mag();
  }
  T Mag(void) const;                                        // return magnitude of this vector
  T MagSquared(void) const;                                 // return magnitude of this vector squared
  Vector4<T> Transform(const Matrix44<T>& matrix) const;    // transform this vector
  Vector3<T> Transform(const Matrix33<T>& matrix) const;    // transform this vector
  Vector3<T> Transform3x3(const Matrix44<T>& matrix) const; // transform this vector

  void Lerp(const Vector3& from, const Vector3& to, T par);
  void Serp(const Vector3& PA, const Vector3& PB, const Vector3& PC, const Vector3& PD, T Par);

  T GetX(void) const {
    return (x);
  }
  T GetY(void) const {
    return (y);
  }
  T GetZ(void) const {
    return (z);
  }

  void Set(T _x, T _y, T _z) {
    x = _x;
    y = _y;
    z = _z;
  }
  void SetX(T _x) {
    x = _x;
  }
  void SetY(T _y) {
    y = _y;
  }
  void SetZ(T _z) {
    z = _z;
  }

  Vector2<T> GetXY(void) const {
    return Vector2<T>(x, y);
  }
  Vector2<T> GetXZ(void) const {
    return Vector2<T>(x, z);
  }

  static Vector3 Zero(void) {
    return Vector3(T(0), T(0), T(0));
  }
  static Vector3 UnitX(void) {
    return Vector3(T(1), T(0), T(0));
  }
  static Vector3 UnitY(void) {
    return Vector3(T(0), T(1), T(0));
  }
  static Vector3 UnitZ(void) {
    return Vector3(T(0), T(0), T(1));
  }
  static Vector3 One(void) {
    return Vector3(T(1), T(1), T(1));
  }

  static T CalcTriArea(const Vector3& V, const Vector3& N);

  Vector3<T> Reflect(const Vector3& N) const; // R = I-(N*2*dot(N,I));

  void SetXYZ(T x, T y, T z) {
    SetX(x);
    SetY(y);
    SetZ(z);
  }

  inline T& operator[](U32 i) {
    T* v = &x;
    OrkAssert(i < 3);
    return v[i];
  }

  inline const T& operator[](U32 i) const {
    const T* v = &x;
    OrkAssert(i < 3);
    return v[i];
  }

  inline Vector3 operator-() const {
    return Vector3(-x, -y, -z);
  }

  inline Vector3 operator+(const Vector3& b) const {
    return Vector3((x + b.x), (y + b.y), (z + b.z));
  }

  inline Vector3 operator*(const Vector3& b) const {
    return Vector3((x * b.x), (y * b.y), (z * b.z));
  }

  inline Vector3 operator*(T scalar) const {
    return Vector3((x * scalar), (y * scalar), (z * scalar));
  }

  inline Vector3 operator-(const Vector3& b) const {
    return Vector3((x - b.x), (y - b.y), (z - b.z));
  }

  inline Vector3 operator/(const Vector3& b) const {
    return Vector3((x / b.x), (y / b.y), (z / b.z));
  }

  inline Vector3 operator/(T scalar) const {
    return Vector3((x / scalar), (y / scalar), (z / scalar));
  }

  inline void operator+=(const Vector3& b) {
    x += b.x;
    y += b.y;
    z += b.z;
  }

  inline void operator-=(const Vector3& b) {
    x -= b.x;
    y -= b.y;
    z -= b.z;
  }

  inline void operator*=(T scalar) {
    x *= scalar;
    y *= scalar;
    z *= scalar;
  }

  inline void operator*=(const Vector3& b) {
    x *= b.x;
    y *= b.y;
    z *= b.z;
  }

  inline void operator/=(const Vector3& b) {
    x /= b.x;
    y /= b.y;
    z /= b.z;
  }

  inline void operator/=(T scalar) {
    x /= scalar;
    y /= scalar;
    z /= scalar;
  }

  inline bool operator==(const Vector3& b) const {
    return (x == b.x && y == b.y && z == b.z);
  }
  inline bool operator!=(const Vector3& b) const {
    return (x != b.x || y != b.y || z != b.z);
  }

  void SetHSV(T h, T s, T v);
  void SetRGB(T r, T g, T b) {
    SetXYZ(r, g, b);
  }
  void setYUV(T Y, T U, T V);

  U32 GetVtxColorAsU32(void) const;
  U32 GetARGBU32(void) const;
  U32 GetABGRU32(void) const;
  U32 GetRGBAU32(void) const;
  U32 GetBGRAU32(void) const;
  U16 GetRGBU16(void) const;

  void SetRGBAU32(U32 uval);
  void SetBGRAU32(U32 uval);
  void SetARGBU32(U32 uval);
  void SetABGRU32(U32 uval);

  uint64_t GetRGBAU64(void) const;
  void SetRGBAU64(uint64_t v);

  static const Vector3& Black(void);
  static const Vector3& DarkGrey(void);
  static const Vector3& MediumGrey(void);
  static const Vector3& LightGrey(void);
  static const Vector3& White(void);

  static const Vector3& Red(void);
  static const Vector3& Green(void);
  static const Vector3& Blue(void);
  static const Vector3& Magenta(void);
  static const Vector3& Cyan(void);
  static const Vector3& Yellow(void);

  T* GetArray(void) const {
    return const_cast<T*>(&x);
  }

  template <typename U> static Vector3 FromVector3(Vector3<U> vec) {
    return Vector3(T::FromFX(vec.GetX().FXCast()), T::FromFX(vec.GetY().FXCast()), T::FromFX(vec.GetZ().FXCast()));
  }

  T x; // x component of this vector
  T y; // y component of this vector
  T z; // z component of this vector
};

typedef Vector3<float> fvec3;
typedef Vector3<float> fvec3;
typedef Vector3<double> dvec3;
typedef fvec3 CColor3;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////

template <typename T> inline ork::Vector3<T> operator*(T scalar, const ork::Vector3<T>& b) {
  return ork::Vector3<T>((scalar * b.GetX()), (scalar * b.GetY()), (scalar * b.GetZ()));
}

///////////////////////////////////////////////////////////////////////////////
