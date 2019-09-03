////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#include <ork/pch.h>

#include <ork/math/misc_math.h>
#include <ork/math/cfixed.h>
#include <ork/math/cvector4.h>
#include <iostream>
#include <ork/reflect/Serialize.h>
#include <ork/reflect/BidirectionalSerializer.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

fvec4 PickIdToVertexColor( uint64_t pid ){
	fvec4 out; out.SetRGBAU64(pid);
	printf( "PickIdToVertexColor uint64_t<0x%zx> fvec4<%g %g %g %g>\n", pid, out.x, out.y, out.z, out.w );
	return out;
}

/*template<> void reflect::Serialize( const Float*in, Float*out, reflect::BidirectionalSerializer& bidi )
{
	if( bidi.Serializing() )
	{
		bidi | in->FloatCast();
	}
	else
	{
		float val;
		bidi | val;
		*out = val;
	}
}*/

int* Perlin2D::p = 0;
f32* Perlin2D::g2 = 0;

///////////////////////////////////////////////////////////////////////////////

//Could add this function to the fvec4 class
XYZ RotatePointAboutLine(XYZ p, double theta, XYZ p1, XYZ p2)
{
   XYZ u,q1,q2;
   double d;

   /* Step 1 */
   q1.x = p.x - p1.x;
   q1.y = p.y - p1.y;
   q1.z = p.z - p1.z;

   u.x = p2.x - p1.x;
   u.y = p2.y - p1.y;
   u.z = p2.z - p1.z;
   Normalise(&u);
   d = std::sqrt(u.y*u.y + u.z*u.z);

   /* Step 2 */
   if (std::fabs(d) > 0.000001) {
      q2.x = q1.x;
      q2.y = q1.y * u.z / d - q1.z * u.y / d;
      q2.z = q1.y * u.y / d + q1.z * u.z / d;
   } else {
      q2 = q1;
   }

   /* Step 3 */
   q1.x = q2.x * d - q2.z * u.x;
   q1.y = q2.y;
   q1.z = q2.x * u.x + q2.z * d;

   /* Step 4 */
   q2.x = q1.x * std::cos(theta) - q1.y * std::sin(theta);
   q2.y = q1.x * std::sin(theta) + q1.y * std::cos(theta);
   q2.z = q1.z;

   /* Inverse of step 3 */
   q1.x =   q2.x * d + q2.z * u.x;
   q1.y =   q2.y;
   q1.z = - q2.x * u.x + q2.z * d;

   /* Inverse of step 2 */
   if (std::fabs(d) > 0.000001) {
      q2.x =   q1.x;
      q2.y =   q1.y * u.z / d + q1.z * u.y / d;
      q2.z = - q1.y * u.y / d + q1.z * u.z / d;
   } else {
      q2 = q1;
   }

   /* Inverse of step 1 */
   q1.x = q2.x + p1.x;
   q1.y = q2.y + p1.y;
   q1.z = q2.z + p1.z;
   return(q1);
}

void Normalise( XYZ *p )
{
	float ork = (float) std::sqrt( p->x * p->x + p->y * p->y + p->z * p->z );
	if( ork > 0.000001f )
	{
		p->x /= ork;
		p->y /= ork;
		p->z /= ork;
	}
	else
	{
		OrkAssert(false);
		/*
		if( p->x > p->y )
			if( p->x > p->z )
			{
				p->x = 1.0f; p->y = 0.0f; p->z = 0.0f;
			}
			else
			{
				p->x = 0.0f; p->y = 0.0f; p->z = 1.0f;
			}
		*/
	}
}

///////////////////////////////////////////////////////////////////////////////

Polynomial Polynomial::Differentiate() const
{
	Polynomial result;

    result.SetCoefs(1, float(0));
    result.SetCoefs(2, coefs[0] * float(3));
    result.SetCoefs(3, coefs[1] * float(2));
    result.SetCoefs(4, coefs[2]);

    return result;
}

void Polynomial::SetCoefs(const float *array)
{
	memcpy(coefs, array, 4 * sizeof(float));
}

void Polynomial::SetCoefs(int i, float num)
{
	OrkAssert(i >= 1);
	OrkAssert(i <= 4);
	coefs[i-1] = num;
}

float Polynomial::GetCoefs(int i) const
{
	OrkAssert(i >= 1);
	OrkAssert(i <= 4);
	return coefs[i-1];
}

float Polynomial::Evaluate(float val) const
{
	return (((coefs[0]) * val + coefs[1]) * val + coefs[2]) * val + coefs[3];
}

float Polynomial::operator()(float val) const
{
	return (((coefs[0]) * val + coefs[1]) * val + coefs[2]) * val + coefs[3];
}

Polynomial Polynomial::operator = ( const Polynomial & a )
{
	SetCoefs(1, a.GetCoefs(1));
	SetCoefs(2, a.GetCoefs(2));
	SetCoefs(3, a.GetCoefs(3));
	SetCoefs(4, a.GetCoefs(4));
	return *this;
}

Polynomial Polynomial::operator + ( const Polynomial & a )
{
	Polynomial result;
	result.SetCoefs(1, a.GetCoefs(1) + coefs[0]);
	result.SetCoefs(1, a.GetCoefs(2) + coefs[1]);
	result.SetCoefs(1, a.GetCoefs(3) + coefs[2]);
	result.SetCoefs(1, a.GetCoefs(4) + coefs[3]);
	return result;

}

Polynomial Polynomial::operator - ( const Polynomial & a )
{
	Polynomial result;
	result.SetCoefs(1, a.GetCoefs(1) - GetCoefs(1));
	result.SetCoefs(1, a.GetCoefs(2) - GetCoefs(2));
	result.SetCoefs(1, a.GetCoefs(3) - GetCoefs(3));
	result.SetCoefs(1, a.GetCoefs(4) - GetCoefs(4));
	return result;
}

} // namespace ork
