////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <ork/orkstd.h> // For OrkAssert
#include <ork/math/math_types.h>
#include <ork/math/cvector2.h>
#include <ork/config/config.h>

#include <glm/glm.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

template <typename T> struct Matrix44;
template <typename T> struct Matrix33;
template <typename T> struct Vector4;
template <typename T> struct Vector2;

///////////////////////////////////////////////////////////////////////////////

template <typename T> struct Vector3 final
  : public glm::vec<3, T, glm::defaultp> {

  using base_t = glm::vec<3, T, glm::defaultp>;
  using base4_t = glm::vec<4, T, glm::defaultp>;

  Vector3();
  explicit Vector3(T _x, T _y, T _z); 
  explicit Vector3(T _a); 
  Vector3(const base_t& vec);        
  Vector3(const Vector3& vec);        
  Vector3(const Vector4<T>& vec);
  Vector3(const Vector2<T>& vec);

  Vector3(const kln::point& klein_point);
  Vector3(const kln::direction& klein_direction);

  ~Vector3(){}; 

  base_t asGlmVec3() const {
    return base_t(this->x,this->y,this->z);
  }
  base4_t asGlmVec4() const {
    return base4_t(this->x,this->y,this->z,1);
  }

  kln::point asKleinPoint() const;
  kln::direction asKleinDirection() const;

  void rotateOnX(T rad);
  void rotateOnY(T rad);
  void rotateOnZ(T rad);

  T angle(const Vector3& vec) const; // radians 
  T orientedAngle(const Vector3& vec, const Vector3& refaxis) const; // radians 

  Vector3 saturated() const;
  Vector3 clamped(T min, T max) const;

  T dotWith(const Vector3& vec) const;         
  Vector3 crossWith(const Vector3& vec) const; 

  void normalizeInPlace(); // normalize this vector
  Vector3 normalized() const;
  Vector3 quantized(float v) const;
  
  inline T length() const {
    return magnitude();
  }
  T magnitude() const;                                        
  T magnitudeSquared() const;                                 
  Vector4<T> transform(const Matrix44<T>& matrix) const;    
  Vector3<T> transform(const Matrix33<T>& matrix) const;    
  Vector3<T> transform3x3(const Matrix44<T>& matrix) const; 

  void lerp(const Vector3& from, const Vector3& to, T par); 
  void serp(const Vector3& PA, const Vector3& PB, const Vector3& PC, const Vector3& PD, T par_x, T par_y); // 2D interp

  void set(T _x, T _y, T _z) {
    this->x = _x;
    this->y = _y;
    this->z = _z;
  }

  Vector2<T> xy() const {
    return Vector2<T>(this->x, this->y);
  }
  Vector2<T> xz() const {
    return Vector2<T>(this->x, this->z);
  }

  static Vector3 zero() {
    return Vector3(T(0), T(0), T(0));
  }
  static Vector3 one() {
    return Vector3(T(1), T(1), T(1));
  }

  static T calcTriangularArea(const Vector3& V, const Vector3& N);

  Vector3<T> reflect(const Vector3& N) const; // R = I-(N*2*dot(N,I));

  inline T& operator[](U32 i) {
    T* v = &this->x;
    OrkAssert(i < 3);
    return v[i];
  }

  inline const T& operator[](U32 i) const {
    const T* v = &this->x;
    OrkAssert(i < 3);
    return v[i];
  }

  inline Vector3 operator-() const {
    return Vector3(-this->x, -this->y, -this->z);
  }

  inline Vector3 operator+(const Vector3& b) const {
    return Vector3((this->x + b.x), (this->y + b.y), (this->z + b.z));
  }

  inline Vector3 operator*(const Vector3& b) const {
    return Vector3((this->x * b.x), (this->y * b.y), (this->z * b.z));
  }

  inline Vector3 operator*(T scalar) const {
    return Vector3((this->x * scalar), (this->y * scalar), (this->z * scalar));
  }

  inline Vector3 operator-(const Vector3& b) const {
    return Vector3((this->x - b.x), (this->y - b.y), (this->z - b.z));
  }

  inline Vector3 operator/(const Vector3& b) const {
    return Vector3((this->x / b.x), (this->y / b.y), (this->z / b.z));
  }

  inline Vector3 operator/(T scalar) const {
    return Vector3((this->x / scalar), (this->y / scalar), (this->z / scalar));
  }

  inline void operator+=(const Vector3& b) {
    this->x += b.x;
    this->y += b.y;
    this->z += b.z;
  }

  inline void operator-=(const Vector3& b) {
    this->x -= b.x;
    this->y -= b.y;
    this->z -= b.z;
  }

  inline void operator*=(T scalar) {
    this->x *= scalar;
    this->y *= scalar;
    this->z *= scalar;
  }

  inline void operator*=(const Vector3& b) {
    this->x *= b.x;
    this->y *= b.y;
    this->z *= b.z;
  }

  inline void operator/=(const Vector3& b) {
    this->x /= b.x;
    this->y /= b.y;
    this->z /= b.z;
  }

  inline void operator/=(T scalar) {
    this->x /= scalar;
    this->y /= scalar;
    this->z /= scalar;
  }

  inline bool operator==(const Vector3& b) const {
    return (this->x == b.x && this->y == b.y && this->z == b.z);
  }
  inline bool operator!=(const Vector3& b) const {
    return (this->x != b.x || this->y != b.y || this->z != b.z);
  }

  void setHSV(T h, T s, T v);
  void setRGB(T r, T g, T b) {
    this->x = r;
    this->y = g;
    this->z = b;
  }
  void setYUV(T Y, T U, T V);
  Vector3<T> YUV() const;

  U32 VtxColorAsU32() const;
  U32 ARGBU32() const;
  U32 ABGRU32() const;
  U32 RGBAU32() const;
  U32 BGRAU32() const;
  U16 RGBU16() const;

  void setRGBAU32(U32 uval);
  void setBGRAU32(U32 uval);
  void setARGBU32(U32 uval);
  void setABGRU32(U32 uval);

  Vector3<T> absolute() const;
  Vector3<T> minXYZ( const Vector3<T>& rhs ) const;
  Vector3<T> maxXYZ( const Vector3<T>& rhs ) const;

  Vector2<T> normalOctahedronEncoded() const;
  void decodeNormalOctahedronEncoded(Vector2<T> enc);

  uint64_t RGBAU64() const;
  void setRGBAU64(uint64_t v);

  static const Vector3& Black();
  static const Vector3& DarkGrey();
  static const Vector3& MediumGrey();
  static const Vector3& LightGrey();
  static const Vector3& White();

  static const Vector3& Red();
  static const Vector3& LightRed();
  static const Vector3& Green();
  static const Vector3& LightGreen();
  static const Vector3& Blue();
  static const Vector3& LightBlue();
  static const Vector3& Magenta();
  static const Vector3& Cyan();
  static const Vector3& Yellow();

  static const Vector3 unitX() { return Vector3(T(1),T(0),T(0)); }
  static const Vector3 unitY() { return Vector3(T(0),T(1),T(0)); }
  static const Vector3 unitZ() { return Vector3(T(0),T(0),T(1)); }
  static const Vector3 unitCircleXZ(float radians) { 
    return Vector3(T(sinf(radians)),T(0),T(-cosf(radians)));
  }
  static const Vector3 unitCircleXY(float radians) { 
    return Vector3(T(sinf(radians)),T(-cosf(radians)),T(0));
  }
  static const Vector3 unitCircleYZ(float radians) { 
    return Vector3(T(0),T(sinf(radians)),T(-cosf(radians)));
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

using fvec3       = Vector3<float>;
using dvec3       = Vector3<double>;
using fvec3_ptr_t = std::shared_ptr<fvec3>;
using dvec3_ptr_t = std::shared_ptr<dvec3>;
using fcolor3     = fvec3;

template <>                       //
struct use_custom_serdes<fvec3> { //
  static constexpr bool enable = true;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////

template <typename T> inline ork::Vector3<T> operator*(T scalar, const ork::Vector3<T>& b) {
  return ork::Vector3<T>((scalar * b.x), (scalar * b.y), (scalar * b.z));
}

///////////////////////////////////////////////////////////////////////////////
