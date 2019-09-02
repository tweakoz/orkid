////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/math/misc_math.h>
#include <ork/math/units.h>

namespace ork
{

///////////////////////////////////////////////////////////////////////////////
// default to meters

LengthUnit LengthUnit::geProjectUnits( float(1.0f) ); 
const LengthUnit LengthUnit::mMeters( float(1.0f) );
const LengthUnit LengthUnit::mCentimeters( float(0.01f) );

///////////////////////////////////////////////////////////////////////////////
// default to seconds

TimeUnit TimeUnit::geProjectUnits( float(1.0f) );
const TimeUnit TimeUnit::mSeconds( float(1.0f) );
const TimeUnit TimeUnit::mMilliseconds( float(0.01f) );
const TimeUnit TimeUnit::mMinutes( float(60.0f) );
const TimeUnit TimeUnit::mHours( float(3600.0f) );

///////////////////////////////////////////////////////////////////////////////

}
