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

std::string indent(int count) {
  std::string rval = "";
  for (int i = 0; i < count; i++)
    rval += "  ";
  return rval;
}
static thread_local int _dbglevel = 0;
static thread_local std::stack<std::string> _groupstack;

#if defined(__APPLE__)
void ContextGL::debugPushGroup(const std::string str) {
  auto mstr = indent(_dbglevel++) + str;
  _groupstack.push(mstr);
  GL_ERRORCHECK();
  glPushGroupMarkerEXT(mstr.length(), mstr.c_str());
}
void ContextGL::debugPopGroup() {
  glPopGroupMarkerEXT();
  _dbglevel--;
}
void ContextGL::debugMarker(const std::string str) {
}
void ContextGL::debugLabel(GLenum target, GLuint object, std::string name) {
  glLabelObjectEXT(target, object, name.length(), name.c_str());
}
#else
void ContextGL::debugLabel(GLenum target, GLuint object, std::string name) {
  glObjectLabel(target, object, name.length(), name.c_str());
}
void ContextGL::debugPushGroup(const std::string str) {
  auto mstr = indent(_dbglevel++) + str;
  _groupstack.push(mstr);
  GL_ERRORCHECK();
  glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, mstr.length(), mstr.c_str());
  GL_ERRORCHECK();
}
void ContextGL::debugPopGroup() {
  // auto mstr = indent(_dbglevel--) + _prevgroup;
  std::string top = _groupstack.top();
  _groupstack.pop();
  GL_ERRORCHECK();
  glPopDebugGroup();
  GL_ERRORCHECK();
  _dbglevel--;
}
void ContextGL::debugMarker(const std::string str) {
  auto mstr = indent(_dbglevel) + str;
  // printf( "Marker:: %s\n", mstr.c_str() );

  GL_ERRORCHECK();
  glDebugMessageInsert(
      GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0, GL_DEBUG_SEVERITY_NOTIFICATION, mstr.length(), mstr.c_str());
  GL_ERRORCHECK();
}
#endif

bool ContextGL::SetDisplayMode(DisplayMode* mode) {
  return false;
}

/////////////////////////////////////////////////////////////////////////

void recomputeHIDPI(void* plato);

void ContextGL::_doResizeMainSurface(int ix, int iy, int iw, int ih) {
  miX                      = ix;
  miY                      = iy;
  miW                      = iw;
  miH                      = ih;
  mTargetDrawableSizeDirty = true;
  recomputeHIDPI(mPlatformHandle);
  // mFbI.DeviceReset(ix,iy,iw,ih );
}

/////////////////////////////////////////////////////////////////////////

std::string GetGlErrorString(int iGLERR) {
  std::string RVAL = (std::string) "GL_UNKNOWN_ERROR";

  switch (iGLERR) {
    case GL_NO_ERROR:
      RVAL = "GL_NO_ERROR";
      break;
    case GL_INVALID_ENUM:
      RVAL = (std::string) "GL_INVALID_ENUM";
      break;
    case GL_INVALID_VALUE:
      RVAL = (std::string) "GL_INVALID_VALUE";
      break;
    case GL_INVALID_OPERATION:
      RVAL = (std::string) "GL_INVALID_OPERATION";
      break;
      //	case GL_STACK_OVERFLOW:
      //	RVAL =  (std::string) "GL_STACK_OVERFLOW";
      //	break;
      //		case GL_STACK_UNDERFLOW:
      //			RVAL =  (std::string) "GL_STACK_UNDERFLOW";
      //			break;
    case GL_OUT_OF_MEMORY:
      RVAL = (std::string) "GL_OUT_OF_MEMORY";
      break;
    default:
      break;
  }
  return RVAL;
}

/////////////////////////////////////////////////////////////////////////

void check_debug_log();

int GetGlError(void) {
  int err = glGetError();

  if (err != GL_NO_ERROR) {
    std::string errstr = GetGlErrorString(err);
    orkprintf("GLERROR [%s]\n", errstr.c_str());
    check_debug_log();
  }

  return err;
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
