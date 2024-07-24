////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
///////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <ork/orkstd.h>   // For OrkAssert
#include <ork/orktypes.h> // for U32 etc

#include <ork/config/config.h>

#include <ork/math/math_types.h>
#include <glm/glm.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

template <typename T> struct Vector3;

template <typename T> struct Vector2 final
  : public glm::vec<2, T, glm::defaultp> {

  constexpr static int knumelements = 2;
  using element_t = T;
  using base_t = glm::vec<2, T, glm::defaultp>;

  Vector2();
  explicit Vector2(T _x, T _y);
  Vector2(const Vector2& vec);
  Vector2(const Vector3<T>& vec);
  ~Vector2(){}; 

  void rotate(T rad);
  static Vector2 fromScalar(T _x);

  Vector2 yx() const { return Vector2(this->y,this->x); }

  T angle(const Vector2& vec) const; 
  T orientedAngle(const Vector2& vec) const; 

  T dotWith(const Vector2& vec) const; 
  T perpDotWith(const Vector2& vec) const;

  void normalizeInPlace(); 
  Vector2 normalized() const;

  T magnitude() const; 
  T length() const {
    return magnitude();
  }
  T magnitudeSquared() const; 

  void lerp(const Vector2& from, const Vector2& to, T par);
  void serp(const Vector2& PA, const Vector2& PB, const Vector2& PC, const Vector2& PD, T par_x, T par_y);

  Vector2 floor() const;
  Vector2 fract() const;

  bool isNan() const;
  
  static Vector2 zero() {
    return Vector2(T(0), T(0));
  }

  inline T& operator[](U32 i) {
    T* v = &this->x;
    OrkAssert(i < 2);
    return v[i];
  }

  inline Vector2 operator-() const {
    return Vector2(-this->x, -this->y);
  }

  inline Vector2 operator+(const Vector2& b) const {
    return Vector2((this->x + b.x), (this->y + b.y));
  }

  inline Vector2 operator*(const Vector2& b) const {
    return Vector2((this->x * b.x), (this->y * b.y));
  }

  inline Vector2 operator*(T scalar) const {
    return Vector2((this->x * scalar), (this->y * scalar));
  }

  inline Vector2 operator-(const Vector2& b) const {
    return Vector2((this->x - b.x), (this->y - b.y));
  }

  inline Vector2 operator/(const Vector2& b) const {
    return Vector2((this->x / b.x), (this->y / b.y));
  }

  inline Vector2 operator/(T scalar) const {
    return Vector2((this->x / scalar), (this->y / scalar));
  }

  inline void operator+=(const Vector2& b) {
    this->x += b.x;
    this->y += b.y;
  }

  inline void operator-=(const Vector2& b) {
    this->x -= b.x;
    this->y -= b.y;
  }

  inline void operator*=(T scalar) {
    this->x *= scalar;
    this->y *= scalar;
  }

  inline void operator*=(const Vector2& b) {
    this->x *= b.x;
    this->y *= b.y;
  }

  inline void operator/=(const Vector2& b) {
    this->x /= b.x;
    this->y /= b.y;
  }

  inline void operator/=(T scalar) {
    this->x /= scalar;
    this->y /= scalar;
  }

  inline bool operator==(const Vector2& b) const {
    return (this->x == b.x && this->y == b.y);
  }
  inline bool operator!=(const Vector2& b) const {
    return (this->x != b.x || this->y != b.y);
  }

  uint64_t hash(T quantization) const;
  
  inline Vector2 quantized(float v) const {
    Vector2 rval;
    rval.x = float(int(this->x*v))/v;
    rval.y = float(int(this->y*v))/v;
    return rval;    
  }

  T* asArray() const {
    return const_cast<T*>(&this->x);
  }

private:

  static T Sin(T);
  static T Cos(T);
  static T Sqrt(T);
  static T Epsilon();
  static T Abs(T);

};

using fvec2       = Vector2<float>;
using dvec2       = Vector2<double>;
using fvec2_ptr_t = std::shared_ptr<fvec2>;
using dvec2_ptr_t = std::shared_ptr<dvec2>;

template <>                       //
struct use_custom_serdes<fvec2> { //
  static constexpr bool enable = true;
};


fvec2 dvec2_to_fvec2(const dvec2& dvec);
dvec2 fvec2_to_dvec2(const fvec2& dvec);

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////

template <typename T> inline ork::Vector2<T> operator*(T scalar, const ork::Vector2<T>& b) {
  return ork::Vector2<T>((scalar * b.x), (scalar * b.y));
}
