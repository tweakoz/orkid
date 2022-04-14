////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include<ork/pch.h>

#include<ork/math/PIDController.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork
{

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
PIDController<T>::PIDController(
		T position,
		T proportional,
		T integral,
		T derivative,
		const Vector2<T> &irange,
		const Vector2<T> &maxdelta)
	: mProportionalFactor(proportional)
	, mIntegralFactor(integral)
	, mDerivativeFactor(derivative)
	, mIntegralRange(irange)
	, mMaxDelta(maxdelta)
{
	InitPosition(position);
}

///////////////////////////////////////////////////////////////////////////////

template<typename T>
void PIDController<T>::Configure(
	T proportional,
	T integral,
	T derivative,
	const Vector2<T> &irange,
	const Vector2<T> &maxdelta)
{
	mProportionalFactor = proportional;
	mIntegralFactor = integral;
	mDerivativeFactor = derivative;
	mIntegralRange = irange;
	mMaxDelta = maxdelta;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T>
void PIDController<T>::InitPosition(T target)
{
	mIntegral = 0;
	mPosition = mPreviousPosition = target;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T>
T PIDController<T>::Update(T target)
{
	T error = target - mPosition;

	mIntegral += error;

	mIntegral = maximum(mIntegral, mIntegralRange.x);
	mIntegral = minimum(mIntegral, mIntegralRange.y);

	T P = mProportionalFactor * error;
	T I = mIntegralFactor * mIntegral;
	T D = mDerivativeFactor * (mPosition - mPreviousPosition);

	mPreviousPosition = mPosition;

	T delta = P + I + D;

	delta = maximum(delta, mMaxDelta.x);
	delta = minimum(delta, mMaxDelta.y);

	mPosition += delta;

	return mPosition;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
T PIDController2<T>::Update(T MeasuredError, float dt )
{
    mIntegral += MeasuredError*dt;
	mIntegral *= powf(mIntegralDecay, dt);

	//mIntegral = maximum(mIntegral, mMaxDelta.x);
	//mIntegral = minimum(mIntegral, mMaxDelta.y);

    T P =  MeasuredError;

	T D = (MeasuredError - mLastError)/dt;

    T output = P*mProportionalFactor + mIntegral*mIntegralFactor + D*mDerivativeFactor;

    //output = maximum(output, mMaxDelta.x);
    //output = minimum(output, mMaxDelta.y);

    mLastError = MeasuredError;

    return output;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T>
PIDController2<T>::PIDController2(T proportional, T integral, T derivative, Vector2<T> irange, Vector2<T> maxdelta, float decay)
    : mLastError(0.0f)
	, mIntegral(0.0f)
    , mProportionalFactor(proportional)
    , mIntegralFactor(integral)
    , mDerivativeFactor(derivative)
    , mIntegralRange(irange)
    , mMaxDelta(maxdelta)
	, mIntegralDecay(decay)
{
}

///////////////////////////////////////////////////////////////////////////////

template<typename T>
void PIDController2<T>::Configure(
    T proportional,
    T integral,
    T derivative,
    Vector2<T> irange,
    Vector2<T> maxdelta, 
	float decay)
{
    mProportionalFactor = proportional;
    mIntegralFactor = integral;
    mDerivativeFactor = derivative;
    mIntegralRange = irange;
    mMaxDelta = maxdelta;
	mIntegralDecay = decay;

   // mIntegral = 0.0f;
}

} //namespace ork

template struct ork::PIDController<float>;
template class ork::PIDController2<float>;
