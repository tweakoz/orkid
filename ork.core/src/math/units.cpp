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

LengthUnit LengthUnit::geProjectUnits( CReal(1.0f) ); 
const LengthUnit LengthUnit::mMeters( CReal(1.0f) );
const LengthUnit LengthUnit::mCentimeters( CReal(0.01f) );

///////////////////////////////////////////////////////////////////////////////
// default to seconds

TimeUnit TimeUnit::geProjectUnits( CReal(1.0f) );
const TimeUnit TimeUnit::mSeconds( CReal(1.0f) );
const TimeUnit TimeUnit::mMilliseconds( CReal(0.01f) );
const TimeUnit TimeUnit::mMinutes( CReal(60.0f) );
const TimeUnit TimeUnit::mHours( CReal(3600.0f) );

///////////////////////////////////////////////////////////////////////////////

}
