////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include "gl.h"

#include <ork/lev2/ui/ui.h>

namespace ork { namespace lev2 {

GlMatrixStackInterface::GlMatrixStackInterface( Context& target )
	: MatrixStackInterface(target)
{
}

fmtx4 GlMatrixStackInterface::Frustum( float left, float right, float top, float bottom, float zn, float zf ) // virtual
{
	fmtx4 rval;

	if( 1 ) // GL3 core
	{
		rval.SetToIdentity();

		const float two_near_dist = 2.0f * zn;
		const float right_minus_left = right - left;
		const float top_minus_bottom = top - bottom;
		const float far_minus_near = zf - zn;
		
		const float m00 = two_near_dist / right_minus_left;
		const float m02 = (right + left) / right_minus_left;
		const float m11 = two_near_dist / top_minus_bottom;
		const float m12 = (top + bottom) / top_minus_bottom;
		const float m22 = -(zf + zn) / far_minus_near;
		const float m23 = -(2.0f * zf * zn) / far_minus_near;
		const float m32 = -1.0f;
		

		rval.SetRow(0, fvec4(m00, 0.0f, m02, 0.0f) );
		rval.SetRow(1, fvec4(0.0f, m11, m12, 0.0f) );
		rval.SetRow(2, fvec4(0.0f, 0.0f, m22, m23) );
		rval.SetRow(3, fvec4(0.0f, 0.0f, m32, 0.0f) );
	}
	else
	{	/*
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glFrustum(left,right,bottom,top,zn,zf);
		glGetFloatv(GL_PROJECTION_MATRIX, rval.GetArray() );
		GL_ERRORCHECK();*/
//		rval.Transpose();
	}

/*	rval.SetCol(0, fvec4(m00, 0.0f, m02, 0.0f) );
	rval.SetRow(1, fvec4(0.0f, m11, m12, 0.0f) );
	rval.SetRow(2, fvec4(0.0f, 0.0f, m22, m23) );
	rval.SetRow(3, fvec4(0.0f, 0.0f, m32, 0.0f) );*/

	return rval;
}

fmtx4 GlMatrixStackInterface::Ortho( float left, float right, float top, float bottom, float fnear, float ffar )
{

	fmtx4 rval;
	
	if(1)
	{
		float zero(0.0f);
		float one(1.0f);
		float two(2.0f);

		float invWidth = one / float(right - left);
		float invHeight = one / float(top - bottom);
		float invDepth = one / float(ffar - fnear);
		float fScaleX = two * invWidth;
		float fScaleY = two * invHeight;
		float fScaleZ = -two * invDepth;
		float TransX = -float(right + left) * invWidth;
		float TransY = -float(top + bottom) * invHeight;
		float TransZ = -float(ffar + fnear) * invDepth;


		rval.SetElemYX( 0,0, fScaleX );
		rval.SetElemYX( 1,0, zero );
		rval.SetElemYX( 2,0, zero );
		rval.SetElemYX( 3,0, zero );

		rval.SetElemYX( 0,1, zero );
		rval.SetElemYX( 1,1, fScaleY );
		rval.SetElemYX( 2,1, zero );
		rval.SetElemYX( 3,1, zero );

		rval.SetElemYX( 0,2, zero );
		rval.SetElemYX( 1,2, zero );
		rval.SetElemYX( 2,2, fScaleZ );
		rval.SetElemYX( 3,2, zero );

		rval.SetElemYX( 0,3, TransX );
		rval.SetElemYX( 1,3, TransY );
		rval.SetElemYX( 2,3, TransZ );
		rval.SetElemYX( 3,3, one );
	}
	else
	{	/*
		if( right==left )
		{	assert(false);
			right = left+1.0;
		}
		if( top==bottom )
			top = bottom+1.0;
		if( ffar==fnear )
			ffar = fnear+1.0;

		GL_ERRORCHECK();
		glMatrixMode(GL_PROJECTION);
		GL_ERRORCHECK();
		glLoadIdentity();
		GL_ERRORCHECK();
		printf( "l<%f> r<%f> b<%f> t<%f> n<%f> f<%f>\n", left,right,bottom,top,fnear,ffar);
		glOrtho(left,right,bottom,top,fnear,ffar);
		GL_ERRORCHECK();
		glGetFloatv(GL_PROJECTION_MATRIX, rval.GetArray() );
		GL_ERRORCHECK();*/
//	
	}

	return rval;
}

} }

