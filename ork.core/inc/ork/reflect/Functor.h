////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/Function.h>
#include <ork/reflect/Serializable.h>
#include <ork/reflect/BidirectionalSerializer.h>
#include <functional>

namespace ork { namespace reflect { 

class IInvokation
{
public:
	virtual int GetNumParameters() const = 0;
	virtual void ApplyParam(BidirectionalSerializer &, int) = 0;
	virtual void *ParameterData() = 0;
	virtual ~IInvokation() {}
};

class IFunctor 
{
public:
	virtual void invoke(const IInvokation *, BidirectionalSerializer * = 0) const = 0;
	virtual IInvokation *CreateInvokation() const = 0;
};

class IObjectFunctor 
{
public:
	virtual void invoke(Object *, const IInvokation *, BidirectionalSerializer * = 0) const = 0;
	virtual IInvokation *CreateInvokation() const = 0;
	virtual bool Pure() const = 0;
};



template<typename ParameterType>
class Invokation : public IInvokation
{
	ParameterType mParameters;
public:
	int GetNumParameters() const
	{
		return ParameterType::Count;
	}

	void ApplyParam(BidirectionalSerializer &bidi, int param)
	{
		mParameters.Apply(bidi, param);
	}

	const ParameterType &GetParams() const { return mParameters; }

	void *ParameterData() { return &mParameters; }
};

template<typename FType>
class Functor : public IFunctor
{
public:
	Functor(FType function)
		: mFunction(function)
	{}

private:
	FType mFunction;

	/*virtual*/ IInvokation *CreateInvokation() const
	{
		return new Invokation<typename Function<FType>::Parameters>();
	}

	/*virtual*/ void invoke(const IInvokation *invokation, BidirectionalSerializer *bidi) const
	{
			Function<FType>::invoke(mFunction,
				static_cast<const Invokation<typename Function<FType>::Parameters> *>
				(invokation)->GetParams());
	}
};


template<typename ReturnType>
static void WriteResult__(BidirectionalSerializer *bidi, const ReturnType& result)
{
	if(bidi) *bidi | result;
}

template<typename FType, typename ResultType = typename Function<FType>::ReturnType>
class ObjectFunctor : public IObjectFunctor
{
public:
	ObjectFunctor(FType function, bool pure = false)
		: mFunction(function)
		, mPure(pure)
	{}

private:
	FType mFunction;
	bool mPure;

	/*virtual*/ IInvokation *CreateInvokation() const
	{
		return new Invokation<typename Function<FType>::Parameters>();
	}

	/*virtual*/ void invoke(Object *obj, const IInvokation *invokation, BidirectionalSerializer *bidi) const
	{
		WriteResult__(bidi, Function<FType>::invoke(
				*static_cast<typename Function<FType>::ClassType *>(obj),
				mFunction,
				static_cast<const Invokation<typename Function<FType>::Parameters> *>
				(invokation)->GetParams()));
	}

	/*virtual*/ bool Pure() const
	{
		return mPure;
	}
};


template<typename FType>
class ObjectFunctor<FType, void> : public IObjectFunctor
{
public:
	ObjectFunctor(FType function, bool)
		: mFunction(function)
	{}

private:
	FType mFunction;

	IInvokation *CreateInvokation() const override
	{
		return new Invokation<typename Function<FType>::Parameters>();
	}

	void invoke(Object *obj, const IInvokation *invokation, BidirectionalSerializer *bidi) const override
	{
		Function<FType>::invoke(
				*static_cast<typename Function<FType>::ClassType *>(obj),
				mFunction,
				static_cast<const Invokation<typename Function<FType>::Parameters> *>
				(invokation)->GetParams());
	}

	bool Pure() const override
	{
		return false;
	}
};

class LambdaInvokation : public IInvokation
{
	int GetNumParameters() const override { return 0; }
	void ApplyParam(BidirectionalSerializer &, int) override {}
	void *ParameterData() override { return nullptr; }
};

struct LambdaFunctor 
{
	typedef std::function<void(Object*)> lambda_t;

	IInvokation *CreateInvokation() const
	{
		return new LambdaInvokation;
	}

	void invoke(Object* obj, const IInvokation *, BidirectionalSerializer * = 0) const
	{
		if( mLambda )
			mLambda(obj);
	}

	LambdaFunctor() : mLambda(nullptr) {}

	lambda_t mLambda;
};

////////////////////////////////////////////////////////////////////////////

template<typename FType>
inline IObjectFunctor *CreateObjectFunctor(FType f, bool pure = false)
{
	return new ObjectFunctor<FType>(f, pure);
}

////////////////////////////////////////////////////////////////////////////

template<typename FType>
inline IFunctor *CreateFunctor(FType f)
{
	return new Functor<FType>(f);
}

////////////////////////////////////////////////////////////////////////////

bool SetInvokationParameter(IInvokation *invokation, int param, const char *paramdata);

} }
