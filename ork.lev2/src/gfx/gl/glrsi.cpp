////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include "gl.h"

#include <ork/math/cmatrix4.h>
#include <ork/math/quaternion.h>

//#include <UI/UI.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

GlRasterStateInterface::GlRasterStateInterface( Context& target )
	: RasterStateInterface(target) {}

void GlRasterStateInterface::SetZWriteMask( bool bv )
{
	GLenum zmask = bv ? GL_TRUE : GL_FALSE;
	glDepthMask( zmask );
}
void GlRasterStateInterface::SetRGBAWriteMask( bool rgb, bool a )
{
	_curmask._r = rgb;
	_curmask._g = rgb;
	_curmask._b = rgb;
	_curmask._a = a;

	GLenum rgbmask = rgb ? GL_TRUE : GL_FALSE;
	GLenum amask = a ? GL_TRUE : GL_FALSE;
	glColorMask( rgbmask,
				 rgbmask,
				 rgbmask,
				 amask );
}
RGBAMask GlRasterStateInterface::SetRGBAWriteMask( const RGBAMask& newmask)
{
	RGBAMask oldmask = _curmask;
	_curmask = newmask;

	glColorMask( newmask._r ? GL_TRUE : GL_FALSE,
				 newmask._g ? GL_TRUE : GL_FALSE,
				 newmask._b ? GL_TRUE : GL_FALSE,
				 newmask._a ? GL_TRUE : GL_FALSE );

	return oldmask;
}
void GlRasterStateInterface::SetBlending( Blending eVal )
{
	switch( eVal )
	{

		case Blending::OFF:
			glDisable( GL_BLEND );
			break;
		case Blending::DSTALPHA:
			glEnable( GL_BLEND );
			glBlendFunc( GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA );
			break;
		case Blending::PREMA:
			glEnable( GL_BLEND );
			glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
			//glBlendFunc( GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA );
			break;
		case Blending::ALPHA:
			glEnable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			//glBlendFunc( GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA );
			break;
		case Blending::ADDITIVE:
			glEnable( GL_BLEND );
			glBlendFunc( GL_ONE, GL_ONE );
			break;
		case Blending::SUBTRACTIVE:
			glEnable( GL_BLEND );
			glBlendFunc( GL_ZERO, GL_ONE_MINUS_SRC_COLOR );
			break;
		case Blending::ALPHA_ADDITIVE:
			glEnable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE );
			break;
		case Blending::ALPHA_SUBTRACTIVE:
			glEnable( GL_BLEND );
			glBlendFunc( GL_ZERO, GL_ONE_MINUS_SRC_ALPHA );
			break;
		case Blending::MODULATE:
			glEnable( GL_BLEND );
			glBlendFunc( GL_ZERO, GL_SRC_ALPHA );
			break;
		default :
		  printf( "BAD RASTERSTATE<%zx>\n", size_t(eVal) );
			OrkAssert( false );
			break;

	}
}
void GlRasterStateInterface::SetDepthTest( EDepthTest eVal )
{
	GL_ERRORCHECK();
	switch( eVal )
	{
		case EDepthTest::OFF:
			glDisable( GL_DEPTH_TEST );
			break;
		case EDepthTest::LESS:
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_LESS );
			break;
		case EDepthTest::LEQUALS:
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_LEQUAL );
			break;
		case EDepthTest::GREATER:
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_GREATER );
			break;
		case EDepthTest::GEQUALS:
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_GEQUAL );
			break;
		case EDepthTest::EQUALS:
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_EQUAL );
			break;
		case EDepthTest::ALWAYS:
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_ALWAYS );
			break;
		default:
			OrkAssert( false );
			break;
	}
	GL_ERRORCHECK();
}
void GlRasterStateInterface::SetCullTest( ECullTest eVal )
{
	GL_ERRORCHECK();
	switch( eVal )
	{
		case ECullTest::OFF:
			glDisable( GL_CULL_FACE );
			break;
		case ECullTest::PASS_FRONT:
			glCullFace( GL_BACK );
			glFrontFace( GL_CCW );
			glEnable( GL_CULL_FACE );
			break;
		case ECullTest::PASS_BACK:
			glCullFace( GL_FRONT );
			glFrontFace( GL_CCW );
			glEnable( GL_CULL_FACE );
			break;
	}
	GL_ERRORCHECK();
}
void GlRasterStateInterface::setScissorTest( EScissorTest eVal )
{	GL_ERRORCHECK();
	switch( eVal )
	{
		case ESCISSORTEST_OFF:
//			glDisable( GL_SCISSOR_TEST );
			break;
		case ESCISSORTEST_ON:
//			glEnable( GL_SCISSOR_TEST );
			break;
	}
	GL_ERRORCHECK();

}

