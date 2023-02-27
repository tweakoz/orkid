////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/gradient.h>
#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/register.h>
#include <ork/reflect/enum_serializer.inl>
#include <ork/reflect/properties/DirectTyped.h>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <math.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

void GradientBase::Describe() {
}

GradientBase::GradientBase() {
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Gradient<T>::Gradient() {
  _data.AddSorted(0.0f, T());
  _data.AddSorted(1.0f, T());
}

template <typename T> bool Gradient<T>::preDeserialize(ork::reflect::serdes::IDeserializer& deser) {
  _data.clear();
  return true;
}

template <typename T> void Gradient<T>::addDataPoint(float flerp, const T& data) {
  _data.AddSorted(flerp, data);
}

template <typename T> T Gradient<T>::sample(float fu) {

  if (fu < 0.0f)
    return T();
  if (fu > 1.0f)
    return T();
  if (isnan(fu))
    return T();

  bool bdone = false;
  int isega  = 0;
  int isegb  = 0;
  int inumv  = int(_data.size());
  while (false == bdone) {
    isegb = (isega + 1);
    OrkAssert(isegb < inumv);
    if ((fu >= _data.GetItemAtIndex(isega).first) && (fu <= _data.GetItemAtIndex(isegb).first)) {
      bdone = true;
    } else {
      isega++;
    }
  }
  const std::pair<float, T>& VA = _data.GetItemAtIndex(isega);
  const std::pair<float, T>& VB = _data.GetItemAtIndex(isegb);
  float dU                      = VB.first - VA.first;
  float Base                    = VA.first;
  float fsu                     = (fu - Base) / dU;
  float fisu                    = 1.0f - fsu;
  T rval                        = (VA.second * fisu) + (VB.second * fsu);
  return rval;
}

template <typename T> void Gradient<T>::Describe() {
  /*ork::reflect::RegisterMapProperty( "points", & Gradient<T>::_data );
  ork::reflect::annotatePropertyForEditor<Gradient>( "points", "editor.visible", "false" );
  ork::reflect::annotateClassForEditor< Gradient >( "editor.class", ConstString("ged.factory.gradient") );
*/
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork

INSTANTIATE_TRANSPARENT_RTTI(ork::GradientBase, "GradientBase");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(ork::gradient_fvec4, "GradientV4");

template class ork::orklut<float, ork::fvec4>;
// template class ork::orklut<float, ork::orklut<float,ork::fvec4> >;
template struct ork::Gradient<ork::fvec4>;
// template class ork::GradLut< ork::GradLut<ork::fvec4> >; //ork::orklut<float,ork::fvec4>;
