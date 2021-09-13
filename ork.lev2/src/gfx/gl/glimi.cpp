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

GlImiInterface::GlImiInterface( ContextGL& target )
	: ImmInterface( target )
{
}

///////////////////////////////////////////////////////////////////////////////

void GlImiInterface::DrawPrim( const fvec4 *Points, int inumpoints, PrimitiveType eType )
{
/*	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	int inumpasses = mTarget.currentMaterial()->BeginBlock(&mTarget);

	for( int ipass=0; ipass<inumpasses; ipass++ )
	{
		bool bDRAW = mTarget.currentMaterial()->BeginPass( &mTarget,ipass );

		if( bDRAW )
		{
			switch( eType )
			{
				case PrimitiveType::QUADS:
				{
					glBegin( GL_QUADS );
					for( int itri=0; itri<inumpoints; itri++ )
					{
						int ibase = itri*4;
						glVertex3fv( (float*)Points[ibase+0].asArray() );
						glVertex3fv( (float*)Points[ibase+1].asArray() );
						glVertex3fv( (float*)Points[ibase+2].asArray() );
						glVertex3fv( (float*)Points[ibase+3].asArray() );

					}
					glEnd();
					break;
				}

				case PrimitiveType::TRIANGLES:
				{
					glBegin( GL_TRIANGLES );
					for( int itri=0; itri<inumpoints; itri++ )
					{
						int ibase = itri*3;
						glVertex3fv( (float*)Points[ibase+0].asArray() );
						glVertex3fv( (float*)Points[ibase+1].asArray() );
						glVertex3fv( (float*)Points[ibase+2].asArray() );

					}
					glEnd();

					break;
				}
			}
		}
	}*/
}

void GlImiInterface::DrawLine( const fvec4 &From, const fvec4 &To )
{
/*	glBindBuffer( GL_ARRAY_BUFFER_ARB, 0 );
	int inumpasses = mTarget.currentMaterial()->BeginBlock(&mTarget);

	for( int ipass=0; ipass<inumpasses; ipass++ )
	{
		bool bDRAW = mTarget.currentMaterial()->BeginPass( &mTarget,ipass );

		if( bDRAW )
		{
			glBegin( GL_LINES );
				glVertex3fv( (float*)const_cast<fvec4&>(From).asArray() );
				glVertex3fv( (float*)const_cast<fvec4&>(To).asArray() );
			glEnd();
		}
		mTarget.currentMaterial()->EndPass(&mTarget);

	}

	mTarget.currentMaterial()->EndBlock(&mTarget);*/
}

void GlImiInterface::DrawPoint( F32 fx, F32 fy, F32 fz )
{
	/*glBindBuffer( GL_ARRAY_BUFFER_ARB, 0 );
	int inumpasses = mTarget.currentMaterial()->BeginBlock(&mTarget);

	for( int ipass=0; ipass<inumpasses; ipass++ )
	{
		bool bDRAW = mTarget.currentMaterial()->BeginPass( &mTarget, ipass );

		if( bDRAW )
		{
			glPointSize( 3.0f );
			glBegin( GL_POINTS );
				glVertex3f( fx, fy, fz );
			glEnd();
		}

		mTarget.currentMaterial()->EndPass(&mTarget);
	}

	mTarget.currentMaterial()->EndBlock(&mTarget);*/
}

} }

