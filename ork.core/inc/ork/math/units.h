////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _MATH_UNITS_H
#define _MATH_UNITS_H

namespace ork {
	
///////////////////////////////////////////////////////////////////////////////

enum EUnitType
{
	EUNITTYPE_TIME = 0,	// t
	EUNITTYPE_OVERTIME,	// t ^ -1
	EUNITTYPE_LENGTH,		// l ^ 1
	EUNITTYPE_AREA,		// l ^ 2
	EUNITTYPE_VOLUME,		// l ^ 3
};

///////////////////////////////////////////////////////////////////////////////

template <typename Unit>
class UnitizedReal // internal format is in meters
{
	float mReal; 

	UnitizedReal( const float& v );

public:

	UnitizedReal( const float& v, const Unit& elunit );
	float GetValue( const Unit& elunit ) const;
	void SetValue( const float& v, const Unit& elunit );
	bool operator< ( const UnitizedReal<Unit>& oth );
	bool operator> ( const UnitizedReal<Unit>& oth );
	UnitizedReal<Unit> operator* ( const float& scalar );
	void operator*= ( const float& scalar );
	UnitizedReal<Unit> operator+ ( const UnitizedReal<Unit>& oth );
	UnitizedReal<Unit> operator+= ( const UnitizedReal<Unit>& oth );
};

///////////////////////////////////////////////////////////////////////////////

template <typename Unit>
UnitizedReal<Unit>::UnitizedReal( const float& v )
	: mReal( v, Unit::GetUnitType() )
{
}

template <typename Unit>
UnitizedReal<Unit>::UnitizedReal( const float& v, const Unit& elunit )
	: mReal( float(0.0f) )
{
	SetValue( v, elunit );
}

template <typename Unit>
float UnitizedReal<Unit>::GetValue( const Unit& elunit ) const
{
	return elunit.FromNative()*mReal;
}

template <typename Unit>
void UnitizedReal<Unit>::SetValue( const float& v, const Unit& elunit )
{
	mReal = elunit.ToNative()*v;
}

template <typename Unit>
UnitizedReal<Unit> UnitizedReal<Unit>::operator* ( const float& scalar )
{
	return UnitizedReal<Unit>( mReal*scalar, Unit::GetNativeUnits() );
}

template <typename Unit>
void UnitizedReal<Unit>::operator*= ( const float& scalar )
{
	mReal*=scalar;
}

template <typename Unit>
UnitizedReal<Unit> UnitizedReal<Unit>::operator+ ( const UnitizedReal<Unit>& oth )
{
	return UnitizedReal<Unit>( mReal+oth.mReal, Unit::GetNativeUnits() );
}

template <typename Unit>
UnitizedReal<Unit> UnitizedReal<Unit>::operator+= ( const UnitizedReal<Unit>& oth )
{
	mReal += oth.mReal;
	return *this;
}

template <typename Unit>
bool UnitizedReal<Unit>::operator< ( const UnitizedReal<Unit>& oth )
{
	return mReal<oth.mReal;
}

template <typename Unit>
bool UnitizedReal<Unit>::operator> ( const UnitizedReal<Unit>& oth )
{
	return mReal>oth.mReal;
}

///////////////////////////////////////////////////////////////////////////////

struct LengthUnit
{
	LengthUnit( float tometers )
		: mToMeters( tometers )
		, mFromMeters( float(1.0f)/tometers )
	{
	}

	float ToNative() const { return mToMeters; }
	float FromNative() const { return mFromMeters; }

	static const LengthUnit& GetProjectUnits() { return geProjectUnits; }
	static const LengthUnit& GetNativeUnits() { return mMeters; }
	static const LengthUnit& Meters() { return mMeters; }
	static const LengthUnit& Centimeters() { return mCentimeters; }
	static void SetProjectUnits( const LengthUnit& eunit ) { geProjectUnits=eunit; }

	static const EUnitType GetUnitType() { return EUNITTYPE_LENGTH; }

private:

	float mToMeters;
	float mFromMeters;

	static LengthUnit						geProjectUnits;
	static const LengthUnit				mMeters;
	static const LengthUnit				mCentimeters;
};

///////////////////////////////////////////////////////////////////////////////

struct TimeUnit
{
	TimeUnit( float toseconds )
		: mToSeconds( toseconds )
		, mFromSeconds( float(1.0f)/toseconds )
	{
	}

	float ToNative() const { return mToSeconds; }
	float FromNative() const { return mFromSeconds; }

	static const TimeUnit& GetProjectUnits() { return geProjectUnits; }
	static const TimeUnit& GetNative() { return mSeconds; }
	static const TimeUnit& Seconds() { return mSeconds; }
	static const TimeUnit& Milliseconds() { return mMilliseconds; }
	static void SetProjectUnits( const TimeUnit& eunit ) { geProjectUnits=eunit; }

	static const EUnitType GetUnitType() { return EUNITTYPE_TIME; }

private:

	float mToSeconds;
	float mFromSeconds;

	static TimeUnit						geProjectUnits;
	static const TimeUnit					mSeconds;
	static const TimeUnit					mMinutes;
	static const TimeUnit					mHours;

	static const TimeUnit					mMilliseconds;
};

///////////////////////////////////////////////////////////////////////////////

typedef UnitizedReal<LengthUnit> LengthReal;
typedef UnitizedReal<TimeUnit> TimeReal;

///////////////////////////////////////////////////////////////////////////////

}

#endif
