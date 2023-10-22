////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/gfxenv.h>
#include "gl.h"
#include <ork/lev2/gfx/gfxprimitives.h>

#if defined(ORK_CONFIG_OPENGL) && defined(LINUX)

#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace ork::lev2 {
extern int G_MSAASAMPLES;
}

#include <ork/lev2/glfw/ctx_glfw.h>
#include <GL/glx.h>
#include "glad/glad.h"

extern "C" {
#include <X11/extensions/Xrandr.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
}

extern "C" {
extern bool gbVSYNC;
}

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

struct GlIxPlatformObject : public GlPlatformObject {
  static GlIxPlatformObject* _global_plato;
  /////////////////////////////////////
  Display* getDisplay() {
    return glfwGetX11Display();
  }
  /////////////////////////////////////
  int getXwindowID() {
    int x11window = glfwGetX11Window(_ctxbase->_glfwWindow);
    return x11window;
  }
  /////////////////////////////////////
  int getXscreenID() {
    XWindowAttributes xattrs;
    Status xstat = XGetWindowAttributes(getDisplay(), getXwindowID(), &xattrs);
    int x_screen = XScreenNumberOfScreen(xattrs.screen);
    return x_screen;
  }
  /////////////////////////////////////
  GlIxPlatformObject() {
    _bindop = [=]() {};
  }
  /////////////////////////////////////
};
GlIxPlatformObject* GlIxPlatformObject::_global_plato = nullptr;
/////////////////////////////////////////////////////////////////////////

struct GlxLoadContext {
  GlIxPlatformObject* _global_plato = nullptr;
  GLFWwindow* _pushedContext        = nullptr;
};

/////////////////////////////////////////////////////////////////////////

ctx_platform_handle_t ContextGL::_doClonePlatformHandle() const {
  ctx_platform_handle_t rval;
  auto plato = _impl.getShared<GlIxPlatformObject>();
  auto new_plato = rval.makeShared<GlIxPlatformObject>();
  new_plato->_ctxbase = nullptr; //plato->_ctxbase;
  //new_plato->_context = plato->_context;
  new_plato->_needsInit   = false;
  //new_plato->_bindop = plato->_bindop;

  // TODO : https://github.com/tweakoz/orkid/issues/139
  
  return rval;
}

/////////////////////////////////////////////////////////////////////////

static ork::atomic<int> atomic_init;
///////////////////////////////////////////////////////////////////////////////

void check_debug_log() {
  constexpr GLuint count    = 1024; // max. num. of messages that will be read from the log
  constexpr GLsizei bufsize = 32768;

  GLenum sources[count];
  GLenum types[count];
  GLuint ids[count];
  GLenum severities[count];
  GLsizei lengths[count];
  GLchar messageLog[bufsize];

  auto retVal = glGetDebugMessageLog(count, bufsize, sources, types, ids, severities, lengths, messageLog);
  if (retVal > 0) {
    int pos = 0;
    for (int i = 0; i < retVal; i++) {
      // DebugOutputToFile(sources[i], types[i], ids[i], severities[i],&messageLog[pos]);
      printf("GLDEBUG msg<%d:%s>\n", i, &messageLog[pos]);

      pos += lengths[i];
    }
  }
}

void ContextGL::GLinit() {

  int iinit = atomic_init++;

  if (0 != iinit) {
    return;
  }

  auto global_ctxbase = CtxGLFW::globalOffscreenContext();

  GlIxPlatformObject::_global_plato               = new GlIxPlatformObject;
  GlIxPlatformObject::_global_plato->_ctxbase = global_ctxbase;

  ////////////////////////////////////
  // load extensions
  ////////////////////////////////////

  GlIxPlatformObject::_global_plato->makeCurrent();
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
  
  ////////////////////////////////////
  
  //GLint num_gpus = 0;
  //glGetIntegerv(GL_MULTICAST_GPUS_NV, &num_gpus);
  //printf( "GPU count <%d>\n", num_gpus );
  //OrkAssert(false);

  ////////////////////////////////////
  for (int i = 0; i < 1; i++) {
    GlxLoadContext* loadctx = new GlxLoadContext;
    loadctx->_global_plato  = GlIxPlatformObject::_global_plato;
    _loadTokens.push((void*)loadctx);
  }
}

///////////////////////////////////////////////////////////////////////////////

std::string GetGlErrorString(void);

context_ptr_t OpenGlContextInit() {
  auto clazz = dynamic_cast<object::ObjectClass*>(ContextGL::GetClassStatic());
  GfxEnv::setContextClass(clazz);
  ContextGL::GLinit();
  auto target = std::make_shared<ContextGL>();
  target->initializeLoaderContext();
  GfxEnv::initializeWithContext(target);
  return target;
}

/////////////////////////////////////////////////////////////////////////

ContextGL::ContextGL()
    : Context()
    , mImI(*this)
    , mFxI(*this)
    //, mRsI(*this)
    , mMtxI(*this)
    , mGbI(*this)
    , mFbI(*this)
    , mTxI(*this)
    , mDWI(*this)
    , mCI(*this)
    , mTargetDrawableSizeDirty(true) {
  ContextGL::GLinit();
  FxInit();
}

