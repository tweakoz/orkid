////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef __VECTOR_3D_H__
#define __VECTOR_3D_H__

///////////////////////////////////////////////////////////////////////////////

#include <ork/orkstd.h> // For OrkAssert
#include <ork/math/cvector2.h>

#include <ork/config/config.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork{
///////////////////////////////////////////////////////////////////////////////

template <typename T> class TMatrix4;
template <typename T> class TMatrix3;
template <typename T> class TVector4;
template <typename T> class TVector2;

///////////////////////////////////////////////////////////////////////////////

template <typename T> class  TVector3
{
	static T Sin( T );
	static T Cos( T );
	static T Sqrt( T );
	static T Epsilon();
	static T Abs( T );

public:

	TVector3();
	explicit TVector3( T x, T y, T z);												// constructor from 3 floats
	TVector3( const TVector3 &vec);											// constructor from a vector
	TVector3( const TVector4<T> &vec);
	TVector3( const TVector2<T> &vec);
	TVector3( U32 uval ) { SetRGBAU32( uval ); }
	~TVector3() {};															// default destructor, does nothing

	void				RotateX(T rad);
	void				RotateY(T rad);
	void				RotateZ(T rad);

	TVector3			Saturate() const; 

	T					Dot( const TVector3 &vec) const;					// dot product of two vectors
	TVector3			Cross( const TVector3 &vec) const;					// cross product of two vectors

	void				Normalize(void);									// normalize this vector
	TVector3			Normal() const;

	T					Mag(void) const;									// return magnitude of this vector
	T					MagSquared(void) const;								// return magnitude of this vector squared
	TVector4<T>			Transform( const TMatrix4<T> &matrix) const;		// transform this vector
	TVector3<T>			Transform( const TMatrix3<T> &matrix) const;		// transform this vector
	TVector3<T>			Transform3x3( const TMatrix4<T> &matrix) const;		// transform this vector

	void				Lerp( const TVector3 &from, const TVector3 &to, T par );
	void				Serp( const TVector3 & PA, const TVector3 & PB, const TVector3 & PC, const TVector3 & PD, T Par );

	T					GetX(void) const { return (m_x); }
	T					GetY(void) const { return (m_y); }
	T					GetZ(void) const { return (m_z); }

	void			 	Set(T x,T y,T z) { m_x = x; m_y = y; m_z = z;}
	void			 	SetX(T x) { m_x = x; }
	void				SetY(T y) { m_y = y; }
	void				SetZ(T z) { m_z = z; }

	TVector2<T>			GetXY( void ) const { return TVector2<T>(*this); }

	static	TVector3	Zero(void) { return TVector3(T(0), T(0), T(0)); }
	static	TVector3	UnitX(void) { return TVector3(T(1), T(0), T(0)); }
	static	TVector3	UnitY(void) { return TVector3(T(0), T(1), T(0)); }
	static	TVector3	UnitZ(void) { return TVector3(T(0), T(0), T(1)); }
	static  TVector3	One(void) { return TVector3(T(1), T(1), T(1)); }

	static	T			CalcTriArea( const TVector3 &V, const TVector3 & N );

	TVector3<T>			Reflect( const TVector3 &N ) const; //R = I-(N*2*dot(N,I));

	void SetXYZ( T x, T y, T z )
	{
		SetX(x);
		SetY(y);
		SetZ(z);
	}

	inline T &operator[]( U32 i )
	{
		T *v = & m_x;
		OrkAssert( i<3 );
		return v[i];
	}

	inline const T &operator[]( U32 i ) const
	{
		const T *v = & m_x;
		OrkAssert( i<3 );
		return v[i];
	}

	inline TVector3 operator-() const
	{
		return TVector3( -m_x, -m_y, -m_z );
	}

	inline TVector3 operator+( const TVector3 &b ) const
	{
		return TVector3( (m_x+b.m_x), (m_y+b.m_y), (m_z+b.m_z) );
	}

	inline TVector3 operator*( const TVector3 &b ) const
	{
		return TVector3( (m_x*b.m_x), (m_y*b.m_y), (m_z*b.m_z) );
	}

	inline TVector3 operator*( T scalar ) const
	{
		return TVector3( (m_x*scalar), (m_y*scalar), (m_z*scalar) );
	}

	inline TVector3 operator-( const TVector3 &b ) const
	{
		return TVector3( (m_x-b.m_x), (m_y-b.m_y), (m_z-b.m_z) );
	}

	inline TVector3 operator/( const TVector3 &b ) const
	{
		return TVector3( (m_x/b.m_x), (m_y/b.m_y), (m_z/b.m_z) );
	}

	inline TVector3 operator/( T scalar ) const
	{
		return TVector3( (m_x/scalar), (m_y/scalar), (m_z/scalar) );
	}

	inline void operator+=( const TVector3 & b )
	{
		m_x+=b.m_x;
		m_y+=b.m_y;
		m_z+=b.m_z;
	}

	inline void operator-=( const TVector3 & b )
	{
		m_x-=b.m_x;
		m_y-=b.m_y;
		m_z-=b.m_z;
	}

	inline void operator*=( T scalar )
	{
		m_x*=scalar;
		m_y*=scalar;
		m_z*=scalar;
	}

	inline void operator*=( const TVector3 & b )
	{
		m_x*=b.m_x;
		m_y*=b.m_y;
		m_z*=b.m_z;
	}

	inline void operator/=( const TVector3 &b )
	{
		m_x/=b.m_x;
		m_y/=b.m_y;
		m_z/=b.m_z;
	}

	inline void operator/=( T scalar )
	{
		m_x/=scalar;
		m_y/=scalar;
		m_z/=scalar;
	}

	inline bool operator==( const TVector3 &b ) const
	{
		return ( m_x == b.m_x && m_y == b.m_y && m_z == b.m_z );
	}
	inline bool operator!=( const TVector3 &b ) const
	{
		return ( m_x != b.m_x || m_y != b.m_y || m_z != b.m_z );
	}

	void SetHSV( T h, T s, T v );
	void SetRGB( T r, T g, T b ) { SetXYZ( r, g, b ); }

	U32 GetVtxColorAsU32(void) const;
	U32 GetARGBU32( void ) const;
	U32 GetABGRU32( void ) const;
	U32 GetRGBAU32( void ) const;
	U32 GetBGRAU32( void ) const;
	U16 GetRGBU16( void ) const;

	void SetRGBAU32( U32 uval );
	void SetBGRAU32( U32 uval );
	void SetARGBU32( U32 uval );
	void SetABGRU32( U32 uval );

	static const TVector3 & Black( void );
	static const TVector3 & DarkGrey( void );
	static const TVector3 & MediumGrey( void );
	static const TVector3 & LightGrey( void );
	static const TVector3 & White( void );

	static const TVector3 & Red( void );
	static const TVector3 & Green( void );
	static const TVector3 & Blue( void );
	static const TVector3 & Magenta( void );
	static const TVector3 & Cyan( void );
	static const TVector3 & Yellow( void );

	T *GetArray( void ) const { return const_cast<T*>( & m_x ); }

	template <typename U>
	static TVector3 FromTVector3(TVector3<U> vec)
	{
		return TVector3(T::FromFX(vec.GetX().FXCast()),
						T::FromFX(vec.GetY().FXCast()),
						T::FromFX(vec.GetZ().FXCast()));
	}

protected:

	T					m_x; // x component of this vector
	T					m_y; // y component of this vector
	T					m_z; // z component of this vector

};

typedef TVector3<float> CVector3;
typedef TVector3<float> fvec3;
typedef TVector3<double> dvec3;
typedef CVector3 CColor3;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////

template <typename T>
inline ork::TVector3<T> operator*( T scalar, const ork::TVector3<T> &b )
{
	return ork::TVector3<T>( (scalar*b.GetX()), (scalar*b.GetY()), (scalar*b.GetZ()) );
}

///////////////////////////////////////////////////////////////////////////////

#endif
