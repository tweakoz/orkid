////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxenv.h>
#include "gl.h"
#include <ork/kernel/string/deco.inl>

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
  //printf( "Marker:: %s\n", mstr.c_str() );

  GL_ERRORCHECK();
  if(1)glDebugMessageInsert(
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

/////////////////////////////////////////////////////////////////////////

void ContextGL::_validateCurrentShaderProgram() const {
  // validate that the current shader program is valid


  GLint currentProgram = 0;
  glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
  GLint linkStatus = 0;
  glGetProgramiv(currentProgram, GL_LINK_STATUS, &linkStatus);
  glValidateProgram(currentProgram);
  GLint validateStatus = 0;
  glGetProgramiv(currentProgram, GL_VALIDATE_STATUS, &validateStatus);
  printf("currentProgram<%d> linkStatus<%d> validateStatus<%d>\n", currentProgram, linkStatus, validateStatus);
  OrkAssert(linkStatus == GL_TRUE);
  OrkAssert(validateStatus == GL_TRUE);
  // validate all bound shader parameters are valid
  GLint numUniforms = 0;
  glGetProgramiv(currentProgram, GL_ACTIVE_UNIFORMS, &numUniforms);
  printf("numUniforms<%d>\n", numUniforms);
  
  std::vector<GLuint> uniformIndices(numUniforms);
  std::vector<GLint> uniformBlockIndices(numUniforms);

  for (int i = 0; i < numUniforms; i++) {
    uniformIndices[i] = i;
  }
  glGetActiveUniformsiv(currentProgram, numUniforms, uniformIndices.data(), GL_UNIFORM_BLOCK_INDEX, uniformBlockIndices.data());

  for (int i = 0; i < numUniforms; i++) {
    GLint nameLength = 0;
    GLint size       = 0;
    GLenum type      = GL_NONE;
    GLchar name[256];
    glGetActiveUniform(currentProgram, i, sizeof(name), &nameLength, &size, &type, name);
    // print value 
    GLint location = glGetUniformLocation(currentProgram, name);
    std::string value_str;
    switch(type){
      case GL_FLOAT:{
        float value = 0.0f;
        glGetUniformfv(currentProgram, location, &value);
        value_str = FormatString("float(%g)", value);
        break;
      }
      case GL_FLOAT_VEC2:{
        fvec2 value;
        glGetUniformfv(currentProgram, location, value.asArray());
        value_str = FormatString("vec2(%g %g)", value.x, value.y);
        break;
      }
      case GL_FLOAT_VEC3:{
        fvec3 value;
        glGetUniformfv(currentProgram, location, value.asArray());
        value_str = FormatString("vec3(%g %g %g)", value.x, value.y, value.z);
        break;
      }
      case GL_FLOAT_VEC4:{
        fvec4 value;
        glGetUniformfv(currentProgram, location, value.asArray());
        value_str = FormatString("vec4(%g %g %g %g)", value.x, value.y, value.z, value.w);
        break;
      }
      case GL_FLOAT_MAT4: {
        fvec4 value[4];
        glGetUniformfv(currentProgram, location, value[0].asArray());
        glGetUniformfv(currentProgram, location, value[1].asArray());
        glGetUniformfv(currentProgram, location, value[2].asArray());
        glGetUniformfv(currentProgram, location, value[3].asArray());
        value_str = FormatString("mat4(%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g)", value[0].x, value[0].y, value[0].z, value[0].w, value[1].x, value[1].y, value[1].z, value[1].w, value[2].x, value[2].y, value[2].z, value[2].w, value[3].x, value[3].y, value[3].z, value[3].w);
        break;
      }
      case GL_FLOAT_MAT3: {
        fvec3 value[3];
        glGetUniformfv(currentProgram, location, value[0].asArray());
        glGetUniformfv(currentProgram, location, value[1].asArray());
        glGetUniformfv(currentProgram, location, value[2].asArray());
        value_str = FormatString("mat3(%g %g %g %g %g %g %g %g %g)", value[0].x, value[0].y, value[0].z, value[1].x, value[1].y, value[1].z, value[2].x, value[2].y, value[2].z);
        break;
      }
      case GL_INT:{
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("int(%d)", value);
        break;
      }
      case GL_INT_VEC2:{
        int value[2];
        glGetUniformiv(currentProgram, location, value);
        value_str = FormatString("int2(%d %d)", value[0], value[1]);
        break;
      }
      case GL_INT_VEC3:{
        int value[3];
        glGetUniformiv(currentProgram, location, value);
        value_str = FormatString("int3(%d %d %d)", value[0], value[1], value[2]);
        break;
      }
      case GL_SAMPLER_2D:{
        int unit = 0;
        glGetUniformiv(currentProgram, location, &unit);
        // query texture array dimensions of texture @ location (width, height, depth, nummips)
        // use statless query to get texture array size, or restore state after query
        
        // store current texture binding
        GLint currentTexture = 0;
        glGetIntegeri_v(GL_TEXTURE_BINDING_2D, unit, &currentTexture);
        // bind texture @ location
        glBindTexture(GL_TEXTURE_2D, currentTexture);
        // query texture array size
        GLint width = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        GLint height = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
        // restore texture binding
        glBindTexture(GL_TEXTURE_2D, currentTexture);

        value_str = FormatString("samp2D(unit: %d tobj: %d dim<%dx%d>)", unit, currentTexture, width, height);
        break;
      }
      case GL_SAMPLER_3D:{
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp3d(%d)", value);
        break;
      }
      case GL_SAMPLER_CUBE:{
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("sampCUBE(%d)", value);
        break;
      }
      case GL_SAMPLER_2D_SHADOW:{
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp2DSHAD(%d)", value);
        break;
      }
      case GL_SAMPLER_2D_ARRAY:{
        int unit = 0;
        glGetUniformiv(currentProgram, location, &unit);
        // value represents the texture unit
        
        // query texture array dimensions of texture @ unit (width, height, depth, nummips)
        // use statless query to get texture array size, or restore state after query
        
        // store current texture binding
        GLint currentTexture = 0;
        glGetIntegeri_v(GL_TEXTURE_BINDING_2D_ARRAY, unit, &currentTexture);
        // bind texture @ location
        glActiveTexture(GL_TEXTURE0 + unit);
        //glBindTexture(GL_TEXTURE_2D_ARRAY, value);
        // query texture array size
        GLint depth = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_DEPTH, &depth);
        // query w,h
        GLint width = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_WIDTH, &width);
        GLint height = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_HEIGHT, &height);
        // restore texture binding
        glBindTexture(GL_TEXTURE_2D_ARRAY, currentTexture);
               
        value_str = FormatString("samp2Darr(unit: %d tobj: %d dim<%dx%dx%d>)", unit, currentTexture, width, height, depth);
        break;
      }
      case GL_SAMPLER_2D_ARRAY_SHADOW:{
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp2DarrSHAD(%d)", value);
        break;
      }
      case GL_SAMPLER_2D_MULTISAMPLE:{
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp2Dmultisamp(%d)", value);
        break;
      }
      case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:{
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp2Dmultisamparr(%d)", value);
        break;
      }
      case GL_SAMPLER_CUBE_SHADOW:{
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("sampCUBESHAD(%d)", value);
        break;
      }
      case GL_SAMPLER_1D:{
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp1D(%d)", value);
        break;
      }
      case GL_SAMPLER_1D_ARRAY:{
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp1Darr(%d)", value);
        break;
      }
      case GL_SAMPLER_1D_SHADOW:{
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp1DSHAD(%d)", value);
        break;
      }
      case GL_SAMPLER_BUFFER:{
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("sampbuff(%d)", value);
        break;
      }
      case GL_SAMPLER_2D_RECT:{
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp2Drect(%d)", value);
        break;
      }
      case GL_SAMPLER_2D_RECT_SHADOW:{
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp2DrectSHAD(%d)", value);
        break;
      }
      case GL_INT_SAMPLER_1D:{
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("intsamp1D(%d)", value);
        break;
      }

    } // switch(type){

    std::string out_str;
    if(location == -1){
      int block_index = uniformBlockIndices[i];
      std::string tmp_str = FormatString("uni<%d : %s> BLOCK<%d> size<%d> value: %s\n", i, name, block_index, size, value_str.c_str() );
      if(block_index==-1){
        out_str = deco::string(tmp_str,255,0,0);
      }
      else{
        out_str = deco::string(tmp_str,255,255,128);
      }
    }else{
      std::string tmp_str = FormatString("uni<%d : %s> loc<%d> size<%d> value: %s\n", i, name, location, size, value_str.c_str() );
      out_str = deco::string(tmp_str,255,255,255);
    }
    printf("%s", out_str.c_str());
  }
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::_validateCurrentFramebuffer() const{
  // validate that the current FBO state is valid
  GLint currentFBO = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);
  GLint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  printf("currentFBO<%d> status<%x>\n", currentFBO, status);
  OrkAssert(status == GL_FRAMEBUFFER_COMPLETE);
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::_validateCurrentGeomBuffers() const{
  // validate that the current IBO state is valid
  GLint currentIBO = 0;
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &currentIBO);
  printf("currentIBO<%d>\n", currentIBO);
  OrkAssert(currentIBO != 0);
  // validate that the current VAO state is valid
  GLint currentVAO = 0;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &currentVAO);
  printf("currentVAO<%d>\n", currentVAO);
  OrkAssert(currentVAO != 0);
  // validate all bound VBOs are valid
  GLint numVBOs = 0;
  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &numVBOs);
  printf("numAttribs<%d>\n", numVBOs);
  for (int i = 0; i < numVBOs; i++) {
    GLint currentVBO = 0;
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &currentVBO);
    // get attrib format
    GLint size = 0;
    GLenum type = GL_NONE;
    GLboolean normalized = GL_FALSE;
    GLint stride = 0;
    GLint offset = 0;
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, (GLint*)&type);
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, (GLint*)&normalized);
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_POINTER, &offset);
    printf("attrib<%d> vbo<%d> size<%d> type<%x> normalized<%d> stride<%d> offset<%d>\n", i, currentVBO, size, type, normalized, stride, offset);
  }
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
