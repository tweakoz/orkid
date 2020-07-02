////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix3.hpp>
#include <ork/kernel/string/string.h>
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>

namespace ork {
template <> const EPropType PropType<fmtx3>::meType   = EPROPTYPE_MAT33REAL;
template <> const char* PropType<fmtx3>::mstrTypeName = "MAT33REAL";
template <> void PropType<fmtx3>::ToString(const fmtx3& Value, PropTypeString& tstr) {
  const fmtx3& v = Value;

  std::string result;
  for (int i = 0; i < 9; i++)
    result += CreateFormattedString("%g ", F32(v.elements[i / 3][i % 3]));
  result += CreateFormattedString("%g", F32(v.elements[2][2]));
  tstr.format("%s", result.c_str());
}

template <> fmtx3 PropType<fmtx3>::FromString(const PropTypeString& String) {
  float m[3][3];
  sscanf(
      String.c_str(),
      "%g %g %g %g %g %g %g %g %g",
      &m[0][0],
      &m[0][1],
      &m[0][2],
      &m[1][0],
      &m[1][1],
      &m[1][2],
      &m[2][0],
      &m[2][1],
      &m[2][2]);
  fmtx3 result;
  for (int i = 0; i < 9; i++)
    result.elements[i / 3][i % 3] = m[i / 3][i % 3];
  return result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace reflect {
template <> //
void ::ork::reflect::ITyped<fmtx3>::serialize(serdes::node_ptr_t sernode) const {
  using namespace serdes;
  auto serializer        = sernode->_serializer;
  auto instance          = sernode->_ser_instance;
  auto arynode           = serializer->pushNode(_name, serdes::NodeType::ARRAY);
  arynode->_parent       = sernode;
  arynode->_ser_instance = instance;
  fmtx3 value;
  get(value, instance);
  for (int i = 0; i < 9; i++)
    serializeArraySubLeaf(arynode, value.GetArray()[i], i);
  serializer->popNode(); // pop arraynode
}
template <> //
void ::ork::reflect::ITyped<fmtx3>::deserialize(serdes::node_ptr_t arynode) const {
  using namespace serdes;
  auto deserializer  = arynode->_deserializer;
  auto instance      = arynode->_deser_instance;
  size_t numelements = arynode->_numchildren;
  OrkAssert(numelements == 9);

  fmtx3 value;
  for (int i = 0; i < 9; i++)
    value.GetArray()[i] = deserializeArraySubLeaf<float>(arynode, i);

  set(value, instance);

  OrkAssert(false);
}

/*
template <> void Serialize(const fmtx3* in, fmtx3* out, reflect::BidirectionalSerializer& bidi) {
using namespace std::literals;
if (bidi.Serializing()) {
  bidi.Serializer()->Hint("type", "fmtx3"s);
  for (int i = 0; i < 9; i++) {
    bidi | in->GetArray()[i];
  }
} else {
  for (int i = 0; i < 9; i++) {
    bidi | out->GetArray()[i];
  }
}
}*/
} // namespace reflect

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template class PropType<fmtx3>;
template class Matrix33<float>; // explicit template instantiation

} // namespace ork
