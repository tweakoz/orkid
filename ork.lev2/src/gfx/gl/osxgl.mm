////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/gfxenv.h>
#include "gl.h"

#if defined(ORK_CONFIG_OPENGL) && defined(ORK_OSX)

#define GLFW_EXPOSE_NATIVE_COCOA

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace ork::lev2 {
extern int G_MSAASAMPLES;
}


#include <ork/lev2/glfw/ctx_glfw.h>
//#include <GL/glx.h>
#include "glad/glad.h"

extern "C" {
extern bool gbVSYNC;
}

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

bool g_allow_HIDPI = false;

bool _hakHIDPI       = false;
bool _hakMixedDPI    = false;
float _hakCurrentDPI = 95.0f;
bool _macosUseHIDPI = false;

ork::MpMcBoundedQueue<void*> ContextGL::_loadTokens;

struct GlIxPlatformObject {
  static GlIxPlatformObject* _global_plato;
  static GlIxPlatformObject* _current;
  /////////////////////////////////////
  GlIxPlatformObject() {
    _bindop = [=]() {};
  }
  /////////////////////////////////////
  void makeCurrent() {
    _current = this;
    _thiscontext->makeCurrent();
  }
  void swapBuffers() {
    _thiscontext->swapBuffers();
  }
  /////////////////////////////////////
  CtxGLFW* _thiscontext = nullptr;
  bool _needsInit       = true;
  void_lambda_t _bindop;
  /////////////////////////////////////
};
GlIxPlatformObject* GlIxPlatformObject::_global_plato = nullptr;
GlIxPlatformObject* GlIxPlatformObject::_current      = nullptr;
/////////////////////////////////////////////////////////////////////////

struct GlxLoadContext {
  GlIxPlatformObject* _global_plato  = nullptr;
  GLFWwindow* _pushedContext = nullptr;
};

/////////////////////////////////////////////////////////////////////////

static ork::atomic<int> atomic_init;
///////////////////////////////////////////////////////////////////////////////

void check_debug_log() {
  GLuint count    = 1024; // max. num. of messages that will be read from the log
  GLsizei bufsize = 32768;
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
  GlIxPlatformObject::_global_plato->_thiscontext = global_ctxbase;

  ////////////////////////////////////
  // load extensions
  ////////////////////////////////////

  global_ctxbase->makeCurrent();
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  ////////////////////////////////////
  for (int i = 0; i < 1; i++) {
    GlxLoadContext* loadctx = new GlxLoadContext;
    loadctx->_global_plato  = GlIxPlatformObject::_global_plato;
    _loadTokens.push((void*)loadctx);
  }
} // namespace lev2

///////////////////////////////////////////////////////////////////////////////

std::string GetGlErrorString(void);

void OpenGlContextInit() {
  auto clazz = dynamic_cast<object::ObjectClass*>(ContextGL::GetClassStatic());
  GfxEnv::setContextClass(clazz);
  ContextGL::GLinit();
  auto target = new ContextGL;
  target->initializeLoaderContext();
  GfxEnv::GetRef().SetLoaderTarget(target);
}

/////////////////////////////////////////////////////////////////////////

ContextGL::ContextGL()
    : Context()
    , mFxI(*this)
    , mImI(*this)
    , mRsI(*this)
    , mGbI(*this)
    , mFbI(*this)
    , mTxI(*this)
    , mMtxI(*this)
    , mDWI(*this)
    //, mCI(*this)
    , mTargetDrawableSizeDirty(true) {
  ContextGL::GLinit();
  FxInit();
}

/////////////////////////////////////////////////////////////////////////

