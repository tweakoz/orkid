////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/lev2_asset.h>
#include <ork/asset/Asset.inl>

#include "gl.h"

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::lev2::ContextGL, "ContextGL");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

ork::MpMcBoundedQueue<load_token_t> ContextGL::_loadTokens;

GlPlatformObject* GlPlatformObject::_current = nullptr;

void GlPlatformObject::makeCurrent() {
    _current = this;
    if(_ctxbase){
      auto window = _ctxbase->_glfwWindow;
      //printf( "_glfwWindow<%p> made current\n", (void*) window );
      glfwMakeContextCurrent(window);
    }
    else{
      OrkAssert(false);
    }
    //_ctxbase->makeCurrent();
}
void GlPlatformObject::swapBuffers() {

}


namespace opengl{

  void touchClasses() {
    ContextGL::GetClassStatic();
  }

  context_ptr_t createLoaderContext() {

    ///////////////////////////////////////////////////////////
    auto loader = std::make_shared<FxShaderLoader>();
    FxShader::RegisterLoaders("shaders/glfx/", "glfx");
    auto shadctx = FileEnv::contextForUriProto("orkshader://");
    auto democtx = FileEnv::contextForUriProto("demo://");
    loader->addLocation(shadctx, ".glfx"); // for glsl targets
    if (democtx) {
      loader->addLocation(democtx, ".glfx"); // for glsl targets
    }
    ///////////////////////////////////////////////////////////

    asset::registerLoader<FxShaderAsset>(loader);

    //_GVI       = std::make_shared<VulkanInstance>();
    auto clazz = dynamic_cast<object::ObjectClass*>(ContextGL::GetClassStatic());
    GfxEnv::setContextClass(clazz);
    auto target = std::make_shared<ContextGL>();
    target->initializeLoaderContext();
    GfxEnv::initializeWithContext(target);
    return target;
  }  
}

GlPlatformObject::GlPlatformObject(): _bindop([=](){}) {}
GlPlatformObject::~GlPlatformObject() {}


