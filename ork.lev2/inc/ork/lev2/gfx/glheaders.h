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
#elif defined( IX )
/////////////////////////////
  	#define GL_GLEXT_PROTOTYPES
    #include "glcorearb.h"
    #include <GL/glu.h>
/////////////////////////////
#endif
/////////////////////////////
