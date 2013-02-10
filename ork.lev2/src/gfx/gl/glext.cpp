////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#if defined( ORK_CONFIG_OPENGL ) && ! defined(ORK_OSX) && ! defined(IX)

#include <ork/lev2/gfx/gfxenv.h>
#include "gl.h"
#include "glfxi.h"

#include <ork/lev2/ui/ui.h>

#include <GLEW/glew.h>
#if defined( _WIN32 ) || defined(IX)
#include <GLEW/glew.c>
bool ork::lev2::GfxTargetGL::HaveGLExtension( const std::string & extname )
{	return glewGetExtension( extname.c_str() );
}
#elif defined( _LINUX ) || defined( ORK_OSX )
#include <GLEW/glew.c>
bool GfxTargetGL::HaveGLExtension( const std::string & extname )
{	bool bval = glewGetExtension( extname.c_str() );
	orkprintf( "Extension [%s] %d\n", extname.c_str(), bval );
	return bval;
}
#endif

#include <OpenGL/OpenGL.h>

namespace ork { namespace lev2 {

extern CGLContextObj gOGLdefaultctx;

EOpenGLRenderPath GfxTargetGL::geRenderPath = EGLRPATH_ARBVP;


GLenum						GfxTargetGL::geFBOSupport = GfxTargetGL::kNoFBOSupport;
orkvector< std::string >	GfxTargetGL::gGLExtensions;
orkset< std::string >		GfxTargetGL::gGLExtensionSet;
U32							GfxTargetGL::gNumTexUnits;
bool						GfxTargetGL::gbUseVBO;
bool						GfxTargetGL::gbUseIBO;

///////////////////////////////////////////////////////////////////////////////

void GfxTargetGL::InitGLExt( void )
{
	static bool gbINIT = true;

	if( gbINIT )
	{
		gbINIT = false;

	# if defined( _WIN32 )
		mhDC = wglGetCurrentDC();
		mhGLRC = wglGetCurrentContext();

	#endif

		orkprintf( "////////////////////////////////////////////////\n" );
		orkprintf( "// Getting OpenGL Status\n" );
		orkprintf( "GL_VENDOR: %s\n", glGetString( GL_VENDOR ) );
		orkprintf( "GL_VERSION: %s\n", glGetString( GL_VERSION ) );
		orkprintf( "GL_RENDERER: %s\n", glGetString( GL_RENDERER ) );

		std::string glrendererstring = (std::string) (char*) glGetString( GL_RENDERER );
		transform( glrendererstring.begin(), glrendererstring.end(), glrendererstring.begin(), lower() );

		GLint ntu, zbits, sbits, abits;
		glGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, & ntu );
		glGetIntegerv( GL_ALPHA_BITS, & abits );
		glGetIntegerv( GL_DEPTH_BITS, & zbits );
		glGetIntegerv( GL_STENCIL_BITS, & sbits );
		gNumTexUnits = ntu;
		orkprintf( "NUMTEXUNITS	[%d]\n", gNumTexUnits );
		orkprintf( "ZBITS			[%d]\n", zbits );
		orkprintf( "SBITS			[%d]\n", sbits );
		orkprintf( "ABITS			[%d]\n", abits );

		bool IsGeforce = (-1!=glrendererstring.find((std::string)"geforce"));
		bool IsRadeon = (-1!=glrendererstring.find((std::string)"radeon" ));
		gbUseVBO = false;
		gbUseIBO = false;

		if( IsGeforce )
		{
			bool IsGFFX5200 = (-1!=glrendererstring.find((std::string)"geforce fx 5200"));
			bool IsGFFX5600 = (-1!=glrendererstring.find((std::string)"geforce fx 5600"));
			bool IsGFFX5700 = (-1!=glrendererstring.find((std::string)"geforce fx 5700"));
			bool IsGFFX5800 = (-1!=glrendererstring.find((std::string)"geforce fx 5800"));
			bool IsGFFX5900 = (-1!=glrendererstring.find((std::string)"geforce fx 5900"));
			bool IsGFFX5950 = (-1!=glrendererstring.find((std::string)"geforce fx 5950"));

			if( IsGFFX5200 || IsGFFX5600 || IsGFFX5700 || IsGFFX5800 || IsGFFX5900 || IsGFFX5950 )
			{
		//		mbUseVBO = true;
		//		mbUseIBO = true;
			}
		}
		else if( IsRadeon )
		{
			bool IsRAD9500 = (-1!=glrendererstring.find((std::string)"9500"));
			bool IsRAD9600 = (-1!=glrendererstring.find((std::string)"9600"));
			bool IsRAD9700 = (-1!=glrendererstring.find((std::string)"9700"));
			bool IsRAD9800 = (-1!=glrendererstring.find((std::string)"9800"));

			if( IsRAD9500 || IsRAD9600 || IsRAD9700 || IsRAD9800 )
			{
				gbUseVBO = true;
				gbUseIBO = true;
			}
		}

		orkprintf( "////////////////////////////////////////////////\n" );

		// TODO: Can we count these please? And then reserve on mGLExtensions?
		char *extstr = _strdup( (char *) glGetString( GL_EXTENSIONS ) );

		char *tok = strtok( extstr, " " );

		while( tok != 0 )
		{
			orkprintf( "EXT %s\n", tok );
			gGLExtensions.push_back( (std::string) tok );
			tok = strtok( 0, " " );
			if( tok )
				OrkSTXSetInsert( gGLExtensionSet, (std::string) tok );
		}

		orkprintf( "////////////////////////////////////////////////\n" );
		
		///////////////////////////////////////////////////////////////////////////
		// probe for extensions
		
		#if defined( _WIN32 ) || defined( _OSX ) || defined(IX) || defined( _LINUX )
		int iGlewErr = glewInit();
		#endif
		
		#if defined( _WIN32 )
		bool bBUFR = wglewGetExtension( "WGL_ARB_buffer_region" );
		bool bPBUF = wglewGetExtension( "WGL_ARB_pbuffer" );
		bool bPIXF = wglewGetExtension( "WGL_ARB_pixel_format" );
		bool bRRTX = wglewGetExtension( "WGL_ARB_render_texture" );
		bool bWGLR = wglewGetExtension( "WGL_ARB_make_current_read" );
		orkprintf( "GlewErr %d\n", iGlewErr );
		//	wglewInit();
		OrkAssert( GLEW_OK == iGlewErr );
		#endif
			
		bool bExtFBO = HaveGLExtension( "GL_EXT_framebuffer_object" );
		bool bArbVBO = HaveGLExtension( "GL_ARB_vertex_buffer_object" );
		bool bArbVP = HaveGLExtension( "GL_ARB_vertex_program" );
		bool bArbFP = HaveGLExtension( "GL_ARB_fragment_program" );
		bool bArbTexEnvCombine = HaveGLExtension( "GL_ARB_texture_env_combine" );
		bool bArbTexEnvAdd = HaveGLExtension( "GL_ARB_texture_env_add" );
		bool bArbTexEnvCrossbar = HaveGLExtension( "GL_ARB_fragment_crossbar" );
		bool bNviRC = HaveGLExtension( "GL_NV_register_combiners" );
		bool bMTex	= HaveGLExtension( "GL_ARB_multitexture" );
		bool bVTXW = HaveGLExtension( "GL_ARB_vertex_blend" );

		bool bFLOAT = HaveGLExtension( "GL_ARB_color_buffer_float" );
		
		bool bPOINTSPRITES = HaveGLExtension( "GL_ARB_point_sprite" );
		
		bool bCoordFrame = HaveGLExtension( "GL_EXT_coordinate_frame" );

		///////////////////////////////////////////////////////////////////////////
		// configure pipeline
		
		
		if( false == bArbVBO )
		{
			gbUseVBO = false;
			gbUseIBO = false;
		}

		if( !bArbVP )
		{
			orkprintf( "GL Display device not supported, no ARB_vertex_program!!\n" );
			exit(-1);
		}
		if( !bMTex )
		{
			orkprintf( "GL Display device not supported, no GL_ARB_multitexture!!\n" );
			exit(-1);
		}

		SetRenderPath( EGLRPATH_ARBVP );

		if( (true==bArbVP) && (true==bArbTexEnvCombine) && (true==bArbTexEnvAdd) )
		{
			SetRenderPath( EGLRPATH_ARBVP_ARBTE );
		}

		if( (true==bArbVP) && (true==bNviRC) )
		{
			SetRenderPath( EGLRPATH_ARBVP_NVRC );
			orkprintf( "Setting ARBVP_NVRC RenderPath\n" );
		}
		
		///////////////////////////////////////////////////////////////////////////
		if( bArbVP && bArbFP )
		{
			SetRenderPath( EGLRPATH_ARBVP_ARBFP );
		}
		//else if (bArbVP && bNviRC )
		//	SetRenderPath( EGLRPATH_ARBVP_NV20 );
		//else if( bArbVP )
		///////////////////////////////////////////////////////////////////////////

		if( bExtFBO )
		{
			geFBOSupport = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
			
			switch(geFBOSupport)
			{
				case GL_FRAMEBUFFER_COMPLETE_EXT:
					break;
				case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
					OrkAssert(false);
					// choose different formats
					break;
				default:
					// programming error; will fail on all hardware
					//OrkAssert(false);
					break;
			}
		}

		///////////////////////////////////////////////////////////////////////////
		#if defined( _LINUX ) 
		/*
		orkprintf( "// LINUX/X11/GLX detected\n" );
		Display *dpy = XOpenDisplay(0);
		const char *glxstr;
		glxstr = glXQueryServerString( dpy, 0, GLX_VENDOR );
		orkprintf( "GLX Server: %s\n", glxstr );
		glxstr = glXQueryServerString( dpy, 0, GLX_VERSION );
		orkprintf( "GLX Version: %s\n", glxstr );
		glxstr = glXQueryServerString( dpy, 0, GLX_EXTENSIONS );
		orkprintf( "GLX Extensions: %s\n", glxstr );
		orkprintf( "////////////////////////////////////////////////\n" );
		*/
		#endif
		///////////////////////////////////////////////////////////////////////////

		orkprintf( "Extensions Initialized\n" );
	}
}

} } //namespace ork::lev2

#endif
