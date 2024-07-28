////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/gradient.h>
#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/enum_serializer.inl>
#include <ork/reflect/properties/DirectTyped.h>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <math.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

template class orklut<std::string,gradient_fvec4_ptr_t>;

void GradientBase::describeX(class_t* clazz) {
}

GradientBase::GradientBase() {
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Gradient<T>::Gradient() {
  _data.AddSorted(0.0f, T(0,0,0,1));
  _data.AddSorted(1.0f, T(1,1,1,1));
}

template <typename T> bool Gradient<T>::preDeserialize(ork::reflect::serdes::IDeserializer& deser) {
  _data.clear();
  return true;
}

template <typename T> void Gradient<T>::addDataPoint(float flerp, const T& data) {
  _data.AddSorted(flerp, data);
}

template <typename T> T Gradient<T>::sample(float fu) const {

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
    if(isegb >= inumv){
      isegb = inumv-1;
      printf("isegb<%d> inumv<%d>\n", isegb, inumv);
    }
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

template <typename T> T Gradient<T>::average() const{
  T accum;
  for( int i=0; i<64; i++ ){
    float fi = float(i)/64.0f;
    accum += sample(fi);
  }
  accum *= (1.0f/64.0f);
  return accum;
}

template <typename T> void Gradient<T>::describeX(class_t* clazz) {
  clazz->directMapProperty("points", &Gradient<T>::_data) //
       ->template annotate<bool>("editor.visible", false);
  clazz->template annotateTyped<ConstString>("editor.ged.node.factory", "GedNodeFactoryGradient");
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork

ImplementReflectionX(ork::GradientBase, "GradientBase");
ImplementTemplateReflectionX(ork::gradient_fvec4_t, "GradientV4");

template class ork::orklut<float, ork::fvec4>;
template struct ork::Gradient<ork::fvec4>;
