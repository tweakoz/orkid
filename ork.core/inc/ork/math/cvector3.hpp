////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#pragma once
////////////////////////////////////////////////////////////////
#include <ork/math/math_types.inl>
#include <ork/math/cmatrix4.h>
#include <ork/math/cmatrix3.h>
#include <ork/math/cvector4.h>
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

template <typename T> 
T Vector3<T>::areaOfTriangle(const Vector3<T>& a, const Vector3<T>& b, const Vector3<T>& c){
  Vector3<T> ab = b - a;
  Vector3<T> ac = c - a;
  Vector3<T> cross = ab.crossWith(ac);
  return 0.5 * cross.magnitude();
}
template <typename T> 
T Vector3<T>::areaOfGeneralPolygon(const std::vector<Vector3<T>>& points){
  T area = 0;
  for (int i = 0; i < points.size(); i++) {
    int j = (i + 1) % points.size();
    int k = (i + 2) % points.size();
    area += areaOfTriangle(points[i], points[j], points[k]);
  }
  return area;
}

template <typename T> Vector3<T> Vector3<T>::clamped(T min, T max) const {
  Vector3<T> rval = *this;
  rval.x          = (rval.x > max) ? max : (rval.x < min) ? min : rval.x;
  rval.y          = (rval.y > max) ? max : (rval.y < min) ? min : rval.y;
  rval.z          = (rval.z > max) ? max : (rval.z < min) ? min : rval.z;
  return rval;
}

template <typename T> Vector3<T> Vector3<T>::saturated() const {
  return this->clamped(0, 1);
}

template <typename T> Vector3<T> Vector3<T>::absolute() const {
  Vector3<T> as_abs;
  as_abs.x = fabs(this->x);
  as_abs.y = fabs(this->y);
  as_abs.z = fabs(this->z);
  return as_abs;
}

template <typename T> Vector3<T> Vector3<T>::minXYZ(const Vector3<T>& rhs) const {
  Vector3<T> rval;
  rval.x = std::min(this->x, rhs.x);
  rval.y = std::min(this->y, rhs.y);
  rval.z = std::min(this->z, rhs.z);
  return rval;
}

