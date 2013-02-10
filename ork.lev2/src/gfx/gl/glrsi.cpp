////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include "gl.h"

#include <ork/math/cmatrix4.h>
#include <ork/math/quaternion.h>

//#include <UI/UI.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {


void GlRasterStateInterface::BindRasterState( SRasterState const &refNewState, bool bForce )
{
	bForce = true;

	SRasterState rNewState = refNewState;

	SRasterState overridden;
	GetOverrideMergedRasterState(rNewState, overridden);

	SRasterState &rLast = mLastState;
	
	bool bAlphaTestChanged =		(	overridden.GetAlphaTest()	!=	rLast.GetAlphaTest()		);
//	bool bTextureModeChanged =		(	overridden.GetTextureMode()	!=	rLast.GetTextureMode()		);
//	bool bTextureActiveChanged =	(	overridden.GetTextureActive()!=	rLast.GetTextureActive()	);
	bool bBlendingChanged =			(	overridden.GetBlending()		!=	rLast.GetBlending()			);
	bool bDepthTestChanged =		(	overridden.GetDepthTest()	!=	rLast.GetDepthTest()		);
	//bool bStencilModeChanged =		(	overridden.GetStencilID()	!=	rLast.GetStencilID()		);
	bool bShadeModelChanged =		(	overridden.GetShadeModel()	!=	rLast.GetShadeModel()		);
	bool bCullTestChanged =			(	overridden.GetCullTest()		!=	rLast.GetCullTest()			);
	bool bScissorTestChanged =		(	overridden.GetScissorTest()	!=	rLast.GetScissorTest()		);
	
	GL_ERRORCHECK();

	if( true )
	{
		glDisable( GL_CULL_FACE );
	}
	else if( bCullTestChanged || bForce )
	{
		switch( overridden.GetCullTest() )
		{
			case ECULLTEST_OFF:
				glDisable( GL_CULL_FACE );
				break;
			case ECULLTEST_PASS_FRONT:
				glCullFace( GL_BACK );
				glFrontFace( GL_CCW );
				glEnable( GL_CULL_FACE );
				break;
			case ECULLTEST_PASS_BACK:
				glCullFace( GL_FRONT );
				glFrontFace( GL_CCW );
				glEnable( GL_CULL_FACE );
				break;
		}
	}

	//////////////////////////////
	/*#if( _BUILD_LEVEL > 1 )
	if( IsPickState() )
	{
		bBlendingChanged = true;
		bDepthTestChanged = true;
		bShadeModelChanged = true;
		//bTextureModeChanged = true;
		bAlphaTestChanged = true;

		overridden.SetBlending( EBLENDING_OFF );
		overridden.SetAlphaTest( EALPHATEST_OFF, 0 );
		overridden.SetDepthTest( EDEPTHTEST_LEQUALS );
		overridden.SetShadeModel( ESHADEMODEL_FLAT );
		//overridden.SetTextureMode( ETEXMODE_OFF );
	}
	#endif*/
	//////////////////////////////

	GL_ERRORCHECK();
				
	/////////////////////////////////////////////////

	glDepthMask( overridden.GetZWriteMask() );
	glColorMask( overridden.GetRGBWriteMask(), overridden.GetRGBWriteMask(), overridden.GetRGBWriteMask(), overridden.GetAWriteMask() );

//	glDepthMask( GL_TRUE );
//	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );

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

		F32 fAlphaRef = frecip * (F32) overridden.muAlphaRef;

		switch( overridden.muAlphaTest )
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

	if( false )
	{
		glDisable( GL_DEPTH_TEST );
	}
	else if( bDepthTestChanged || bForce )
	{	
		GL_ERRORCHECK();
		switch( overridden.GetDepthTest() )
		{	
			case EDEPTHTEST_OFF:
				glDisable( GL_DEPTH_TEST );
				break;
			case EDEPTHTEST_LESS:
				glEnable( GL_DEPTH_TEST );
				glDepthFunc( GL_LESS );
				break;
			case EDEPTHTEST_LEQUALS:
				glEnable( GL_DEPTH_TEST );
				glDepthFunc( GL_LEQUAL );
				break;
			case EDEPTHTEST_GREATER:
				glEnable( GL_DEPTH_TEST );
				glDepthFunc( GL_GREATER );
				break;
			case EDEPTHTEST_GEQUALS:
				glEnable( GL_DEPTH_TEST );
				glDepthFunc( GL_GEQUAL );
				break;
			case EDEPTHTEST_EQUALS:
				glEnable( GL_DEPTH_TEST );
				glDepthFunc( GL_EQUAL );
				break;
			case EDEPTHTEST_ALWAYS:
				glEnable( GL_DEPTH_TEST );
				glDepthFunc( GL_ALWAYS );
				break;
			default:
				OrkAssert( false );
				break;
		}
		GL_ERRORCHECK();
	}

	/////////////////////////////////////////////////

	if( false )
	{
		glDisable( GL_SCISSOR_TEST );
	}
	else if( bScissorTestChanged || bForce )
	{
		GL_ERRORCHECK();
		switch( overridden.GetScissorTest() )
		{
			case ESCISSORTEST_OFF:
//				glDisable( GL_SCISSOR_TEST );
				break;
			case ESCISSORTEST_ON:
//				glEnable( GL_SCISSOR_TEST );
				break;
		}
		GL_ERRORCHECK();
	}
	
	/////////////////////////////////////////////////

	if( false )
	{
		glDisable( GL_BLEND );
	}
	else if( bBlendingChanged || bForce )
	{
		//glDisable( GL_BLEND );
		//glEnable( GL_BLEND );
		//glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

		switch( overridden.GetBlending() )
		{

			case EBLENDING_OFF:
				glDisable( GL_BLEND );
				break;
			case EBLENDING_DSTALPHA:
				glEnable( GL_BLEND );
				glBlendFunc( GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA );
				break;
			case EBLENDING_PREMA:
				glEnable( GL_BLEND );
				glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
				//glBlendFunc( GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA );
				break;
			case EBLENDING_ALPHA:
				glEnable( GL_BLEND );
				glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
				//glBlendFunc( GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA );
				break;
			case EBLENDING_ADDITIVE:
				glEnable( GL_BLEND );
				glBlendFunc( GL_ONE, GL_ONE );
				break;
			case EBLENDING_SUBTRACTIVE:
				glEnable( GL_BLEND );
				glBlendFunc( GL_ZERO, GL_ONE_MINUS_SRC_COLOR );
				break;
			case EBLENDING_ALPHA_ADDITIVE:
				glEnable( GL_BLEND );
				glBlendFunc( GL_SRC_ALPHA, GL_ONE );
				break;
			case EBLENDING_ALPHA_SUBTRACTIVE:
				glEnable( GL_BLEND );
				glBlendFunc( GL_ZERO, GL_ONE_MINUS_SRC_ALPHA );
				break;
			case EBLENDING_MODULATE:
				glEnable( GL_BLEND );
				glBlendFunc( GL_ZERO, GL_SRC_ALPHA );
				break;
			default :
				OrkAssert( false );
				break;

		}
	}
	
	GL_ERRORCHECK();
	
	if( false )
	{
		//glShadeModel( GL_FLAT );
	}
	else if( bShadeModelChanged || bForce )
	{
		switch( overridden.GetShadeModel() )
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

	u32 upolyoffset = overridden.GetPolyOffset();

	if( false ) //0 == upolyoffset )
	{
		glDisable( GL_POLYGON_OFFSET_FILL );
	}
	else
	{
		glEnable( GL_POLYGON_OFFSET_FILL );
		glPolygonOffset( -1.0f, float(upolyoffset) );
	}
	GL_ERRORCHECK();
}

} } //namespace ork::lev2
