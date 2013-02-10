#ifndef _CFIXED_DS_H
#define _CFIXED_DS_H

#include <cmath>
#include <cstdlib>

namespace ork
{

// basic binary math operators
inline CFixed CFixed::operator +(const CFixed &r) const { return X(mValue + r.mValue); }
inline CFixed CFixed::operator -(const CFixed &r) const { return X(mValue - r.mValue); }
inline CFixed CFixed::operator *(const CFixed &r) const { return X(FX_Mul(mValue, r.mValue)); }
inline CFixed CFixed::operator /(const CFixed &r) const { return X(FX_Div(mValue, r.mValue)); }
inline CFixed CFixed::operator %(const CFixed &r) const { return X(FX_Mod(mValue, r.mValue)); }

// static methods
inline CFixed CFixed::Ceil(const CFixed &a)
{
	return CFixed::FromFX(FX_Floor(a.mValue + One().mValue - 1));
}

inline CFixed CFixed::Floor(const CFixed &a)
{
	return CFixed::FromFX(FX_Floor(a.mValue));
}

inline CFixed CFixed::Pow(const CFixed &a, const CFixed &b)
{
	return CFixed::FromFloat(std::pow(a, b));
}

inline CFixed CFixed::Sqrt(const CFixed &r)
{
	return CFixed::FromFX(FX_Sqrt(r.mValue));
}

inline CFixed CFixed::Cos(const CFixed &x)
{
	return CFixed::FromFX(FX_CosIdx(FX_RAD_TO_IDX(x.FXCast())));
}

inline CFixed CFixed::Abs(const CFixed &x)
{
	return maximum(x, -x);
}

inline CFixed CFixed::ArcCos(const CFixed &x)
{
	return CFixed(std::acos(x));
}

inline CFixed CFixed::Sin(const CFixed &x)
{
	return CFixed::FromFX(FX_SinIdx(FX_RAD_TO_IDX(x.FXCast())));
}

inline CFixed CFixed::ArcTan(const CFixed &x)
{
  return CFixed::FromFX(FX_Atan(x.mValue));
}

inline CFixed CFixed::ArcTan2(const CFixed &x, const CFixed &y)
{
	return CFixed::FromFX(FX_Atan2(x.mValue, y.mValue));
}

inline CFixed CFixed::Tan(const CFixed &x)
{
	return Sin(x) / Cos(x);
}

inline CFixed CFixed::Min(const CFixed &a, const CFixed &b)
{
	return a < b ? a : b;
}

inline CFixed CFixed::Max(const CFixed &a, const CFixed &b)
{
	return a > b ? a : b;
}

inline CFixed CFixed::Rand(CFixed low, CFixed high)
{
	return CFixed::FromFX((unsigned)::std::rand() % (high.FXCast() - low.FXCast()) + low.FXCast());
}

inline bool CFixed::RelCompare(const CFixed& a, const CFixed& b, const CFixed& toler)
{
	return CFixed::Abs(a - b) <= toler * CFixed::Max(CFixed::One(), CFixed::Max(a, b));
}

}

#endif