template <typename T> Vector3<T> Vector3<T>::maxXYZ(const Vector3<T>& rhs) const {
  Vector3<T> rval;
  rval.x = std::max(this->x, rhs.x);
  rval.y = std::max(this->y, rhs.y);
  rval.z = std::max(this->z, rhs.z);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::Black() {
  static const Vector3<T> Black(T(0.0f), T(0.0f), T(0.0f));
  return Black;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::DarkGrey() {
  static const Vector3<T> DarkGrey(T(0.250f), T(0.250f), T(0.250f));
  return DarkGrey;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::MediumGrey() {
  static const Vector3<T> MediumGrey(T(0.50f), T(0.50f), T(0.50f));
  return MediumGrey;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::LightGrey() {
  static const Vector3<T> LightGrey(T(0.75f), T(0.75f), T(0.75f));
  return LightGrey;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::White() {
  static const Vector3<T> White(T(1.0f), T(1.0f), T(1.0f));
  return White;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::Red() {
  static const Vector3<T> Red(T(1.0f), T(0.0f), T(0.0f));
  return Red;
}
template <typename T> const Vector3<T>& Vector3<T>::LightRed() {
  static const Vector3<T> LightRed(T(1.0f), T(0.5f), T(0.5f));
  return LightRed;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::Green() {
  static const Vector3<T> Green(T(0.0f), T(1.0f), T(0.0f));
  return Green;
}
template <typename T> const Vector3<T>& Vector3<T>::LightGreen() {
  static const Vector3<T> LightGreen(T(0.5f), T(1.0f), T(0.5f));
  return LightGreen;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::Blue() {
  static const Vector3<T> Blue(T(0.0f), T(0.0f), T(1.0f));
  return Blue;
}
template <typename T> const Vector3<T>& Vector3<T>::LightBlue() {
  static const Vector3<T> LightBlue(T(0.5f), T(0.5f), T(1.0f));
  return LightBlue;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::Magenta() {
  static const Vector3<T> Magenta(T(1.0f), T(0.0f), T(1.0f));
  return Magenta;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::Cyan() {
  static const Vector3<T> Cyan(T(0.0f), T(1.0f), T(1.0f));
  return Cyan;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::Yellow() {
  static const Vector3<T> Yellow(T(1.0f), T(1.0f), T(0.0f));
  return Yellow;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Vector3<T>::Vector3()
    : base_t(0, 0, 0) {
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Vector3<T>::Vector3(T _x, T _y, T _z)
    : base_t(_x, _y, _z) {
}

template <typename T>
Vector3<T>::Vector3(T _a)
    : base_t(_a, _a, _a) {
}

template <typename T>
Vector3<T>::Vector3(const kln::point& klein_point)
    : base_t(T(klein_point.x()), T(klein_point.y()), T(klein_point.z())) {
}
template <typename T>
Vector3<T>::Vector3(const kln::direction& klein_direction)
    : base_t(T(klein_direction.x()), T(klein_direction.y()), T(klein_direction.z())) {
}

template <typename T> kln::point Vector3<T>::asKleinPoint() const {
  return kln::point(float(this->x), float(this->y), float(this->z));
}
template <typename T> kln::direction Vector3<T>::asKleinDirection() const {
  auto N = normalized();
  return kln::direction(float(N.x), float(N.y), float(N.z));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> U32 Vector3<T>::VtxColorAsU32() const {
  U32 r = U32(this->x * T(255.0f));
  U32 g = U32(this->y * T(255.0f));
  U32 b = U32(this->z * T(255.0f));
  U32 a = 255;

#if defined(ORK_CONFIG_DARWIN) || defined(ORK_CONFIG_IX)
  return U32((a << 24) | (b << 16) | (g << 8) | r);
#else // WIN32/DX
  return U32((a << 24) | (r << 16) | (g << 8) | b);
#endif
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> U32 Vector3<T>::ABGRU32() const {
  U32 r = U32(this->x * T(255.0f));
  U32 g = U32(this->y * T(255.0f));
  U32 b = U32(this->z * T(255.0f));
  U32 a = 255;

  return U32((a << 24) | (b << 16) | (g << 8) | r);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> U32 Vector3<T>::ARGBU32() const {
  U32 r = U32(this->x * T(255.0f));
  U32 g = U32(this->y * T(255.0f));
  U32 b = U32(this->z * T(255.0f));
  U32 a = 255;

  return U32((a << 24) | (r << 16) | (g << 8) | b);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> U32 Vector3<T>::RGBAU32() const {
  U32 r = U32(this->x * T(255.0f));
  U32 g = U32(this->y * T(255.0f));
  U32 b = U32(this->z * T(255.0f));
  U32 a = 255;

  U32 rval = 0;

  rval = ((r << 24) | (g << 16) | (b << 8) | a);

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> U32 Vector3<T>::BGRAU32() const {
  U32 r = U32(this->x * T(255.0f));
  U32 g = U32(this->y * T(255.0f));
  U32 b = U32(this->z * T(255.0f));
  U32 a = 255;

  return U32((b << 24) | (g << 16) | (r << 8) | a);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> U16 Vector3<T>::RGBU16() const {
  U32 r = U32(this->x * T(31.0f));
  U32 g = U32(this->y * T(31.0f));
  U32 b = U32(this->z * T(31.0f));

  U16 rval = U16((b << 10) | (g << 5) | r);

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::setRGBAU32(U32 uval) {
  U32 r = (uval >> 24) & 0xff;
  U32 g = (uval >> 16) & 0xff;
  U32 b = (uval >> 8) & 0xff;

  static const T kfic(1.0f / 255.0f);

  this->x = (kfic * T(int(r)));
  this->y = (kfic * T(int(g)));
  this->z = (kfic * T(int(b)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::setBGRAU32(U32 uval) {
  U32 b = (uval >> 24) & 0xff;
  U32 g = (uval >> 16) & 0xff;
  U32 r = (uval >> 8) & 0xff;

  static const T kfic(1.0f / 255.0f);

  this->x = (kfic * T(int(r)));
  this->y = (kfic * T(int(g)));
  this->z = (kfic * T(int(b)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::setARGBU32(U32 uval) {
  U32 r = (uval >> 16) & 0xff;
  U32 g = (uval >> 8) & 0xff;
  U32 b = (uval)&0xff;

  static const T kfic(1.0f / 255.0f);

  this->x = (kfic * T(int(r)));
  this->y = (kfic * T(int(g)));
  this->z = (kfic * T(int(b)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::setABGRU32(U32 uval) {
  U32 b = (uval >> 16) & 0xff;
  U32 g = (uval >> 8) & 0xff;
  U32 r = (uval)&0xff;

  static const T kfic(1.0f / 255.0f);

  this->x = (kfic * T(int(r)));
  this->y = (kfic * T(int(g)));
  this->z = (kfic * T(int(b)));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> uint64_t Vector3<T>::RGBAU64() const {
  uint64_t r = round(this->x * T(65535.0f));
  uint64_t g = round(this->y * T(65535.0f));
  uint64_t b = round(this->z * T(65535.0f));
  return ((r << 48) | (g << 32) | (b << 16));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::setRGBAU64(uint64_t inp) {
  static constexpr T kfic(T(1.0) / T(65535.0));
  uint64_t r = (inp >> 48) & 0xffff;
  uint64_t g = (inp >> 32) & 0xffff;
  uint64_t b = (inp >> 16) & 0xffff;
  this->x    = (kfic * T(uint64_t(r)));
  this->y    = (kfic * T(uint64_t(g)));
  this->z    = (kfic * T(uint64_t(b)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T> Vector3<T>::convertRgbToHsv() const {

  T r = this->x;
  T g = this->y;
  T b = this->z;

  T h = T(0);
  T s = T(0);
  T v = T(0);

  T min = std::min(r, std::min(g, b));
  T max = std::max(r, std::max(g, b));

  v = max;

  T delta = max - min;

  if (max != T(0)) {
    s = delta / max;
  } else {
    s = T(0);
    h = T(-1);
    return Vector3<T>(h, s, v);
  }

  if (r == max) {
    h = (g - b) / delta;
  } else if (g == max) {
    h = T(2) + (b - r) / delta;
  } else {
    h = T(4) + (r - g) / delta;
  }

  h *= T(60);

  if (h < T(0)) {
    h += T(360);
  }

  return Vector3<T>(h/T(360), s, v);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::setHSV(T h, T s, T v) {
  //  hsv.x = saturate(hsv.x);
  //  hsv.y = saturate(hsv.y);
  //  hsv.z = saturate(hsv.z);

  if (s == 0.0f) {
    // Grayscale
    this->x = (v);
    this->y = (v);
    this->z = (v);
  } else {
    const T kone(1.0f);

    if (kone <= h)
      h -= kone;
    h *= 6.0f;
    T i  = T(floor(h));
    T f  = h - i;
    T aa = v * (kone - s);
    T bb = v * (kone - (s * f));
    T cc = v * (kone - (s * (kone - f)));
    if (i < kone) {
      this->x = (v);
      this->y = (cc);
      this->z = (aa);
    } else if (i < 2.0f) {
      this->x = (bb);
      this->y = (v);
      this->z = (aa);
    } else if (i < 3.0f) {
      this->x = (aa);
      this->y = (v);
      this->z = (cc);
    } else if (i < 4.0f) {
      this->x = (aa);
      this->y = (bb);
      this->z = (v);
    } else if (i < 5.0f) {
      this->x = (cc);
      this->y = (aa);
      this->z = (v);
    } else {
      this->x = (v);
      this->y = (aa);
      this->z = (bb);
    }
  }
}

template <typename T> Vector3<T> Vector3<T>::reflect(const Vector3& N) const {
  const Vector3<T>& I = *this;
  Vector3<T> R        = I - (N * 2.0f * N.dotWith(I));
  return R;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::setYUV(T Y, T U, T V) {
  Y -= T(1.0 / 16.0);
  U -= T(0.5);
  V -= T(0.5);
  this->x = 1.164 * Y + 1.596 * V;
  this->y = 1.164 * Y - 0.392 * U - 0.813 * V;
  this->z = 1.164 * Y + 2.017 * U;
}

template <typename T> Vector3<T> Vector3<T>::YUV() const {
  T R = T(this->x);
  T G = T(this->y);
  T B = T(this->z);
  T Y = T(0.299) * R + T(0.587) * G + T(0.114) * B;
  T U = T(0.492) * (B - Y);
  T V = T(0.877) * (R - Y);
  return Vector3<T>(Y, U, V);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T>::Vector3(const Vector3<T>::base_t& vec) {
  this->x = vec.x;
  this->y = vec.y;
  this->z = vec.z;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T>::Vector3(const Vector3<T>& vec) {
  this->x = vec.x;
  this->y = vec.y;
  this->z = vec.z;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T>::Vector3(const Vector4<T>& vec) {
  this->x = vec.x;
  this->y = vec.y;
  this->z = vec.z;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T>::Vector3(const Vector2<T>& vec) {
  this->x = vec.x;
  this->y = vec.y;
  this->z = T(0);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T Vector3<T>::dotWith(const Vector3<T>& vec) const {
  const base_t& as_base_t = *this;
  return glm::dot(as_base_t, vec);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
Vector3<T> Vector3<T>::crossWith(const Vector3<T>& vec) const // c = this X vec
{
  const base_t& as_base_t = *this;
  return Vector3<T>(glm::cross(as_base_t, vec));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::normalizeInPlace() {
  *this = normalized();
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T> Vector3<T>::normalized() const {
  T mag = magnitude();
  if (mag > Epsilon()) {
    const base_t& as_base_t = *this;
    return Vector3<T>(glm::normalize(as_base_t));
  }
  return *this;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T Vector3<T>::magnitude() const {
  const base_t& as_base_t = *this;
  return glm::length(as_base_t);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T Vector3<T>::magnitudeSquared() const {
  return dotWith(*this);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> Vector4<T> Vector3<T>::transform(const Matrix44<T>& matrix) const {
  const auto& xf = matrix.asGlmMat4();
  const auto& v  = this->asGlmVec4();
  return Vector4<T>(xf * v);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T> Vector3<T>::transform(const Matrix33<T>& matrix) const {
  auto xf = matrix.asGlmMat4();
  auto v  = this->asGlmVec4();
  return base_t(xf * v);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T> Vector3<T>::transform3x3(const Matrix44<T>& matrix) const {
  auto xf  = matrix.asGlmMat4();
  xf[0][3] = 0;
  xf[1][3] = 0;
  xf[2][3] = 0;
  xf[3][0] = 0;
  xf[3][1] = 0;
  xf[3][2] = 0;
  auto v   = this->asGlmVec4();
  return base_t(xf * v);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
void Vector3<T>::serp(
    const Vector3<T>& PA, //
    const Vector3<T>& PB, //
    const Vector3<T>& PC, //
    const Vector3<T>& PD,
    T par_x,
    T par_y) {
  Vector3<T> PAB, PCD;
  PAB.lerp(PA, PB, par_x);
  PCD.lerp(PC, PD, par_x);
  lerp(PAB, PCD, par_y);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T ork::Vector3<T>::angle(const Vector3& vec) const {
  const base_t& a = *this;
  const base_t& b = vec;

  return T(glm::angle(a, b));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T ork::Vector3<T>::orientedAngle(const Vector3& vec, const Vector3& refaxis) const {
  const base_t& a = *this;
  const base_t& b = vec;

  return T(glm::orientedAngle(a, b, refaxis));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::rotateOnX(T rad) {
  T previousY = this->y;
  T previousZ = this->z;
  this->y     = (previousY * Cos(rad) - previousZ * Sin(rad));
  this->z     = (previousY * Sin(rad) + previousZ * Cos(rad));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::rotateOnY(T rad) {
  T previousX = this->x;
  T previousZ = this->z;

  this->x = (previousX * Cos(rad) - previousZ * Sin(rad));
  this->z = (previousX * Sin(rad) + previousZ * Cos(rad));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::rotateOnZ(T rad) {
  T previousX = this->x;
  T previousY = this->y;

  this->x = (previousX * Cos(rad) - previousY * Sin(rad));
  this->y = (previousX * Sin(rad) + previousY * Cos(rad));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::lerp(const Vector3<T>& from, const Vector3<T>& to, T par) {
  if (par < T(0.0f))
    par = T(0.0f);
  if (par > T(1.0f))
    par = T(1.0f);
  T ipar  = T(1.0f) - par;
  this->x = (from.x * ipar) + (to.x * par);
  this->y = (from.y * ipar) + (to.y * par);
  this->z = (from.z * ipar) + (to.z * par);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T Vector3<T>::calcTriangularArea(const Vector3<T>& V, const Vector3<T>& N) {
  return T(0);
}

template <typename T> Vector3<T> Vector3<T>::quantized(T v) const {
  Vector3<T> rval;
  rval.x = T(int((this->x * v)+0.5)) / v;
  rval.y = T(int((this->y * v)+0.5)) / v;
  rval.z = T(int((this->z * v)+0.5)) / v;
  return rval;
}
template <typename T> uint64_t Vector3<T>::hash(T quantization) const{
  if(quantization<=T(0)) {
     const size_t bytes = sizeof(T) * 3; // Total bytes in the vector
      uint64_t hash = 0xcbf29ce484222325; // FNV-1a initial value
      uint64_t prime = 0x100000001b3;     // FNV-1a prime
      
      uint8_t data[bytes];
      memcpy(&data[0], &this->x, sizeof(T));
      memcpy(&data[sizeof(T)], &this->y, sizeof(T));
      memcpy(&data[2 * sizeof(T)], &this->z, sizeof(T));

      for (size_t i = 0; i < bytes; ++i) {
          hash ^= data[i];
          hash *= prime;
      }
      return hash;
  }
  else{
    int64_t a = int64_t((this->x * quantization)+0.5)+(1<<19);
    int64_t b = int64_t((this->y * quantization)+0.5)+(1<<19);
    int64_t c = int64_t((this->z * quantization)+0.5)+(1<<19);
    return (uint64_t(a)<<42) | (uint64_t(b)<<21) | uint64_t(c);
  }
}

template <typename T> Vector2<T> octahedronWrap(Vector2<T> v) {
  T x = (T(1) - abs(v.y)) * (v.x >= T(0) ? T(1) : T(-1));
  T y = (T(1) - abs(v.x)) * (v.y >= T(0) ? T(1) : T(-1));
  return Vector2<T>(x, y);
}
template <typename T> uint64_t Vector3<T>::hashNormal(T quantization) const{
  auto nenc = normalOctahedronEncoded();
  OrkAssert(quantization>T(0));
  OrkAssert(quantization<=T(1e9));
  uint64_t ux = uint64_t(nenc.x*quantization)+uint64_t(0x80000000);        // 16 bits
  uint64_t uy = uint64_t(nenc.y*quantization)+uint64_t(0x80000000);        // 16 bits  (total of 2^28 possible normals ~= )
  return (uy<<32) | ux;
}

template <typename T> Vector2<T> Vector3<T>::normalOctahedronEncoded() const {

  Vector3<T> temp_n = *this;
  temp_n *= (1.0f / (abs(temp_n.x) + abs(temp_n.y) + abs(temp_n.z)));
  Vector2<T> xy = temp_n.xy();
  xy            = (temp_n.z >= 0.0f) ? xy : octahedronWrap(xy);
  xy            = (xy * T(0.5)) + Vector2<T>(T(0.5), T(0.5));
  return xy;
}

template <typename T> void Vector3<T>::decodeNormalOctahedronEncoded(Vector2<T> enc) {
  enc     = enc * T(2) - Vector2<T>(T(1),T(1));
  this->z = T(1) - abs(enc.x) - abs(enc.y);
  auto xy = (this->z >= T(0)) ? enc : octahedronWrap(enc);
  this->x = xy.x;
  this->y = xy.y;
  normalizeInPlace();
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
T Vector3<T>::calcTriangularArea(const Vector3<T>& V0, const Vector3<T>& V1, const Vector3<T>& V2, const Vector3<T>& N) {
  // select largest abs coordinate to ignore for projection
  T ax = Abs(N.x);
  T ay = Abs(N.y);
  T az = Abs(N.z);

  int coord = (ax > ay) ? ((ax > az) ? 1 : 3) : ((ay > az) ? 2 : 3);

  // compute area of the 2D projection

  Vector3<T> Ary[3];
  Ary[0] = V0;
  Ary[1] = V1;
  Ary[2] = V2;
  T area(0.0f);

  for (int i = 1, j = 2, k = 0; i <= 3; i++, j++, k++) {
    int ii = i % 3;
    int jj = j % 3;
    int kk = k % 3;

    switch (coord) {
      case 1:
        area += (Ary[ii].y * (Ary[jj].z - Ary[kk].z));
        continue;
      case 2:
        area += (Ary[ii].x * (Ary[jj].z - Ary[kk].z));
        continue;
      case 3:
        area += (Ary[ii].x * (Ary[jj].y - Ary[kk].y));
        continue;
    }
  }

  T an = Sqrt((T)(ax * ax + ay * ay + az * az));

  switch (coord) {
    case 1:
      OrkAssert(ax != T(0));
      area *= (an / (T(2.0f) * ax));
      break;
    case 2:
      OrkAssert(ay != T(0));
      area *= (an / (T(2.0f) * ay));
      break;
    case 3:
      OrkAssert(az != T(0));
      area *= (an / (T(2.0f) * az));
      break;
  }

  return Abs(area);
}
} // namespace ork

///////////////////////////////////////////////////////////////////////////////

namespace ork::reflect {
using namespace serdes;
template <> //
inline void ::ork::reflect::ITyped<fvec3>::serialize(serdes::node_ptr_t sernode) const {
  auto serializer        = sernode->_serializer;
  auto instance          = sernode->_ser_instance;
  auto arynode           = serializer->pushNode(_name, serdes::NodeType::ARRAY);
  arynode->_parent       = sernode;
  arynode->_ser_instance = instance;
  fvec3 value;
  get(value, instance);
  serializeArraySubLeaf(arynode, value.x, 0);
  serializeArraySubLeaf(arynode, value.y, 1);
  serializeArraySubLeaf(arynode, value.z, 2);
  serializer->popNode(); // pop arraynode
}
template <> //
inline void ::ork::reflect::ITyped<fvec3>::deserialize(serdes::node_ptr_t arynode) const {
  using namespace serdes;
  auto deserializer  = arynode->_deserializer;
  auto instance      = arynode->_deser_instance;
  size_t numelements = arynode->_numchildren;
  OrkAssert(numelements == 3);

  fvec3 outval;
  outval.x = deserializeArraySubLeaf<float>(arynode, 0);
  outval.y = deserializeArraySubLeaf<float>(arynode, 1);
  outval.z = deserializeArraySubLeaf<float>(arynode, 2);
  set(outval, instance);
}

} // namespace ork::reflect
