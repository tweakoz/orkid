////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxenv.h>
#include "gl.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

bool GfxTargetGL::SetDisplayMode(DisplayMode *mode)
{
	return false;
}

/////////////////////////////////////////////////////////////////////////

void GfxTargetGL::SetSize(int ix, int iy, int iw, int ih) {
  miX = ix;
  miY = iy;
  miW = iw;
  miH = ih;
  mTargetDrawableSizeDirty = true;
  // mFbI.DeviceReset(ix,iy,iw,ih );
}

/////////////////////////////////////////////////////////////////////////

std::string GetGlErrorString( int iGLERR )
{
	std::string RVAL = (std::string) "GL_UNKNOWN_ERROR";

	switch( iGLERR )
	{
		case GL_NO_ERROR:
			RVAL =  "GL_NO_ERROR";
			break;
		case GL_INVALID_ENUM:
			RVAL =  (std::string) "GL_INVALID_ENUM";
			break;
		case GL_INVALID_VALUE:
			RVAL =  (std::string) "GL_INVALID_VALUE";
			break;
		case GL_INVALID_OPERATION:
			RVAL =  (std::string) "GL_INVALID_OPERATION";
			break;
	//	case GL_STACK_OVERFLOW:
		//	RVAL =  (std::string) "GL_STACK_OVERFLOW";
		//	break;
//		case GL_STACK_UNDERFLOW:
//			RVAL =  (std::string) "GL_STACK_UNDERFLOW";
//			break;
		case GL_OUT_OF_MEMORY:
			RVAL =  (std::string) "GL_OUT_OF_MEMORY";
			break;
		default:
			break;
	}
	return RVAL;
}

/////////////////////////////////////////////////////////////////////////

void check_debug_log();

int GetGlError( void )
{
	int err = glGetError();

	std::string errstr = GetGlErrorString( err );

	if( err != GL_NO_ERROR )
	{
		orkprintf( "GLERROR [%s]\n", errstr.c_str() );
		check_debug_log();
	}

	return err;
}

///////////////////////////////////////////////////////////////////////////////
}} //namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
