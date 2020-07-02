////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/math/quaternion.h>
#include <ork/math/quaternion.hpp>
#include <ork/kernel/string/string.h>
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>

namespace ork {
template class Quaternion<float>; // explicit template instantiation
typedef class Quaternion<float> fquat;
template <> const EPropType PropType<fquat>::meType   = EPROPTYPE_QUATERNION;
template <> const char* PropType<fquat>::mstrTypeName = "QUATERNION";

///////////////////////////////////////////////////////////////////////////////

template <> void PropType<fquat>::ToString(const fquat& Value, PropTypeString& tstr) {
  const fquat& v = Value;

  std::string result;
  result += CreateFormattedString("%g ", v.GetX());
  result += CreateFormattedString("%g ", v.GetY());
  result += CreateFormattedString("%g ", v.GetZ());
  result += CreateFormattedString("%g ", v.width());
  tstr.format("%s", result.c_str());
}

///////////////////////////////////////////////////////////////////////////////

template <> fquat PropType<fquat>::FromString(const PropTypeString& String) {
  float m[4];
  sscanf(String.c_str(), "%g %g %g %g", &m[0], &m[1], &m[2], &m[3]);
  fquat result;
  result.x = (m[0]);
  result.y = (m[1]);
  result.z = (m[2]);
  result.w = (m[3]);
  return result;
}

///////////////////////////////////////////////////////////////////////////////

namespace reflect {

template <> //
void ::ork::reflect::ITyped<fquat>::serialize(serdes::node_ptr_t sernode) const {
  using namespace serdes;
  auto serializer        = sernode->_serializer;
  auto instance          = sernode->_ser_instance;
  auto arynode           = serializer->pushNode(_name, serdes::NodeType::ARRAY);
  arynode->_parent       = sernode;
  arynode->_ser_instance = instance;
  fquat value;
  get(value, instance);
  serializeArraySubLeaf(arynode, value.x, 0);
  serializeArraySubLeaf(arynode, value.y, 1);
  serializeArraySubLeaf(arynode, value.z, 2);
  serializeArraySubLeaf(arynode, value.w, 3);
  serializer->popNode(); // pop arraynode
}
template <> //
void ::ork::reflect::ITyped<fquat>::deserialize(serdes::node_ptr_t arynode) const {
  using namespace serdes;
  auto deserializer  = arynode->_deserializer;
  auto instance      = arynode->_deser_instance;
  size_t numelements = arynode->_numchildren;
  OrkAssert(numelements == 4);

  fquat outval;
  outval.x = deserializeArraySubLeaf<float>(arynode, 0);
  outval.y = deserializeArraySubLeaf<float>(arynode, 1);
  outval.z = deserializeArraySubLeaf<float>(arynode, 2);
  outval.w = deserializeArraySubLeaf<float>(arynode, 3);
  set(outval, instance);

  OrkAssert(false);
}

/*template <> void Serialize(const fquat* in, fquat* out, reflect::BidirectionalSerializer& bidi) {
  using namespace std::literals;
  if (bidi.Serializing()) {
    bidi.Serializer()->Hint("type", "fquat"s);
    bidi | in->GetX();
    bidi | in->GetY();
    bidi | in->GetZ();
    bidi | in->width();
  } else {
    float m[4];
    for (int i = 0; i < 4; i++) {
      bidi | m[i];
    }
    out->x = (m[0]);
    out->y = (m[1]);
    out->z = (m[2]);
    out->w = (m[3]);
  }
}
*/
} // namespace reflect
} // namespace ork
