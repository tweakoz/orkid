////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/math/cvector4.h>
#include <ork/math/cvector4.hpp>
#include <ork/math/matrix_inverseGEMS.hpp>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/reflect/Serialize.h>
#include <ork/reflect/BidirectionalSerializer.h>

namespace ork {
///////////////////////////////////////////////////////////////////////////////

template <> float Vector4<float>::Sin(float fin) {
  return sinf(fin);
}
template <> float Vector4<float>::Cos(float fin) {
  return cosf(fin);
}
template <> float Vector4<float>::Sqrt(float fin) {
  return sqrtf(fin);
}
template <> float Vector4<float>::Epsilon() {
  return Float::Epsilon();
}
template <> float Vector4<float>::Abs(float fin) {
  return fabs(fin);
}

///////////////////////////////////////////////////////////////////////////////

template <> double Vector4<double>::Sin(double fin) {
  return sinf(fin);
}
template <> double Vector4<double>::Cos(double fin) {
  return cosf(fin);
}
template <> double Vector4<double>::Sqrt(double fin) {
  return sqrtf(fin);
}
template <> double Vector4<double>::Epsilon() {
  return double(Float::Epsilon());
}
template <> double Vector4<double>::Abs(double fin) {
  return fabs(fin);
}

///////////////////////////////////////////////////////////////////////////////

template <> const EPropType PropType<fvec4>::meType   = EPROPTYPE_VEC4REAL;
template <> const char* PropType<fvec4>::mstrTypeName = "VEC4REAL";
template <> void PropType<fvec4>::ToString(const fvec4& Value, PropTypeString& tstr) {
  tstr.format("%g %g %g %g", float(Value.x), float(Value.y), float(Value.z), float(Value.w));
}

template <> fvec4 PropType<fvec4>::FromString(const PropTypeString& String) {
  float x, y, z, w;
  sscanf(String.c_str(), "%g %g %g %g", &x, &y, &z, &w);
  return fvec4(float(x), float(y), float(z), float(w));
}

///////////////////////////////////////////////////////////////////////////////

template class Vector4<float>;  // explicit template instantiation
template class Vector4<double>; // explicit template instantiation
template class PropType<fvec4>;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace reflect {

template <> void Serialize(const fvec4* in, fvec4* out, reflect::BidirectionalSerializer& bidi) {
  using namespace std::literals;
  if (bidi.Serializing()) {
    bidi.Serializer()->Hint("type", "fvec4"s);
    for (int i = 0; i < 4; i++) {
      bidi | in->GetArray()[i];
    }
  } else {
    for (int i = 0; i < 4; i++) {
      bidi | out->GetArray()[i];
    }
  }
}

template <> void Serialize(const orkmap<float, fvec4>* in, orkmap<float, fvec4>* out, reflect::BidirectionalSerializer& bidi) {
  if (bidi.Serializing()) {
  } else {
  }
}
} // namespace reflect
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} // namespace ork
