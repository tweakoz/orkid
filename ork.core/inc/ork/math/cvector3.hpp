////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
#pragma once
////////////////////////////////////////////////////////////////
#include <ork/math/cmatrix4.h>
#include <ork/math/cmatrix3.h>
#include <ork/math/cvector4.h>
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T> Vector3<T>::clamped(float min, float max) const {
  Vector3<T> rval = *this;
  rval.x          = (rval.x > max) ? max : (rval.x < min) ? min : rval.x;
  rval.y          = (rval.y > max) ? max : (rval.y < min) ? min : rval.y;
  rval.z          = (rval.z > max) ? max : (rval.z < min) ? min : rval.z;
  return rval;
}

template <typename T> Vector3<T> Vector3<T>::saturated(void) const {
  return this->clamped(0, 1);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::Black(void) {
  static const Vector3<T> Black(T(0.0f), T(0.0f), T(0.0f));
  return Black;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::DarkGrey(void) {
  static const Vector3<T> DarkGrey(T(0.250f), T(0.250f), T(0.250f));
  return DarkGrey;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::MediumGrey(void) {
  static const Vector3<T> MediumGrey(T(0.50f), T(0.50f), T(0.50f));
  return MediumGrey;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::LightGrey(void) {
  static const Vector3<T> LightGrey(T(0.75f), T(0.75f), T(0.75f));
  return LightGrey;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::White(void) {
  static const Vector3<T> White(T(1.0f), T(1.0f), T(1.0f));
  return White;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::Red(void) {
  static const Vector3<T> Red(T(1.0f), T(0.0f), T(0.0f));
  return Red;
}
template <typename T> const Vector3<T>& Vector3<T>::LightRed(void) {
  static const Vector3<T> LightRed(T(1.0f), T(0.5f), T(0.5f));
  return LightRed;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::Green(void) {
  static const Vector3<T> Green(T(0.0f), T(1.0f), T(0.0f));
  return Green;
}
template <typename T> const Vector3<T>& Vector3<T>::LightGreen(void) {
  static const Vector3<T> LightGreen(T(0.5f), T(1.0f), T(0.5f));
  return LightGreen;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::Blue(void) {
  static const Vector3<T> Blue(T(0.0f), T(0.0f), T(1.0f));
  return Blue;
}
template <typename T> const Vector3<T>& Vector3<T>::LightBlue(void) {
  static const Vector3<T> LightBlue(T(0.5f), T(0.5f), T(1.0f));
  return LightBlue;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::Magenta(void) {
  static const Vector3<T> Magenta(T(1.0f), T(0.0f), T(1.0f));
  return Magenta;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::Cyan(void) {
  static const Vector3<T> Cyan(T(0.0f), T(1.0f), T(1.0f));
  return Cyan;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector3<T>& Vector3<T>::Yellow(void) {
  static const Vector3<T> Yellow(T(1.0f), T(1.0f), T(0.0f));
  return Yellow;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Vector3<T>::Vector3()
    : x(T(0.0f))
    , y(T(0.0f))
    , z(T(0.0f)) {
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Vector3<T>::Vector3(T _x, T _y, T _z)
    : x(_x)
    , y(_y)
    , z(_z) {
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> U32 Vector3<T>::GetVtxColorAsU32(void) const {
  U32 r = U32(GetX() * T(255.0f));
  U32 g = U32(GetY() * T(255.0f));
  U32 b = U32(GetZ() * T(255.0f));
  U32 a = 255;

#if defined(ORK_CONFIG_DARWIN) || defined(ORK_CONFIG_IX)
  return U32((a << 24) | (b << 16) | (g << 8) | r);
#else // WIN32/DX
  return U32((a << 24) | (r << 16) | (g << 8) | b);
#endif
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> U32 Vector3<T>::GetABGRU32(void) const {
  U32 r = U32(GetX() * T(255.0f));
  U32 g = U32(GetY() * T(255.0f));
  U32 b = U32(GetZ() * T(255.0f));
  U32 a = 255;

  return U32((a << 24) | (b << 16) | (g << 8) | r);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> U32 Vector3<T>::GetARGBU32(void) const {
  U32 r = U32(GetX() * T(255.0f));
  U32 g = U32(GetY() * T(255.0f));
  U32 b = U32(GetZ() * T(255.0f));
  U32 a = 255;

  return U32((a << 24) | (r << 16) | (g << 8) | b);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> U32 Vector3<T>::GetRGBAU32(void) const {
  U32 r = U32(GetX() * T(255.0f));
  U32 g = U32(GetY() * T(255.0f));
  U32 b = U32(GetZ() * T(255.0f));
  U32 a = 255;

  U32 rval = 0;

  rval = ((r << 24) | (g << 16) | (b << 8) | a);

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> U32 Vector3<T>::GetBGRAU32(void) const {
  U32 r = U32(GetX() * T(255.0f));
  U32 g = U32(GetY() * T(255.0f));
  U32 b = U32(GetZ() * T(255.0f));
  U32 a = 255;

  return U32((b << 24) | (g << 16) | (r << 8) | a);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> U16 Vector3<T>::GetRGBU16() const {
  U32 r = U32(GetX() * T(31.0f));
  U32 g = U32(GetY() * T(31.0f));
  U32 b = U32(GetZ() * T(31.0f));

  U16 rval = U16((b << 10) | (g << 5) | r);

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::SetRGBAU32(U32 uval) {
  U32 r = (uval >> 24) & 0xff;
  U32 g = (uval >> 16) & 0xff;
  U32 b = (uval >> 8) & 0xff;

  static const T kfic(1.0f / 255.0f);

  SetX(kfic * T(int(r)));
  SetY(kfic * T(int(g)));
  SetZ(kfic * T(int(b)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::SetBGRAU32(U32 uval) {
  U32 b = (uval >> 24) & 0xff;
  U32 g = (uval >> 16) & 0xff;
  U32 r = (uval >> 8) & 0xff;

  static const T kfic(1.0f / 255.0f);

  SetX(kfic * T(int(r)));
  SetY(kfic * T(int(g)));
  SetZ(kfic * T(int(b)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::SetARGBU32(U32 uval) {
  U32 r = (uval >> 16) & 0xff;
  U32 g = (uval >> 8) & 0xff;
  U32 b = (uval)&0xff;

  static const T kfic(1.0f / 255.0f);

  SetX(kfic * T(int(r)));
  SetY(kfic * T(int(g)));
  SetZ(kfic * T(int(b)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::SetABGRU32(U32 uval) {
  U32 b = (uval >> 16) & 0xff;
  U32 g = (uval >> 8) & 0xff;
  U32 r = (uval)&0xff;

  static const T kfic(1.0f / 255.0f);

  SetX(kfic * T(int(r)));
  SetY(kfic * T(int(g)));
  SetZ(kfic * T(int(b)));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> uint64_t Vector3<T>::GetRGBAU64(void) const {
  uint64_t r = round(x * T(65535.0f));
  uint64_t g = round(y * T(65535.0f));
  uint64_t b = round(z * T(65535.0f));
  return ((r << 48) | (g << 32) | (b << 16));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::SetRGBAU64(uint64_t inp) {
  static constexpr T kfic(T(1.0) / T(65535.0));
  uint64_t r = (inp >> 48) & 0xffff;
  uint64_t g = (inp >> 32) & 0xffff;
  uint64_t b = (inp >> 16) & 0xffff;
  x          = (kfic * T(uint64_t(r)));
  y          = (kfic * T(uint64_t(g)));
  z          = (kfic * T(uint64_t(b)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::SetHSV(T h, T s, T v) {
}

template <typename T> Vector3<T> Vector3<T>::Reflect(const Vector3& N) const {
  const Vector3<T>& I = *this;
  Vector3<T> R        = I - (N * 2.0f * N.Dot(I));
  return R;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::setYUV(T Y, T U, T V) {
  Y -= T(1.0 / 16.0);
  U -= T(0.5);
  V -= T(0.5);
  x = 1.164 * Y + 1.596 * V;
  y = 1.164 * Y - 0.392 * U - 0.813 * V;
  z = 1.164 * Y + 2.017 * U;
}

template <typename T> Vector3<T> Vector3<T>::getYUV() const {
  T R = T(x);
  T G = T(y);
  T B = T(z);
  T Y = T(0.299) * R + T(0.587) * G + T(0.114) * B;
  T U = T(0.492) * (B - Y);
  T V = T(0.877) * (R - Y);
  return Vector3<T>(Y, U, V);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T>::Vector3(const Vector3<T>& vec) {
  x = vec.x;
  y = vec.y;
  z = vec.z;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T>::Vector3(const Vector4<T>& vec) {
  x = vec.GetX();
  y = vec.GetY();
  z = vec.GetZ();
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T>::Vector3(const Vector2<T>& vec) {
  x = vec.GetX();
  y = vec.GetY();
  z = T(0);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T Vector3<T>::Dot(const Vector3<T>& vec) const {
#if defined WII
  return __fmadds(x, vec.x, __fmadds(y, vec.y, __fmadds(z, vec.z, 0.0f)));
#else
  return ((x * vec.x) + (y * vec.y) + (z * vec.z));
#endif
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
Vector3<T> Vector3<T>::Cross(const Vector3<T>& vec) const // c = this X vec
{
  T vx = ((y * vec.GetZ()) - (z * vec.GetY()));
  T vy = ((z * vec.GetX()) - (x * vec.GetZ()));
  T vz = ((x * vec.GetY()) - (y * vec.GetX()));

  return (Vector3<T>(vx, vy, vz));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::Normalize(void) {
  T mag = Mag();
  if (mag > Epsilon()) {
    T distance = (T)1.0f / mag;

    x *= distance;
    y *= distance;
    z *= distance;
  }
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T> Vector3<T>::Normal() const {
  Vector3<T> vec(*this);
  vec.Normalize();

  return vec;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T Vector3<T>::Mag(void) const {
  return Sqrt(x * x + y * y + z * z);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T Vector3<T>::MagSquared(void) const {
  T mag = (x * x + y * y + z * z);
  return mag;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> Vector4<T> Vector3<T>::Transform(const Matrix44<T>& matrix) const {
  T tx, ty, tz, tw;

  T* mp = (T*)matrix.elements;
  T _x  = x;
  T _y  = y;
  T _z  = z;
  T _w  = T(1.0f);

#if 0 // defined WII
	tx = __fmadds(x,vec.x,__fmadds(y,vec.y,__fmadds(z,vec.z,0.0f)));
#else
  tx = _x * mp[0] + _y * mp[4] + _z * mp[8] + _w * mp[12];
  ty = _x * mp[1] + _y * mp[5] + _z * mp[9] + _w * mp[13];
  tz = _x * mp[2] + _y * mp[6] + _z * mp[10] + _w * mp[14];
  tw = _x * mp[3] + _y * mp[7] + _z * mp[11] + _w * mp[15];
#endif

  return Vector4<T>(tx, ty, tz, tw);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T> Vector3<T>::Transform(const Matrix33<T>& matrix) const {
  T tx, ty, tz;

  T* mp = (T*)matrix.elements;
  T _x  = x;
  T _y  = y;
  T _z  = z;

  tx = _x * mp[0] + _y * mp[3] + _z * mp[6];
  ty = _x * mp[1] + _y * mp[4] + _z * mp[7];
  tz = _x * mp[2] + _y * mp[5] + _z * mp[8];

  return Vector3<T>(tx, ty, tz);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> Vector3<T> Vector3<T>::Transform3x3(const Matrix44<T>& matrix) const {
  T tx, ty, tz;
  T* mp = (T*)matrix.elements;
  T _x  = x;
  T _y  = y;
  T _z  = z;

  tx = _x * mp[0] + _y * mp[4] + _z * mp[8];
  ty = _x * mp[1] + _y * mp[5] + _z * mp[9];
  tz = _x * mp[2] + _y * mp[6] + _z * mp[10];

  return Vector3<T>(tx, ty, tz);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
void Vector3<T>::Serp(const Vector3<T>& PA, const Vector3<T>& PB, const Vector3<T>& PC, const Vector3<T>& PD, T Par) {
  Vector3<T> PAB, PCD;
  PAB.Lerp(PA, PB, Par);
  PCD.Lerp(PC, PD, Par);
  Lerp(PAB, PCD, Par);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::RotateX(T rad) {
  T oldY = y;
  T oldZ = z;
  y      = (oldY * Cos(rad) - oldZ * Sin(rad));
  z      = (oldY * Sin(rad) + oldZ * Cos(rad));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::RotateY(T rad) {
  T oldX = x;
  T oldZ = z;

  x = (oldX * Cos(rad) - oldZ * Sin(rad));
  z = (oldX * Sin(rad) + oldZ * Cos(rad));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::RotateZ(T rad) {
  T oldX = x;
  T oldY = y;

  x = (oldX * Cos(rad) - oldY * Sin(rad));
  y = (oldX * Sin(rad) + oldY * Cos(rad));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector3<T>::Lerp(const Vector3<T>& from, const Vector3<T>& to, T par) {
  if (par < T(0.0f))
    par = T(0.0f);
  if (par > T(1.0f))
    par = T(1.0f);
  T ipar = T(1.0f) - par;
  x      = (from.x * ipar) + (to.x * par);
  y      = (from.y * ipar) + (to.y * par);
  z      = (from.z * ipar) + (to.z * par);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T Vector3<T>::CalcTriArea(const Vector3<T>& V, const Vector3<T>& N) {
  return T(0);
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
