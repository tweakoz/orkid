////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/math/polar.h>
#include <ork/math/misc_math.h>
#include <cmath>


// As psp-gcc does _not_ qualify sqrtf with std:: we must make CW 'using' it
#ifdef NITRO
using std::fabs;
using std::sinf;
using std::cosf;
using std::sqrtf;
using std::atanf;
#endif

///////////////////////////////////////////////////////////////////////////////

F32 pol2rect_x( F32 ang, F32 rad )
{
	F32 x = rad * ork::cosf( ang );
	return x;
}

///////////////////////////////////////////////////////////////////////////////

F32 pol2rect_y( F32 ang, F32 rad )
{
	F32 y = rad * ork::sinf( ang );
	return y;
}

///////////////////////////////////////////////////////////////////////////////

F32 rect2pol_ang( F32 x, F32 y )
{
	F32 ang = 0.0f;

	// AXIS
	if( x == 0.0f )
	{	if( y == 0.0f )
			ang = 0.0f;
		else if( y > 0.0f )
			ang = PI_DIV_2;
		else if( y < 0.0f )
			ang = (3.0f*PI_DIV_2);
	}
	else if( y == 0.0f )
	{	if( x < 0.0f )
			ang = PI;
		else if( x > 0.0f )
			ang = 0.0f;
	}
	
	// Q0 ( bottom right )
	else if( (x>0.0f) && (y>0.0f) )
	{	ang = ork::atanf( y/x );
	}
	// Q1 ( bottom left )
	else if( (x<0.0f) && (y>0.0f) )
	{	ang = PI+ork::atanf( y/x );
	}
	// Q2 ( top left )
	else if( (x<0.0f) && (y<0.0f) )
	{	ang = PI+ork::atanf( y/x );
	}
	// Q3 ( top right )
	else if( (x>0.0f) && (y<0.0f) )
	{	ang = ork::atanf( y/x );
	}
	
	return ang;
}

///////////////////////////////////////////////////////////////////////////////

F32 rect2pol_angr( F32 x, F32 y )
{
	F32 ang = 0.0f;

	// AXIS
	if( x == 0.0f )
	{	if( y == 0.0f )
			ang = 0.0f;
		else if( y > 0.0f )
			ang = PI_DIV_2;
		else if( y < 0.0f )
			ang = (3.0f*PI_DIV_2);
	}
	else if( y == 0.0f )
	{	if( x < 0.0f )
			ang = PI;
		else if( x > 0.0f )
			ang = 0.0f;
	}
	
	// Q0 ( bottom right )
	else if( (x>0.0f) && (y>0.0f) )
	{	ang = ork::atanf( y/x );
	}
	// Q1 ( bottom left )
	else if( (x<0.0f) && (y>0.0f) )
	{	ang = PI+ork::atanf( y/x );
	}
	// Q2 ( top left )
	else if( (x<0.0f) && (y<0.0f) )
	{	ang = PI+ork::atanf( y/x );
	}
	// Q3 ( top right )
	else if( (x>0.0f) && (y<0.0f) )
	{	ang = ork::atanf( y/x );
	}
	
	return ang;
}

///////////////////////////////////////////////////////////////////////////////

F32 rect2pol_rad( F32 x, F32 y )
{
	F32 rad = ork::sqrtf( (x*x)+(y*y) );
	return rad;
}
