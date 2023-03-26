////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////


#include <ork/reflect/Functor.h>

namespace ork::reflect {

  int LambdaInvokation::GetNumParameters() const  {
    return 0;
  }
  void LambdaInvokation::ApplyParam(serdes::BidirectionalSerializer&, int)  {
  }
  void* LambdaInvokation::ParameterData()  {
    return nullptr;
  }

  IInvokation* LambdaFunctor::CreateInvokation() const {
    return new LambdaInvokation;
  }

  void LambdaFunctor::invoke(Object* obj, const IInvokation*, serdes::BidirectionalSerializer*) const {
    if (mLambda)
      mLambda(obj);
  }

  LambdaFunctor::LambdaFunctor()
      : mLambda(nullptr) {
  }


} // namespace ork { namespace reflect {
