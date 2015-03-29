////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef __VECTOR_4D_H__
#define __VECTOR_4D_H__

///////////////////////////////////////////////////////////////////////////////
#include <ork/orkstd.h> // For OrkAssert
#include <ork/orktypes.h> // For CReal
#include <ork/math/cvector3.h>
#include <ork/config/config.h>
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
namespace ork{
///////////////////////////////////////////////////////////////////////////////

template <typename T> class TMatrix4;
template <typename T> class TVector3;

template <typename T> class  TVector4
{
	static T Sin( T );
	static T Cos( T );
	static T Sqrt( T );
	static T Epsilon();
	static T Abs( T );

public:

	TVector4();
	explicit TVector4( T x, T y, T z, T w=T(1.0f));							// constructor from 3 floats
	TVector4( const TVector4 &vec);											// constructor from a vector
	TVector4( const TVector3<T> &vec, T w=T(1.0f));							// constructor from a vector
	TVector4( U32 uval ) { SetRGBAU32( uval ); }
	~TVector4() {};															// default destructor, does nothing

	void				RotateX(T rad);
	void				RotateY(T rad);
	void				RotateZ(T rad);

	TVector4			Saturate() const; 
	T					Dot( const TVector4 &vec) const;					// dot product of two vectors
	TVector4			Cross( const TVector4 &vec) const;					// cross product of two vectors

	void				Normalize(void);									// normalize this vector
	TVector4			Normal() const;

	T					Mag(void) const;									// return magnitude of this vector
	T					MagSquared(void) const;								// return magnitude of this vector squared
	TVector4			Transform( const TMatrix4<T> &matrix) const;		// transform this vector
	void				PerspectiveDivide( void );

	void				Lerp( const TVector4 &from, const TVector4 &to, T par );
	void				Serp( const TVector4 & PA, const TVector4 & PB, const TVector4 & PC, const TVector4 & PD, T Par );

	T					GetX(void) const { return (m_x); }
	T					GetY(void) const { return (m_y); }
	T					GetZ(void) const { return (m_z); }
	T					GetW(void) const { return (m_w); }

	// sometimes 4-vectors are used for XYWH format
	T					GetWidth(void)  const { return (m_z); }
	T					GetHeight(void) const { return (m_w); }

	void			 	Set(T x,T y,T z,T w) { m_x = x; m_y = y; m_z = z; m_w = w;}
	void			 	Set(T x,T y,T z) { m_x = x; m_y = y; m_z = z; m_w = (T)1.0f;}
	void			 	SetX(T x) { m_x = x; }
	void				SetY(T y) { m_y = y; }
	void				SetZ(T z) { m_z = z; }
	void				SetW(T w) { m_w = w; }

	// sometimes 4-vectors are used for XYWH format
	void				SetWidth(T width)   { m_z = width;  }
	void				SetHeight(T height) { m_w = height; }

	TVector3<T>			GetXYZ( void ) const { return TVector3<T>(*this); }

	static	TVector4	Zero(void) { return TVector4(T(0), T(0), T(0), T(0)); }
	static T			CalcTriArea( const TVector4 &V0, const TVector4 &V1, const TVector4 &V2, const TVector4 & N );

	void SetXYZ( T x, T y, T z )
	{
		SetX(x);
		SetY(y);
		SetZ(z);
	}

	inline T &operator[]( U32 i )
	{
		T *v = & m_x;
		OrkAssert( i<4 );
		return v[i];
	}

	inline const T &operator[]( U32 i ) const
	{
		const T *v = & m_x;
		OrkAssert( i<4 );
		return v[i];
	}

	inline TVector4 operator-() const
	{
		return TVector4( -m_x, -m_y, -m_z, -m_w );
	}

	inline TVector4 operator+( const TVector4 &b ) const
	{
		return TVector4( (m_x+b.m_x), (m_y+b.m_y), (m_z+b.m_z), (m_w+b.m_w) );
	}

	inline TVector4 operator*( const TVector4 &b ) const
	{
		return TVector4( (m_x*b.m_x), (m_y*b.m_y), (m_z*b.m_z), (m_w*b.m_w) );
	}

	inline TVector4 operator*( T scalar ) const
	{
		return TVector4( (m_x*scalar), (m_y*scalar), (m_z*scalar), (m_w*scalar) );
	}

	inline TVector4 operator-( const TVector4 &b ) const
	{
		return TVector4( (m_x-b.m_x), (m_y-b.m_y), (m_z-b.m_z), (m_w-b.m_w) );
	}

	inline TVector4 operator/( const TVector4 &b ) const
	{
		return TVector4( (m_x/b.m_x), (m_y/b.m_y), (m_z/b.m_z), (m_w/b.m_w) );
	}

	inline TVector4 operator/( T scalar ) const
	{
		return TVector4( (m_x/scalar), (m_y/scalar), (m_z/scalar), (m_w/scalar) );
	}

	inline void operator+=( const TVector4 & b )
	{
		m_x+=b.m_x;
		m_y+=b.m_y;
		m_z+=b.m_z;
		m_w+=b.m_w;
	}

	inline void operator-=( const TVector4 & b )
	{
		m_x-=b.m_x;
		m_y-=b.m_y;
		m_z-=b.m_z;
		m_w-=b.m_w;
	}

	inline void operator*=( T scalar )
	{
		m_x*=scalar;
		m_y*=scalar;
		m_z*=scalar;
		m_w*=scalar;
	}

	inline void operator*=( const TVector4 & b )
	{
		m_x*=b.m_x;
		m_y*=b.m_y;
		m_z*=b.m_z;
		m_w*=b.m_w;
	}

	inline void operator/=( const TVector4 &b )
	{
		m_x/=b.m_x;
		m_y/=b.m_y;
		m_z/=b.m_z;
		m_w/=b.m_w;
	}

	inline void operator/=( T scalar )
	{
		m_x/=scalar;
		m_y/=scalar;
		m_z/=scalar;
		m_w/=scalar;
	}

	inline bool operator==( const TVector4 &b ) const
	{
		return ( m_x == b.m_x && m_y == b.m_y && m_z == b.m_z && m_w == b.m_w );
	}
	inline bool operator!=( const TVector4 &b ) const
	{
		return ( m_x != b.m_x || m_y != b.m_y || m_z != b.m_z || m_w != b.m_w );
	}

	void SetHSV( T h, T s, T v );
	void SetRGB( T r, T g, T b ) { SetXYZ( r, g, b ); }
	U32 GetABGRU32( void ) const;
	U32 GetARGBU32( void ) const;
	U32 GetRGBAU32( void ) const;
	U32 GetBGRAU32( void ) const;
	U16 GetRGBU16( void ) const;
	U32 GetVtxColorAsU32(void) const;

	void SetRGBAU32( U32 uval );
	void SetBGRAU32( U32 uval );
	void SetARGBU32( U32 uval );
	void SetABGRU32( U32 uval );

	static const TVector4 & Black( void );
	static const TVector4 & DarkGrey( void );
	static const TVector4 & MediumGrey( void );
	static const TVector4 & LightGrey( void );
	static const TVector4 & White( void );

	static const TVector4 & Red( void );
	static const TVector4 & Green( void );
	static const TVector4 & Blue( void );
	static const TVector4 & Magenta( void );
	static const TVector4 & Cyan( void );
	static const TVector4 & Yellow( void );

	T *GetArray( void ) const { return const_cast<T*>( & m_x ); }

	template <typename U>
	static TVector4 FromTVector4(TVector4<U> vec)
	{
		return TVector4(T::FromFX(vec.GetX().FXCast()),
						T::FromFX(vec.GetY().FXCast()),
						T::FromFX(vec.GetZ().FXCast()),
						T::FromFX(vec.GetW().FXCast()));
	}

protected:

	T					m_x; // x component of this vector
	T					m_y; // y component of this vector
	T					m_z; // z component of this vector
	T					m_w;

};

typedef TVector4<float> CVector4; // this alias is deprecated! fvec4 is the new alias
typedef TVector4<float> fvec4;
typedef TVector4<double> dvec4;
typedef CVector4 CColor4;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////

template <typename T>
inline ork::TVector4<T> operator*( T scalar, const ork::TVector4<T> &b )
{
	return ork::TVector4<T>( (scalar*b.GetX()), (scalar*b.GetY()), (scalar*b.GetZ()) );
}

///////////////////////////////////////////////////////////////////////////////

#endif
