////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/math/cvector2.h>
#include <ork/math/cvector2.hpp>
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

template <> float Vector2<float>::Sin(float fin) {
  return sinf(fin);
}
template <> float Vector2<float>::Cos(float fin) {
  return cosf(fin);
}
template <> float Vector2<float>::Sqrt(float fin) {
  return sqrtf(fin);
}
template <> float Vector2<float>::Epsilon() {
  return Float::Epsilon();
}
template <> float Vector2<float>::Abs(float fin) {
  return fabs(fin);
}

///////////////////////////////////////////////////////////////////////////////

template <> double Vector2<double>::Sin(double fin) {
  return sinf(fin);
}
template <> double Vector2<double>::Cos(double fin) {
  return cosf(fin);
}
template <> double Vector2<double>::Sqrt(double fin) {
  return sqrtf(fin);
}
template <> double Vector2<double>::Epsilon() {
  return double(Float::Epsilon());
}
template <> double Vector2<double>::Abs(double fin) {
  return fabs(fin);
}

///////////////////////////////////////////////////////////////////////////////

template <> const EPropType PropType<fvec2>::meType   = EPROPTYPE_VEC2REAL;
template <> const char* PropType<fvec2>::mstrTypeName = "VEC2REAL";
template <> void PropType<fvec2>::ToString(const fvec2& Value, PropTypeString& tstr) {
  fvec2 v = Value;
  tstr.format("%g %g", float(v.GetX()), float(v.GetY()));
}

template <> fvec2 PropType<fvec2>::FromString(const PropTypeString& String) {
  float x, y;
  sscanf(String.c_str(), "%g %g", &x, &y);
  return fvec2(float(x), float(y));
}

///////////////////////////////////////////////////////////////////////////////

template class Vector2<float>;  // explicit template instantiation
template class Vector2<double>; // explicit template instantiation
template class PropType<fvec2>;

///////////////////////////////////////////////////////////////////////////////
namespace reflect {
///////////////////////////////////////////////////////////////////////////////
template <> //
void ::ork::reflect::ITyped<fvec2>::serialize(serdes::node_ptr_t sernode) const {
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
void ::ork::reflect::ITyped<fvec2>::deserialize(serdes::node_ptr_t desernode) const {
  // auto instance      = desernode->_deser_instance;
  // const auto& var    = desernode->_value;
  // const auto& as_fv2 = var.Get<fvec2>();
  // set(as_fv2, instance);
  OrkAssert(false);
}
///////////////////////////////////////////////////////////////////////////////
/*template <> void Serialize(const fvec2* in, fvec2* out, reflect::BidirectionalSerializer& bidi) {

  using namespace std::literals;
  if (bidi.Serializing()) {
    bidi.Serializer()->Hint("type", "fvec2"s);
    for (int i = 0; i < 2; i++) {
      bidi | in->GetArray()[i];
    }
  } else {
    for (int i = 0; i < 2; i++) {
      bidi | out->GetArray()[i];
    }
  }
}*/
///////////////////////////////////////////////////////////////////////////////
} // namespace reflect
} // namespace ork
