////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <cmath>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>

namespace ork {

template <typename T> const TMatrix3<T> TMatrix3<T>::Identity;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::SetToIdentity(void)
{
	int	j,k;

	for (j=0;j<3;j++)
	{
		for (k=0;k<3;k++)
		{
			elements[j][k] = T( (j==k) );
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::dump( STRING name )
{
	orkprintf( "Matrix %p %s\n{	", this, name  );

	for( int i=0; i<4; i++ )
	{

		for( int j=0; j<4; j++ )
		{
			orkprintf( "%f ", elements[i][j] );
		}

		orkprintf( "\n	" );
	}

	orkprintf( "\n}\n" );

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::SetElemYX( int ix, int iy, T val )
{
	elements[iy][ix] = val;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> T TMatrix3<T>::GetElemYX( int ix, int iy ) const
{
	return elements[iy][ix];
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::SetElemXY( int ix, int iy, T val )
{
	elements[ix][iy] = val;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> T TMatrix3<T>::GetElemXY( int ix, int iy ) const
{
	return elements[ix][iy];
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//	set rotation explicitly

template <typename T> void TMatrix3<T>::SetRotateX(T rad)
{
	T cosa, sina;

	cosa = CFloat::Cos( rad );
	sina = CFloat::Sin( rad );

    elements[0][0] = 1.0f;
    elements[0][1] = 0.0f;
    elements[0][2] = 0.0f;

    elements[1][0] = 0.0f;
	elements[1][1] = cosa;
    elements[1][2] = sina;

    elements[2][0] = 0.0f;
	elements[2][1] = -sina;
    elements[2][2] = cosa;
}

///////////////////////////////////////////////////////////////////////////////
//	set rotation explicitly

template <typename T> void TMatrix3<T>::SetRotateY(T rad)
{
	T cosa, sina;

	cosa = CFloat::Cos( rad );
    sina = CFloat::Sin( rad );

	elements[0][0] = cosa;
    elements[0][1] = 0.0f;
    elements[0][2] = -sina;

    elements[1][0] = 0.0f;
    elements[1][1] = 1.0f;
    elements[1][2] = 0.0f;

	elements[2][0] = sina;
    elements[2][1] = 0.0f;
    elements[2][2] = cosa;
}

///////////////////////////////////////////////////////////////////////////////
//	set rotation explicitly

template <typename T> void TMatrix3<T>::SetRotateZ(T rad)
{
	T cosa, sina;

    cosa = CFloat::Cos( rad );
    sina = CFloat::Sin( rad );

    elements[0][0] = cosa;
    elements[0][1] = sina;
    elements[0][2] = 0.0f;

	elements[1][0] = -sina;
    elements[1][1] = cosa;
    elements[1][2] = 0.0f;

    elements[2][0] = 0.0f;
    elements[2][1] = 0.0f;
    elements[2][2] = 1.0f;
}

///////////////////////////////////////////////////////////////////////////////
//	rotate in place

template <typename T> void TMatrix3<T>::RotateX( T rad )
{
	TMatrix3<T> temp, res;
	temp.SetRotateX(rad);
	res = temp * *this;
	*this = res;
}

///////////////////////////////////////////////////////////////////////////////
//	rotate in place

template <typename T> void TMatrix3<T>::RotateY( T rad )
{
	TMatrix3<T> temp, res;
	temp.SetRotateY(rad);
	res = temp * *this;
	*this = res;
}

///////////////////////////////////////////////////////////////////////////////
//	rotate in place

template <typename T> void TMatrix3<T>::RotateZ( T rad )
{
	TMatrix3<T> temp, res;
	temp.SetRotateZ(rad);
	res = temp * *this;
	*this = res;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::SetScale( const TVector4<T> &vec)
{
	SetScale( vec.GetX(), vec.GetY(), vec.GetZ() );
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::SetScale(T x, T y, T z)
{
	SetElemXY( 0, 0, x );
	SetElemXY( 1, 0, 0.0f );
	SetElemXY( 2, 0, 0.0f );

	SetElemXY( 0, 1, 0.0f );
	SetElemXY( 1, 1, y );
	SetElemXY( 2, 1, 0.0f );

	SetElemXY( 0, 2, 0.0f );
	SetElemXY( 1, 2, 0.0f );
	SetElemXY( 2, 2, z );
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::SetScale(T s)
{
	SetScale( s,s,s );
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::Scale( const TVector4<T> &vec )
{
	TMatrix3<T> temp, res;
	temp.SetScale( vec );
	res = temp * *this;
	*this = res;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::Scale( T xscl, T yscl, T zscl )
{
	TMatrix3<T> temp, res;
	temp.SetScale( xscl, yscl, zscl );
	res = temp * *this;
	*this = res;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// sm - rotation matrix from quaternion
template <typename T> void TMatrix3<T>::FromQuaternion(TQuaternion<T> quat)
{
	T xx = quat.GetX() * quat.GetX();
	T yy = quat.GetY() * quat.GetY();
	T zz = quat.GetZ() * quat.GetZ();
	T xy = quat.GetX() * quat.GetY();
	T zw = quat.GetZ() * quat.GetW();
	T zx = quat.GetZ() * quat.GetX();
	T yw = quat.GetY() * quat.GetW();
	T yz = quat.GetY() * quat.GetZ();
	T xw = quat.GetX() * quat.GetW();

	elements[0][0] = T(1.0f) - (T(2.0f) * (yy + zz));
	elements[0][1] = T(2.0f) * (xy + zw);
	elements[0][2] = T(2.0f) * (zx - yw);
	elements[1][0] = T(2.0f) * (xy - zw);
	elements[1][1] = T(1.0f) - (T(2.0f) * (zz + xx));
	elements[1][2] = T(2.0f) * (yz + xw);
	elements[2][0] = T(2.0f) * (zx + yw);
	elements[2][1] = T(2.0f) * (yz - xw);
	elements[2][2] = T(1.0f) - (T(2.0f) * (yy + xx));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> void fpuM33xM33( const T a[3][3], const T b[3][3], T c[3][3] )
{
	const T *fa = & a[0][0];
	const T *fb = & b[0][0];
	T *fc = & c[0][0];

//    y  x
//    i  j      i  k    k  j      i  k    k  j      i  k    k  j      i  k    k  j

	fc[0] = fa[0]*fb[0] + fa[1]*fb[4] + fa[2]*fb[8] + fa[3]*fb[12];
	fc[1] = fa[0]*fb[1] + fa[1]*fb[5] + fa[2]*fb[9] + fa[3]*fb[13];
	fc[2] = fa[0]*fb[2] + fa[1]*fb[6] + fa[2]*fb[10] + fa[3]*fb[14];
	fc[3] = fa[0]*fb[3] + fa[1]*fb[7] + fa[2]*fb[11] + fa[3]*fb[15];

	fc[4] = fa[4]*fb[0] + fa[5]*fb[4] + fa[6]*fb[8] + fa[7]*fb[12];
	fc[5] = fa[4]*fb[1] + fa[5]*fb[5] + fa[6]*fb[9] + fa[7]*fb[13];
	fc[6] = fa[4]*fb[2] + fa[5]*fb[6] + fa[6]*fb[10] + fa[7]*fb[14];
	fc[7] = fa[4]*fb[3] + fa[5]*fb[7] + fa[6]*fb[11] + fa[7]*fb[15];

	fc[8] = fa[8]*fb[0] + fa[9]*fb[4] + fa[10]*fb[8] + fa[11]*fb[12];

}

///////////////////////////////////////////////////////////////////////////////

template <typename T> TMatrix3<T> TMatrix3<T>::MatrixMult( const TMatrix3<T> &mat1 ) const
{
	TMatrix3<T>	result;

	////////////////////////////////////////////////////////////////
	//              i  j                i  k                  k  j

	result.elements[0][0]	= (elements[0][0] * mat1.elements[0][0])
							+ (elements[0][1] * mat1.elements[1][0])
							+ (elements[0][2] * mat1.elements[2][0]);

	result.elements[0][1]	= (elements[0][0] * mat1.elements[0][1])
							+ (elements[0][1] * mat1.elements[1][1])
							+ (elements[0][2] * mat1.elements[2][1]);

	result.elements[0][2]	= (elements[0][0] * mat1.elements[0][2])
							+ (elements[0][1] * mat1.elements[1][2])
							+ (elements[0][2] * mat1.elements[2][2]);
														
	////////////////////////////////////////////////////////////////
	//              i  j                i  k                  k  j

	result.elements[1][0]	= (elements[1][0] * mat1.elements[0][0])
							+ (elements[1][1] * mat1.elements[1][0])
							+ (elements[1][2] * mat1.elements[2][0]);

	result.elements[1][1]	= (elements[1][0] * mat1.elements[0][1])
							+ (elements[1][1] * mat1.elements[1][1])
							+ (elements[1][2] * mat1.elements[2][1]);

	result.elements[1][2]	= (elements[1][0] * mat1.elements[0][2])
							+ (elements[1][1] * mat1.elements[1][2])
							+ (elements[1][2] * mat1.elements[2][2]);
							
	////////////////////////////////////////////////////////////////
	//              i  j                i  k                  k  j

	result.elements[2][0]	= (elements[2][0] * mat1.elements[0][0])
							+ (elements[2][1] * mat1.elements[1][0])
							+ (elements[2][2] * mat1.elements[2][0]);

	result.elements[2][1]	= (elements[2][0] * mat1.elements[0][1])
							+ (elements[2][1] * mat1.elements[1][1])
							+ (elements[2][2] * mat1.elements[2][1]);

	result.elements[2][2]	= (elements[2][0] * mat1.elements[0][2])
							+ (elements[2][1] * mat1.elements[1][2])
							+ (elements[2][2] * mat1.elements[2][2]);
							
	////////////////////////////////////////////////////////////////
	
	return( result );
}

template <typename T> TMatrix3<T> TMatrix3<T>::Mult( T scalar ) const
{
	TMatrix3<T> res;

	for(int i = 0; i < 3; ++i)
	{
		for(int j = 0; j < 3; ++j)
		{
			res.elements[i][j] = elements[i][j] * scalar;
		}
	}

	return res;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::CorrectionMatrix( const TMatrix3<T> &from, const TMatrix3<T> &to )
{
	/////////////////////////
	//
	//	GENERATE CORRECTION	TO GET FROM A to C
	//
	//	A * B = C				(A and C are known we dont know B)
	//	(A * iA) * B = iA * C
	//	B = iA * C				we now know B
	//
	/////////////////////////

	TMatrix3<T> iFrom = from;

	iFrom.Inverse();

	*this = (iFrom * to); // B
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::SetRotation( const TMatrix3<T> &from )
{
	TMatrix3<T> rval = from;
	rval.Normalize();
	*this = rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::SetScale( const TMatrix3<T> &from ) // assumes rot is zero!
{
	TMatrix3<T> RS = from;

	TMatrix3<T> R = RS;
	R.Normalize();

	TMatrix3<T> S;
	S.CorrectionMatrix( R, RS );

	*this = S;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::Lerp( const TMatrix3<T> &from, const TMatrix3<T> &to, T par ) // par 0.0f .. 1.0f
{
	TMatrix3<T> FromR, ToR, CORR, matR;
	TQuaternion<T> FromQ, ToQ, Qidn, Qrot;

	//////////////////

	FromR.SetRotation( from );	// froms ROTATION
	ToR.SetRotation( to );	// froms ROTATION

	FromQ.FromMatrix3( FromR );
	ToQ.FromMatrix3( ToR );

	CORR.CorrectionMatrix( from, to );	//CORR.Normalize();

	Qrot.FromMatrix3( CORR );

	TQuaternion<T>	dQ = Qrot;
					dQ.Sub( Qidn );
					dQ.Scale( par );
					dQ.Add( Qidn );

					if( dQ.Magnitude() > T(0.0f) )
						dQ.Negate();
	
	TQuaternion<T>	newQrot = dQ;

	matR = newQrot.ToMatrix3();
	
	//////////////////

	*this = ( FromR * matR );

}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void TMatrix3<T>::DecomposeMatrix( TQuaternion<T>& qrot, T& Scale ) const
{
	TMatrix3<T> rot = *this;

	TVector3<T> UnitVector( T(1.0f), T(0.0f), T(0.0f) );
	TVector3<T> XFVector = UnitVector.Transform( rot );

	Scale = XFVector.Mag();

	for( int i=0; i<3; i++ )
	{
		for( int j=0; j<3; j++ )
		{
			rot.SetElemXY( i,j, rot.GetElemXY( i,j ) / Scale );
		}
	}

	qrot.FromMatrix3( rot );
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void TMatrix3<T>::ComposeMatrix( const TQuaternion<T>& qrot, const T& Scale )
{
	*this = qrot.ToMatrix3();
	for( int i=0; i<3; i++ )
	{
		for( int j=0; j<3; j++ )
		{
			SetElemYX( i,j, GetElemYX( i,j ) * Scale );
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> TVector3<T> TMatrix3<T>::GetRow( int irow ) const
{
    TVector3<T> out;
	out.SetX(GetElemXY(0,irow));
	out.SetY(GetElemXY(1,irow));
	out.SetZ(GetElemXY(2,irow));
    return out;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> TVector3<T> TMatrix3<T>::GetColumn( int icol ) const
{
    TVector3<T> out;
	out.SetX(GetElemXY(icol,0));
	out.SetY(GetElemXY(icol,1));
	out.SetZ(GetElemXY(icol,2));
    return out;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::SetRow( int irow, const TVector3<T>& v )
{
	SetElemXY( 0, irow, v.GetX() );
	SetElemXY( 1, irow, v.GetY() );
	SetElemXY( 2, irow, v.GetZ() );
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::SetColumn( int icol, const TVector3<T>& v )
{
	SetElemXY( icol, 0, v.GetX() );
	SetElemXY( icol, 1, v.GetY() );
	SetElemXY( icol, 2, v.GetZ() );
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::NormalVectorsIn( const TVector3<T>& xv, const TVector3<T>& yv, const TVector3<T>& zv )
{
	SetColumn( 0, xv );
	SetColumn( 1, yv );
	SetColumn( 2, zv );
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::NormalVectorsOut( TVector3<T>& xv, TVector3<T>& yv, TVector3<T>& zv ) const
{
	xv = GetColumn( 0 );
	yv = GetColumn( 1 );
	zv = GetColumn( 2 );
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::Transpose(void)
{
	TMatrix3<T> temp = *this;

	for( int i=0; i<3; i++ )
	{
		for( int j=0; j<3; j++ )
		{
			SetElemYX(i,j, temp.GetElemYX(j,i));
		}
	}

}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::Inverse( void )
{
	TMatrix3<T> result;

	/////////////
	// The rotational part of the matrix is simply the transpose of the original matrix.

	for( int i=0; i<=2; i++ )
	{
		for( int j=0; j<=2; j++ )
		{
			result.SetElemYX( i,j, GetElemYX( j, i ) );
		}
	}

	////////////

	*this = result;

}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::InverseTranspose( void )
{
	TMatrix3<T> result;

	/////////////
	// The rotational part of the matrix is simply the transpose of the original matrix.

	for( int i=0; i<=3; i++ )
	{
		for( int j=0; j<=3; j++ )
		{
			result.SetElemXY( i,j, GetElemYX( j, i ) );
		}
	}

	////////////
	
	*this = result;

}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void TMatrix3<T>::Normalize( void )
{
	TMatrix3<T> result;
	
	T Xx = GetElemXY( 0,0 );
	T Xy = GetElemXY( 0,1 );
	T Xz = GetElemXY( 0,2 );
	T Yx = GetElemXY( 1,0 );
	T Yy = GetElemXY( 1,1 );
	T Yz = GetElemXY( 1,2 );
	T Zx = GetElemXY( 2,0 );
	T Zy = GetElemXY( 2,1 );
	T Zz = GetElemXY( 2,2 );

	T Xi = T(1.0f) / CFloat::Sqrt( (Xx*Xx) + (Xy*Xy) + (Xz*Xz) );
	T Yi = T(1.0f) / CFloat::Sqrt( (Yx*Yx) + (Yy*Yy) + (Yz*Yz) );
	T Zi = T(1.0f) / CFloat::Sqrt( (Zx*Zx) + (Zy*Zy) + (Zz*Zz) );

	Xx *= Xi;
	Xy *= Xi;
	Xz *= Xi;
	
	Yx *= Yi;
	Yy *= Yi;
	Yz *= Yi;
	
	Zx *= Zi;
	Zy *= Zi;
	Zz *= Zi;

	result.SetElemXY( 0,0, Xx );
	result.SetElemXY( 0,1, Xy );
	result.SetElemXY( 0,2, Xz );

	result.SetElemXY( 1,0, Yx );
	result.SetElemXY( 1,1, Yy );
	result.SetElemXY( 1,2, Yz );

	result.SetElemXY( 2,0, Zx );
	result.SetElemXY( 2,1, Zy );
	result.SetElemXY( 2,2, Zz );

	*this = result;
}

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
