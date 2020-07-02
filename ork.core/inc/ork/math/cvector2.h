////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once
///////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <ork/orkstd.h>   // For OrkAssert
#include <ork/orktypes.h> // for U32 etc

#include <ork/config/config.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

template <typename T> class Vector3;

template <typename T> class Vector2 {

  static T Sin(T);
  static T Cos(T);
  static T Sqrt(T);
  static T Epsilon();
  static T Abs(T);

public:
  Vector2();
  explicit Vector2(T x, T y);
  Vector2(const Vector2& vec);
  Vector2(const Vector3<T>& vec);
  ~Vector2(){}; // default destructor, does nothing

  void Rotate(T rad);

  T Dot(const Vector2& vec) const; // dot product of two vectors
  T PerpDot(const Vector2& vec) const;

  void Normalize(void); // normalize this vector
  Vector2 Normal() const;

  T Mag(void) const; // return magnitude of this vector
  T Length(void) const {
    return Mag();
  }
  T MagSquared(void) const; // return magnitude of this vector squared

  void Lerp(const Vector2& from, const Vector2& to, T par);
  void Serp(const Vector2& PA, const Vector2& PB, const Vector2& PC, const Vector2& PD, T Par);

  T GetX(void) const {
    return (x);
  }
  T GetY(void) const {
    return (y);
  }

  void Set(T _x, T _y) {
    x = _x;
    y = _y;
  }
  void SetX(T _x) {
    x = _x;
  }
  void SetY(T _y) {
    y = _y;
  }

  static Vector2 Zero(void) {
    return Vector2(T(0), T(0));
  }

  inline T& operator[](U32 i) {
    T* v = &x;
    OrkAssert(i < 2);
    return v[i];
  }

  inline Vector2 operator-() const {
    return Vector2(-x, -y);
  }

  inline Vector2 operator+(const Vector2& b) const {
    return Vector2((x + b.x), (y + b.y));
  }

  inline Vector2 operator*(const Vector2& b) const {
    return Vector2((x * b.x), (y * b.y));
  }

  inline Vector2 operator*(T scalar) const {
    return Vector2((x * scalar), (y * scalar));
  }

  inline Vector2 operator-(const Vector2& b) const {
    return Vector2((x - b.x), (y - b.y));
  }

  inline Vector2 operator/(const Vector2& b) const {
    return Vector2((x / b.x), (y / b.y));
  }

  inline Vector2 operator/(T scalar) const {
    return Vector2((x / scalar), (y / scalar));
  }

  inline void operator+=(const Vector2& b) {
    x += b.x;
    y += b.y;
  }

  inline void operator-=(const Vector2& b) {
    x -= b.x;
    y -= b.y;
  }

  inline void operator*=(T scalar) {
    x *= scalar;
    y *= scalar;
  }

  inline void operator*=(const Vector2& b) {
    x *= b.x;
    y *= b.y;
  }

  inline void operator/=(const Vector2& b) {
    x /= b.x;
    y /= b.y;
  }

  inline void operator/=(T scalar) {
    x /= scalar;
    y /= scalar;
  }

  inline bool operator==(const Vector2& b) const {
    return (x == b.x && y == b.y);
  }
  inline bool operator!=(const Vector2& b) const {
    return (x != b.x || y != b.y);
  }

  T* GetArray(void) const {
    return const_cast<T*>(&x);
  }

  /*template <typename U>
  static Vector2 FromVector2(Vector2<U> vec)
  {
      return Vector2(T::FromFX(vec.GetX().FXCast()),
                      T::FromFX(vec.GetY().FXCast()));
  }*/

  T x; // x component of this vector
  T y; // y component of this vector
};

using fvec2       = Vector2<float>;
using dvec2       = Vector2<double>;
using fvec2_ptr_t = std::shared_ptr<fvec2>;
using dvec2_ptr_t = std::shared_ptr<dvec2>;

template <>                       //
struct use_custom_serdes<fvec2> { //
  static constexpr bool enable = true;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////

template <typename T> inline ork::Vector2<T> operator*(T scalar, const ork::Vector2<T>& b) {
  return ork::Vector2<T>((scalar * b.GetX()), (scalar * b.GetY()));
}
