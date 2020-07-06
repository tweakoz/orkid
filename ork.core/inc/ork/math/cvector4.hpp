////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
#pragma once
////////////////////////////////////////////////////////////////
#if defined(_WIN32) && !defined(_XBOX)
#include <pmmintrin.h>
#endif
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector4<T> Vector4<T>::Saturate(void) const {
  Vector4<T> rval = *this;
  rval.x          = (rval.x > 1.0f) ? 1.0f : (rval.x < 0.0f) ? 0.0f : rval.x;
  rval.y          = (rval.y > 1.0f) ? 1.0f : (rval.y < 0.0f) ? 0.0f : rval.y;
  rval.z          = (rval.z > 1.0f) ? 1.0f : (rval.z < 0.0f) ? 0.0f : rval.z;
  rval.w          = (rval.w > 1.0f) ? 1.0f : (rval.w < 0.0f) ? 0.0f : rval.w;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::Black(void) {
  static const Vector4<T> Black(T(0.0f), T(0.0f), T(0.0f), T(1.0f));
  return Black;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::DarkGrey(void) {
  static const Vector4<T> DarkGrey(T(0.250f), T(0.250f), T(0.250f), T(1.0f));
  return DarkGrey;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::MediumGrey(void) {
  static const Vector4<T> MediumGrey(T(0.50f), T(0.50f), T(0.50f), T(1.0f));
  return MediumGrey;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::LightGrey(void) {
  static const Vector4<T> LightGrey(T(0.75f), T(0.75f), T(0.75f), T(1.0f));
  return LightGrey;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::White(void) {
  static const Vector4<T> White(T(1.0f), T(1.0f), T(1.0f), T(1.0f));
  return White;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::Red(void) {
  static const Vector4<T> Red(T(1.0f), T(0.0f), T(0.0f), T(1.0f));
  return Red;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::Green(void) {
  static const Vector4<T> Green(T(0.0f), T(1.0f), T(0.0f), T(1.0f));
  return Green;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::Blue(void) {
  static const Vector4<T> Blue(T(0.0f), T(0.0f), T(1.0f), T(1.0f));
  return Blue;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::Magenta(void) {
  static const Vector4<T> Magenta(T(1.0f), T(0.0f), T(1.0f), T(1.0f));
  return Magenta;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::Cyan(void) {
  static const Vector4<T> Cyan(T(0.0f), T(1.0f), T(1.0f), T(1.0f));
  return Cyan;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const Vector4<T>& Vector4<T>::Yellow(void) {
  static const Vector4<T> Yellow(T(1.0f), T(1.0f), T(0.0f), T(1.0f));
  return Yellow;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Vector4<T>::Vector4()
    : x(T(0.0f))
    , y(T(0.0f))
    , z(T(0.0f))
    , w(T(1.0f)) {
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Vector4<T>::Vector4(T _x, T _y, T _z, T _w)
    : x(_x)
    , y(_y)
    , z(_z)
    , w(_w) {
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
Vector4<T>::Vector4(const Vector3<T>& in, T _w)
    : x(in.x)
    , y(in.y)
    , z(in.z)
    , w(_w) {
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> uint64_t Vector4<T>::GetRGBAU64(void) const {
  uint64_t r = round(x * T(65535.0f));
  uint64_t g = round(y * T(65535.0f));
  uint64_t b = round(z * T(65535.0f));
  uint64_t a = round(w * T(65535.0f));
  return ((r << 48) | (g << 32) | (b << 16) | a);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::SetRGBAU64(uint64_t inp) {
  static constexpr T kfic(T(1.0) / T(65535.0));
  uint16_t r = (inp)&0xffff;
  uint16_t g = (inp >> 16) & 0xffff;
  uint16_t b = (inp >> 32) & 0xffff;
  uint16_t a = (inp >> 48) & 0xffff;
  x          = T(r);
  y          = T(g);
  z          = T(b);
  w          = T(a);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> U32 Vector4<T>::GetVtxColorAsU32(void) const {
  U32 r = U32(x * T(255.0f));
  U32 g = U32(y * T(255.0f));
  U32 b = U32(z * T(255.0f));
  U32 a = U32(w * T(255.0f));

  //#if defined(ORK_CONFIG_DARWIN)||defined(ORK_CONFIG_IX)//GL
  return U32((a << 24) | (b << 16) | (g << 8) | r);
  //#else // WIN32/DX
  //	return U32( (a<<24)|(r<<16)|(g<<8)|b );
  //#endif
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> U32 Vector4<T>::GetABGRU32(void) const {
  U32 r = U32(x * T(255.0f));
  U32 g = U32(y * T(255.0f));
  U32 b = U32(z * T(255.0f));
  U32 a = U32(w * T(255.0f));

  return U32((a << 24) | (b << 16) | (g << 8) | r);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> U32 Vector4<T>::GetARGBU32(void) const {
  U32 r = U32(x * T(255.0f));
  U32 g = U32(y * T(255.0f));
  U32 b = U32(z * T(255.0f));
  U32 a = U32(w * T(255.0f));

  return U32((a << 24) | (r << 16) | (g << 8) | b);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> U32 Vector4<T>::GetRGBAU32(void) const {
  S32 r = U32(x * T(255.0f));
  S32 g = U32(y * T(255.0f));
  S32 b = U32(z * T(255.0f));
  S32 a = U32(w * T(255.0f));

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

template <typename T> U32 Vector4<T>::GetBGRAU32(void) const {
  U32 r = U32(x * T(255.0f));
  U32 g = U32(y * T(255.0f));
  U32 b = U32(z * T(255.0f));
  U32 a = U32(w * T(255.0f));

  return U32((b << 24) | (g << 16) | (r << 8) | a);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> U16 Vector4<T>::GetRGBU16(void) const {
  U32 r = U32(x * T(31.0f));
  U32 g = U32(y * T(31.0f));
  U32 b = U32(z * T(31.0f));

  U16 rval = U16((b << 10) | (g << 5) | r);

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::SetRGBAU32(U32 uval) {
  U32 r = (uval >> 24) & 0xff;
  U32 g = (uval >> 16) & 0xff;
  U32 b = (uval >> 8) & 0xff;
  U32 a = (uval)&0xff;

  static const T kfic(1.0f / 255.0f);

  SetX(kfic * T(int(r)));
  SetY(kfic * T(int(g)));
  SetZ(kfic * T(int(b)));
  SetW(kfic * T(int(a)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::SetBGRAU32(U32 uval) {
  U32 b = (uval >> 24) & 0xff;
  U32 g = (uval >> 16) & 0xff;
  U32 r = (uval >> 8) & 0xff;
  U32 a = (uval)&0xff;

  static const T kfic(1.0f / 255.0f);

  SetX(kfic * T(int(r)));
  SetY(kfic * T(int(g)));
  SetZ(kfic * T(int(b)));
  SetW(kfic * T(int(a)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::SetARGBU32(U32 uval) {
  U32 a = (uval >> 24) & 0xff;
  U32 r = (uval >> 16) & 0xff;
  U32 g = (uval >> 8) & 0xff;
  U32 b = (uval)&0xff;

  static const T kfic(1.0f / 255.0f);

  SetX(kfic * T(int(r)));
  SetY(kfic * T(int(g)));
  SetZ(kfic * T(int(b)));
  SetW(kfic * T(int(a)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::SetABGRU32(U32 uval) {
  U32 a = (uval >> 24) & 0xff;
  U32 b = (uval >> 16) & 0xff;
  U32 g = (uval >> 8) & 0xff;
  U32 r = (uval)&0xff;

  static const T kfic(1.0f / 255.0f);

  SetX(kfic * T(int(r)));
  SetY(kfic * T(int(g)));
  SetZ(kfic * T(int(b)));
  SetW(kfic * T(int(a)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::SetHSV(T h, T s, T v) {
  //	hsv.x = saturate(hsv.x);
  //	hsv.y = saturate(hsv.y);
  //	hsv.z = saturate(hsv.z);

  if (s == 0.0f) {
    // Grayscale
    SetX(v);
    SetY(v);
    SetZ(v);
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
      SetX(v);
      SetY(cc);
      SetZ(aa);
    } else if (i < 2.0f) {
      SetX(bb);
      SetY(v);
      SetZ(aa);
    } else if (i < 3.0f) {
      SetX(aa);
      SetY(v);
      SetZ(cc);
    } else if (i < 4.0f) {
      SetX(aa);
      SetY(bb);
      SetZ(v);
    } else if (i < 5.0f) {
      SetX(cc);
      SetY(aa);
      SetZ(v);
    } else {
      SetX(v);
      SetY(aa);
      SetZ(bb);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::PerspectiveDivide(void) {
  T iw = T(1.0f) / w;
  x *= iw;
  y *= iw;
  z *= iw;
  w = T(1.0f);
}

template <typename T> Vector4<T> Vector4<T>::perspectiveDivided(void) const {
  Vector4<T> rval;
  T iw   = T(1.0f) / w;
  rval.x = x * iw;
  rval.y = y * iw;
  rval.z = z * iw;
  rval.w = T(1.0);
  return rval;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> Vector4<T>::Vector4(const Vector4<T>& vec) {
  x = vec.x;
  y = vec.y;
  z = vec.z;
  w = vec.w;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T Vector4<T>::Dot(const Vector4<T>& vec) const {
  return ((x * vec.x) + (y * vec.y) + (z * vec.z));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
Vector4<T> Vector4<T>::Cross(const Vector4<T>& vec) const // c = this X vec
{
  T vx = ((y * vec.z) - (z * vec.y));
  T vy = ((z * vec.x) - (x * vec.z));
  T vz = ((x * vec.y) - (y * vec.x));

  return (Vector4<T>(vx, vy, vz));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::Normalize(void) {
  T distance = (T)1.0f / Mag();

  x *= distance;
  y *= distance;
  z *= distance;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> Vector4<T> Vector4<T>::Normal() const {
  T fmag = Mag();
  fmag   = (fmag == (T)0.0f) ? (T)0.00001f : fmag;
  T s    = (T)1.0f / fmag;
  return Vector4<T>(x * s, y * s, z * s, w);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T Vector4<T>::Mag(void) const {
  return Sqrt(x * x + y * y + z * z);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T Vector4<T>::MagSquared(void) const {
  T mag = (x * x + y * y + z * z);
  return mag;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> Vector4<T> Vector4<T>::Transform(const Matrix44<T>& matrix) const {
  T tx, ty, tz, tw;

  T* mp = (T*)matrix.elements;
  T _x  = x;
  T _y  = y;
  T _z  = z;
  T _w  = w;

  tx = _x * mp[0] + _y * mp[4] + _z * mp[8] + _w * mp[12];
  ty = _x * mp[1] + _y * mp[5] + _z * mp[9] + _w * mp[13];
  tz = _x * mp[2] + _y * mp[6] + _z * mp[10] + _w * mp[14];
  tw = _x * mp[3] + _y * mp[7] + _z * mp[11] + _w * mp[15];

  return (Vector4<T>(tx, ty, tz, tw));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
void Vector4<T>::Serp(const Vector4<T>& PA, const Vector4<T>& PB, const Vector4<T>& PC, const Vector4<T>& PD, T Par) {
  Vector4<T> PAB, PCD;
  PAB.Lerp(PA, PB, Par);
  PCD.Lerp(PC, PD, Par);
  Lerp(PAB, PCD, Par);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::RotateX(T rad) {
  T oldY = y;
  T oldZ = z;
  y      = (oldY * Cos(rad) - oldZ * Sin(rad));
  z      = (oldY * Sin(rad) + oldZ * Cos(rad));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::RotateY(T rad) {
  T oldX = x;
  T oldZ = z;

  x = (oldX * Cos(rad) - oldZ * Sin(rad));
  z = (oldX * Sin(rad) + oldZ * Cos(rad));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::RotateZ(T rad) {
  T oldX = x;
  T oldY = y;

  x = (oldX * Cos(rad) - oldY * Sin(rad));
  y = (oldX * Sin(rad) + oldY * Cos(rad));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Vector4<T>::Lerp(const Vector4<T>& from, const Vector4<T>& to, T par) {
  if (par < T(0.0f))
    par = T(0.0f);
  if (par > T(1.0f))
    par = T(1.0f);
  T ipar = T(1.0f) - par;
  x      = (from.x * ipar) + (to.x * par);
  y      = (from.y * ipar) + (to.y * par);
  z      = (from.z * ipar) + (to.z * par);
  w      = (from.w * ipar) + (to.w * par);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
T Vector4<T>::CalcTriArea(const Vector4<T>& V0, const Vector4<T>& V1, const Vector4<T>& V2, const Vector4<T>& N) {
  // select largest abs coordinate to ignore for projection
  T ax = Abs(N.x);
  T ay = Abs(N.y);
  T az = Abs(N.z);

  int coord = (ax > ay) ? ((ax > az) ? 1 : 3) : ((ay > az) ? 2 : 3);

  // compute area of the 2D projection

  Vector4<T> Ary[3];
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
