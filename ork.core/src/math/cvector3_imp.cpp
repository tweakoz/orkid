////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/math/cvector3.h>
#include <ork/math/cvector3.hpp>
#include <ork/math/misc_math.h>

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

template <> Vector3<float> Vector3<float>::fromScalar(float fin) {
  return Vector3<float>(fin, fin, fin);
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

template <> Vector3<double> Vector3<double>::fromScalar(double fin) {
  return Vector3<double>(fin, fin, fin);
}

// FIXED ///////////////////////////////////////////////////////////////////////

template <> const EPropType PropType<Vector3<float>>::meType   = EPROPTYPE_VEC3FLOAT;
template <> const char* PropType<Vector3<float>>::mstrTypeName = "VEC3FLOAT";
template <> void PropType<Vector3<float>>::ToString(const Vector3<float>& Value, PropTypeString& tstr) {
  Vector3<float> v = Value;
  tstr.format("%g %g %g", float(v.x), float(v.y), float(v.z));
}

template <> Vector3<float> PropType<Vector3<float>>::FromString(const PropTypeString& String) {
  float x, y, z;
  sscanf(String.c_str(), "%g %g %g", &x, &y, &z);
  return Vector3<float>(float(x), float(y), float(z));
}

///////////////////////////////////////////////////////////////////////////////

template struct Vector3<float>; // explicit template instantiation
template struct PropType<Vector3<float>>;

template struct Vector3<double>; // explicit template instantiation

dvec3 fvec3_to_dvec3(const fvec3& v) {
  return dvec3(v.x, v.y, v.z);
}

fvec3 dvec3_to_fvec3(const dvec3& v) {
  return fvec3(v.x, v.y, v.z);
}

} // namespace ork
