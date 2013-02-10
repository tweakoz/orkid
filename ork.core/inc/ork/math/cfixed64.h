////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _CFIXED64_H
#define _CFIXED64_H

#include <limits>
//#include <iosfwd>
#include <ork/orktypes.h>

#undef max
#undef min

///////////////////////////////////////////////////////////////////////////////

inline FX64 FloatToFixed64(f32 x)
{
	return ((FX64)(((x) > 0) ? 
				((x) * ((FX64)1 << fx64_SHIFT) + 0.5f ) : 
				((x) * ((FX64)1 << fx64_SHIFT) - 0.5f )));
}

///////////////////////////////////////////////////////////////////////////////

inline f32 Fixed64ToFloat(fx64 x)
{
	return ((f32)((x) / (f32)((FX64)1 << fx64_SHIFT)));
}

///////////////////////////////////////////////////////////////////////////////

namespace ork
{

class CFixed64
{
private:

	typedef FX64 value_type;
	
	value_type mValue; //  1 sign bit, 31 whole . 32 fraction

	static CFixed64 X(value_type v)
	{
		CFixed64 x;
		x.mValue = v;
		return x;
	}
	
public:
	static const int kShift = fx64_SHIFT;
	
	// constructors

	CFixed64(const CFixed64 &c) { mValue = c.mValue; }
	CFixed64() { mValue = 0; }
	
	CFixed64(short x)  : mValue(value_type(x) << kShift) {}
	CFixed64(int x)    : mValue(value_type(x) << kShift) {}
	CFixed64(long x)   : mValue(value_type(x) << kShift) {}
	CFixed64(float x)  : mValue(value_type(FloatToFixed64(x))) {}
	CFixed64(double x) : mValue(value_type(FloatToFixed64(float(x)))) {}

	template<typename T> static CFixed64 FromReal(T x) { return X(value_type(x.FX64Cast())); }
	template<typename T> static CFixed64 FromFX(T x) { return X(value_type(x << (kShift - fx32_SHIFT))); }
	template<typename T> static CFixed64 FromFX64(T x) { return X(value_type(x)); }
	template<typename T> static CFixed64 FromNumeric(T x) { return X(value_type(FX64(x) << kShift)); }
	template<typename T> static CFixed64 FromFloat(T x) { return X(FloatToFixed64(x)); }

	// casting
	template<typename T> T GenericFXCast()      const { return T(mValue >> (kShift - fx32_SHIFT)); }
	template<typename T> T GenericFX64Cast()    const { return T(mValue); }
	template<typename T> T GenericNumericCast() const { return T(mValue >> kShift); }
	template<typename T> T GenericFloatCast()   const { return T(Fixed64ToFloat(mValue)); }

	FX32 FXCast()       const { return GenericFXCast<FX32>(); }
	FX64 FX64Cast()       const { return GenericFX64Cast<FX64>(); }
	int NumericCast()   const { return GenericNumericCast<int>(); }
	float FloatCast()   const { return GenericFloatCast<float>(); }
	double DoubleCast() const { return GenericFloatCast<double>(); }

	operator float()  const { return FloatCast(); }
	operator double() const { return DoubleCast(); }

	// unary math operator(s)
	CFixed64 operator-() const { return X(-mValue); }

	// assignment
	const CFixed64 &operator =(const CFixed64 &r) { mValue = r.mValue; return *this; }
	const CFixed64 &operator+=(const CFixed64 &r) { return *this = *this + r; } 
	const CFixed64 &operator-=(const CFixed64 &r) { return *this = *this - r; }
	const CFixed64 &operator*=(const CFixed64 &r) { return *this = *this * r; }
	const CFixed64 &operator/=(const CFixed64 &r) { return *this = *this / r; }
	const CFixed64 &operator%=(const CFixed64 &r) { return *this = *this % r; }
	
	// comparisons
	bool operator==(const CFixed64 &r) const { return mValue == r.mValue; }
	bool operator!=(const CFixed64 &r) const { return mValue != r.mValue; }
	bool operator> (const CFixed64 &r) const { return mValue > r.mValue; }
	bool operator< (const CFixed64 &r) const { return mValue < r.mValue; }
	bool operator>=(const CFixed64 &r) const { return mValue >= r.mValue; }
	bool operator<=(const CFixed64 &r) const { return mValue <= r.mValue; }
		
	// basic binary math operators
	CFixed64 operator+(const CFixed64 &r) const;
	CFixed64 operator-(const CFixed64 &r) const;
	CFixed64 operator*(const CFixed64 &r) const;
	CFixed64 operator/(const CFixed64 &r) const;
	CFixed64 operator%(const CFixed64 &r) const;

	// static constants
	static CFixed64 Zero()    { return CFixed64(0); }
	static CFixed64 One()     { return CFixed64(1); }
	static CFixed64 Pi()      { return CFixed64(3.1415927f); }
	static CFixed64 E()       { return CFixed64(2.7182818f); }
	static CFixed64 Epsilon() { return CFixed64(0.001f); }
	static CFixed64 TypeMax() { return X(::std::numeric_limits<value_type>::max()); }
	static CFixed64 TypeMin() { return X(::std::numeric_limits<value_type>::min()); }

	// static methods
	static CFixed64 Ceil(const CFixed64 &a);
	static CFixed64 Floor(const CFixed64 &a);
	static CFixed64 Pow(const CFixed64 &a, const CFixed64 &b);
	static CFixed64 Sqrt(const CFixed64 &r);
	static CFixed64 Cos(const CFixed64 &x);
	static CFixed64 Abs(const CFixed64 &x);
	static CFixed64 ArcCos(const CFixed64 &x);
	static CFixed64 Sin(const CFixed64 &x);
	static CFixed64 ArcTan(const CFixed64 &x);
	static CFixed64 ArcTan2(const CFixed64 &x, const CFixed64 &y);
	static CFixed64 Tan(const CFixed64 &x);
	static CFixed64 Min(const CFixed64 &a, const CFixed64 &b);
	static CFixed64 Max(const CFixed64 &a, const CFixed64 &b);
	
	static CFixed64 Rand(CFixed64 low = CFixed64(0), CFixed64 high = CFixed64(1));

	static bool RelCompare(const CFixed64& a, const CFixed64& b, const CFixed64& toler = CFixed64::Epsilon());
};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

//std::ostream& operator<<(std::ostream& o, const ork::CFixed64& fixed); //In misc_math.cpp

#include <ork/math/pc/cfixed64_pc.h>

#endif
