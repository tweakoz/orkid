////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/math/cmatrix4.h>
#include <ork/math/cmatrix3.h>
#include <ork/math/cvector4.h>

#if defined WII
#include <revolution/os.h>
#include <revolution/base/PPCArch.h>
#endif

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

template <typename T> TVector3<T> TVector3<T>::Saturate( void ) const
{
	TVector3<T> rval = *this;
	rval.m_x = (rval.m_x>1.0f) ? 1.0f : (rval.m_x<0.0f) ? 0.0f : rval.m_x;
	rval.m_y = (rval.m_y>1.0f) ? 1.0f : (rval.m_y<0.0f) ? 0.0f : rval.m_y;
	rval.m_z = (rval.m_z>1.0f) ? 1.0f : (rval.m_z<0.0f) ? 0.0f : rval.m_z;
	return rval;
}


template <typename T> const TVector3<T> & TVector3<T>::Black( void )
{
	static const TVector3<T> Black( T(0.0f), T(0.0f), T(0.0f) );
	return Black;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector3<T> & TVector3<T>::DarkGrey( void )
{
	static const TVector3<T> DarkGrey( T(0.250f), T(0.250f), T(0.250f) );
	return DarkGrey;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector3<T> & TVector3<T>::MediumGrey( void )
{
	static const TVector3<T> MediumGrey( T(0.50f), T(0.50f), T(0.50f) );
	return MediumGrey;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector3<T> & TVector3<T>::LightGrey( void )
{
	static const TVector3<T> LightGrey(T(0.75f), T(0.75f), T(0.75f) );
	return LightGrey;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector3<T> & TVector3<T>::White( void )
{
	static const TVector3<T> White(  T(1.0f), T(1.0f), T(1.0f) );
	return White;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector3<T> & TVector3<T>::Red( void )
{
	static const TVector3<T> Red( T(1.0f), T(0.0f), T(0.0f) );
	return Red;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector3<T> & TVector3<T>::Green( void )
{
	static const TVector3<T> Green( T(0.0f), T(1.0f), T(0.0f) );
	return Green;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector3<T> & TVector3<T>::Blue( void )
{
	static const TVector3<T> Blue( T(0.0f), T(0.0f), T(1.0f) );
	return Blue;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector3<T> & TVector3<T>::Magenta( void )
{
	static const TVector3<T> Magenta( T(1.0f), T(0.0f), T(1.0f) );
	return Magenta;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector3<T> & TVector3<T>::Cyan( void )
{
	static const TVector3<T> Cyan( T(0.0f), T(1.0f), T(1.0f) );
	return Cyan;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector3<T> & TVector3<T>::Yellow( void )
{
	static const TVector3<T> Yellow( T(1.0f), T(1.0f), T(0.0f) );
	return Yellow;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> TVector3<T>::TVector3()
	: m_x(T(0.0f))
	, m_y(T(0.0f))
	, m_z(T(0.0f))
{
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> TVector3<T>::TVector3( T x, T y, T z)
	: m_x(x)
	, m_y(y)
	, m_z(z)
{
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> U32 TVector3<T>::GetVtxColorAsU32( void ) const 
{
	U32 r = U32(GetX()*T(255.0f));
	U32 g = U32(GetY()*T(255.0f));
	U32 b = U32(GetZ()*T(255.0f));
	U32 a = 255;
	
#if defined(_DARWIN)||defined(IX)
	return U32( (a<<24)|(b<<16)|(g<<8)|r );
#else // WIN32/DX
	return U32( (a<<24)|(r<<16)|(g<<8)|b );
#endif
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> U32 TVector3<T>::GetABGRU32( void ) const 
{
	U32 r = U32(GetX()*T(255.0f));
	U32 g = U32(GetY()*T(255.0f));
	U32 b = U32(GetZ()*T(255.0f));
	U32 a = 255;
	
	return U32( (a<<24)|(b<<16)|(g<<8)|r );
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> U32 TVector3<T>::GetARGBU32( void ) const 
{
	U32 r = U32(GetX()*T(255.0f));
	U32 g = U32(GetY()*T(255.0f));
	U32 b = U32(GetZ()*T(255.0f));
	U32 a = 255;
	
	return U32( (a<<24)|(r<<16)|(g<<8)|b );
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> U32 TVector3<T>::GetRGBAU32( void ) const 
{
	U32 r = U32(GetX()*T(255.0f));
	U32 g = U32(GetY()*T(255.0f));
	U32 b = U32(GetZ()*T(255.0f));
	U32 a = 255;

	U32 rval = 0;

	rval = ( (r<<24)|(g<<16)|(b<<8)|a );

	return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> U32 TVector3<T>::GetBGRAU32( void ) const
{	U32 r = U32(GetX()*T(255.0f));
	U32 g = U32(GetY()*T(255.0f));
	U32 b = U32(GetZ()*T(255.0f));
	U32 a = 255;
	
	return U32( (b<<24)|(g<<16)|(r<<8)|a );
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> U16 TVector3<T>::GetRGBU16() const 
{
	U32 r = U32(GetX() * T(31.0f));
	U32 g = U32(GetY() * T(31.0f));
	U32 b = U32(GetZ() * T(31.0f));

	U16 rval = U16((b<<10)|(g<<5)|r);

	return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector3<T>::SetRGBAU32(U32 uval) 
{	
	U32 r = (uval>>24) & 0xff;
	U32 g = (uval>>16) & 0xff;
	U32 b = (uval>>8) & 0xff;

	static const T kfic(1.0f / 255.0f);

	SetX(kfic * T(int(r)));
	SetY(kfic * T(int(g)));
	SetZ(kfic * T(int(b)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector3<T>::SetBGRAU32( U32 uval ) 
{	
	U32 b = (uval>>24) & 0xff;
	U32 g = (uval>>16) & 0xff;
	U32 r = (uval>>8) & 0xff;

	static const T kfic(1.0f / 255.0f);

	SetX(kfic * T(int(r)));
	SetY(kfic * T(int(g)));
	SetZ(kfic * T(int(b)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector3<T>::SetARGBU32(U32 uval) 
{	
	U32 r = (uval>>16) & 0xff;
	U32 g = (uval>>8) & 0xff;
	U32 b = (uval) & 0xff;

	static const T kfic(1.0f / 255.0f);

	SetX(kfic * T(int(r)));
	SetY(kfic * T(int(g)));
	SetZ(kfic * T(int(b)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector3<T>::SetABGRU32(U32 uval) 
{	
	U32 b = (uval>>16) & 0xff;
	U32 g = (uval>>8) & 0xff;
	U32 r = (uval) & 0xff;

	static const T kfic(1.0f / 255.0f);

	SetX(kfic * T(int(r)));
	SetY(kfic * T(int(g)));
	SetZ(kfic * T(int(b)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector3<T>::SetHSV( T h, T s, T v )
{
}

template <typename T> TVector3<T> TVector3<T>::Reflect( const TVector3 &N ) const
{
	const TVector3<T>& I = *this;
	TVector3<T> R = I-(N*2.0f*N.Dot(I));
	return R;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> TVector3<T>::TVector3( const TVector3<T> &vec)
{
	m_x = vec.m_x;
	m_y = vec.m_y;
	m_z = vec.m_z;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> TVector3<T>::TVector3( const TVector4<T> &vec)
{
	m_x = vec.GetX();
	m_y = vec.GetY();
	m_z = vec.GetZ();
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> TVector3<T>::TVector3( const TVector2<T> &vec)
{
	m_x = vec.GetX();
	m_y = vec.GetY();
	m_z = T(0);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T TVector3<T>::Dot( const TVector3<T> &vec) const
{
#if defined WII
	return __fmadds(m_x,vec.m_x,__fmadds(m_y,vec.m_y,__fmadds(m_z,vec.m_z,0.0f)));
#else
	return ( (m_x * vec.m_x) + (m_y * vec.m_y) + (m_z * vec.m_z) );
#endif
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> TVector3<T> TVector3<T>::Cross( const TVector3<T> &vec) const // c = this X vec
{
	T vx = ((m_y * vec.GetZ()) - (m_z * vec.GetY()));
	T vy = ((m_z * vec.GetX()) - (m_x * vec.GetZ()));
	T vz = ((m_x * vec.GetY()) - (m_y * vec.GetX()));

	return ( TVector3<T>( vx, vy, vz ) );
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector3<T>::Normalize(void)
{
	T mag = Mag();
	if( mag > Epsilon() )
	{
		T	distance = (T) 1.0f / mag ;

		m_x *= distance;
		m_y *= distance;
		m_z *= distance;
	}
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> TVector3<T> TVector3<T>::Normal() const
{
	TVector3<T> vec(*this);
	vec.Normalize();

	return vec;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T TVector3<T>::Mag(void) const
{
	return Sqrt(m_x * m_x + m_y * m_y + m_z * m_z);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T TVector3<T>::MagSquared(void) const
{
	T mag = (m_x * m_x + m_y * m_y + m_z * m_z);
	return mag;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> TVector4<T> TVector3<T>::Transform( const TMatrix4<T> &matrix ) const
{
	T	tx,ty,tz,tw;

	T *mp = (T *) matrix.elements;
	T x = m_x;
	T y = m_y;
	T z = m_z;
	T w = T(1.0f);

#if 0 //defined WII
	tx = __fmadds(m_x,vec.m_x,__fmadds(m_y,vec.m_y,__fmadds(m_z,vec.m_z,0.0f)));
#else
	tx = x*mp[0] + y*mp[4] + z*mp[8] + w*mp[12];
	ty = x*mp[1] + y*mp[5] + z*mp[9] + w*mp[13];
	tz = x*mp[2] + y*mp[6] + z*mp[10] + w*mp[14];
	tw = x*mp[3] + y*mp[7] + z*mp[11] + w*mp[15];
#endif

	return TVector4<T>( tx, ty, tz, tw );
}


////////////////////////////////////////////////////////////////////////////////

template <typename T> TVector3<T> TVector3<T>::Transform( const TMatrix3<T> &matrix ) const
{
	T	tx,ty,tz;

	T *mp = (T *) matrix.elements;
	T x = m_x;
	T y = m_y;
	T z = m_z;

	tx = x*mp[0] + y*mp[3] + z*mp[6];
	ty = x*mp[1] + y*mp[4] + z*mp[7];
	tz = x*mp[2] + y*mp[5] + z*mp[8];

	return TVector3<T>( tx, ty, tz );
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> TVector3<T> TVector3<T>::Transform3x3( const TMatrix4<T> &matrix ) const
{
	T	tx,ty,tz;
	T *mp = (T *) matrix.elements;
	T x = m_x;
	T y = m_y;
	T z = m_z;

	tx = x*mp[0] + y*mp[4] + z*mp[8];
	ty = x*mp[1] + y*mp[5] + z*mp[9];
	tz = x*mp[2] + y*mp[6] + z*mp[10];

	return TVector3<T>( tx, ty, tz );
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector3<T>::Serp( const TVector3<T> & PA, const TVector3<T> & PB, const TVector3<T> & PC, const TVector3<T> & PD, T Par )
{
	TVector3<T> PAB, PCD;
	PAB.Lerp( PA, PB, Par );
	PCD.Lerp( PC, PD, Par );
	Lerp( PAB, PCD, Par );
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector3<T>::RotateX(T rad)
{
	T	oldY = m_y;
	T	oldZ = m_z;
	m_y = (oldY * Cos(rad) - oldZ * Sin(rad));
	m_z = (oldY * Sin(rad) + oldZ * Cos(rad));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector3<T>::RotateY(T rad)
{
	T	oldX = m_x;
	T	oldZ = m_z;

	m_x = (oldX * Cos(rad) - oldZ * Sin(rad));
	m_z = (oldX * Sin(rad) + oldZ * Cos(rad));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector3<T>::RotateZ(T rad)
{
	T	oldX = m_x;
	T	oldY = m_y;

	m_x = (oldX * Cos(rad) - oldY * Sin(rad));
	m_y = (oldX * Sin(rad) + oldY * Cos(rad));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector3<T>::Lerp( const TVector3<T> &from, const TVector3<T> &to, T par )
{
	if( par < T(0.0f) ) par = T(0.0f);
	if( par > T(1.0f) ) par = T(1.0f);
	T ipar = T(1.0f) - par;
	m_x = (from.m_x*ipar) + (to.m_x*par);
	m_y = (from.m_y*ipar) + (to.m_y*par);
	m_z = (from.m_z*ipar) + (to.m_z*par);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T TVector3<T>::CalcTriArea( const TVector3<T> &V, const TVector3<T> & N ) 
{
    return T(0);
}

}
