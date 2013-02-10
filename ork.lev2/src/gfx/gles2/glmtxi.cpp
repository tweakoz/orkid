////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#if defined( ORK_CONFIG_OPENGL )
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include "gl.h"

#include <ork/lev2/ui/ui.h>

namespace ork { namespace lev2 {

GlMatrixStackInterface::GlMatrixStackInterface( GfxTarget& target )
	: MatrixStackInterface(target)
{
}

CMatrix4 GlMatrixStackInterface::Frustum( float left, float right, float top, float bottom, float zn, float zf ) // virtual
{
	CMatrix4 rval;

	rval.SetToIdentity();
/*	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	{
		glLoadIdentity();
		glFrustum(	(GLdouble) left,
					(GLdouble) right,
					(GLdouble) bottom,
					(GLdouble) top,
					(GLdouble) zn,
					(GLdouble) zf );
	
		glGetFloatv( GL_PROJECTION_MATRIX, rval.GetArray() );
	}
	glPopMatrix();
*/
	return rval;
}

CMatrix4 GlMatrixStackInterface::Ortho( float left, float right, float top, float bottom, float fnear, float ffar )
{
	CReal zero(0.0f);
	CReal one(1.0f);
	CReal two(2.0f);

	CReal invWidth = one / CReal(right - left);
	CReal invHeight = one / CReal(top - bottom);
	CReal invDepth = one / CReal(ffar - fnear);
	CReal fScaleX = two * invWidth;
	CReal fScaleY = two * invHeight;
	CReal fScaleZ = -two * invDepth;
	CReal TransX = -CReal(right + left) * invWidth;
	CReal TransY = -CReal(top + bottom) * invHeight;
	CReal TransZ = -CReal(ffar + fnear) * invDepth;

	CMatrix4 rval;

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


	return rval;
}

} }

#endif
