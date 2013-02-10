////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _TNUMBER_H_
#define _TNUMBER_H_

///////////////////////////////////////////////////////////////////////////////

#include <cstdlib>
#include <cmath>
#include <ork/math/misc_math.h>

///////////////////////////////////////////////////////////////////////////////

// As psp-gcc does _not_ qualify these with std:: we must make CW 'using' it
#ifdef NITRO
using std::cosf;
using std::sinf;
#endif

///////////////////////////////////////////////////////////////////////////////
namespace ork{
///////////////////////////////////////////////////////////////////////////////

template <typename T> class TNumber
{
protected:

	T value;

public:

	// constructors
	TNumber()
		{ value = T(0); }
	explicit TNumber(const T &v)
		{ value = v; }

	// cast operators
	inline operator T() const
		{ return value; }

	// unary math operator(s)
	inline TNumber operator-() const
		{ return TNumber(-value); }

	// basic binary math operators
	inline const TNumber operator+(const TNumber &r) const
		{ return TNumber(value + r.value); }
	inline const TNumber operator-(const TNumber &r) const
		{ return TNumber(value - r.value); }
	inline const TNumber operator*(const TNumber &r) const
		{ return TNumber(value * r.value); }
	inline const TNumber operator/(const TNumber &r) const
		{ return TNumber(value / r.value); }
	inline const TNumber operator%(const TNumber &r) const
		{ return TNumber(T((S32(value) % S32(r.value)))); }

	// assignment
	inline const TNumber &operator=(const TNumber &r)
		{ value = r.value; return *this; }
	inline const TNumber &operator+=(const TNumber &r)
		{ return *this = *this + r; }
	inline const TNumber &operator-=(const TNumber &r)
		{ return *this = *this - r; }
	inline const TNumber &operator*=(const TNumber &r)
		{ return *this = *this * r; }
	inline const TNumber &operator/=(const TNumber &r)
		{ return *this = *this / r; }
	inline const TNumber &operator%=(const TNumber &r)
		{ return *this = *this % r; }

	// comparisons
	inline const bool operator==(const TNumber &r) const
		{ return value == r.value; }
	inline const bool operator!=(const TNumber &r) const
		{ return value != r.value; }
	inline const bool operator>(const TNumber &r) const
		{ return value > r.value; }
	inline const bool operator<(const TNumber &r) const
		{ return value < r.value; }
	inline const bool operator>=(const TNumber &r) const
		{ return value >= r.value; }
	inline const bool operator<=(const TNumber &r) const
		{ return value <= r.value; }

	// operations
	inline const TNumber Sqrt()
		{ return TNumber::Sqrt(TNumber(value)); }
	inline const TNumber Cos()
		{ return TNumber::Cos(TNumber(value)); }
	inline const TNumber Sin()
		{ return TNumber::Sin(CInt(value)); }
	inline const TNumber Atan()
		{ return TNumber::Atan(TNumber(value)); }
	inline const TNumber Atan2(const TNumber &y)
		{ return TNumber::Atan2(TNumber(value), TNumber(y.value)); }
	inline const TNumber Clamp(const TNumber &lo, const TNumber &hi)
		{ return TNumber::Clamp(TNumber(value), lo, hi); }

	// static constants
	static inline const TNumber Zero(void)
		{ return TNumber(T(0.0f)); }
	static inline const TNumber One(void)
		{ return TNumber(T(1.0f)); }
	static inline const TNumber Pi(void)
		{ return TNumber(3.1415926f); }
	static inline const TNumber E(void)
		{ return TNumber(2.7182818f); }
	static inline const TNumber Epsilon(void)
		{ return TNumber(T(0.001f)); }

	// static methods
	static inline const TNumber Ceil(const TNumber &a)
		{ return TNumber(ork::ceil(a.value)); }
	static inline const TNumber Floor(const TNumber &a)
		{ return TNumber(ork::floor(a.value)); }
	static inline const TNumber Pow(const TNumber &a,const TNumber &b)
		{ return TNumber((F32)ork::pow(a.value, b.value)); }
	static inline const TNumber Sqrt(const TNumber &r)
		{ return TNumber(T(ork::sqrtf(F32(r.value)))); }
	static inline const TNumber Cos(const TNumber &x)
		{ return TNumber(T(ork::cosf(F32(T(x))))); }
	static inline const TNumber Acos(const TNumber &x)
		{ return TNumber(ork::acosf(F32(x))); }
	static inline const TNumber Abs(const TNumber &x)
		{ return TNumber(ork::fabs(F32(x))); }
	static inline const TNumber Sin(const TNumber &x)
		{ return TNumber(T(ork::sinf(F32(T(x))))); }
	static inline const TNumber Atan(const TNumber &x)
		{ return TNumber(ork::atanf(F32(x))); }
	static inline const TNumber Atan2(const TNumber &x, const TNumber &y)
		{ return TNumber(ork::atan2f(F32(x), F32(y))); }
	static inline const TNumber Tan(const TNumber &x)
		{ return TNumber(ork::tanf(x.value)); }
	static inline const TNumber Min(const TNumber &a, const TNumber &b)
		{ return a < b ? a : b; }
	static inline const TNumber Max(const TNumber &a, const TNumber &b)
		{ return a > b ? a : b; }
	static inline const TNumber Clamp(const TNumber &a, const TNumber &lo, const TNumber &hi)
		{ return hi >= lo ? (a < lo ? lo : (a > hi ? hi : a)) : a; }
	static inline const TNumber Rand(const TNumber &min, const TNumber &max)
	{
		if (max < min) return Rand(max, min);
		return TNumber(float(std::rand()) / RAND_MAX * float(max - min) + float(min));
	}

	static inline const TNumber DoubleEpsilon()
	{
		return TNumber(2.2204460492503131e-016);
	}
};

///////////////////////////////////////////////////////////////////////////////

typedef TNumber<unsigned int> Cu32;

// Specialization for unsigned values. A unary minus operator on an unsigned value
// is technically undefined, but in order to let TNumber use the unary minus operator
// for unsigned types we'll just make it return the same value.
template <>
inline Cu32 Cu32::operator-() const
{
	return Cu32(value);
}

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

#endif
