////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#pragma once
////////////////////////////////////////////////////////////////
#include <ork/math/cvector3.h>
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.inl>

#include <glm/gtx/vector_angle.hpp>

///////////////////////////////////////////////////////////////////////////////

template <typename T>
ork::Vector2<T>::Vector2()
    : base_t(0,0) {
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
ork::Vector2<T>::Vector2(T _x, T _y)
    : base_t(_x,_y) {
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
ork::Vector2<T>::Vector2(const Vector2<T>& vec)
    : base_t(vec) {
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
ork::Vector2<T>::Vector2(const Vector3<T>& vec)
    : base_t(vec.x,vec.y) {
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T ork::Vector2<T>::dotWith(const Vector2<T>& vec) const {
  return ((this->x * vec.x) + (this->y * vec.y));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T ork::Vector2<T>::perpDotWith(const Vector2<T>& oth) const {
  return (this->x * oth.y) - (this->y * oth.x);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void ork::Vector2<T>::normalizeInPlace() {
  T distance = T(1) / magnitude();

  this->x *= distance;
  this->y *= distance;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> ork::Vector2<T> ork::Vector2<T>::normalized() const {
  T fmag = magnitude();
  fmag   = (fmag == T(0)) ? Epsilon() : fmag;
  T s    = T(1) / fmag;
  return Vector2<T>(this->x * s, this->y * s);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T ork::Vector2<T>::magnitude() const {
  return Sqrt(this->x * this->x + this->y * this->y);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T ork::Vector2<T>::magnitudeSquared() const {
  T mag = (this->x * this->x + this->y * this->y);
  return mag;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> //
uint64_t ork::Vector2<T>::hash(T quantization) const{
  boost::Crc64 crc64;
  crc64.init();

  int bias = (1<<24); // to make negative numbers positive
  
  int xx = int((this->x * quantization)+0.5);
  int yy = int((this->y * quantization)+0.5);
  OrkAssert((xx+bias)>=0);
  OrkAssert((yy+bias)>=0);
  uint64_t a = (xx+bias);
  uint64_t b = (yy+bias);
  OrkAssert(a<0xffffffff);
  OrkAssert(b<0xffffffff);
  crc64.accumulateItem(a);
  crc64.accumulateItem(b);
  crc64.finish();

  return crc64.result();}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
T ork::Vector2<T>::angle(const Vector2& vec) const{
  const base_t& a = *this;
  const base_t& b = vec;

  return T(glm::angle(a,b));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
void ork::Vector2<T>::serp(const Vector2<T>& PA, //
                           const Vector2<T>& PB, //
                           const Vector2<T>& PC, //
                           const Vector2<T>& PD, //
                           T par_x, T par_y) {
  Vector2<T> PAB, PCD;
  PAB.lerp(PA, PB, par_x);
  PCD.lerp(PC, PD, par_x);
  lerp(PAB, PCD, par_y);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void ork::Vector2<T>::rotate(T rad) {
  T previousX = this->x;
  T previousY = this->y;

  this->x = (previousX * Sin(rad) - previousY * Cos(rad));
  this->y = (previousX * Cos(rad) + previousY * Sin(rad));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void ork::Vector2<T>::lerp(const Vector2<T>& from, const Vector2<T>& to, T par) {
  if (par < T(0))
    par = T(0);
  if (par > T(1))
    par = T(1);
  T ipar = T(1) - par;
  this->x      = (from.x * ipar) + (to.x * par);
  this->y      = (from.y * ipar) + (to.y * par);
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