/////////////////////////////////////////////////////////////////////////

ContextGL::~ContextGL() {
}

/////////////////////////////////////////////////////////////////////////

static void _ixDisableVIRGL(ContextGL* cgl){
    cgl->_SUPPORTS_BINARY_PIPELINE = false;
    cgl->_SUPPORTS_BUFFER_STORAGE = false;
    cgl->_SUPPORTS_PERSISTENT_MAP = false;
    cgl->_SUPPORTS_EXTERNAL_MEMORY_OBJECT = false;
    GfxEnv::disableBC7();
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::initializeWindowContext(Window* pWin, CTXBASE* pctxbase) {
  meTargetType = TargetType::WINDOW;
  ///////////////////////
  auto glfw_container = (CtxGLFW*)pctxbase;
  auto glfw_window    = glfw_container->_glfwWindow;
  ///////////////////////
  auto plato = _impl.makeShared<GlIxPlatformObject>();
  plato->_ctxbase       = glfw_container;
  mCtxBase                  = pctxbase;
  ///////////////////////
  plato->makeCurrent();
  mFbI.SetThisBuffer(pWin);
  _GL_RENDERER = (const char*) glGetString(GL_RENDERER);
  printf( "GL_RENDERER<%s>\n", _GL_RENDERER.c_str() );
  if(_GL_RENDERER.find("virgl")!=std::string::npos){
    _ixDisableVIRGL(this);
  }
  #if ! defined(OPENGL_46)
    _ixDisableVIRGL(this);
  #endif

}

/////////////////////////////////////////////////////////////////////////

void ContextGL::initializeOffscreenContext(DisplayBuffer* pBuf) {

  meTargetType = TargetType::OFFSCREEN;

  ///////////////////////

  miW = pBuf->GetBufferW();
  miH = pBuf->GetBufferH();

  mCtxBase = 0;

  auto ixplato = _impl.makeShared<GlIxPlatformObject>();
  mFbI.SetThisBuffer(pBuf);

  auto global_plato = GlIxPlatformObject::_global_plato;

  ixplato->_ctxbase = global_plato->_ctxbase;
  ixplato->_needsInit   = false;

  _defaultRTG  = new RtGroup(this, miW, miH, MsaaSamples::MSAA_1X);
  auto rtb     = _defaultRTG->createRenderTarget(EBufferFormat::RGBA8);
  auto texture = rtb->texture();
  FBI()->SetBufferTexture(texture);
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::initializeLoaderContext() {

  meTargetType = TargetType::LOADING;

  miW = 8;
  miH = 8;

  mCtxBase = 0;

  auto ixplato = _impl.makeShared<GlIxPlatformObject>();

  auto global_plato   = GlIxPlatformObject::_global_plato;
  ixplato->_ctxbase = global_plato->_ctxbase;
  ixplato->_needsInit   = false;

  _defaultRTG  = new RtGroup(this, miW, miH, MsaaSamples::MSAA_1X);
  auto rtb     = _defaultRTG->createRenderTarget(EBufferFormat::RGBA8);
  auto texture = rtb->texture();
  FBI()->SetBufferTexture(texture);

  ixplato->_bindop = [=]() {
    if (this->mTargetDrawableSizeDirty) {
      int w = mainSurfaceWidth();
      int h = mainSurfaceHeight();
      // printf("resizing defaultRTG<%p>\n", _defaultRTG);
      _defaultRTG->Resize(w, h);
      mTargetDrawableSizeDirty = false;
      #if ! defined(OPENGL_46)
          _ixDisableVIRGL(this);
      #endif
    }
  };
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::makeCurrentContext(void) {
  auto ixplato = _impl.getShared<GlIxPlatformObject>();
  OrkAssert(ixplato);
  if (ixplato) {
    ixplato->makeCurrent();
    ixplato->_bindop();
  }
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::SwapGLContext(CTXBASE* pCTFL) {
  auto ixplato = _impl.getShared<GlIxPlatformObject>();
  OrkAssert(ixplato);
  if (ixplato && (ixplato->getXwindowID() > 0)) {
    ixplato->makeCurrent();
    ixplato->swapBuffers();
  }
}


/////////////////////////////////////////////////////////////////////////

load_token_t ContextGL::_doBeginLoad() {
  load_token_t loadtoken;

  while (false == _loadTokens.try_pop(loadtoken)) {
    ork::usleep(1000);
  }
  auto loadctx    = loadtoken.getShared<GlxLoadContext>();
  GLFWwindow* current_window = glfwGetCurrentContext();

  loadctx->_pushedContext = current_window;
  loadctx->_global_plato->makeCurrent();
  return loadtoken;
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::_doEndLoad(load_token_t loadtok) {
  auto loadctx = loadtok.getShared<GlxLoadContext>();

  auto pushed = loadctx->_pushedContext;
  glfwMakeContextCurrent(pushed);
  _loadTokens.push(loadtok);
}

/////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////

#endif // #if defined( ORK_CONFIG_OPENGL ) && defined(ORK_CONFIG_IX)
