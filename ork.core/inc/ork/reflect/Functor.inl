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
  void Functor<FType>::invoke(const IInvokation* invokation, serdes::BidirectionalSerializer* bidi) const {
    Function<FType>::invoke(
        mFunction, static_cast<const Invokation<typename Function<FType>::Parameters>*>(invokation)->GetParams());
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
  void ObjectFunctor<FType,ResultType>::invoke(Object* obj, const IInvokation* invokation, serdes::BidirectionalSerializer* bidi) const {
    WriteResult__(
        bidi,
        Function<FType>::invoke(
            *static_cast<typename Function<FType>::ClassType*>(obj),
            mFunction,
            static_cast<const Invokation<typename Function<FType>::Parameters>*>(invokation)->GetParams()));
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
  void ObjectFunctor<FType, void>::invoke(Object* obj, const IInvokation* invokation, serdes::BidirectionalSerializer* bidi) const  {
    Function<FType>::invoke(
        *static_cast<typename Function<FType>::ClassType*>(obj),
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
