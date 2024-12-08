////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
		rval.setToIdentity();

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
		

		rval.setRow(0, fvec4(m00, 0.0f, m02, 0.0f) );
		rval.setRow(1, fvec4(0.0f, m11, m12, 0.0f) );
		rval.setRow(2, fvec4(0.0f, 0.0f, m22, m23) );
		rval.setRow(3, fvec4(0.0f, 0.0f, m32, 0.0f) );
	}
	return rval;
}

fmtx4 GlMatrixStackInterface::Ortho( float left, float right, float top, float bottom, float fnear, float ffar )
{

	fmtx4 rval;
	
	if(1)
	{
		//rval = glm::ortho(left, right, bottom, top, fnear, ffar);

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

		rval[0,0] = fScaleX;
		rval[0,1] = zero;
		rval[0,2] = zero;
		rval[0,3] = zero;

		rval[1,0] = zero;
		rval[1,1] = fScaleY;
		rval[1,2] = zero;
		rval[1,3] = zero;

		rval[2,0] = zero;
		rval[2,1] = zero;
		rval[2,2] = fScaleZ;
		rval[2,3] = zero;

		rval[3,0] = TransX;
		rval[3,1] = TransY;
		rval[3,2] = TransZ;
		rval[3,3] = one;

  //rval.dump("ORTHO");

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
		glGetFloatv(GL_PROJECTION_MATRIX, rval.asArray() );
		GL_ERRORCHECK();*/
//	
	}

	return rval;
}

} }

