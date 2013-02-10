////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef __VECTOR_2D_H__
#define __VECTOR_2D_H__

///////////////////////////////////////////////////////////////////////////////

#include <ork/orkstd.h> // For OrkAssert
#include <ork/orktypes.h> // for U32 etc

#include <ork/config/config.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork{
///////////////////////////////////////////////////////////////////////////////

template <typename T> class TVector3;

template <typename T> class  TVector2
{
	static T Sin( T );
	static T Cos( T );
	static T Sqrt( T );
	static T Epsilon();
	static T Abs( T );

public:

	TVector2();
	explicit TVector2( T x, T y );
	TVector2( const TVector2 &vec);
	TVector2( const TVector3<T> &vec);
	~TVector2() {};															// default destructor, does nothing

	void				Rotate(T rad);

	T					Dot( const TVector2 &vec) const;					// dot product of two vectors
	T					PerpDot( const TVector2& vec ) const;

	void				Normalize(void);									// normalize this vector
	TVector2			Normal() const;

	T					Mag(void) const;									// return magnitude of this vector
	T					Length(void) const { return Mag(); }
	T					MagSquared(void) const;								// return magnitude of this vector squared

	void				Lerp( const TVector2 &from, const TVector2 &to, T par );
	void				Serp( const TVector2 & PA, const TVector2 & PB, const TVector2 & PC, const TVector2 & PD, T Par );

	T					GetX(void) const { return (m_x); }
	T					GetY(void) const { return (m_y); }

	void			 	Set(T x, T y) { m_x = x; m_y = y;}
	void			 	SetX(T x) { m_x = x; }
	void				SetY(T y) { m_y = y; }

	static	TVector2	Zero(void) { return TVector2(T(0), T(0)); }

	inline T &operator[]( U32 i )
	{
		T *v = & m_x;
		OrkAssert( i<2 );
		return v[i];
	}

	inline TVector2 operator-() const
	{
		return TVector2( -m_x, -m_y );
	}

	inline TVector2 operator+( const TVector2 &b ) const
	{
		return TVector2( (m_x+b.m_x), (m_y+b.m_y) );
	}

	inline TVector2 operator*( const TVector2 &b ) const
	{
		return TVector2( (m_x*b.m_x), (m_y*b.m_y) );
	}

	inline TVector2 operator*( T scalar ) const
	{
		return TVector2( (m_x*scalar), (m_y*scalar) );
	}

	inline TVector2 operator-( const TVector2 &b ) const
	{
		return TVector2( (m_x-b.m_x), (m_y-b.m_y) );
	}

	inline TVector2 operator/( const TVector2 &b ) const
	{
		return TVector2( (m_x/b.m_x), (m_y/b.m_y) );
	}

	inline TVector2 operator/( T scalar ) const
	{
		return TVector2( (m_x/scalar), (m_y/scalar) );
	}

	inline void operator+=( const TVector2 & b )
	{
		m_x+=b.m_x;
		m_y+=b.m_y;
	}

	inline void operator-=( const TVector2 & b )
	{
		m_x-=b.m_x;
		m_y-=b.m_y;
	}

	inline void operator*=( T scalar )
	{
		m_x*=scalar;
		m_y*=scalar;
	}

	inline void operator*=( const TVector2 & b )
	{
		m_x*=b.m_x;
		m_y*=b.m_y;
	}

	inline void operator/=( const TVector2 &b )
	{
		m_x/=b.m_x;
		m_y/=b.m_y;
	}

	inline void operator/=( T scalar )
	{
		m_x/=scalar;
		m_y/=scalar;
	}

	inline bool operator==( const TVector2 &b ) const
	{
		return ( m_x == b.m_x && m_y == b.m_y );
	}
	inline bool operator!=( const TVector2 &b ) const
	{
		return ( m_x != b.m_x || m_y != b.m_y );
	}

	T *GetArray( void ) const { return const_cast<T*>( & m_x ); }

	/*template <typename U>
	static TVector2 FromTVector2(TVector2<U> vec)
	{
		return TVector2(T::FromFX(vec.GetX().FXCast()),
						T::FromFX(vec.GetY().FXCast()));
	}*/

protected:

	T					m_x; // x component of this vector
	T					m_y; // y component of this vector

};

typedef TVector2<CReal> CVector2;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////

template <typename T>
inline ork::TVector2<T> operator*( T scalar, const ork::TVector2<T> &b )
{
	return ork::TVector2<T>( (scalar*b.GetX()), (scalar*b.GetY()) );
}

///////////////////////////////////////////////////////////////////////////////

#endif
