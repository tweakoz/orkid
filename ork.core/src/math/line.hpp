////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _ORK_MATH_LINE_HPP
#define _ORK_MATH_LINE_HPP

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

template< typename T >
TLineSegment2Helper<T>::TLineSegment2Helper()
{

}
template< typename T >
TLineSegment2Helper<T>::TLineSegment2Helper( const vec2_type& s, const vec2_type& e ) : mStart(s), mEnd(e)
{
   mOrigin = mEnd - mStart;
   mMag = mOrigin.magnitude();

}

template< typename T >
void TLineSegment2Helper<T>::SetStartEnd( const vec2_type& s, const vec2_type& e )
{

   mStart.x = (s.x);
   mStart.y = (s.y);
   mEnd.x = (e.x);
   mEnd.y = (e.y);
   mOrigin = mEnd - mStart;
   mMag = mOrigin.magnitude();

}

template< typename T >
float TLineSegment2Helper<T>::pointDistanceSquared( const vec2_type  &pt ) const
{
	vec2_type pt_origin = pt - mStart;
	T U_num = (pt_origin).dotWith(mOrigin);
    T U_den = ( mMag * mMag );

	T U_num2 =  ((pt.x - mStart.x) * (mEnd.x - mStart.x) + ((pt.y - mStart.y) * (mEnd.y - mStart.y)));

	if(fabs(U_num) < U_den)
	{
		T U = U_num / U_den;
		return(pt_origin - (mOrigin * U)).magnitudeSquared();
	}
	return T(0.0f);
}


template< typename T >
float TLineSegment2Helper<T>::pointDistancePercent( const vec2_type  &pt ) const
{

	/*
	 float U_num = ( ( ( pt.x - mStart.x ) * ( mEnd.x - mStart.x ) ) +
        ( ( pt.y - mStart.y ) * ( mEnd.y - mStart.y ))) ;

	*/
	vec2_type pt_origin = pt - mStart;
	T U_num = (pt_origin).dotWith(mOrigin);
    T U_den = ( mMag * mMag );
	if(fabs(U_num) < U_den)
	{
		return(U_num / U_den);
	}
	return T(0.0f);
}

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

#endif