void GlRasterStateInterface::BindRasterState( SRasterState const &newstate, bool bForce )
{
	bForce = true;
	bool bAlphaTestChanged =	(newstate.GetAlphaTest()	!=	mLastState.GetAlphaTest()		);
//	bool bTextureModeChanged =	(newstate.GetTextureMode()	!=	rLast.GetTextureMode()		);
//	bool bTextureActiveChanged =(newstate.GetTextureActive()!=	rLast.GetTextureActive()	);
	bool bBlendingChanged =		(newstate.GetBlending()		!=	mLastState.GetBlending()			);
	bool bDepthTestChanged =	(newstate.GetDepthTest()	!=	mLastState.GetDepthTest()		);
	//bool bStencilModeChanged =(newstate.GetStencilID()	!=	rLast.GetStencilID()		);
	bool bShadeModelChanged =	(newstate.GetShadeModel()	!=	mLastState.GetShadeModel()		);
	bool bCullTestChanged =		(newstate.GetCullTest()		!=	mLastState.GetCullTest()			);
	bool bScissorTestChanged =	(newstate.GetScissorTest()	!=	mLastState.GetScissorTest()		);

	GL_ERRORCHECK();

	if( bCullTestChanged || bForce )
	{
		SetCullTest( newstate.GetCullTest() );
	}

	/////////////////////////////////////////////////

	SetZWriteMask( newstate.GetZWriteMask() );
	SetRGBAWriteMask( newstate.GetRGBWriteMask(), newstate.GetAWriteMask() );

	/////////////////////////////////////////////////

	if( true )
	{
		glDisable( GL_STENCIL_TEST );
	}

	/////////////////////////////////////////////////
	//	Win32 GL Alpha

	if( false )
	{
	//	glDisable( GL_ALPHA_TEST );
	}
	else if( bAlphaTestChanged || bForce )
	{
		static const F32 frecip = 1.0f / 15.0f;

		F32 fAlphaRef = frecip * (F32) newstate.muAlphaRef;

		switch( newstate.muAlphaTest )
		{	case EALPHATEST_OFF:
				//glDisable( GL_ALPHA_TEST );
				GL_ERRORCHECK();
				break;
			case EALPHATEST_GREATER:
				//glEnable( GL_ALPHA_TEST );
				//glAlphaFunc( GL_GREATER, fAlphaRef );
				GL_ERRORCHECK();
				break;
			case EALPHATEST_LESS:
				//glEnable( GL_ALPHA_TEST );
				//glAlphaFunc( GL_LESS, fAlphaRef );
				GL_ERRORCHECK();
				break;
		}
	}

	/////////////////////////////////////////////////
	//	Win32 GL Depth

	if(1)// bDepthTestChanged || bForce )
		SetDepthTest( newstate.GetDepthTest() );

	/////////////////////////////////////////////////

	if( bScissorTestChanged || bForce )
	{
		setScissorTest( newstate.GetScissorTest() );
	}

	/////////////////////////////////////////////////

	if( bBlendingChanged || bForce )
	{
		SetBlending( newstate.GetBlending() );
	}

	GL_ERRORCHECK();

	if( false )
	{
		//glShadeModel( GL_FLAT );
	}
	else if( bShadeModelChanged || bForce )
	{
		switch( newstate.GetShadeModel() )
		{
			case ESHADEMODEL_FLAT:
				//glShadeModel( GL_FLAT );
				break;
			case ESHADEMODEL_SMOOTH:
				//glShadeModel( GL_SMOOTH );
				break;
			default:
				break;
		}
	}
	GL_ERRORCHECK();

}

} } //namespace ork::lev2
