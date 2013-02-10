#ifndef _CFIXED64_DS_H
#define _CFIXED64_DS_H

#include <cmath>
#include <cstdlib>

namespace ork
{

// basic binary math operators
inline CFixed64 CFixed64::operator+(const CFixed64 &r) const { return X(mValue + r.mValue); }
inline CFixed64 CFixed64::operator-(const CFixed64 &r) const { return X(mValue - r.mValue); }
inline CFixed64 CFixed64::operator*(const CFixed64 &r) const { return X((mValue * r.mValue) >> fx64_SHIFT); }
inline CFixed64 CFixed64::operator/(const CFixed64 &r) const { return X((mValue << fx64_SHIFT) / r.mValue); }
inline CFixed64 CFixed64::operator%(const CFixed64 &r) const { return X(mValue % r.mValue); }

// static methods
inline CFixed64 CFixed64::Ceil(const CFixed64 &a)
{
	return CFixed64::FromFX(FX_Floor(a.FXCast() + One().FXCast() - 1));
}

inline CFixed64 CFixed64::Floor(const CFixed64 &a)
{
	return CFixed64::FromFX(FX_Floor(a.FXCast()));
}

inline CFixed64 CFixed64::Pow(const CFixed64 &a, const CFixed64 &b)
{
	return CFixed64::FromFloat(std::pow(a, b));
}

inline CFixed64 CFixed64::Sqrt(const CFixed64 &r)
{
	return CFixed64::FromFX(FX_Sqrt(r.FXCast()));
}

inline CFixed64 CFixed64::Cos(const CFixed64 &x)
{
	return CFixed64::FromFX(FX_CosIdx(FX_RAD_TO_IDX(x.FXCast())));
}

inline CFixed64 CFixed64::Abs(const CFixed64 &x)
{
	return maximum(x, -x);
}

inline CFixed64 CFixed64::ArcCos(const CFixed64 &x)
{
	return CFixed64(std::acos(x));
}

inline CFixed64 CFixed64::Sin(const CFixed64 &x)
{
	return CFixed64::FromFX(FX_SinIdx(FX_RAD_TO_IDX(x.FXCast())));
}

inline CFixed64 CFixed64::ArcTan(const CFixed64 &x)
{
  return CFixed64::FromFX(FX_Atan(x.FXCast()));
}

inline CFixed64 CFixed64::ArcTan2(const CFixed64 &x, const CFixed64 &y)
{
	return CFixed64::FromFX(FX_Atan2(x.FXCast(), y.FXCast()));
}

inline CFixed64 CFixed64::Tan(const CFixed64 &x)
{
	return Sin(x) / Cos(x);
}

inline CFixed64 CFixed64::Min(const CFixed64 &a, const CFixed64 &b)
{
	return a < b ? a : b;
}

inline CFixed64 CFixed64::Max(const CFixed64 &a, const CFixed64 &b)
{
	return a > b ? a : b;
}

inline CFixed64 CFixed64::Rand(CFixed64 low, CFixed64 high)
{
	return CFixed64::FromFX((unsigned)::std::rand() % (high.FXCast() - low.FXCast()) + low.FXCast());
}

inline bool CFixed64::RelCompare(const CFixed64& a, const CFixed64& b, const CFixed64& toler)
{
	return CFixed64::Abs(a - b) <= toler * CFixed64::Max(CFixed64::One(), CFixed64::Max(a, b));
}

}

#endif
