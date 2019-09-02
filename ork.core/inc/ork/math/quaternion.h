////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef __QUATERNION_H__
#define __QUATERNION_H__

#include <ork/config/config.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork{
///////////////////////////////////////////////////////////////////////////////

struct QuatCodec
{
	unsigned int	milargest : 2;
	unsigned int	miwsign	  : 1;
	int				miElem0   : 16;
	int				miElem1   : 16;
	int				miElem2   : 16;

	QuatCodec() : milargest(0), miElem0(0), miElem1(0), miElem2( 0 ), miwsign( 0 ) {}
};
				
///////////////////////////////////////////////////////////////////////////////
template <typename T> class Matrix44;
template <typename T> class Matrix33;
template <typename T> class Vector4;
template <typename T> class Vector3;

template <typename T> class  TQuaternion
{
	public:
		
		/////////
		
	TQuaternion(void)
	{
		Identity();
	}
	
	TQuaternion(T x, T y, T z, T w);

	TQuaternion(const Matrix44<T> &matrix);
	
	~TQuaternion()
	{
	}

	/////////
	
	const T& GetX() const { return (m_v[0]); }
	const T& GetY() const { return (m_v[1]); }
	const T& GetZ() const { return (m_v[2]); }
	const T& GetW() const { return (m_v[3]); }

	/////////
	
	void SetX(T x) { m_v[0]=x; }
	void SetY(T y) { m_v[1]=y; }
	void SetZ(T z) { m_v[2]=z; }
	void SetW(T w) { m_v[3]=w; }
	
	/////////
	
	void			FromMatrix(const Matrix44<T> &matrix);
	Matrix44<T>		ToMatrix(void) const;

	void			FromMatrix3(const Matrix33<T> &matrix);
	Matrix33<T>		ToMatrix3(void) const;

	void			Scale(T scalar);
	TQuaternion		Multiply(const TQuaternion &q) const;
	void			Divide(TQuaternion &a);
	void			Sub(TQuaternion &a);
	void			Add(TQuaternion &a);
	
	Vector4<T>		ToAxisAngle(void) const;
	void			FromAxisAngle( const Vector4<T> &v );
	
	T				Magnitude(void);
	TQuaternion		Conjugate(TQuaternion &a);
	TQuaternion		Square(void);
	TQuaternion		Negate(void);

	void Normalize();

	static TQuaternion Lerp(  const TQuaternion &a , const TQuaternion &b, T alpha );
	static TQuaternion Slerp( const TQuaternion &a , const TQuaternion &b, T alpha );

	void			Identity(void);
	void			ShortestRotationArc( Vector4<T> v0, Vector4<T> v1 );

	/////////
	
	void			dump( void );

	QuatCodec		Compress( void ) const ;
	void			DeCompress( QuatCodec qc );

	protected:
	
	T	m_v[4];	// x,y,z,w component of this quaternion

};

typedef TQuaternion<float> CQuaternion;
typedef TQuaternion<float> fquat;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork
#endif
