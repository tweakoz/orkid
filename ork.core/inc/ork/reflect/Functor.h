////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/Function.h>
#include <ork/reflect/Serializable.h>
#include <ork/reflect/BidirectionalSerializer.h>
#include <functional>

namespace ork::reflect {

class IInvokation {
public:
  virtual int GetNumParameters() const                           = 0;
  virtual void ApplyParam(serdes::BidirectionalSerializer&, int) = 0;
  virtual void* ParameterData()                                  = 0;
  virtual ~IInvokation() {
  }
};

class IFunctor {
public:
  virtual void invoke(const IInvokation*, serdes::BidirectionalSerializer* = 0) const = 0;
  virtual IInvokation* CreateInvokation() const                                       = 0;
};

class IObjectFunctor {
public:
  virtual void invoke(Object*, const IInvokation*, serdes::BidirectionalSerializer* = 0) const = 0;
  virtual IInvokation* CreateInvokation() const                                                = 0;
  virtual bool Pure() const                                                                    = 0;
};

template <typename ParameterType> class Invokation : public IInvokation {
  ParameterType mParameters;

public:
  int GetNumParameters() const;
  void ApplyParam(serdes::BidirectionalSerializer& bidi, int param);
  const ParameterType& GetParams() const;
  void* ParameterData();
};

template <typename FType> class Functor : public IFunctor {
public:
  Functor(FType function);

private:
  FType mFunction;

  IInvokation* CreateInvokation() const override;
  void invoke(const IInvokation* invokation, serdes::BidirectionalSerializer* bidi) const override;
};

template <typename ReturnType> //
static void WriteResult__(serdes::BidirectionalSerializer* bidi, const ReturnType& result);

template <typename FType, typename ResultType = typename Function<FType>::ReturnType> //
class ObjectFunctor : public IObjectFunctor {
public:
  ObjectFunctor(FType function, bool pure = false);

private:
  FType mFunction;
  bool mPure;

  IInvokation* CreateInvokation() const override;

  void invoke(Object* obj, const IInvokation* invokation, serdes::BidirectionalSerializer* bidi) const override;
  bool Pure() const override;
};

template <typename FType> class ObjectFunctor<FType, void> : public IObjectFunctor {
public:
  ObjectFunctor(FType function, bool);

private:
  FType mFunction;
  IInvokation* CreateInvokation() const override;
  void invoke(Object* obj, const IInvokation* invokation, serdes::BidirectionalSerializer* bidi) const override;
  bool Pure() const override;
};

class LambdaInvokation : public IInvokation {
  int GetNumParameters() const override;
  void ApplyParam(serdes::BidirectionalSerializer&, int) override;
  void* ParameterData() override;
};

struct LambdaFunctor {
  typedef std::function<void(Object*)> lambda_t;

  IInvokation* CreateInvokation() const;
  void invoke(Object* obj, const IInvokation*, serdes::BidirectionalSerializer* = 0) const;
  LambdaFunctor();

  lambda_t mLambda;
};

////////////////////////////////////////////////////////////////////////////

template <typename FType> //
IObjectFunctor* CreateObjectFunctor(FType f, bool pure = false);

////////////////////////////////////////////////////////////////////////////

template <typename FType> //
IFunctor* CreateFunctor(FType f);

////////////////////////////////////////////////////////////////////////////

bool SetInvokationParameter(IInvokation* invokation, int param, const char* paramdata);

} // namespace ork::reflect
