////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
//////////////////////////////////////////////////////////////// 


#ifndef _ORK_PIDCONTROLLER_H_
#define _ORK_PIDCONTROLLER_H_

#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork{
///////////////////////////////////////////////////////////////////////////////

/// TODO: Apply Delta time to this implementation
template<typename T> struct PIDController
{
	PIDController(
		T position = T(0.0f),
		T proportional = T(0.075f),
		T integral = T(0.0f),
		T derivative = T(0.05f),
		const Vector2<T> &irange = Vector2<T>(-10.0f, 10.0f),
		const Vector2<T> &maxdelta = Vector2<T>(-0.5f, 0.5f));

	void Configure(
		T proportional = T(0.075f),
		T integral = T(0.00f),
		T derivative = T(0.05f),
		const Vector2<T> &irange = Vector2<T>(-10.0, 10.0),
		const Vector2<T> &maxdelta = Vector2<T>(-0.5, 0.5));

	void InitPosition(T target);

	/// TODO: Apply Delta time to this implementation
	T Update(T target);

	operator T () const { return mPosition; }

	/// Tuning Parameters

	T _proportionalFactor;
	T _integralFactor;
	T _derivativeFactor;
	Vector2<T> _integralRange;
	Vector2<T> _maxDelta;

	/// Runtime Parameters

	T mPosition;
	T mPreviousPosition;
	T _integral;
};

///////////////////////////////////////////////////////////////////////////////

template<typename T> struct PIDController2
{
public:
    PIDController2(
            T proportional = T(0.075f),
            T integral = T(0.0f),
            T derivative = T(0.05f),
            Vector2<T> irange = Vector2<T>(-10.0f,10.0f),
            Vector2<T> maxdelta = Vector2<T>(-0.5f, 0.5f),
			float decay = 0.1f
            );

    T Update( T measured_error, float dt );

    void Configure(
            T proportional = T(0.075f),
            T integral = T(0.00f),
            T derivative = T(0.05f),
            Vector2<T> irange = Vector2<T>(-10,10),
            Vector2<T> maxdelta = Vector2<T>(-0.5, 0.5),
			float decay = 0.1f);
private:
    T _lastError;
    T _integral;

    T _proportionalFactor;
    T _integralFactor;
    T _derivativeFactor;
    Vector2<T> _integralRange;
    Vector2<T> _maxDelta;
    float _integralDecay;
};



template<typename T>
PIDController<T>::PIDController(
		T position,
		T proportional,
		T integral,
		T derivative,
		const Vector2<T> &irange,
		const Vector2<T> &maxdelta)
	: _proportionalFactor(proportional)
	, _integralFactor(integral)
	, _derivativeFactor(derivative)
	, _integralRange(irange)
	, _maxDelta(maxdelta)
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
	_proportionalFactor = proportional;
	_integralFactor = integral;
	_derivativeFactor = derivative;
	_integralRange = irange;
	_maxDelta = maxdelta;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T>
void PIDController<T>::InitPosition(T target)
{
	_integral = 0;
	mPosition = mPreviousPosition = target;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T>
T PIDController<T>::Update(T target)
{
	T error = target - mPosition;

	_integral += error;

	_integral = maximum(_integral, _integralRange.x);
	_integral = minimum(_integral, _integralRange.y);

	T P = _proportionalFactor * error;
	T I = _integralFactor * _integral;
	T D = _derivativeFactor * (mPosition - mPreviousPosition);

	mPreviousPosition = mPosition;

	T delta = P + I + D;

	delta = maximum(delta, _maxDelta.x);
	delta = minimum(delta, _maxDelta.y);

	mPosition += delta;

	return mPosition;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
T PIDController2<T>::Update(T measured_error, float dt )
{
    T P =  measured_error*_proportionalFactor;

    _integral += measured_error*dt;
	_integral *= powf(_integralDecay, dt);


	T D = (measured_error - _lastError)/dt;

    T output = P*_proportionalFactor + _integral*_integralFactor + D*_derivativeFactor;

    _lastError = measured_error;

    return output;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T>
PIDController2<T>::PIDController2(T proportional, T integral, T derivative, Vector2<T> irange, Vector2<T> maxdelta, float decay)
    : _lastError(T(0))
	, _integral(T(0))
    , _proportionalFactor(proportional)
    , _integralFactor(integral)
    , _derivativeFactor(derivative)
    , _integralRange(irange)
    , _maxDelta(maxdelta)
	, _integralDecay(decay)
{
}

///////////////////////////////////////////////////////////////////////////////

template<typename T>
void PIDController2<T>::Configure(
    T proportional,
    T integral,
    T derivative,
    Vector2<T> integral_range,
    Vector2<T> maxdelta, 
	float decay)
{
    _proportionalFactor = proportional;
    _integralFactor = integral;
    _derivativeFactor = derivative;
    _integralRange = integral_range;
    _maxDelta = maxdelta;
	_integralDecay = decay;

   _integral = T(0);
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork
///////////////////////////////////////////////////////////////////////////////

#endif
