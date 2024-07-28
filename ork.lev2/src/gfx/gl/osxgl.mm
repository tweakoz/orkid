////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#import <ork/pch.h>
#if defined( ORK_OSX )
#define GLFW_EXPOSE_NATIVE_COCOA
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>
#import <OpenGL/OpenGL.h>
#import <dispatch/dispatch.h>
#import <ork/lev2/gfx/gfxenv.h>
#import "gl.h"
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#import <ork/lev2/glfw/ctx_glfw.h>
#import <ork/file/fileenv.h>
#import <ork/file/filedev.h>
#import <ork/kernel/opq.h>

#import <ork/kernel/objc.h>
#include <GLFW/glfw3native.h>
#include <objc/objc.h>
#include <objc/message.h>
#include <ork/util/logger.h>

extern "C"
{
	bool gbVSYNC = false;
}

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_osxgl = logger()->createChannel("OSXGL", fvec3(0.8, 0.8, 0.5), true);

static void _osxDisableMacOs(ContextGL* cgl){
    cgl->_SUPPORTS_BINARY_PIPELINE = false;
    cgl->_SUPPORTS_BUFFER_STORAGE = false;
    cgl->_SUPPORTS_PERSISTENT_MAP = false;
    cgl->_SUPPORTS_EXTERNAL_MEMORY_OBJECT = false;
    GfxEnv::disableBC7();
}

/////////////////////////////////////////////////////////////////////////

void setAlwaysOnTop(GLFWwindow *window) {
    id glfwWindow = glfwGetCocoaWindow(window);
    //id nsWindow = ((id(*)(id, SEL))objc_msgSend)(glfwWindow, sel_registerName("window"));
    id nsWindow = glfwWindow;

    NSUInteger windowLevel = ((NSUInteger(*)(id, SEL))objc_msgSend)(nsWindow, sel_registerName("level"));
    windowLevel = CGWindowLevelForKey(kCGFloatingWindowLevelKey);
    ((void(*)(id, SEL, NSUInteger))objc_msgSend)(nsWindow, sel_registerName("setLevel:"), windowLevel);
}

bool _macosUseHIDPI = false;
bool g_allow_HIDPI = false;

ork::MpMcBoundedQueue<void*> ContextGL::_loadTokens;

struct GlOsxPlatformObject
{
  static GlOsxPlatformObject* _global_plato;
  static GlOsxPlatformObject* _current;
  /////////////////////////////////////
	ContextGL*		_context = nullptr;
  CtxGLFW* _ctxbase = nullptr;
  bool _needsInit       = true;
  void_lambda_t _bindop;

	GlOsxPlatformObject()
		: _bindop([=](){}) {
	}
  /////////////////////////////////////
  void makeCurrent() {
    _current = this;
    if(_ctxbase)
      _ctxbase->makeCurrent();
  }
  void swapBuffers() {
    if(_ctxbase)
      _ctxbase->swapBuffers();
  }
  /////////////////////////////////////
};
GlOsxPlatformObject* GlOsxPlatformObject::_global_plato = nullptr;
GlOsxPlatformObject* GlOsxPlatformObject::_current      = nullptr;

/////////////////////////////////////////////////////////////////////////

struct GlOsxLoadContext {
  GlOsxPlatformObject* _global_plato = nullptr;
  GLFWwindow* _pushedContext        = nullptr;
};

/////////////////////////////////////////////////////////////////////////

void check_debug_log()
{
#if 0 
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
#endif
}

///////////////////////////////////////////////////////////////////////////////

void ContextGL::_doTriggerFrameDebugCapture() {
}
void ContextGL::_doBeginFrame() {
}
void ContextGL::_doEndFrame() {
  _isFrameDebugCapture = false;
}

///////////////////////////////////////////////////////////////////////////////

static ork::atomic<int> atomic_init;

void ContextGL::GLinit()
{
  int iinit = atomic_init++;

  if (0 != iinit) {
    return;
  }

  auto global_ctxbase = CtxGLFW::globalOffscreenContext();

  GlOsxPlatformObject::_global_plato               = new GlOsxPlatformObject;
  GlOsxPlatformObject::_global_plato->_ctxbase = global_ctxbase;

  ////////////////////////////////////
  // load extensions
  ////////////////////////////////////

  global_ctxbase->makeCurrent();
  //gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  ////////////////////////////////////
  for (int i = 0; i < 1; i++) {
    GlOsxLoadContext* loadctx = new GlOsxLoadContext;
    loadctx->_global_plato  = GlOsxPlatformObject::_global_plato;
    _loadTokens.push((void*)loadctx);
  }
}

//	NSOpenGLPixelFormat* nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithCGLPixelFormatObj:gpixfmt];
//	plato->mNSOpenGLContext = [[NSOpenGLContext alloc] initWithFormat:nsPixelFormat shareContext:GlOsxPlatformObject::gShareMaster];

///////////////////////////////////////////////////////////////////////////////

std::string GetGlErrorString( void );

