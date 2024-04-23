////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/math/cvector2.h>
#include <ork/math/cvector2.hpp>

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
template <> Vector2<float> Vector2<float>::fromScalar(float fin) {
  return Vector2<float>(fin, fin);
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

template <> Vector2<double> Vector2<double>::fromScalar(double fin) {
  return Vector2<double>(fin, fin);
}

///////////////////////////////////////////////////////////////////////////////

template <> const EPropType PropType<fvec2>::meType   = EPROPTYPE_VEC2REAL;
template <> const char* PropType<fvec2>::mstrTypeName = "VEC2REAL";
template <> void PropType<fvec2>::ToString(const fvec2& Value, PropTypeString& tstr) {
  fvec2 v = Value;
  tstr.format("%g %g", float(v.x), float(v.y));
}

template <> fvec2 PropType<fvec2>::FromString(const PropTypeString& String) {
  float x, y;
  sscanf(String.c_str(), "%g %g", &x, &y);
  return fvec2(float(x), float(y));
}

///////////////////////////////////////////////////////////////////////////////

template struct Vector2<float>;  // explicit template instantiation
template struct Vector2<double>; // explicit template instantiation
template struct PropType<fvec2>;

fvec2 dvec2_to_fvec2(const dvec2& in) {
  return fvec2(in.x, in.y);
}

dvec2 fvec2_to_dvec2(const fvec2& in) {
  return dvec2(in.x, in.y);
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
} // namespace ork
