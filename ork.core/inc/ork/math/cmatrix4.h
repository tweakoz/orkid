////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#ifndef __MATRIX_H__
#define __MATRIX_H__

#include <ork/orktypes.h>
#include <ork/math/quaternion.h>

#include <ork/config/config.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork
{

template <typename T> class TVector4;
template <typename T> class TVector3;
template <typename T> class TQuaternion;

template <typename T> class  TMatrix4
{
	friend class TVector4<T>;

	public:

	typedef T value_type;

	////////////////

	T	elements[4][4];

	TMatrix4(const TMatrix4<T>& m)
	{
		for(int i = 0; i < 4; i++)
		{
			for(int j = 0; j < 4; j++)
			{
				elements[i][j] = m.elements[i][j];
			}
		}
	}

	////////////////

	TMatrix4(void)
	{
		SetToIdentity();
	}

	~TMatrix4()
	{
	}

	/////////

	void SetToIdentity(void);

	/////////

	void SetTranslation( const TVector3<T> &vec);
	void SetTranslation(T x, T y, T z);
	TVector3<T> GetTranslation( void ) const;
	void Translate(const TVector4<T> &vec);
	void Translate(T vx,T vy, T vz );

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
	void CreateBillboard(TVector3<T> objectPos, TVector3<T> viewPos,
							TVector3<T> upVec);

	/////////

	TMatrix4<T> Mult( T scalar ) const;
	TMatrix4<T> MatrixMult( const TMatrix4<T> &mat1 ) const;

	inline TMatrix4<T> operator*( const TMatrix4<T> &mat ) const { return MatrixMult(mat); }

	TMatrix4<T> Concat43( const TMatrix4<T> &mat ) const;
	TMatrix4<T> Concat43Transpose( const TMatrix4<T> &mat ) const;

	void Transpose(void);
	void InverseTranspose();
	void Inverse( void );
	void Normalize( void );
	void GEMSInverse( const TMatrix4<T> &in );

	void CorrectionMatrix( const TMatrix4<T> &from, const TMatrix4<T> &to );
	void SetRotation( const TMatrix4<T> &from );
	void SetTranslation( const TMatrix4<T> &from );
	void SetScale( const TMatrix4<T> &from );

	void Lerp( const TMatrix4<T> &from, const TMatrix4<T> &to, T par ); // par 0.0f .. 1.0f

    void DecomposeMatrix( TVector3<T>& pos, TQuaternion<T>& rot, T& Scale ) const;
    void ComposeMatrix( const TVector3<T>& pos, const TQuaternion<T>& rot, const T& Scale );

	////////////////

	void SetElemYX( int ix, int iy, T val );
	T GetElemYX( int ix, int iy ) const;
	void SetElemXY( int ix, int iy, T val );
	T GetElemXY( int ix, int iy ) const ;

	////////////////

	void dump( const char* name ) const ;

	inline bool operator==( const TMatrix4<T> &b ) const
	{
		bool beq = true;
		for( int i=0; i<4; i++ )
		{
			for( int j=0; j<4; j++ )
			{
				if( elements[i][j] != b.elements[i][j] )
				{
					beq = false;
				}
			}
		}
		return beq;
	}
	inline bool operator!=( const TMatrix4<T> &b ) const
	{
		bool beq = true;
		for( int i=0; i<4; i++ )
		{
			for( int j=0; j<4; j++ )
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

	TVector4<T> GetRow( int irow ) const;
	TVector4<T> GetColumn( int icol ) const;
	void SetRow( int irow, const TVector4<T>& v );
	void SetColumn( int icol, const TVector4<T>& v );

	TVector3<T> GetXNormal( void ) const { return GetColumn(0).xyz(); }
	TVector3<T> GetYNormal( void ) const { return GetColumn(1).xyz(); }
	TVector3<T> GetZNormal( void ) const { return GetColumn(2).xyz(); }

	void fromNormalVectors( const TVector3<T>& xv, const TVector3<T>& yv, const TVector3<T>& zv );
	void toNormalVectors( TVector3<T>& xv, TVector3<T>& yv, TVector3<T>& zv ) const;

	///////////////////////////////////////////////////////////////////////////////

    void Perspective( T fovy, T aspect, T near, T far );
    void Frustum( T left, T right, T top, T bottom, T nearval, T farval );
    void LookAt( T eyex, T eyey, T eyez, T centerx, T centery, T centerz, T upx, T upy, T upz );
	void LookAt( const TVector3<T>& eye, const TVector3<T>& ctr, const TVector3<T>& up );
    void Ortho( T left, T right, T top, T bottom, T fnear, T ffar );

	static bool UnProject( const TMatrix4<T> &rIMVP, const TVector3<T>& ClipCoord, TVector3<T> &rVObj );
    static bool UnProject( const TVector4<T> &rVWin, const TMatrix4<T> &rIMVP, const SRect &rVP, TVector3<T> &rVObj );

    static const TMatrix4<T> Identity;

    T *GetArray( void ) const { return (T*) & elements[0][0]; }

    ///////////////////////////////////////////////////////////////////////////////

};

typedef TMatrix4<float> CMatrix4;
typedef TMatrix4<float> fmtx4;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork

//#include <ork/math/cmatrix4.hpp>

#endif
