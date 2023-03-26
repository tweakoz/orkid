////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "Functor.h"
#include "Function.inl"

namespace ork::reflect {

template <typename ParameterType> 
	int Invokation<ParameterType>::GetNumParameters() const {
    return ParameterType::Count;
  }

template <typename ParameterType> 
  void Invokation<ParameterType>::ApplyParam(serdes::BidirectionalSerializer& bidi, int param) {
    mParameters.Apply(bidi, param);
  }

template <typename ParameterType> 
  const ParameterType& Invokation<ParameterType>::GetParams() const {
    return mParameters;
  }

template <typename ParameterType> 
  void* Invokation<ParameterType>::ParameterData() {
    return &mParameters;
  }

template <typename FType> 
  Functor<FType>::Functor(FType function)
      : mFunction(function) {
  }


template <typename FType> 
   IInvokation* Functor<FType>::CreateInvokation() const {
    return new Invokation<typename Function<FType>::Parameters>();
  }

template <typename FType> 
  void Functor<FType>::invoke(const IInvokation* invokation, //
                              serdes::BidirectionalSerializer* bidi) const { //
    using params_t = typename Function<FType>::Parameters;
    auto params = static_cast<const Invokation<params_t>*>(invokation)->GetParams();
    Function<FType>::invoke(mFunction,params);
  }

template <typename ReturnType> 
	void WriteResult__(serdes::BidirectionalSerializer* bidi, const ReturnType& result) {
  if (bidi)
    *bidi | result;
}

template <typename FType, typename ResultType> 
  ObjectFunctor<FType,ResultType>::ObjectFunctor(FType function, bool pure)
      : mFunction(function)
      , mPure(pure) {
  }

template <typename FType, typename ResultType> 
  IInvokation* ObjectFunctor<FType,ResultType>::CreateInvokation() const {
    return new Invokation<typename Function<FType>::Parameters>();
  }

template <typename FType, typename ResultType> 
  void ObjectFunctor<FType,ResultType>::invoke(object_ptr_t obj, const IInvokation* invokation, serdes::BidirectionalSerializer* bidi) const {
    using classtype_t = typename Function<FType>::ClassType;
    using params_t = typename Function<FType>::Parameters;
    WriteResult__(
        bidi,
        Function<FType>::invoke(
            std::dynamic_pointer_cast<classtype_t>(obj),
            mFunction,
            static_cast<const Invokation<params_t>*>(invokation)->GetParams()));
  }

template <typename FType, typename ResultType> 
  bool ObjectFunctor<FType,ResultType>::Pure() const {
    return mPure;
  }

template <typename FType> 
  ObjectFunctor<FType, void>::ObjectFunctor(FType function, bool)
      : mFunction(function) {
  }

template <typename FType> 
  IInvokation* ObjectFunctor<FType, void>::CreateInvokation() const  {
    return new Invokation<typename Function<FType>::Parameters>();
  }

template <typename FType> 
  void ObjectFunctor<FType, void>::invoke(object_ptr_t obj, //
                                          const IInvokation* invokation, //
                                          serdes::BidirectionalSerializer* bidi) const  { //
    Function<FType>::invoke(
        //*static_cast<typename Function<FType>::ClassType*>(obj),
        std::dynamic_pointer_cast<typename Function<FType>::ClassType>(obj),
        mFunction,
        static_cast<const Invokation<typename Function<FType>::Parameters>*>(invokation)->GetParams());
  }

template <typename FType> 
  bool ObjectFunctor<FType, void>::Pure() const  {
    return false;
  }

////////////////////////////////////////////////////////////////////////////

template <typename FType> IObjectFunctor* CreateObjectFunctor(FType f, bool pure) {
  return new ObjectFunctor<FType>(f, pure);
}

////////////////////////////////////////////////////////////////////////////

template <typename FType> IFunctor* CreateFunctor(FType f) {
  return new Functor<FType>(f);
}

////////////////////////////////////////////////////////////////////////////

bool SetInvokationParameter(IInvokation* invokation, int param, const char* paramdata);

} // namespace ork::reflect
