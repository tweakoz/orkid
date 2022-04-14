////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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
template struct Quaternion<float>; // explicit template instantiation
typedef struct Quaternion<float> fquat;
template <> const EPropType PropType<fquat>::meType   = EPROPTYPE_QUATERNION;
template <> const char* PropType<fquat>::mstrTypeName = "QUATERNION";

///////////////////////////////////////////////////////////////////////////////

template <> void PropType<fquat>::ToString(const fquat& Value, PropTypeString& tstr) {
  const fquat& v = Value;

  std::string result;
  result += CreateFormattedString("%g ", v.x);
  result += CreateFormattedString("%g ", v.y);
  result += CreateFormattedString("%g ", v.z);
  result += CreateFormattedString("%g ", v.w);
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

} // namespace ork