ContextGL::~ContextGL() {
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::initializeWindowContext(Window* pWin, CTXBASE* pctxbase) {
  meTargetType = TargetType::WINDOW;

  ///////////////////////
  auto glfw_container = (CtxGLFW*)pctxbase;
  auto glfw_window    = glfw_container->_glfwWindow;

  ///////////////////////
  GlIxPlatformObject* plato = new GlIxPlatformObject;
  plato->_thiscontext       = glfw_container;
  mCtxBase                  = pctxbase;
  mPlatformHandle           = (void*)plato;
  ///////////////////////

  printf("glfw_container<%p> glfw_window<%p>\n", glfw_container, glfw_window);

  plato->makeCurrent();

  mFbI.SetThisBuffer(pWin);
}

/////////////////////////////////////////////////////////////////////////
// todo :: recomputeHIDPI on window move event
/////////////////////////////////////////////////////////////////////////

void recomputeHIDPI(Context* ctx) {

  _hakHIDPI      = true;
  _hakCurrentDPI = 226;
_hakMixedDPI = false;

} 

/////////////////////////////////////////////////////////////////////////

bool _HIDPI() {
  return _hakHIDPI;
}
bool _MIXEDDPI() {
  return _hakMixedDPI;
}
float _currentDPI() {
  return _hakCurrentDPI;
}
/////////////////////////////////////////////////////////////////////////

void ContextGL::initializeOffscreenContext(OffscreenBuffer* pBuf) {

  meTargetType = TargetType::OFFSCREEN;

  ///////////////////////

  miW = pBuf->GetBufferW();
  miH = pBuf->GetBufferH();

  mCtxBase = 0;

  GlIxPlatformObject* plato = new GlIxPlatformObject;
  mPlatformHandle           = (void*)plato;
  mFbI.SetThisBuffer(pBuf);

  auto global_plato = GlIxPlatformObject::_global_plato;

  plato->_thiscontext = global_plato->_thiscontext;
  plato->_needsInit   = false;

  _defaultRTG = new RtGroup(this, miW, miH, 1);
  auto rtb    = new RtBuffer(RtgSlot::Slot0, EBufferFormat::RGBA8, miW, miH);
  _defaultRTG->SetMrt(0, rtb);
  auto texture = _defaultRTG->GetMrt(0)->texture();
  FBI()->SetBufferTexture(texture);

  // pBuf->SetContext(this);
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::initializeLoaderContext() {

  meTargetType = TargetType::LOADING;

  miW = 8;
  miH = 8;

  mCtxBase = 0;

  GlIxPlatformObject* plato = new GlIxPlatformObject;
  mPlatformHandle           = (void*)plato;

  auto global_plato   = GlIxPlatformObject::_global_plato;
  plato->_thiscontext = global_plato->_thiscontext;
  plato->_needsInit   = false;

  _defaultRTG = new RtGroup(this, miW, miH, 16);
  auto rtb    = new RtBuffer(RtgSlot::Slot0, EBufferFormat::RGBA8, miW, miH);
  _defaultRTG->SetMrt(0, rtb);
  auto texture = _defaultRTG->GetMrt(0)->texture();
  FBI()->SetBufferTexture(texture);

  plato->_bindop = [=]() {
    if (this->mTargetDrawableSizeDirty) {
      int w = mainSurfaceWidth();
      int h = mainSurfaceHeight();
      // printf("resizing defaultRTG<%p>\n", _defaultRTG);
      _defaultRTG->Resize(w, h);
      mTargetDrawableSizeDirty = false;
    }
  };
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::makeCurrentContext(void) {
  auto plato = (GlIxPlatformObject*)mPlatformHandle;
  OrkAssert(plato);
  if (plato) {
    plato->makeCurrent();
    plato->_bindop();
  }
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::SwapGLContext(CTXBASE* pCTFL) {
  GlIxPlatformObject* plato = (GlIxPlatformObject*)mPlatformHandle;
  OrkAssert(plato);
  if (plato) {
    plato->makeCurrent();
    plato->swapBuffers();
  }
}

/////////////////////////////////////////////////////////////////////////

void* ContextGL::_doBeginLoad() {
  void* pvoiddat = nullptr;

  while (false == _loadTokens.try_pop(pvoiddat)) {
    usleep(1 << 10);
  }
  GlxLoadContext* loadctx = (GlxLoadContext*)pvoiddat;
  GLFWwindow* current_window = glfwGetCurrentContext();

  loadctx->_pushedContext = current_window;
  loadctx->_global_plato->makeCurrent();
  return pvoiddat;
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::_doEndLoad(void* ploadtok) {
  GlxLoadContext* loadctx = (GlxLoadContext*)ploadtok;

  auto pushed = loadctx->_pushedContext;
   glfwMakeContextCurrent(pushed);
  _loadTokens.push(ploadtok);
}

/////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////

#endif // #if defined( ORK_CONFIG_OPENGL ) && defined(ORK_CONFIG_IX)
