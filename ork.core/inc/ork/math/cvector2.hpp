////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
#pragma once
////////////////////////////////////////////////////////////////
#include <ork/math/cvector3.h>
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>

///////////////////////////////////////////////////////////////////////////////

template <typename T>
ork::Vector2<T>::Vector2()
    : x(T(0))
    , y(T(0)) {
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
ork::Vector2<T>::Vector2(T _x, T _y)
    : x(_x)
    , y(_y) {
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
ork::Vector2<T>::Vector2(const Vector2<T>& vec)
    : x(vec.GetX())
    , y(vec.GetY()) {
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
ork::Vector2<T>::Vector2(const Vector3<T>& vec)
    : x(vec.GetX())
    , y(vec.GetY()) {
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T ork::Vector2<T>::Dot(const Vector2<T>& vec) const {
  return ((x * vec.x) + (y * vec.y));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T ork::Vector2<T>::PerpDot(const Vector2<T>& oth) const {
  return (x * oth.y) - (y * oth.x);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void ork::Vector2<T>::Normalize(void) {
  T distance = T(1) / Mag();

  x *= distance;
  y *= distance;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> ork::Vector2<T> ork::Vector2<T>::Normal() const {
  T fmag = Mag();
  fmag   = (fmag == T(0)) ? Epsilon() : fmag;
  T s    = T(1) / fmag;
  return Vector2<T>(x * s, y * s);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T ork::Vector2<T>::Mag(void) const {
  return Sqrt(x * x + y * y);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T ork::Vector2<T>::MagSquared(void) const {
  T mag = (x * x + y * y);
  return mag;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
void ork::Vector2<T>::Serp(const Vector2<T>& PA, const Vector2<T>& PB, const Vector2<T>& PC, const Vector2<T>& PD, T Par) {
  Vector2<T> PAB, PCD;
  PAB.Lerp(PA, PB, Par);
  PCD.Lerp(PC, PD, Par);
  Lerp(PAB, PCD, Par);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void ork::Vector2<T>::Rotate(T rad) {
  T oldX = x;
  T oldY = y;

  x = (oldX * Sin(rad) - oldY * Cos(rad));
  y = (oldX * Cos(rad) + oldY * Sin(rad));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void ork::Vector2<T>::Lerp(const Vector2<T>& from, const Vector2<T>& to, T par) {
  if (par < T(0))
    par = T(0);
  if (par > T(1))
    par = T(1);
  T ipar = T(1) - par;
  x      = (from.x * ipar) + (to.x * par);
  y      = (from.y * ipar) + (to.y * par);
}

///////////////////////////////////////////////////////////////////////////////
namespace ork::reflect {
///////////////////////////////////////////////////////////////////////////////
template <> //
inline void ::ork::reflect::ITyped<fvec2>::serialize(serdes::node_ptr_t sernode) const {
  using namespace serdes;
  auto serializer        = sernode->_serializer;
  auto instance          = sernode->_ser_instance;
  auto arynode           = serializer->pushNode(_name, serdes::NodeType::ARRAY);
  arynode->_parent       = sernode;
  arynode->_ser_instance = instance;
  fvec2 value;
  get(value, instance);
  serializeArraySubLeaf(arynode, value.x, 0);
  serializeArraySubLeaf(arynode, value.y, 1);
  serializer->popNode(); // pop arraynode
}
template <> //
inline void ::ork::reflect::ITyped<fvec2>::deserialize(serdes::node_ptr_t arynode) const {
  using namespace serdes;
  auto deserializer  = arynode->_deserializer;
  auto instance      = arynode->_deser_instance;
  size_t numelements = arynode->_numchildren;
  OrkAssert(numelements == 2);

  fvec2 outval;
  outval.x = deserializeArraySubLeaf<float>(arynode, 0);
  outval.y = deserializeArraySubLeaf<float>(arynode, 1);
  set(outval, instance);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect
///////////////////////////////////////////////////////////////////////////////
