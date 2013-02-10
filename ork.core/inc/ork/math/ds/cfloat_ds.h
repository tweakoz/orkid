#ifndef _CFLOAT_DS_H
#define _CFLOAT_DS_H

#include <cmath>
#include <cstdlib>

namespace ork
{

inline CFloat CFloat::operator +(const CFloat &r) const { return CFloat(mValue + r.mValue); }
inline CFloat CFloat::operator -(const CFloat &r) const { return CFloat(mValue - r.mValue); }
inline CFloat CFloat::operator *(const CFloat &r) const { return CFloat(mValue * r.mValue); }
inline CFloat CFloat::operator /(const CFloat &r) const { return CFloat(mValue / r.mValue); }
inline CFloat CFloat::operator %(const CFloat &r) const { return CFloat(::std::fmod(mValue, r.mValue)); }

// static methods
inline CFloat CFloat::Ceil(const CFloat &a)
{
	return CFloat(::std::ceil(a.mValue));
}

inline CFloat CFloat::Floor(const CFloat &a)
{
	return CFloat(::std::floor(a.mValue));
}

inline CFloat CFloat::Pow(const CFloat &a, const CFloat &b)
{
	return CFloat(::std::pow(a.mValue, b.mValue));
}

inline CFloat CFloat::Sqrt(const CFloat &r)
{
	return CFloat(::std::sqrt(r.mValue));
}

inline CFloat CFloat::Cos(const CFloat &x)
{
	return CFloat(::std::cos(x.mValue));
}

inline CFloat CFloat::Abs(const CFloat &x)
{
	return maximum(x, -x);
}

inline CFloat CFloat::ArcCos(const CFloat &x)
{
	return CFloat(::std::acos(x.mValue));
}

inline CFloat CFloat::Sin(const CFloat &x)
{
	return CFloat(::std::sin(x.mValue));
}

inline CFloat CFloat::ArcTan(const CFloat &x)
{
	return CFloat(::std::atan(x.mValue));
}

inline CFloat CFloat::ArcTan2(const CFloat &x, const CFloat &y)
{
	return CFloat(::std::atan2(x.mValue, y.mValue));
}

inline CFloat CFloat::Tan(const CFloat &x)
{
	return Sin(x) / Cos(x);
}

inline CFloat CFloat::Min(const CFloat &a, const CFloat &b)
{
	return a < b ? a : b;
}

inline CFloat CFloat::Max(const CFloat &a, const CFloat &b)
{
	return a > b ? a : b;
}

inline CFloat CFloat::Rand(CFloat low, CFloat high)
{
	return CFloat::FromFX((unsigned)::std::rand() % (high.FXCast() - low.FXCast()) + low.FXCast());
}

inline bool CFloat::RelCompare(const CFloat& a, const CFloat& b, const CFloat& toler)
{
	return CFloat::Abs(a - b) <= toler * CFloat::Max(CFloat::One(), CFloat::Max(a, b));
}

}

#endif
