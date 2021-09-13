////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <memory>
#include <ork/config/config.h>
#include <ork/math/cvector3.h>
#include <ork/orkstd.h>   // For OrkAssert
#include <ork/orktypes.h> // For float
///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

template <typename T> class Matrix44;
template <typename T> class Vector3;

template <typename T> class Vector4 {
  static T Sin(T);
  static T Cos(T);
  static T Sqrt(T);
  static T Epsilon();
  static T Abs(T);

public:
  Vector4();
  explicit Vector4(T x, T y, T z, T w = T(1.0f)); // constructor from 3 floats
  Vector4(const Vector4& vec);                    // constructor from a vector
  Vector4(const Vector3<T>& vec, T w = T(1.0f));  // constructor from a vector
  Vector4(U32 uval) {
    SetRGBAU32(uval);
  }
  ~Vector4(){}; // default destructor, does nothing

  void RotateX(T rad);
  void RotateY(T rad);
  void RotateZ(T rad);

  Vector4 Saturate() const;
  T Dot(const Vector4& vec) const;         // dot product of two vectors
  Vector4 Cross(const Vector4& vec) const; // cross product of two vectors

  void Normalize(void); // normalize this vector
  Vector4 Normal() const;

  T Mag(void) const;                                  // return magnitude of this vector
  T MagSquared(void) const;                           // return magnitude of this vector squared
  Vector4 Transform(const Matrix44<T>& matrix) const; // transform this vector
  void PerspectiveDivide(void);
  Vector4 perspectiveDivided(void) const;

  void lerp(const Vector4& from, const Vector4& to, T par);
  void serp(const Vector4& PA, const Vector4& PB, const Vector4& PC, const Vector4& PD, T par_x, T par_y);

  void set(T _x, T _y, T _z, T _w) {
    x = _x;
    y = _y;
    z = _z;
    w = _w;
  }
  void set(T _x, T _y, T _z) {
    x = _x;
    y = _y;
    z = _z;
    w = (T)1.0f;
  }
  void setX(T _x) {
    x = _x;
  }
  void setY(T _y) {
    y = _y;
  }
  void setZ(T _z) {
    z = _z;
  }
  void setW(T _w) {
    w = _w;
  }

  Vector3<T> xyz(void) const {
    return Vector3<T>(*this);
  }
  Vector3<T> xzy(void) const {
    return Vector3<T>(x, z, y);
  }
  Vector3<T> zxy(void) const {
    return Vector3<T>(z, x, y);
  }
  Vector3<T> zyx(void) const {
    return Vector3<T>(z, y, x);
  }
  Vector3<T> yxz(void) const {
    return Vector3<T>(y, x, z);
  }
  Vector3<T> yzx(void) const {
    return Vector3<T>(y, z, x);
  }

  static Vector4 Zero(void) {
    return Vector4(T(0), T(0), T(0), T(0));
  }
  static T calcTriangularArea(const Vector4& V0, const Vector4& V1, const Vector4& V2, const Vector4& N);

  void SetXYZ(T _x, T _y, T _z) {
    setX(_x);
    setY(_y);
    setZ(_z);
  }

  inline T& operator[](U32 i) {
    T* v = &x;
    OrkAssert(i < 4);
    return v[i];
  }

  inline const T& operator[](U32 i) const {
    const T* v = &x;
    OrkAssert(i < 4);
    return v[i];
  }

  inline Vector4 operator-() const {
    return Vector4(-x, -y, -z, -w);
  }

  inline Vector4 operator+(const Vector4& b) const {
    return Vector4((x + b.x), (y + b.y), (z + b.z), (w + b.w));
  }

  inline Vector4 operator*(const Vector4& b) const {
    return Vector4((x * b.x), (y * b.y), (z * b.z), (w * b.w));
  }

  inline Vector4 operator*(T scalar) const {
    return Vector4((x * scalar), (y * scalar), (z * scalar), (w * scalar));
  }

  inline Vector4 operator-(const Vector4& b) const {
    return Vector4((x - b.x), (y - b.y), (z - b.z), (w - b.w));
  }

  inline Vector4 operator/(const Vector4& b) const {
    return Vector4((x / b.x), (y / b.y), (z / b.z), (w / b.w));
  }

  inline Vector4 operator/(T scalar) const {
    return Vector4((x / scalar), (y / scalar), (z / scalar), (w / scalar));
  }

  inline void operator+=(const Vector4& b) {
    x += b.x;
    y += b.y;
    z += b.z;
    w += b.w;
  }

  inline void operator-=(const Vector4& b) {
    x -= b.x;
    y -= b.y;
    z -= b.z;
    w -= b.w;
  }

  inline void operator*=(T scalar) {
    x *= scalar;
    y *= scalar;
    z *= scalar;
    w *= scalar;
  }

  inline void operator*=(const Vector4& b) {
    x *= b.x;
    y *= b.y;
    z *= b.z;
    w *= b.w;
  }

  inline void operator/=(const Vector4& b) {
    x /= b.x;
    y /= b.y;
    z /= b.z;
    w /= b.w;
  }

  inline void operator/=(T scalar) {
    x /= scalar;
    y /= scalar;
    z /= scalar;
    w /= scalar;
  }

  inline bool operator==(const Vector4& b) const {
    return (x == b.x && y == b.y && z == b.z && w == b.w);
  }
  inline bool operator!=(const Vector4& b) const {
    return (x != b.x || y != b.y || z != b.z || w != b.w);
  }

  void SetHSV(T h, T s, T v);
  void SetRGB(T r, T g, T b) {
    SetXYZ(r, g, b);
  }
  U32 GetABGRU32(void) const;
  U32 GetARGBU32(void) const;
  U32 GetRGBAU32(void) const;
  U32 GetBGRAU32(void) const;
  U16 GetRGBU16(void) const;
  U32 GetVtxColorAsU32(void) const;

  void SetRGBAU32(U32 uval);
  void SetBGRAU32(U32 uval);
  void SetARGBU32(U32 uval);
  void SetABGRU32(U32 uval);

  uint64_t GetRGBAU64(void) const;
  void SetRGBAU64(uint64_t v);

  static const Vector4& Black(void);
  static const Vector4& DarkGrey(void);
  static const Vector4& MediumGrey(void);
  static const Vector4& LightGrey(void);
  static const Vector4& White(void);

  static const Vector4& Red(void);
  static const Vector4& Green(void);
  static const Vector4& Blue(void);
  static const Vector4& Magenta(void);
  static const Vector4& Cyan(void);
  static const Vector4& Yellow(void);

  T* asArray(void) const {
    return const_cast<T*>(&x);
  }

public:
  T x, y, z, w; 
};

using fvec4       = Vector4<float>;
using dvec4       = Vector4<double>;
using fvec4_ptr_t = std::shared_ptr<fvec4>;
using dvec4_ptr_t = std::shared_ptr<dvec4>;
using fcolor4     = fvec4;

template <>                       //
struct use_custom_serdes<fvec4> { //
  static constexpr bool enable = true;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////

template <typename T> inline ork::Vector4<T> operator*(T scalar, const ork::Vector4<T>& b) {
  return ork::Vector4<T>((scalar * b.x), (scalar * b.y), (scalar * b.z));
}

///////////////////////////////////////////////////////////////////////////////
