////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef ORK_MATRIX_INVERSE_GEMS_HPP_
#define ORK_MATRIX_INVERSE_GEMS_HPP_

#include <ork/math/cmatrix4.h>

// As psp-gcc does _not_ qualify sqrtf with std:: we must make CW 'using' it
#ifdef NITRO
using std::fabs;
#endif

#define SMALL_NUMBER	1.e-24f

namespace ork
{

///////////////////////////////////////////////////////////////////////////////

/*
 * F32 = det2x2( F32 a, F32 b, F32 c, F32 d )
 * 
 * calculate the determinant of a 2x2 matrix.
 */

template <typename T> T det2x2( T a, T b, T c, T d )
{
    T ans = a * d - b * c;
    return ans;
}

///////////////////////////////////////////////////////////////////////////////

/*
 * F32 = det3x3(  a1, a2, a3, b1, b2, b3, c1, c2, c3 )
 * 
 * calculate the determinant of a 3x3 matrix
 * in the form
 *
 *     | a1,  b1,  c1 |
 *     | a2,  b2,  c2 |
 *     | a3,  b3,  c3 |
 */

template <typename T>T det3x3( T a1, T a2, T a3, T b1, T b2, T b3, T c1, T c2, T c3 )
{
    T ans =	a1 * det2x2( b2, b3, c2, c3 )
				- b1 * det2x2( a2, a3, c2, c3 )
				+ c1 * det2x2( a2, a3, b2, b3 );

    return ans;
}

///////////////////////////////////////////////////////////////////////////////

/*
 * F32 = det4x4( matrix )
 * 
 * calculate the determinant of a 4x4 matrix.
 */

template <typename T> T det4x4( const TMatrix4<T> &m )
{
    // assign to individual variable names to aid selecting  correct elements

	//GetElemYX(0,0)

	T a1 = m.GetElemYX( 0, 0 );
	T b1 = m.GetElemYX( 0, 1 ); 
	T c1 = m.GetElemYX( 0, 2 ); 
	T d1 = m.GetElemYX( 0, 3 );

	T a2 = m.GetElemYX( 1, 0 ); 
	T b2 = m.GetElemYX( 1, 1 ); 
	T c2 = m.GetElemYX( 1, 2 ); 
	T d2 = m.GetElemYX( 1, 3 );

	T a3 = m.GetElemYX( 2, 0 ); 
	T b3 = m.GetElemYX( 2, 1 ); 
	T c3 = m.GetElemYX( 2, 2 ); 
	T d3 = m.GetElemYX( 2, 3 );

	T a4 = m.GetElemYX( 3, 0 ); 
	T b4 = m.GetElemYX( 3, 1 ); 
	T c4 = m.GetElemYX( 3, 2 ); 
	T d4 = m.GetElemYX( 3, 3 );

    T ans =	a1 * det3x3( b2, b3, b4, c2, c3, c4, d2, d3, d4)
				- b1 * det3x3( a2, a3, a4, c2, c3, c4, d2, d3, d4)
				+ c1 * det3x3( a2, a3, a4, b2, b3, b4, d2, d3, d4)
				- d1 * det3x3( a2, a3, a4, b2, b3, b4, c2, c3, c4);

	return ans;
}

///////////////////////////////////////////////////////////////////////////////

/* 
 *   adjoint( original_matrix, inverse_matrix )
 * 
 *     calculate the adjoint of a 4x4 matrix
 *
 *      Let  a   denote the minor determinant of matrix A obtained by
 *           ij
 *
 *      deleting the ith row and jth column from A.
 *
 *                    i+j
 *     Let  b   = (-1)    a
 *          ij            ji
 *
 *    The matrix B = (b  ) is the adjoint of A
 *                     ij
 */

template <typename T> void GEMSadjoint( const TMatrix4<T> &in, TMatrix4<T> &out )// Matrix4 *in; Matrix4 *out;
{
    T a1, a2, a3, a4, b1, b2, b3, b4;
    T c1, c2, c3, c4, d1, d2, d3, d4;

    /* assign to individual variable names to aid  */
    /* selecting correct values  */

	a1 = in.GetElemYX( 0, 0 );
	b1 = in.GetElemYX( 0, 1 ); 
	c1 = in.GetElemYX( 0, 2 ); 
	d1 = in.GetElemYX( 0, 3 );

	a2 = in.GetElemYX( 1, 0 );
	b2 = in.GetElemYX( 1, 1 ); 
	c2 = in.GetElemYX( 1, 2 );
	d2 = in.GetElemYX( 1, 3 );

	a3 = in.GetElemYX( 2, 0 );
	b3 = in.GetElemYX( 2, 1 );
	c3 = in.GetElemYX( 2, 2 );
	d3 = in.GetElemYX( 2, 3 );

	a4 = in.GetElemYX( 3, 0 );
	b4 = in.GetElemYX( 3, 1 ); 
	c4 = in.GetElemYX( 3, 2 );
	d4 = in.GetElemYX( 3, 3 );

    /* row column labeling reversed since we transpose rows & columns */

    out.SetElemYX( 0, 0,    det3x3( b2, b3, b4, c2, c3, c4, d2, d3, d4) );
    out.SetElemYX( 1, 0,  - det3x3( a2, a3, a4, c2, c3, c4, d2, d3, d4) );
    out.SetElemYX( 2, 0,    det3x3( a2, a3, a4, b2, b3, b4, d2, d3, d4) );
    out.SetElemYX( 3, 0,  - det3x3( a2, a3, a4, b2, b3, b4, c2, c3, c4) );
        
    out.SetElemYX( 0, 1,  - det3x3( b1, b3, b4, c1, c3, c4, d1, d3, d4) );
    out.SetElemYX( 1, 1,    det3x3( a1, a3, a4, c1, c3, c4, d1, d3, d4) );
    out.SetElemYX( 2, 1,  - det3x3( a1, a3, a4, b1, b3, b4, d1, d3, d4) );
    out.SetElemYX( 3, 1,    det3x3( a1, a3, a4, b1, b3, b4, c1, c3, c4) );
        
    out.SetElemYX( 0, 2,    det3x3( b1, b2, b4, c1, c2, c4, d1, d2, d4) );
    out.SetElemYX( 1, 2,  - det3x3( a1, a2, a4, c1, c2, c4, d1, d2, d4) );
    out.SetElemYX( 2, 2,    det3x3( a1, a2, a4, b1, b2, b4, d1, d2, d4) );
    out.SetElemYX( 3, 2,  - det3x3( a1, a2, a4, b1, b2, b4, c1, c2, c4) );
        
    out.SetElemYX( 0, 3,  - det3x3( b1, b2, b3, c1, c2, c3, d1, d2, d3) );
    out.SetElemYX( 1, 3,    det3x3( a1, a2, a3, c1, c2, c3, d1, d2, d3) );
    out.SetElemYX( 2, 3,  - det3x3( a1, a2, a3, b1, b2, b3, d1, d2, d3) );
    out.SetElemYX( 3, 3,    det3x3( a1, a2, a3, b1, b2, b3, c1, c2, c3) );
}

///////////////////////////////////////////////////////////////////////////////

/* 
 *   inverse( original_matrix, inverse_matrix )
 * 
 *    calculate the inverse of a 4x4 matrix
 *
 *     -1     
 *     A  = ___1__ adjoint A
 *         det A
 */


template <typename T> void GEMSMatrixInverse( const TMatrix4<T> &in, TMatrix4<T> &out )
{
    // calculate the adjoint matrix

    GEMSadjoint<T>( in, out );

    //	calculate the 4x4 determinant, if the determinant is zero then the inverse matrix is not unique

    T det = det4x4( in );

    if(CFloat::Abs(det) < T(SMALL_NUMBER))
	{
		OrkAssert( 0 );
		//fl_fatalerror( "Non-singular matrix, no inverse!\n" );
    }

    // scale the adjoint matrix to get the inverse

	for( int i=0; i<4; i++ )
    {	for( int j=0; j<4; j++ )
		{	out.SetElemYX( i, j,  out.GetElemYX( i, j ) / det );
		}
	}

}

template <typename T> void TMatrix4<T>::GEMSInverse( const TMatrix4<T> &in )
{
    TMatrix4<T> out;
	
    GEMSadjoint<T>( in, out ); // calculate the adjoint matrix
    T det = det4x4<T>( in ); //	calculate the 4x4 determinant, if the determinant is zero then the inverse matrix is not unique

    if(CFloat::Abs(det) < T(SMALL_NUMBER))
	{
        printf( "Non-singular matrix, no inverse!\n" );
        return;
    }

    // scale the adjoint matrix to get the inverse
	for( int i=0; i<4; i++ )
    {	for( int j=0; j<4; j++ )
		{	out.SetElemYX( i, j,  out.GetElemYX( i, j ) / det );
		}
	}

	*this = out;
}

} // namespace ork

#endif
