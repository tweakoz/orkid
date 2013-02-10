////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#if defined(_WIN32) && ! defined(_XBOX)
#include <pmmintrin.h>
#endif

namespace ork {

///////////////////////////////////////////////////////////////////////////////

template <typename T> TVector4<T> TVector4<T>::Saturate( void ) const
{
	TVector4<T> rval = *this;
	rval.m_x = (rval.m_x>1.0f) ? 1.0f : (rval.m_x<0.0f) ? 0.0f : rval.m_x;
	rval.m_y = (rval.m_y>1.0f) ? 1.0f : (rval.m_y<0.0f) ? 0.0f : rval.m_y;
	rval.m_z = (rval.m_z>1.0f) ? 1.0f : (rval.m_z<0.0f) ? 0.0f : rval.m_z;
	rval.m_w = (rval.m_w>1.0f) ? 1.0f : (rval.m_w<0.0f) ? 0.0f : rval.m_w;
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector4<T> & TVector4<T>::Black( void )
{
	static const TVector4<T> Black( T(0.0f), T(0.0f), T(0.0f), T(1.0f) );
	return Black;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector4<T> & TVector4<T>::DarkGrey( void )
{
	static const TVector4<T> DarkGrey( T(0.250f), T(0.250f), T(0.250f), T(1.0f) );
	return DarkGrey;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector4<T> & TVector4<T>::MediumGrey( void )
{
	static const TVector4<T> MediumGrey( T(0.50f), T(0.50f), T(0.50f), T(1.0f) );
	return MediumGrey;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector4<T> & TVector4<T>::LightGrey( void )
{
	static const TVector4<T> LightGrey(T(0.75f), T(0.75f), T(0.75f), T(1.0f) );
	return LightGrey;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector4<T> & TVector4<T>::White( void )
{
	static const TVector4<T> White(  T(1.0f), T(1.0f), T(1.0f), T(1.0f) );
	return White;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector4<T> & TVector4<T>::Red( void )
{
	static const TVector4<T> Red( T(1.0f), T(0.0f), T(0.0f), T(1.0f) );
	return Red;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector4<T> & TVector4<T>::Green( void )
{
	static const TVector4<T> Green( T(0.0f), T(1.0f), T(0.0f), T(1.0f)  );
	return Green;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector4<T> & TVector4<T>::Blue( void )
{
	static const TVector4<T> Blue( T(0.0f), T(0.0f), T(1.0f), T(1.0f) );
	return Blue;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector4<T> & TVector4<T>::Magenta( void )
{
	static const TVector4<T> Magenta( T(1.0f), T(0.0f), T(1.0f), T(1.0f) );
	return Magenta;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector4<T> & TVector4<T>::Cyan( void )
{
	static const TVector4<T> Cyan( T(0.0f), T(1.0f), T(1.0f), T(1.0f));
	return Cyan;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const TVector4<T> & TVector4<T>::Yellow( void )
{
	static const TVector4<T> Yellow( T(1.0f), T(1.0f), T(0.0f), T(1.0f) );
	return Yellow;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> TVector4<T>::TVector4()
	: m_x(T(0.0f))
	, m_y(T(0.0f))
	, m_z(T(0.0f))
	, m_w(T(1.0f))
{
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> TVector4<T>::TVector4( T x, T y, T z, T w)
	: m_x(x)
	, m_y(y)
	, m_z(z)
	, m_w(w)
{
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> TVector4<T>::TVector4( const TVector3<T> & in, T w )
	: m_x(in.GetX())
	, m_y(in.GetY())
	, m_z(in.GetZ())
	, m_w(w)
{
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> U32 TVector4<T>::GetVtxColorAsU32( void ) const
{	U32 r = U32(GetX()*T(255.0f));
	U32 g = U32(GetY()*T(255.0f));
	U32 b = U32(GetZ()*T(255.0f));
	U32 a = U32(GetW()*T(255.0f));
	

#if defined(_DARWIN)||defined(IX)//GL
	return U32( (a<<24)|(b<<16)|(g<<8)|r );
#else // WIN32/DX
	return U32( (a<<24)|(r<<16)|(g<<8)|b );
#endif
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> U32 TVector4<T>::GetABGRU32( void ) const
{	U32 r = U32(GetX()*T(255.0f));
	U32 g = U32(GetY()*T(255.0f));
	U32 b = U32(GetZ()*T(255.0f));
	U32 a = U32(GetW()*T(255.0f));
	
	return U32( (a<<24)|(b<<16)|(g<<8)|r );
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> U32 TVector4<T>::GetARGBU32( void ) const
{	U32 r = U32(GetX()*T(255.0f));
	U32 g = U32(GetY()*T(255.0f));
	U32 b = U32(GetZ()*T(255.0f));
	U32 a = U32(GetW()*T(255.0f));
	
	return U32( (a<<24)|(r<<16)|(g<<8)|b );
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> U32 TVector4<T>::GetRGBAU32( void ) const 
{	
	S32 r = U32(GetX()*T(255.0f));
	S32 g = U32(GetY()*T(255.0f));
	S32 b = U32(GetZ()*T(255.0f));
	S32 a = U32(GetW()*T(255.0f));

	if( r<0 ) r=0;
	if( g<0 ) g=0;
	if( b<0 ) b=0;
	if( a<0 ) a=0;

	U32 rval = 0;

	rval = ( (r<<24)|(g<<16)|(b<<8)|a );

	return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> U32 TVector4<T>::GetBGRAU32( void ) const
{	U32 r = U32(GetX()*T(255.0f));
	U32 g = U32(GetY()*T(255.0f));
	U32 b = U32(GetZ()*T(255.0f));
	U32 a = U32(GetW()*T(255.0f));
	
	return U32( (b<<24)|(g<<16)|(r<<8)|a );
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> U16 TVector4<T>::GetRGBU16( void ) const 
{
	U32 r = U32(GetX() * T(31.0f));
	U32 g = U32(GetY() * T(31.0f));
	U32 b = U32(GetZ() * T(31.0f));
								
	U16 rval = U16((b<<10)|(g<<5)|r);

	return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector4<T>::SetRGBAU32( U32 uval ) 
{	
	U32 r = (uval>>24)&0xff;
	U32 g = (uval>>16)&0xff;
	U32 b = (uval>>8)&0xff;
	U32 a = (uval)&0xff;

	static const T kfic( 1.0f / 255.0f );

	SetX(kfic * T(int(r)));
	SetY(kfic * T(int(g)));
	SetZ(kfic * T(int(b)));
	SetW(kfic * T(int(a)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector4<T>::SetBGRAU32( U32 uval ) 
{	
	U32 b = (uval>>24)&0xff;
	U32 g = (uval>>16)&0xff;
	U32 r = (uval>>8)&0xff;
	U32 a = (uval)&0xff;

	static const T kfic( 1.0f / 255.0f );

	SetX(kfic * T(int(r)));
	SetY(kfic * T(int(g)));
	SetZ(kfic * T(int(b)));
	SetW(kfic * T(int(a)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector4<T>::SetARGBU32( U32 uval ) 
{	
	U32 a = (uval>>24)&0xff;
	U32 r = (uval>>16)&0xff;
	U32 g = (uval>>8)&0xff;
	U32 b = (uval)&0xff;

	static const T kfic( 1.0f / 255.0f );

	SetX(kfic * T(int(r)));
	SetY(kfic * T(int(g)));
	SetZ(kfic * T(int(b)));
	SetW(kfic * T(int(a)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector4<T>::SetABGRU32( U32 uval ) 
{	
	U32 a = (uval>>24)&0xff;
	U32 b = (uval>>16)&0xff;
	U32 g = (uval>>8)&0xff;
	U32 r = (uval)&0xff;

	static const T kfic( 1.0f / 255.0f );

	SetX(kfic * T(int(r)));
	SetY(kfic * T(int(g)));
	SetZ(kfic * T(int(b)));
	SetW(kfic * T(int(a)));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector4<T>::SetHSV( T h, T s, T v )
{
//	hsv.x = saturate(hsv.x);
//	hsv.y = saturate(hsv.y);
//	hsv.z = saturate(hsv.z);

	if ( s == 0.0f )
	{
		// Grayscale 
		SetX(v);
		SetY(v);
		SetZ(v);
	}
	else
	{
		const T kone(1.0f);

		if (kone <= h) h -= kone;
		h *= 6.0f;
		T i = T(floor(h));
		T f = h - i;
		T aa = v * (kone - s);
		T bb = v * (kone - (s * f));
		T cc = v * (kone - (s * (kone-f)));
		if( i < kone )
		{
			SetX( v );
			SetY( cc );
			SetZ( aa );
		}
		else if( i < 2.0f )
		{
			SetX( bb );
			SetY( v );
			SetZ( aa );
		}
		else if( i < 3.0f )
		{
			SetX( aa );
			SetY( v );
			SetZ( cc );
		}
		else if( i < 4.0f )
		{
			SetX( aa );
			SetY( bb );
			SetZ( v );
		}
		else if( i < 5.0f )
		{
			SetX( cc );
			SetY( aa );
			SetZ( v );
		}
		else
		{
			SetX( v );
			SetY( aa );
			SetZ( bb );
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector4<T>::PerspectiveDivide( void )
{
	T iw = T(1.0f) / m_w;
	m_x *= iw;
	m_y *= iw;
	m_z *= iw;
	m_w = T(1.0f);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> TVector4<T>::TVector4( const TVector4<T> &vec)
{
	m_x = vec.m_x;
	m_y = vec.m_y;
	m_z = vec.m_z;
	m_w = vec.m_w;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T TVector4<T>::Dot( const TVector4<T> &vec) const
{
	return ( (m_x * vec.m_x) + (m_y * vec.m_y) + (m_z * vec.m_z) );
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> TVector4<T> TVector4<T>::Cross( const TVector4<T> &vec) const // c = this X vec
{
	T vx = ((m_y * vec.GetZ()) - (m_z * vec.GetY()));
	T vy = ((m_z * vec.GetX()) - (m_x * vec.GetZ()));
	T vz = ((m_x * vec.GetY()) - (m_y * vec.GetX()));

	return ( TVector4<T>( vx, vy, vz ) );
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector4<T>::Normalize(void)
{
	T	distance = (T) 1.0f / Mag() ;

	m_x *= distance;
	m_y *= distance;
	m_z *= distance;
	
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> TVector4<T> TVector4<T>::Normal() const
{
	T fmag = Mag();
	fmag = (fmag==(T)0.0f) ? (T)0.00001f : fmag;
	T	s = (T) 1.0f / fmag;
	return TVector4<T>(m_x * s, m_y * s, m_z * s, m_w);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T TVector4<T>::Mag(void) const
{
	return Sqrt(m_x * m_x + m_y * m_y + m_z * m_z);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T TVector4<T>::MagSquared(void) const
{
	T mag = (m_x * m_x + m_y * m_y + m_z * m_z);
	return mag;
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> TVector4<T> TVector4<T>::Transform( const TMatrix4<T> &matrix ) const
{
	T	tx,ty,tz,tw;

	T *mp = (T *) matrix.elements;
	T x = m_x;
	T y = m_y;
	T z = m_z;
	T w = m_w;

	tx = x*mp[0] + y*mp[4] + z*mp[8] + w*mp[12];
	ty = x*mp[1] + y*mp[5] + z*mp[9] + w*mp[13];
	tz = x*mp[2] + y*mp[6] + z*mp[10] + w*mp[14];
	tw = x*mp[3] + y*mp[7] + z*mp[11] + w*mp[15];

	return( TVector4<T>( tx,ty,tz, tw ) );
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector4<T>::Serp( const TVector4<T> & PA, const TVector4<T> & PB, const TVector4<T> & PC, const TVector4<T> & PD, T Par )
{
	TVector4<T> PAB, PCD;
	PAB.Lerp( PA, PB, Par );
	PCD.Lerp( PC, PD, Par );
	Lerp( PAB, PCD, Par );
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector4<T>::RotateX(T rad)
{
	T	oldY = m_y;
	T	oldZ = m_z;
	m_y = (oldY * Cos(rad) - oldZ * Sin(rad));
	m_z = (oldY * Sin(rad) + oldZ * Cos(rad));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector4<T>::RotateY(T rad)
{
	T	oldX = m_x;
	T	oldZ = m_z;

	m_x = (oldX * Cos(rad) - oldZ * Sin(rad));
	m_z = (oldX * Sin(rad) + oldZ * Cos(rad));
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector4<T>::RotateZ(T rad)
{
	T	oldX = m_x;
	T	oldY = m_y;

	m_x = (oldX * Cos(rad) - oldY * Sin(rad));
	m_y = (oldX * Sin(rad) + oldY * Cos(rad));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TVector4<T>::Lerp( const TVector4<T> &from, const TVector4<T> &to, T par )
{
	if( par < T(0.0f) ) par = T(0.0f);
	if( par > T(1.0f) ) par = T(1.0f);
	T ipar = T(1.0f) - par;
	m_x = (from.m_x*ipar) + (to.m_x*par);
	m_y = (from.m_y*ipar) + (to.m_y*par);
	m_z = (from.m_z*ipar) + (to.m_z*par);
	m_w = (from.m_w*ipar) + (to.m_w*par);
}

////////////////////////////////////////////////////////////////////////////////

template <typename T> T TVector4<T>::CalcTriArea( const TVector4<T> &V0, const TVector4<T> &V1, const TVector4<T> &V2, const TVector4<T> & N )
{
	// select largest abs coordinate to ignore for projection
	T ax = Abs( N.GetX() );
	T ay = Abs( N.GetY() );
	T az = Abs( N.GetZ() );

	int coord = (ax>ay) ? ((ax>az) ? 1 : 3) : ((ay>az) ? 2 : 3);

	// compute area of the 2D projection

	TVector4<T> Ary[3];
	Ary[0] = V0;
	Ary[1] = V1;
	Ary[2] = V2;
	T area(0.0f);

	for(	int i=1,
			j=2,
			k=0;
				i<=3;
					i++,
					j++,
					k++ )
	{
		int ii = i%3;
		int jj = j%3;
		int kk = k%3;

		switch (coord)
		{
			case 1:
				area += (Ary[ii].GetY() * (Ary[jj].GetZ() - Ary[kk].GetZ()));
				continue;
			case 2:
				area += (Ary[ii].GetX() * (Ary[jj].GetZ() - Ary[kk].GetZ()));
				continue;
			case 3:
				area += (Ary[ii].GetX() * (Ary[jj].GetY() - Ary[kk].GetY()));
				continue;
		}
	}

	T an = Sqrt( (T) (ax*ax + ay*ay + az*az) );

	switch (coord)
	{
		case 1:
			OrkAssert( ax != T(0) );
			area *= (an / (T(2.0f)*ax));
			break;
		case 2:
			OrkAssert( ay != T(0) );
			area *= (an / (T(2.0f)*ay));
			break;
		case 3:
			OrkAssert( az != T(0) );
			area *= (an / (T(2.0f)*az));
			break;
	}

	return Abs( area );
}

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
