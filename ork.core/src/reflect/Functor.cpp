////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
