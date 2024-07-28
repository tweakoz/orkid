////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#pragma once
////////////////////////////////////////////////////////////////
#if defined(_WIN32) && !defined(_XBOX)
#include <pmmintrin.h>
#endif
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.inl>
#include <ork/math/cmatrix4.hpp>

#include <glm/gtc/matrix_transform.hpp>

namespace ork {

template <typename T>
bool Vector4<T>::isNan() const{
  return (isnan(this->x) || isnan(this->y) || isnan(this->z) || isnan(this->w));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector4<T> Vector4<T>::saturated() const {
  Vector4<T> rval = *this;
  rval.x          = (rval.x > 1.0f) ? 1.0f : (rval.x < 0.0f) ? 0.0f : rval.x;
  rval.y          = (rval.y > 1.0f) ? 1.0f : (rval.y < 0.0f) ? 0.0f : rval.y;
  rval.z          = (rval.z > 1.0f) ? 1.0f : (rval.z < 0.0f) ? 0.0f : rval.z;
  rval.w          = (rval.w > 1.0f) ? 1.0f : (rval.w < 0.0f) ? 0.0f : rval.w;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::Black() {
  static const Vector4<T> Black(T(0), T(0), T(0), T(1));
  return Black;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::DarkGrey() {
  static const Vector4<T> DarkGrey(T(0.25), T(0.25), T(0.25), T(1));
  return DarkGrey;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::MediumGrey() {
  static const Vector4<T> MediumGrey(T(0.5), T(0.5), T(0.5), T(1));
  return MediumGrey;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::LightGrey() {
  static const Vector4<T> LightGrey(T(0.75f), T(0.75f), T(0.75f), T(1));
  return LightGrey;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::White() {
  static const Vector4<T> White(T(1), T(1), T(1), T(1));
  return White;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::Red() {
  static const Vector4<T> Red(T(1), T(0), T(0), T(1));
  return Red;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::Green() {
  static const Vector4<T> Green(T(0), T(1), T(0), T(1));
  return Green;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::Blue() {
  static const Vector4<T> Blue(T(0), T(0), T(1), T(1));
  return Blue;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::Magenta() {
  static const Vector4<T> Magenta(T(1), T(0), T(1), T(1));
  return Magenta;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::Cyan() {
  static const Vector4<T> Cyan(T(0), T(1), T(1), T(1));
  return Cyan;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::Yellow() {
  static const Vector4<T> Yellow(T(1), T(1), T(0), T(1));
  return Yellow;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Vector4<T>::Vector4() : base_t(0,0,0,1){
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Vector4<T>::Vector4(T scalar) : base_t(scalar,scalar,scalar,scalar) {

}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Vector4<T>::Vector4(T _x, T _y, T _z, T _w)
  : base_t(_x,_y,_z,_w){
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Vector4<T>::Vector4(const Vector3<T>& in, T _w)
  : base_t(in.x,in.y,in.z,_w){
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> uint64_t Vector4<T>::RGBAU64() const {
  uint64_t r = round(this->x * T(65535.0f));
  uint64_t g = round(this->y * T(65535.0f));
  uint64_t b = round(this->z * T(65535.0f));
  uint64_t a = round(this->w * T(65535.0f));
  return ((r << 48) | (g << 32) | (b << 16) | a);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::setRGBAU64(uint64_t inp) {
  static constexpr T kfic(T(1.0) / T(65535.0));
  uint16_t r = (inp)&0xffff;
  uint16_t g = (inp >> 16) & 0xffff;
  uint16_t b = (inp >> 32) & 0xffff;
  uint16_t a = (inp >> 48) & 0xffff;
  this->x          = T(r);
  this->y          = T(g);
  this->z          = T(b);
  this->w          = T(a);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> U32 Vector4<T>::vertexColorU32() const {

  #if defined(ORK_CONFIG_DARWIN)
  return ABGRU32();
  #else
  return BGRAU32();
  #endif
  
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> U32 Vector4<T>::ABGRU32() const {
  U32 r = U32(this->x * T(255.0f));
  U32 g = U32(this->y * T(255.0f));
  U32 b = U32(this->z * T(255.0f));
  U32 a = U32(this->w * T(255.0f));

  return U32((a << 24) | (b << 16) | (g << 8) | r);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> U32 Vector4<T>::ARGBU32() const {
  U32 r = U32(this->x * T(255.0f));
  U32 g = U32(this->y * T(255.0f));
  U32 b = U32(this->z * T(255.0f));
  U32 a = U32(this->w * T(255.0f));

  return U32((a << 24) | (r << 16) | (g << 8) | b);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> U32 Vector4<T>::RGBAU32() const {
  S32 r = U32(this->x * T(255.0f));
  S32 g = U32(this->y * T(255.0f));
  S32 b = U32(this->z * T(255.0f));
  S32 a = U32(this->w * T(255.0f));

  if (r < 0)
    r = 0;
  if (g < 0)
    g = 0;
  if (b < 0)
    b = 0;
  if (a < 0)
    a = 0;

  U32 rval = 0;

  rval = ((r << 24) | (g << 16) | (b << 8) | a);

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> U32 Vector4<T>::BGRAU32() const {
  U32 r = U32(this->x * T(255.0f));
  U32 g = U32(this->y * T(255.0f));
  U32 b = U32(this->z * T(255.0f));
  U32 a = U32(this->w * T(255.0f));

  return U32((b << 24) | (g << 16) | (r << 8) | a);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> U16 Vector4<T>::RGBU16() const {
  U32 r = U32(this->x * T(31.0f));
  U32 g = U32(this->y * T(31.0f));
  U32 b = U32(this->z * T(31.0f));

  U16 rval = U16((b << 10) | (g << 5) | r);

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::setRGBAU32(U32 uval) {
  U32 r = (uval >> 24) & 0xff;
  U32 g = (uval >> 16) & 0xff;
  U32 b = (uval >> 8) & 0xff;
  U32 a = (uval)&0xff;

  static const T kfic(1.0f / 255.0f);

  this->x = (kfic * T(int(r)));
  this->y=(kfic * T(int(g)));
  this->z=(kfic * T(int(b)));
  this->w=(kfic * T(int(a)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::setBGRAU32(U32 uval) {
  U32 b = (uval >> 24) & 0xff;
  U32 g = (uval >> 16) & 0xff;
  U32 r = (uval >> 8) & 0xff;
  U32 a = (uval)&0xff;

  static const T kfic(1.0f / 255.0f);

  this->x = (kfic * T(int(r)));
  this->y=(kfic * T(int(g)));
  this->z=(kfic * T(int(b)));
  this->w=(kfic * T(int(a)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::setARGBU32(U32 uval) {
  U32 a = (uval >> 24) & 0xff;
  U32 r = (uval >> 16) & 0xff;
  U32 g = (uval >> 8) & 0xff;
  U32 b = (uval)&0xff;

  static const T kfic(1.0f / 255.0f);

  this->x = (kfic * T(int(r)));
  this->y=(kfic * T(int(g)));
  this->z=(kfic * T(int(b)));
  this->w=(kfic * T(int(a)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::setABGRU32(U32 uval) {
  U32 a = (uval >> 24) & 0xff;
  U32 b = (uval >> 16) & 0xff;
  U32 g = (uval >> 8) & 0xff;
  U32 r = (uval)&0xff;

  static const T kfic(1.0f / 255.0f);

  this->x = (kfic * T(int(r)));
  this->y=(kfic * T(int(g)));
  this->z=(kfic * T(int(b)));
  this->w=(kfic * T(int(a)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::setHSV(T h, T s, T v) {
  //	hsv.x = saturate(hsv.x);
  //	hsv.y = saturate(hsv.y);
  //	hsv.z = saturate(hsv.z);

  if (s == 0.0f) {
    // Grayscale
    this->x = (v);
    this->y=(v);
    this->z=(v);
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
      this->y=(cc);
      this->z=(aa);
    } else if (i < 2.0f) {
      this->x = (bb);
      this->y=(v);
      this->z=(aa);
    } else if (i < 3.0f) {
      this->x = (aa);
      this->y=(v);
      this->z=(cc);
    } else if (i < 4.0f) {
      this->x = (aa);
      this->y=(bb);
      this->z=(v);
    } else if (i < 5.0f) {
      this->x = (cc);
      this->y=(aa);
      this->z=(v);
    } else {
      this->x = (v);
      this->y=(aa);
      this->z=(bb);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::perspectiveDivideInPlace() {
  T iw = T(1) / this->w;
  this->x *= iw;
  this->y *= iw;
  this->z *= iw;
  this->w = T(1);
}

template <typename T> Vector4<T> Vector4<T>::perspectiveDivided() const {
  Vector4<T> rval;
  T iw   = T(1) / this->w;
  rval.x = this->x * iw;
  rval.y = this->y * iw;
  rval.z = this->z * iw;
  rval.w = T(1.0);
  return rval;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> Vector4<T>::Vector4(const base_t& vec) : base_t(vec) {
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> Vector4<T>::Vector4(const Vector4<T>& vec) : base_t(vec) {
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T Vector4<T>::dotWith(const Vector4<T>& vec) const {
  return ((this->x * vec.x) + (this->y * vec.y) + (this->z * vec.z));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
Vector4<T> Vector4<T>::crossWith(const Vector4<T>& vec) const // c = this X vec
{
  T vx = ((this->y * vec.z) - (this->z * vec.y));
  T vy = ((this->z * vec.x) - (this->x * vec.z));
  T vz = ((this->x * vec.y) - (this->y * vec.x));

  return (Vector4<T>(vx, vy, vz));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::normalizeInPlace() {
  T distance = T(1) / magnitude();

  this->x *= distance;
  this->y *= distance;
  this->z *= distance;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> Vector4<T> Vector4<T>::normalized() const {
  T fmag = magnitude();
  fmag   = (fmag == T(0)) ? T(0.00001) : fmag;
  T s    = T(1) / fmag;
  return Vector4<T>(this->x * s, this->y * s, this->z * s, this->w);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> //
uint64_t Vector4<T>::hash(T quantization) const{
  boost::Crc64 crc64;
  crc64.init();

  int bias = (1<<24); // to make negative numbers positive
  
  int xx = int((this->x * quantization)+0.5);
  int yy = int((this->y * quantization)+0.5);
  int zz = int((this->z * quantization)+0.5);
  int ww = int((this->w * quantization)+0.5);
  OrkAssert((xx+bias)>=0);
  OrkAssert((yy+bias)>=0);
  OrkAssert((zz+bias)>=0);
  OrkAssert((ww+bias)>=0);
  uint64_t a = (xx+bias);
  uint64_t b = (yy+bias);
  uint64_t c = (zz+bias);
  uint64_t d = (ww+bias);
  OrkAssert(a<0xffffffff);
  OrkAssert(b<0xffffffff);
  OrkAssert(c<0xffffffff);
  OrkAssert(d<0xffffffff);
  crc64.accumulateItem(a);
  crc64.accumulateItem(b);
  crc64.accumulateItem(c);
  crc64.accumulateItem(d);
  crc64.finish();

  return crc64.result();
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T Vector4<T>::magnitude() const {
  return Sqrt(this->x * this->x + this->y * this->y + this->z * this->z);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T Vector4<T>::magnitudeSquared() const {
  T mag = (this->x * this->x + this->y * this->y + this->z * this->z);
  return mag;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> Vector4<T> Vector4<T>::transform(const Matrix44<T>& matrix) const {
  const auto& xf = matrix.asGlmMat4();
  const auto& v = this->asGlmVec4();
  return Vector4<T>(xf*v);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
void Vector4<T>::serp(const Vector4<T>& PA, //
                      const Vector4<T>& PB, //
                      const Vector4<T>& PC, //
                      const Vector4<T>& PD, //
                      T par_x, T par_y) {
  Vector4<T> PAB, PCD;
  PAB.lerp(PA, PB, par_x);
  PCD.lerp(PC, PD, par_x);
  lerp(PAB, PCD, par_y);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::rotateOnX(T rad) {
  T previousY = this->y;
  T previousZ = this->z;
  this->y      = (previousY * Cos(rad) - previousZ * Sin(rad));
  this->z      = (previousY * Sin(rad) + previousZ * Cos(rad));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::rotateOnY(T rad) {
  T previousX = this->x;
  T previousZ = this->z;

  this->x = (previousX * Cos(rad) - previousZ * Sin(rad));
  this->z = (previousX * Sin(rad) + previousZ * Cos(rad));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::rotateOnZ(T rad) {
  T previousX = this->x;
  T previousY = this->y;

  this->x = (previousX * Cos(rad) - previousY * Sin(rad));
  this->y = (previousX * Sin(rad) + previousY * Cos(rad));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::lerp(const Vector4<T>& from, const Vector4<T>& to, T par) {
  if (par < T(0))
    par = T(0);
  if (par > T(1))
    par = T(1);
  T ipar = T(1) - par;
  this->x      = (from.x * ipar) + (to.x * par);
  this->y      = (from.y * ipar) + (to.y * par);
  this->z      = (from.z * ipar) + (to.z * par);
  this->w      = (from.w * ipar) + (to.w * par);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
T Vector4<T>::calcTriangularArea(const Vector4<T>& V0, const Vector4<T>& V1, const Vector4<T>& V2, const Vector4<T>& N) {
  return Vector3<T>::calcTriangularArea(V0.xyz(), V1.xyz(), V2.xyz(), N.xyz());  
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
namespace ork::reflect {
template <> //
inline void ::ork::reflect::ITyped<fvec4>::serialize(serdes::node_ptr_t sernode) const {
  using namespace serdes;
  auto serializer        = sernode->_serializer;
  auto instance          = sernode->_ser_instance;
  auto arynode           = serializer->pushNode(_name, serdes::NodeType::ARRAY);
  arynode->_parent       = sernode;
  arynode->_ser_instance = instance;
  fvec4 value;
  get(value, instance);
  serializeArraySubLeaf(arynode, value.x, 0);
  serializeArraySubLeaf(arynode, value.y, 1);
  serializeArraySubLeaf(arynode, value.z, 2);
  serializeArraySubLeaf(arynode, value.w, 3);
  serializer->popNode(); // pop arraynode
}
template <> //
inline void ::ork::reflect::ITyped<fvec4>::deserialize(serdes::node_ptr_t arynode) const {
  using namespace serdes;
  auto deserializer  = arynode->_deserializer;
  auto instance      = arynode->_deser_instance;
  size_t numelements = arynode->_numchildren;
  OrkAssert(numelements == 4);

  fvec4 outval;
  outval.x = deserializeArraySubLeaf<float>(arynode, 0);
  outval.y = deserializeArraySubLeaf<float>(arynode, 1);
  outval.z = deserializeArraySubLeaf<float>(arynode, 2);
  outval.w = deserializeArraySubLeaf<float>(arynode, 3);
  set(outval, instance);
}
} // namespace ork::reflect
