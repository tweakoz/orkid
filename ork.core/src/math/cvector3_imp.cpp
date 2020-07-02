////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/math/cvector3.h>
#include <ork/math/cvector3.hpp>
#include <ork/math/misc_math.h>
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

template <> float Vector3<float>::Sin(float fin) {
  return sinf(fin);
}
template <> float Vector3<float>::Cos(float fin) {
  return cosf(fin);
}
template <> float Vector3<float>::Sqrt(float fin) {
  return sqrtf(fin);
}
template <> float Vector3<float>::Epsilon() {
  return Float::FloatEpsilon();
}
template <> float Vector3<float>::Abs(float fin) {
  return fabs(fin);
}

///////////////////////////////////////////////////////////////////////////////

template <> double Vector3<double>::Sin(double fin) {
  return (double)sinf((float)fin);
}
template <> double Vector3<double>::Cos(double fin) {
  return (double)cosf((float)fin);
}
template <> double Vector3<double>::Sqrt(double fin) {
  return (double)sqrtf((float)fin);
}
template <> double Vector3<double>::Epsilon() {
  return Float::DoubleEpsilon();
}
template <> double Vector3<double>::Abs(double fin) {
  return (double)fabs((float)fin);
}

// FIXED ///////////////////////////////////////////////////////////////////////

template <> const EPropType PropType<Vector3<float>>::meType   = EPROPTYPE_VEC3FLOAT;
template <> const char* PropType<Vector3<float>>::mstrTypeName = "VEC3FLOAT";
template <> void PropType<Vector3<float>>::ToString(const Vector3<float>& Value, PropTypeString& tstr) {
  Vector3<float> v = Value;
  tstr.format("%g %g %g", float(v.GetX()), float(v.GetY()), float(v.GetZ()));
}

template <> Vector3<float> PropType<Vector3<float>>::FromString(const PropTypeString& String) {
  float x, y, z;
  sscanf(String.c_str(), "%g %g %g", &x, &y, &z);
  return Vector3<float>(float(x), float(y), float(z));
}

///////////////////////////////////////////////////////////////////////////////

template class Vector3<float>; // explicit template instantiation
template class PropType<Vector3<float>>;

template class Vector3<double>; // explicit template instantiation

///////////////////////////////////////////////////////////////////////////////

namespace reflect {
using namespace serdes;
template <> //
void ::ork::reflect::ITyped<fvec3>::serialize(serdes::node_ptr_t sernode) const {
  OrkAssert(false);
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
void ::ork::reflect::ITyped<fvec3>::deserialize(serdes::node_ptr_t arynode) const {
  OrkAssert(false);
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

  OrkAssert(false);
}

/*
template <> void Serialize(const fvec3* in, fvec3* out, reflect::BidirectionalSerializer& bidi) {
  using namespace std::literals;
  if (bidi.Serializing()) {
    bidi.Serializer()->Hint("type", "fvec3"s);
    for (int i = 0; i < 3; i++) {
      bidi | in->GetArray()[i];
    }
  } else {
    for (int i = 0; i < 3; i++) {
      bidi | out->GetArray()[i];
    }
  }
}
*/
} // namespace reflect
} // namespace ork
