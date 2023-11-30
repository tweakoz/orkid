////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxenv.h>
#include "gl.h"

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::lev2::ContextGL, "ContextGL");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

std::atomic<int> __FIND_IT;

void ContextGL::describeX(class_t* clazz) {
  __FIND_IT.store(0);
}

std::string indent(int count) {
  std::string rval = "";
  for (int i = 0; i < count; i++)
    rval += "  ";
  return rval;
}
static thread_local int _dbglevel = 0;
static thread_local std::stack<std::string> _groupstack;

/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
#if defined(__APPLE__)
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
void ContextGL::debugPushGroup(const std::string str) {
    int level = _dbglevel++;
    auto mstr = indent(level) + str;
    //printf( "PSHGRP CTX<%p> lev<%d> name<%s>\n", this, level, mstr.c_str() );
    _groupstack.push(mstr);
    GL_ERRORCHECK();
    glPushGroupMarkerEXT(mstr.length(), mstr.c_str());
}
/////////////////////////////////////////////////////////////////////////
void ContextGL::debugPopGroup() {
    std::string top = _groupstack.top();
    //printf( "POPGRP CTX<%p> lev<%d> name<%s>\n", this,  _dbglevel, top.c_str() );
    // auto mstr = indent(_dbglevel--) + _prevgroup;
    _groupstack.pop();
    GL_ERRORCHECK();
    glPopGroupMarkerEXT();
    GL_ERRORCHECK();
    _dbglevel--;
}
/////////////////////////////////////////////////////////////////////////
void ContextGL::debugMarker(const std::string str) {
}
/////////////////////////////////////////////////////////////////////////
void ContextGL::debugLabel(GLenum target, GLuint object, std::string name) {
  glLabelObjectEXT(target, object, name.length(), name.c_str());
}

/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
#else // LINUX
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

void ContextGL::debugLabel(GLenum target, GLuint object, std::string name) {
  glObjectLabel(target, object, name.length(), name.c_str());
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::debugPushGroup(const std::string str) {
  int level = _dbglevel++;
  auto mstr = indent(level) + str;
  //printf( "PSHGRP CTX<%p> lev<%d> name<%s>\n", (void*) this, level, mstr.c_str() );
  _groupstack.push(mstr);
  GL_ERRORCHECK();
  glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, mstr.length(), mstr.c_str());
  GL_ERRORCHECK();
  __FIND_IT.fetch_add(1);
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::debugPopGroup() {
  std::string top = _groupstack.top();
  _groupstack.pop();
  //printf( "POPGRP CTX<%p> lev<%d> name<%s>\n", (void*) this, _dbglevel, top.c_str() );
  if(__FIND_IT.exchange(0)==1){
    //OrkAssert(false);
  }
  GL_ERRORCHECK();
  glPopDebugGroup();
  GL_ERRORCHECK();
  _dbglevel--;
}
/////////////////////////////////////////////////////////////////////////

void ContextGL::debugMarker(const std::string str) {
  auto mstr = indent(_dbglevel) + str;
  // printf( "Marker:: %s\n", mstr.c_str() );

  GL_ERRORCHECK();
  glDebugMessageInsert(
      GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0, GL_DEBUG_SEVERITY_NOTIFICATION, mstr.length(), mstr.c_str());
  GL_ERRORCHECK();
}
#endif

/////////////////////////////////////////////////////////////////////////

bool ContextGL::SetDisplayMode(DisplayMode* mode) {
  return false;
}

/////////////////////////////////////////////////////////////////////////

void recomputeHIDPI(Context* ctx);

void ContextGL::_doResizeMainSurface(int iw, int ih) {
  miW                      = iw;
  miH                      = ih;
  mTargetDrawableSizeDirty = true;
  recomputeHIDPI(this);
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
