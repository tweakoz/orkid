////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/math/quaternion.h>
#include <ork/math/quaternion.hpp>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/reflect/Serialize.h>
#include <ork/reflect/BidirectionalSerializer.h>
#include <ork/kernel/string/string.h>

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

/*
namespace reflect {

template <> void Serialize(const fquat* in, fquat* out, reflect::BidirectionalSerializer& bidi) {
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
} // namespace reflect
*/
} // namespace ork
