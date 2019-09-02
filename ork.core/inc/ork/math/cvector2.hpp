////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/math/cvector3.h>

///////////////////////////////////////////////////////////////////////////////

template <typename T> ork::Vector2<T>::Vector2()
	: x(T(0))
	, y(T(0))
{
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> ork::Vector2<T>::Vector2( T _x, T _y)
	: x(_x)
	, y(_y)
{
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> ork::Vector2<T>::Vector2( const Vector2<T>& vec)
	: x( vec.GetX() )
	, y( vec.GetY() )
{
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> ork::Vector2<T>::Vector2( const Vector3<T>& vec)
	: x( vec.GetX() )
	, y( vec.GetY() )
{
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T ork::Vector2<T>::Dot( const Vector2<T>& vec) const
{
	return ( (x * vec.x) + (y * vec.y) );
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T ork::Vector2<T>::PerpDot( const Vector2<T>& oth) const
{
	return (x * oth.y)-(y * oth.x);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void ork::Vector2<T>::Normalize(void)
{
	T	distance = T(1) / Mag() ;

	x *= distance;
	y *= distance;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> ork::Vector2<T> ork::Vector2<T>::Normal() const
{
	T fmag = Mag();
	fmag = (fmag==T(0)) ? Epsilon() : fmag;
	T	s = T(1) / fmag;
	return Vector2<T>(x * s, y * s);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T ork::Vector2<T>::Mag(void) const
{
	return Sqrt(x * x + y * y);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T ork::Vector2<T>::MagSquared(void) const
{
	T mag = (x * x + y * y);
	return mag;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void ork::Vector2<T>::Serp( const Vector2<T> & PA, const Vector2<T> & PB, const Vector2<T> & PC, const Vector2<T> & PD, T Par )
{
	Vector2<T> PAB, PCD;
	PAB.Lerp( PA, PB, Par );
	PCD.Lerp( PC, PD, Par );
	Lerp( PAB, PCD, Par );
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void ork::Vector2<T>::Rotate(T rad)
{
	T	oldX = x;
	T	oldY = y;

	x = (oldX * Sin(rad) - oldY * Cos(rad));
	y = (oldX * Cos(rad) + oldY * Sin(rad));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void ork::Vector2<T>::Lerp( const Vector2<T> &from, const Vector2<T> &to, T par )
{
	if( par < T(0) ) par = T(0);
	if( par > T(1) ) par = T(1);
	T ipar = T(1) - par;
	x = (from.x*ipar) + (to.x*par);
	y = (from.y*ipar) + (to.y*par);
}

///////////////////////////////////////////////////////////////////////////////