context_ptr_t OpenGlContextInit() {
	///////////////////////////////////////////////////////////
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	///////////////////////////////////////////////////////////

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
	, mImI( *this )
	, mFxI( *this )
	, mRsI( *this )
	, mMtxI( *this )
	, mGbI( *this )
	, mFbI( *this )
	, mTxI( *this )
	, mDWI(*this)
	, mTargetDrawableSizeDirty(true)
{
  ContextGL::GLinit();
  FxInit();
}

/////////////////////////////////////////////////////////////////////////

ContextGL::~ContextGL(){}

/////////////////////////////////////////////////////////////////////////

void ContextGL::initializeWindowContext( Window *pWin, CTXBASE* pctxbase  ) {
  meTargetType = TargetType::WINDOW;
  ///////////////////////
  auto glfw_container = (CtxGLFW*)pctxbase;
  auto glfw_window    = glfw_container->_glfwWindow;
  ///////////////////////
  GlOsxPlatformObject* plato = new GlOsxPlatformObject;
  plato->_ctxbase       = glfw_container;
  mCtxBase                  = pctxbase;
  mPlatformHandle           = (void*)plato;
  ///////////////////////
  miW = pWin->GetBufferW();
  miH = pWin->GetBufferH();
  ///////////////////////
  plato->makeCurrent();
  mFbI.SetThisBuffer(pWin);
  _GL_RENDERER = (const char*) glGetString(GL_RENDERER);
  logchan_osxgl->log( "GL_RENDERER<%s>", _GL_RENDERER.c_str() );
  _osxDisableMacOs(this); 
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::initializeOffscreenContext( DisplayBuffer *pBuf )
{
  meTargetType = TargetType::OFFSCREEN;

  ///////////////////////

  miW = pBuf->GetBufferW();
  miH = pBuf->GetBufferH();

  mCtxBase = 0;

  GlOsxPlatformObject* plato = new GlOsxPlatformObject;
  mPlatformHandle           = (void*)plato;
  mFbI.SetThisBuffer(pBuf);

  auto global_plato = GlOsxPlatformObject::_global_plato;

  plato->_ctxbase = global_plato->_ctxbase;
  plato->_needsInit   = false;

  _defaultRTG  = new RtGroup(this, miW, miH, MsaaSamples::MSAA_1X);
  auto rtb     = _defaultRTG->createRenderTarget(EBufferFormat::RGBA8);
  auto texture = rtb->texture();
  FBI()->SetBufferTexture(texture);
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::swapBuffers(CTXBASE* ctxbase) {
  SwapGLContext(ctxbase);
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::initializeLoaderContext() {

  meTargetType = TargetType::LOADING;

  miW = 8;
  miH = 8;

  mCtxBase = 0;

  GlOsxPlatformObject* plato = new GlOsxPlatformObject;
  mPlatformHandle           = (void*)plato;

  auto global_plato   = GlOsxPlatformObject::_global_plato;
  plato->_ctxbase = global_plato->_ctxbase;
  plato->_needsInit   = false;

  _defaultRTG  = new RtGroup(this, miW, miH, MsaaSamples::MSAA_1X);
  auto rtb     = _defaultRTG->createRenderTarget(EBufferFormat::RGBA32F);
  auto texture = rtb->texture();
  FBI()->SetBufferTexture(texture);

  plato->_bindop = [=]() {
    if (this->mTargetDrawableSizeDirty) {
      int w = mainSurfaceWidth();
      int h = mainSurfaceHeight();
      // printf("resizing defaultRTG<%p>\n", _defaultRTG);
      _defaultRTG->Resize(w, h);
      mTargetDrawableSizeDirty = false;
      _osxDisableMacOs(this);
    }
  };
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::makeCurrentContext( void ){
  auto plato = (GlOsxPlatformObject*)mPlatformHandle;
  OrkAssert(plato);
  if (plato) {
    plato->makeCurrent();
    plato->_bindop();
  }
}

/////////////////////////////////////////////////////////////////////////

void* ContextGL::_doClonePlatformHandle() const {
  auto plato = (GlOsxPlatformObject*)mPlatformHandle;
  auto new_plato = new GlOsxPlatformObject;
  new_plato->_ctxbase = nullptr; //plato->_ctxbase;
  new_plato->_context = plato->_context;
  new_plato->_needsInit   = false;
  //new_plato->_bindop = plato->_bindop;

  // TODO : https://github.com/tweakoz/orkid/issues/139

  return new_plato;
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::SwapGLContext( CTXBASE *pCTFL )
{
  GlOsxPlatformObject* plato = (GlOsxPlatformObject*)mPlatformHandle;
  OrkAssert(plato);
  if (plato) {
    plato->makeCurrent();
    plato->swapBuffers();
  }
}

/////////////////////////////////////////////////////////////////////////

void* ContextGL::_doBeginLoad()
{
  void* pvoiddat = nullptr;

  while (false == _loadTokens.try_pop(pvoiddat)) {
    usleep(1 << 10);
  }
  GlOsxLoadContext* loadctx    = (GlOsxLoadContext*)pvoiddat;
  GLFWwindow* current_window = glfwGetCurrentContext();

  loadctx->_pushedContext = current_window;
  loadctx->_global_plato->makeCurrent();
  return pvoiddat;
}

void ContextGL::_doEndLoad(void*ploadtok)
{
  GlOsxLoadContext* loadctx = (GlOsxLoadContext*)ploadtok;
  auto pushed = loadctx->_pushedContext;
  glfwMakeContextCurrent(pushed);
  _loadTokens.push(ploadtok);
}

void recomputeHIDPI(Context* ctx){
}
bool _HIDPI() {
  return _macosUseHIDPI;
}
bool _MIXEDDPI() {
  return false;
}
float _currentDPI(){
  return 221.0f; // hardcoded to macbook pro for now..
}

}}
///////////////////////////////////////////////////////////////////////////////

#endif // #if defined( ORK_CONFIG_OPENGL ) && defined(ORK_CONFIG_IX)
