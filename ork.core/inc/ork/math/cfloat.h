////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _CFLOAT_H
#define _CFLOAT_H

///////////////////////////////////////////////////////////////////////////////

#include <ork/orktypes.h>
#include <ork/math/cfixed.h>
#include <ork/math/cfixed64.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork
{

/// CFixed and CFloat are both wrappers around a non-integral numeric type.
///
/// A CFloat contains a float, and provides the same operators as a normal float,
/// as well as providing a few (hopefully optimized) trigonometric functions.
///
/// The interface of CFixed and CFloat classes should be kept exactly the same, most (if not all) functions inlined,
/// no virtuals, and the sole member variable should be a signed numeric in CFixed, and a float or double in CReal.
///
/// Commonly, the typedef CReal is seen in code, which is the selected as the preferred fractional type on a platform.
///
/// Use FXCast() to convert the internal value to a fixed-point number.
/// Other casts act like normal float<->int casts.
///
/// @see CFixed
class CFloat
{

public:
	
	// static constants
	static float Zero()    { return float(0); }
	static float One()     { return float(1); }
	static float Pi()      { return float(3.1415927f); }
	static float E()       { return float(2.7182818f); }
	static float Epsilon() { return float(0.00001f); }
	static float FloatEpsilon() { return float(1.192092896e-07F); }
	static double DoubleEpsilon() { return double(2.2204460492503131e-016); }
	static float TypeMax() { return float(::std::numeric_limits<float>::max()); }
	static float TypeMin() { return float(::std::numeric_limits<float>::min()); }

	// static methods
	static float Ceil(const float &a);
	static float Floor(const float &a);
	static float Pow(const float &a, const float &b);
	static float Sqrt(const float &r);
	static float Cos(const float &x);
	static float Abs(const float &x);
	static float ArcCos(const float &x);
	static float Sin(const float &x);
	static float ArcTan(const float &x);
	static float ArcTan2(const float &x, const float &y);
	static float Tan(const float &x);
	static float Min(const float &a, const float &b);
	static float Max(const float &a, const float &b);
	static float MAdd( float m1, float m2, float a1 );

	static float Rand(float low = float(0), float high = float(1));

	static bool RelCompare(const float& a, const float& b, const float& toler = CFloat::Epsilon());
};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

#include <ork/math/pc/cfloat_pc.h>

#endif
