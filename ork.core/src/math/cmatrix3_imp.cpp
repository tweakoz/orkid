////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix3.hpp>
#include <ork/kernel/string/string.h>

namespace ork {
template <> const EPropType PropType<fmtx3>::meType   = EPROPTYPE_MAT33REAL;
template <> const char* PropType<fmtx3>::mstrTypeName = "MAT33REAL";
template <> void PropType<fmtx3>::ToString(const fmtx3& Value, PropTypeString& tstr) {
  const fmtx3& v = Value;

  std::string result;
  for (int i = 0; i < 9; i++)
    result += CreateFormattedString("%g ", F32(v.elemXY(i / 3,i % 3)));
  result += CreateFormattedString("%g", F32(v.elemXY(2,2)));
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
    result.setElemXY(i / 3,i % 3, m[i / 3][i % 3]);
  return result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template struct PropType<fmtx3>;
template struct Matrix33<float>; // explicit template instantiation

} // namespace ork
