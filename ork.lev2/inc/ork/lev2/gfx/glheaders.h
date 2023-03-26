////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
/////////////////////////////
#if defined( _WIN32 )
/////////////////////////////
  #define GLEW_STATIC
  #include <GLEW/glew.h>
  //#include <GLEW/glu.h>
  #include <GLEW/wglew.h>
/////////////////////////////
#elif defined( ORK_OSX )
/////////////////////////////
  #include <ork/kernel/objc.h>
    #define GL3_PROTOTYPES 1
    #include <OpenGL/gl3.h>
    #include <OpenGL/glext.h>
/////////////////////////////
#elif defined(ORK_CONFIG_IX)
/////////////////////////////
  	#define GL_GLEXT_PROTOTYPES
    #include "glcorearb.h"
    #include <GL/glu.h>
/////////////////////////////
#endif
/////////////////////////////
