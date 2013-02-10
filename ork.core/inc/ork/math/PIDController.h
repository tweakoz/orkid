////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
		const TVector2<T> &irange = TVector2<T>(-10.0f, 10.0f),
		const TVector2<T> &maxdelta = TVector2<T>(-0.5f, 0.5f));

	void Configure(
		T proportional = T(0.075f),
		T integral = T(0.00f),
		T derivative = T(0.05f),
		const TVector2<T> &irange = TVector2<T>(-10.0, 10.0),
		const TVector2<T> &maxdelta = TVector2<T>(-0.5, 0.5));

	void InitPosition(T target);

	/// TODO: Apply Delta time to this implementation
	T Update(T target);

	operator T () const { return mPosition; }

	/// Tuning Parameters

	T mProportionalFactor;
	T mIntegralFactor;
	T mDerivativeFactor;
	TVector2<T> mIntegralRange;
	TVector2<T> mMaxDelta;

	/// Runtime Parameters

	T mPosition;
	T mPreviousPosition;
	T mIntegral;
};

///////////////////////////////////////////////////////////////////////////////

template<typename T> class PIDController2
{
public:
    PIDController2(
            T proportional = T(0.075f),
            T integral = T(0.0f),
            T derivative = T(0.05f),
            TVector2<T> irange = TVector2<T>(-10.0f,10.0f),
            TVector2<T> maxdelta = TVector2<T>(-0.5f, 0.5f),
			float decay = 0.1f
            );

    T Update( T MeasuredError, float dt );

    void Configure(
            T proportional = T(0.075f),
            T integral = T(0.00f),
            T derivative = T(0.05f),
            TVector2<T> irange = TVector2<T>(-10,10),
            TVector2<T> maxdelta = TVector2<T>(-0.5, 0.5),
			float decay = 0.1f);
private:
    T mLastError;
    T mIntegral;

    // these shouldn't change, but not marked const because we might serialize this class.
    /*const*/ T mProportionalFactor;
    /*const*/ T mIntegralFactor;
    /*const*/ T mDerivativeFactor;
    /*const*/ TVector2<T> mIntegralRange;
    /*const*/ TVector2<T> mMaxDelta;
    /*const*/ float mIntegralDecay;
};

///////////////////////////////////////////////////////////////////////////////
} //namespace ork
///////////////////////////////////////////////////////////////////////////////

#endif