void ContextGL::describeX(class_t* clazz) {
  clazz->annotateTyped<context_factory_t>("context_factory", []() { //
    return std::make_shared<ContextGL>();
  });
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
void ContextGL::debugPushGroup(const std::string str, const fvec4& color) {
  int level = _dbglevel++;
  auto mstr = indent(level) + str;
  // printf( "PSHGRP CTX<%p> lev<%d> name<%s>\n", this, level, mstr.c_str() );
  _groupstack.push(mstr);
  GL_ERRORCHECK();
  glPushGroupMarkerEXT(mstr.length(), mstr.c_str());
}
/////////////////////////////////////////////////////////////////////////
void ContextGL::debugPopGroup() {
  std::string top = _groupstack.top();
  // printf( "POPGRP CTX<%p> lev<%d> name<%s>\n", this,  _dbglevel, top.c_str() );
  //  auto mstr = indent(_dbglevel--) + _prevgroup;
  _groupstack.pop();
  GL_ERRORCHECK();
  glPopGroupMarkerEXT();
  GL_ERRORCHECK();
  _dbglevel--;
}
/////////////////////////////////////////////////////////////////////////
void ContextGL::debugMarker(const std::string str,const fvec4& color) {
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

void ContextGL::debugPushGroup(const std::string str, const fvec4& color) {
  int level = _dbglevel++;
  auto mstr = indent(level) + str;
  // printf( "PSHGRP CTX<%p> lev<%d> name<%s>\n", (void*) this, level, mstr.c_str() );
  _groupstack.push(mstr);
  GL_ERRORCHECK();
  glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, mstr.length(), mstr.c_str());
  GL_ERRORCHECK();
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::debugPopGroup() {
  std::string top = _groupstack.top();
  _groupstack.pop();
  // printf( "POPGRP CTX<%p> lev<%d> name<%s>\n", (void*) this, _dbglevel, top.c_str() );
  GL_ERRORCHECK();
  glPopDebugGroup();
  GL_ERRORCHECK();
  _dbglevel--;
}
/////////////////////////////////////////////////////////////////////////

void ContextGL::debugMarker(const std::string str, const fvec4& color) {
  auto mstr = indent(_dbglevel) + str;
  // printf( "Marker:: %s\n", mstr.c_str() );

  GL_ERRORCHECK();
  if (1)
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

void recomputeHIDPI(GLFWwindow *glfw_window);

void ContextGL::_doResizeMainSurface(int iw, int ih) {
  miW                      = iw;
  miH                      = ih;
  mTargetDrawableSizeDirty = true;
  auto plato = _impl.getShared<GlPlatformObject>();
  auto ctx_glfw = plato->_ctxbase;
  auto win_glfw = ctx_glfw->_glfwWindow;
  recomputeHIDPI(win_glfw);
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
    //_gcurrentContext->stateDebugger();
    check_debug_log();
  }

  return err;
}

/*
Bind2 Tex<0x12662fa30:src://effect_textures/white.dds[filtenvmap-processed-specular]> par<MapSpecularEnv> uniloc<0> teknam<FWD_SKYBOX_MO>
Bind3 pass<FWD_SKYBOX_MO_p0> loc<0> unit<0> obj<20> tgt<3553> dim<64x64x1> tex<0x12662fa30:src://effect_textures/white.dds[filtenvmap-processed-specular]>
Bind2 Tex<0x12662fa30:src://effect_textures/white.dds[filtenvmap-processed-specular]> par<MapSpecularEnv> uniloc<0> teknam<FWD_SKYBOX_MO>
Bind3 pass<FWD_SKYBOX_MO_p0> loc<0> unit<0> obj<20> tgt<3553> dim<64x64x1> tex<0x12662fa30:src://effect_textures/white.dds[filtenvmap-processed-specular]>
Bind2 Tex<0x12662fa30:src://effect_textures/white.dds[filtenvmap-processed-specular]> par<MapSpecularEnv> uniloc<8> teknam<FWD_CT_NM_RI_NI_MO>
Bind3 pass<FWD_CT_NM_RI_NI_MO_p0> loc<8> unit<0> obj<20> tgt<3553> dim<64x64x1> tex<0x12662fa30:src://effect_textures/white.dds[filtenvmap-processed-specular]>
Bind2 Tex<0x126632690:src://effect_textures/white.dds[filtenvmap-processed-diffuse]> par<MapDiffuseEnv> uniloc<26> teknam<FWD_CT_NM_RI_NI_MO>
Bind3 pass<FWD_CT_NM_RI_NI_MO_p0> loc<26> unit<1> obj<21> tgt<3553> dim<64x64x1> tex<0x126632690:src://effect_textures/white.dds[filtenvmap-processed-diffuse]>
binding lighting UBO
Bind2 Tex<0x126575860:> par<light_cookie0> uniloc<15> teknam<FWD_CT_NM_RI_NI_MO>
Bind3 pass<FWD_CT_NM_RI_NI_MO_p0> loc<15> unit<2> obj<2> tgt<3553> dim<64x64x1> tex<0x126575860:>
Bind2 Tex<0x126575860:> par<light_cookie1> uniloc<33> teknam<FWD_CT_NM_RI_NI_MO>
Bind3 pass<FWD_CT_NM_RI_NI_MO_p0> loc<33> unit<3> obj<2> tgt<3553> dim<64x64x1> tex<0x126575860:>
Bind2 Tex<0x126575860:> par<light_cookie2> uniloc<31> teknam<FWD_CT_NM_RI_NI_MO>
Bind3 pass<FWD_CT_NM_RI_NI_MO_p0> loc<31> unit<4> obj<2> tgt<3553> dim<64x64x1> tex<0x126575860:>
Bind2 Tex<0x126575860:> par<light_cookie3> uniloc<7> teknam<FWD_CT_NM_RI_NI_MO>
Bind3 pass<FWD_CT_NM_RI_NI_MO_p0> loc<7> unit<5> obj<2> tgt<3553> dim<64x64x1> tex<0x126575860:>
HUH <0x6000023d2520> <0x126561b00>
Bind2 Tex<0x126561b00:pbrtexarray> par<CNMREA> uniloc<41> teknam<FWD_CT_NM_RI_NI_MO>
Bind3 pass<FWD_CT_NM_RI_NI_MO_p0> loc<41> unit<6> obj<1> tgt<35866> dim<64x64x4> tex<0x126561b00:pbrtexarray>
Bind2 Tex<0x12656cd30:> par<reflectionPROBE> uniloc<34> teknam<FWD_CT_NM_RI_NI_MO>
Bind3 pass<FWD_CT_NM_RI_NI_MO_p0> loc<34> unit<7> obj<3> tgt<34067> dim<64x64x1> tex<0x12656cd30:>
Bind2 Tex<0x142b12d30:> par<LightMapArray> uniloc<39> teknam<FWD_CT_NM_RI_NI_MO>
Bind3 pass<FWD_CT_NM_RI_NI_MO_p0> loc<39> unit<8> obj<4> tgt<35866> dim<64x64x4> tex<0x142b12d30:>
GLERROR [GL_INVALID_OPERATION] cctx<0x153057020>
*/
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
