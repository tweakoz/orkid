#ifndef _CFLOAT_DS_H
#define _CFLOAT_DS_H

#include <cmath>
#include <cstdlib>

namespace ork
{

inline Float Float::operator +(const Float &r) const { return Float(mValue + r.mValue); }
inline Float Float::operator -(const Float &r) const { return Float(mValue - r.mValue); }
inline Float Float::operator *(const Float &r) const { return Float(mValue * r.mValue); }
inline Float Float::operator /(const Float &r) const { return Float(mValue / r.mValue); }
inline Float Float::operator %(const Float &r) const { return Float(::std::fmod(mValue, r.mValue)); }

// static methods
inline Float Float::Ceil(const Float &a)
{
	return Float(::std::ceil(a.mValue));
}

inline Float Float::Floor(const Float &a)
{
	return Float(::std::floor(a.mValue));
}

inline Float Float::Pow(const Float &a, const Float &b)
{
	return Float(::std::pow(a.mValue, b.mValue));
}

inline Float Float::Sqrt(const Float &r)
{
	return Float(::std::sqrt(r.mValue));
}

inline Float Float::Cos(const Float &x)
{
	return Float(::std::cos(x.mValue));
}

inline Float Float::Abs(const Float &x)
{
	return maximum(x, -x);
}

inline Float Float::ArcCos(const Float &x)
{
	return Float(::std::acos(x.mValue));
}

inline Float Float::Sin(const Float &x)
{
	return Float(::std::sin(x.mValue));
}

inline Float Float::ArcTan(const Float &x)
{
	return Float(::std::atan(x.mValue));
}

inline Float Float::ArcTan2(const Float &x, const Float &y)
{
	return Float(::std::atan2(x.mValue, y.mValue));
}

inline Float Float::Tan(const Float &x)
{
	return Sin(x) / Cos(x);
}

inline Float Float::Min(const Float &a, const Float &b)
{
	return a < b ? a : b;
}

inline Float Float::Max(const Float &a, const Float &b)
{
	return a > b ? a : b;
}

inline Float Float::Rand(Float low, Float high)
{
	return Float::FromFX((unsigned)::std::rand() % (high.FXCast() - low.FXCast()) + low.FXCast());
}

inline bool Float::RelCompare(const Float& a, const Float& b, const Float& toler)
{
	return Float::Abs(a - b) <= toler * Float::Max(Float::One(), Float::Max(a, b));
}

}

#endif
