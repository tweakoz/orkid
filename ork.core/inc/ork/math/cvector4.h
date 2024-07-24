////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <memory>
#include <ork/config/config.h>
#include <ork/math/cvector3.h>
#include <ork/orkstd.h>   // For OrkAssert
#include <ork/orktypes.h> // For float
#include <ork/math/math_types.h>

#include <glm/glm.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

template <typename T> struct Matrix44;
template <typename T> struct Vector3;

template <typename T> struct Vector4 final
  : public glm::vec<4, T, glm::defaultp> {

  constexpr static int knumelements = 4;
  using element_t = T;
  using base_t = glm::vec<4, T, glm::defaultp>;

  Vector4();
  Vector4(T scalar);        
  explicit Vector4(T _x, T _y, T _z, T _w = T(1)); 
  Vector4(const base_t& vec);        
  Vector4(const Vector4& vec);                    
  Vector4(const Vector3<T>& vec, T w = T(1.0f));  

  //explicit Vector4(U32 uval) {
    //setRGBAU32(uval);
  //}

  ~Vector4(){}; 

  const base_t& asGlmVec4() const {
    return *this;
  }

  void rotateOnX(T rad);
  void rotateOnY(T rad);
  void rotateOnZ(T rad);

  Vector4 saturated() const;
  T dotWith(const Vector4& vec) const;
  Vector4 crossWith(const Vector4& vec) const; 

  void normalizeInPlace(); 
  Vector4 normalized() const;

  T magnitude() const;                                  
  T magnitudeSquared() const;                           
  Vector4 transform(const Matrix44<T>& matrix) const; 
  
  void perspectiveDivideInPlace();
  Vector4 perspectiveDivided() const;

  bool isNan() const;

  void lerp(const Vector4& from, const Vector4& to, T par);
  void serp(const Vector4& PA, const Vector4& PB, const Vector4& PC, const Vector4& PD, T par_x, T par_y);

  void set(T _x, T _y, T _z, T _w) {
    this->x = _x;
    this->y = _y;
    this->z = _z;
    this->w = _w;
  }
  void set(T _x, T _y, T _z) {
    this->x = _x;
    this->y = _y;
    this->z = _z;
    this->w = T(1);
  }

  Vector3<T> xyz() const {
    return Vector3<T>(*this);
  }
  Vector3<T> xzy() const {
    return Vector3<T>(this->x, this->z, this->y);
  }
  Vector3<T> zxy() const {
    return Vector3<T>(this->z, this->x, this->y);
  }
  Vector3<T> zyx() const {
    return Vector3<T>(this->z, this->y, this->x);
  }
  Vector3<T> yxz() const {
    return Vector3<T>(this->y, this->x, this->z);
  }
  Vector3<T> yzx() const {
    return Vector3<T>(this->y, this->z, this->x);
  }
  Vector4<T> rgba_to_abgr() const {
    return Vector4<T>(this->w, this->z, this->y, this->x);
  }

  static Vector4 zero() {
    return Vector4(T(0), T(0), T(0), T(0));
  }
  static T calcTriangularArea(const Vector4& V0, const Vector4& V1, const Vector4& V2, const Vector4& N);

  void setXYZ(T _x, T _y, T _z) {
    this->x = (_x);
    this->y = (_y);
    this->z = (_z);
  }

  inline T& operator[](U32 i) {
    T* v = &this->x;
    OrkAssert(i < 4);
    return v[i];
  }

  inline const T& operator[](U32 i) const {
    const T* v = &this->x;
    OrkAssert(i < 4);
    return v[i];
  }

  inline Vector4 operator-() const {
    return Vector4(-this->x, -this->y, -this->z, -this->w);
  }

  inline Vector4 operator+(const Vector4& b) const {
    return Vector4((this->x + b.x), (this->y + b.y), (this->z + b.z), (this->w + b.w));
  }

  inline Vector4 operator*(const Vector4& b) const {
    return Vector4((this->x * b.x), (this->y * b.y), (this->z * b.z), (this->w * b.w));
  }

  inline Vector4 operator*(T scalar) const {
    return Vector4((this->x * scalar), (this->y * scalar), (this->z * scalar), (this->w * scalar));
  }

  inline Vector4 operator-(const Vector4& b) const {
    return Vector4((this->x - b.x), (this->y - b.y), (this->z - b.z), (this->w - b.w));
  }

  inline Vector4 operator/(const Vector4& b) const {
    return Vector4((this->x / b.x), (this->y / b.y), (this->z / b.z), (this->w / b.w));
  }

  inline Vector4 operator/(T scalar) const {
    return Vector4((this->x / scalar), (this->y / scalar), (this->z / scalar), (this->w / scalar));
  }

  inline void operator+=(const Vector4& b) {
    this->x += b.x;
    this->y += b.y;
    this->z += b.z;
    this->w += b.w;
  }

  inline void operator-=(const Vector4& b) {
    this->x -= b.x;
    this->y -= b.y;
    this->z -= b.z;
    this->w -= b.w;
  }

  inline void operator*=(T scalar) {
    this->x *= scalar;
    this->y *= scalar;
    this->z *= scalar;
    this->w *= scalar;
  }

  inline void operator*=(const Vector4& b) {
    this->x *= b.x;
    this->y *= b.y;
    this->z *= b.z;
    this->w *= b.w;
  }

  inline void operator/=(const Vector4& b) {
    this->x /= b.x;
    this->y /= b.y;
    this->z /= b.z;
    this->w /= b.w;
  }

  inline void operator/=(T scalar) {
    this->x /= scalar;
    this->y /= scalar;
    this->z /= scalar;
    this->w /= scalar;
  }

  inline bool operator==(const Vector4& b) const {
    return (this->x == b.x && this->y == b.y && this->z == b.z && this->w == b.w);
  }
  inline bool operator!=(const Vector4& b) const {
    return (this->x != b.x || this->y != b.y || this->z != b.z || this->w != b.w);
  }

  uint64_t hash(T quantization) const;

  inline Vector4 quantized(float v) const {
    Vector4 rval;
    rval.x = float(int(this->x*v))/v;
    rval.y = float(int(this->y*v))/v;
    rval.z = float(int(this->z*v))/v;
    rval.w = float(int(this->w*v))/v;
    return rval;    
  }

  void setHSV(T h, T s, T v);
  void setRGB(T r, T g, T b) {
    setXYZ(r, g, b);
  }
  U32 ABGRU32() const;
  U32 ARGBU32() const;
  U32 RGBAU32() const;
  U32 BGRAU32() const;
  U16 RGBU16() const;
  U32 vertexColorU32() const;

  void setRGBAU32(U32 uval);
  void setBGRAU32(U32 uval);
  void setARGBU32(U32 uval);
  void setABGRU32(U32 uval);

  uint64_t RGBAU64() const;
  void setRGBAU64(uint64_t v);

  static const Vector4& Black();
  static const Vector4& DarkGrey();
  static const Vector4& MediumGrey();
  static const Vector4& LightGrey();
  static const Vector4& White();

  static const Vector4& Red();
  static const Vector4& Green();
  static const Vector4& Blue();
  static const Vector4& Magenta();
  static const Vector4& Cyan();
  static const Vector4& Yellow();

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

using fvec4       = Vector4<float>;
using dvec4       = Vector4<double>;
using fvec4_ptr_t = std::shared_ptr<fvec4>;
using dvec4_ptr_t = std::shared_ptr<dvec4>;
using fcolor4     = fvec4;

template <>                       //
struct use_custom_serdes<fvec4> { //
  static constexpr bool enable = true;
};

fvec4 dvec4_to_fvec4(const dvec4& dvec);
dvec4 fvec4_to_dvec4(const fvec4& dvec);

struct u32vec4{
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t z = 0;
  uint32_t w = 0;
};
using u32vec4_ptr_t = std::shared_ptr<u32vec4>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////

template <typename T> inline ork::Vector4<T> operator*(T scalar, const ork::Vector4<T>& b) {
  return ork::Vector4<T>((scalar * b.x), (scalar * b.y), (scalar * b.z));
}

///////////////////////////////////////////////////////////////////////////////
