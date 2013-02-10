////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef __ORK_MATH_MATRIX3_H__
#define __ORK_MATH_MATRIX3_H__

///////////////////////////////////////////////////////////////////////////////

#include <ork/orktypes.h>
#include <ork/math/quaternion.h>

#include <ork/config/config.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork
{

template <typename T> class TVector4;
template <typename T> class TVector3;
template <typename T> class TQuaternion;

template <typename T> class  TMatrix3
{
	friend class TVector4<T>;

	public:

	typedef T value_type;

	////////////////

	T	elements[3][3];

	TMatrix3(const TMatrix3<T>& m)
	{
		for(int i = 0; i < 3; i++)
		{
			for(int j = 0; j < 3; j++)
			{
				elements[i][j] = m.elements[i][j];
			}
		}
	}

	////////////////

	TMatrix3(void)
	{	
		SetToIdentity();
	}

	~TMatrix3()
	{
	}

	/////////

	void SetToIdentity(void);

	/////////

	void RotateX( T rad );
	void RotateY( T rad );
	void RotateZ( T rad );
	void SetRotateX(T rad);
	void SetRotateY(T rad);
	void SetRotateZ(T rad);

	/////////

	void SetScale( const TVector4<T> &vec);
	void SetScale(T x, T y, T z);
	void SetScale(T s);
	void Scale(const TVector4<T> &vec);
	void Scale(T xscl, T yscl, T zscl);

	/////////

	void FromQuaternion(TQuaternion<T> quat);

	/////////

	TMatrix3<T> Mult( T scalar ) const;
	TMatrix3<T> MatrixMult( const TMatrix3<T> &mat1 ) const;

	inline TMatrix3<T> operator*( const TMatrix3<T> &mat ) const { return MatrixMult(mat); }

	void Transpose(void);
	void InverseTranspose();
	void Inverse( void );
	void Normalize( void );
	//void GEMSInverse( const TMatrix3<T> &in );

	void CorrectionMatrix( const TMatrix3<T> &from, const TMatrix3<T> &to );
	void SetRotation( const TMatrix3<T> &from );
	void SetScale( const TMatrix3<T> &from );

	void Lerp( const TMatrix3<T> &from, const TMatrix3<T> &to, T par ); // par 0.0f .. 1.0f

    void DecomposeMatrix( TQuaternion<T>& rot, T& Scale ) const;
    void ComposeMatrix( const TQuaternion<T>& rot, const T& Scale );

	////////////////

	void SetElemYX( int ix, int iy, T val );
	T GetElemYX( int ix, int iy ) const;
	void SetElemXY( int ix, int iy, T val );
	T GetElemXY( int ix, int iy ) const ;

	////////////////

	void dump( STRING name );

	inline bool operator==( const TMatrix3<T> &b ) const
	{
		bool beq = true;
		for( int i=0; i<3; i++ )
		{
			for( int j=0; j<3; j++ )
			{
				if( elements[i][j] != b.elements[i][j] )
				{
					beq = false;
				}
			}
		}
		return beq;
	}
	inline bool operator!=( const TMatrix3<T> &b ) const
	{
		bool beq = true;
		for( int i=0; i<3; i++ )
		{
			for( int j=0; j<3; j++ )
			{
				if( elements[i][j] != b.elements[i][j] )
				{
					beq = false;
				}
			}
		}
		return (false==beq);
	}

	///////////////////////////////////////////////////////////////////////////////
    // Column/Row Accessors
	///////////////////////////////////////////////////////////////////////////////

	TVector3<T> GetRow( int irow ) const;
	TVector3<T> GetColumn( int icol ) const;
	void SetRow( int irow, const TVector3<T>& v );
	void SetColumn( int icol, const TVector3<T>& v );

	TVector3<T> GetXNormal( void ) const { return GetColumn(0); }
	TVector3<T> GetYNormal( void ) const { return GetColumn(1); }
	TVector3<T> GetZNormal( void ) const { return GetColumn(2); }

	void NormalVectorsIn( const TVector3<T>& xv, const TVector3<T>& yv, const TVector3<T>& zv );
	void NormalVectorsOut( TVector3<T>& xv, TVector3<T>& yv, TVector3<T>& zv ) const;

    ///////////////////////////////////////////////////////////////////////////////

	static const TMatrix3<T> Identity;

    T *GetArray( void ) const { return (T*) & elements[0][0]; }

    ///////////////////////////////////////////////////////////////////////////////

};

typedef TMatrix3<CReal> CMatrix3;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork

//#include <ork/math/cmatrix4.hpp>

#endif
