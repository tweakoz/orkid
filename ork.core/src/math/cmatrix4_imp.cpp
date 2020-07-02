////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/math/cmatrix4.h>
#include <ork/math/cmatrix4.hpp>
#include <ork/math/matrix_inverseGEMS.hpp>
#include <ork/kernel/string/string.h>
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>

namespace ork {
template <> const EPropType PropType<fmtx4>::meType   = EPROPTYPE_MAT44REAL;
template <> const char* PropType<fmtx4>::mstrTypeName = "MAT44REAL";
template <> void PropType<fmtx4>::ToString(const fmtx4& Value, PropTypeString& tstr) {
  const fmtx4& v = Value;

  std::string result;
  for (int i = 0; i < 15; i++)
    result += CreateFormattedString("%g ", F32(v.elements[i / 4][i % 4]));
  result += CreateFormattedString("%g", F32(v.elements[3][3]));
  tstr.format("%s", result.c_str());
}

template <> fmtx4 PropType<fmtx4>::FromString(const PropTypeString& String) {
  float m[4][4];
  sscanf(
      String.c_str(),
      "%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g",
      &m[0][0],
      &m[0][1],
      &m[0][2],
      &m[0][3],
      &m[1][0],
      &m[1][1],
      &m[1][2],
      &m[1][3],
      &m[2][0],
      &m[2][1],
      &m[2][2],
      &m[2][3],
      &m[3][0],
      &m[3][1],
      &m[3][2],
      &m[3][3]);
  fmtx4 result;
  for (int i = 0; i < 16; i++)
    result.elements[i / 4][i % 4] = m[i / 4][i % 4];
  return result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace reflect {
template <> //
void ::ork::reflect::ITyped<fmtx4>::serialize(serdes::node_ptr_t sernode) const {
  using namespace serdes;
  auto serializer        = sernode->_serializer;
  auto instance          = sernode->_ser_instance;
  auto arynode           = serializer->pushNode(_name, serdes::NodeType::ARRAY);
  arynode->_parent       = sernode;
  arynode->_ser_instance = instance;
  fmtx4 value;
  get(value, instance);
  for (int i = 0; i < 16; i++)
    serializeArraySubLeaf(arynode, value.GetArray()[i], i);
  serializer->popNode(); // pop arraynode
}
template <> //
void ::ork::reflect::ITyped<fmtx4>::deserialize(serdes::node_ptr_t arynode) const {
  using namespace serdes;
  auto deserializer  = arynode->_deserializer;
  auto instance      = arynode->_deser_instance;
  size_t numelements = arynode->_numchildren;
  OrkAssert(numelements == 16);

  fmtx4 value;
  for (int i = 0; i < 16; i++)
    value.GetArray()[i] = deserializeArraySubLeaf<float>(arynode, i);

  set(value, instance);

  OrkAssert(false);
}

/*template <> void Serialize(const fmtx4* in, fmtx4* out, reflect::BidirectionalSerializer& bidi) {
  if (bidi.Serializing()) {
    using namespace std::literals;
    bidi.Serializer()->Hint("type", "fmtx4"s);
    for (int i = 0; i < 16; i++) {
      bidi | in->GetArray()[i];
    }
  } else {
    for (int i = 0; i < 16; i++) {
      bidi | out->GetArray()[i];
    }
  }
}
*/
} // namespace reflect
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template class PropType<fmtx4>;
template class Matrix44<float>; // explicit template instantiation

} // namespace ork
