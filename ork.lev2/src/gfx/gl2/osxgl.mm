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
#import <ork/file/fileenv.h>
#import <ork/file/filedev.h>
#import <ork/kernel/opq.h>

#import <ork/kernel/objc.h>
#include <GLFW/glfw3native.h>
#include <objc/objc.h>
#include <objc/message.h>

extern "C"
{
	bool gbVSYNC = false;
}

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

static void _osxDisableMacOs(ContextGL* cgl){
    cgl->_SUPPORTS_BINARY_PIPELINE = false;
    cgl->_SUPPORTS_BUFFER_STORAGE = false;
    cgl->_SUPPORTS_PERSISTENT_MAP = false;
    cgl->_SUPPORTS_EXTERNAL_MEMORY_OBJECT = false;
    GfxEnv::disableBC7();
}

/////////////////////////////////////////////////////////////////////////

bool g_allow_HIDPI = false;

struct GlOsxPlatformObject : public GlPlatformObject
{
  static GlOsxPlatformObject* _global_plato;
  /////////////////////////////////////

	GlOsxPlatformObject()
		: GlPlatformObject() {
	}
  /////////////////////////////////////
};
GlOsxPlatformObject* GlOsxPlatformObject::_global_plato = nullptr;

/////////////////////////////////////////////////////////////////////////

struct GlOsxLoadContext {
  GlOsxPlatformObject* _global_plato = nullptr;
  GLFWwindow* _pushedContext        = nullptr;
};

using load_context_ptr_t = std::shared_ptr<GlOsxLoadContext>;

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

  GlOsxPlatformObject::_global_plato->makeCurrent();
  //gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  ////////////////////////////////////
  for (int i = 0; i < 1; i++) {
    load_token_t token;
    auto loadctx = token.makeShared<GlOsxLoadContext>();
    loadctx->_global_plato  = GlOsxPlatformObject::_global_plato;
    _loadTokens.push(loadctx);
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
	, mFxI( *this )
	, mImI( *this )
	, mGbI( *this )
	, mFbI( *this )
	, mTxI( *this )
	, mMtxI( *this )
	, mDWI(*this)
	, mCI(*this)
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
  auto plato = std::make_shared<GlOsxPlatformObject>();
  plato->_ctxbase       = glfw_container;
  mCtxBase                  = pctxbase;
  _impl.set<glplato_ptr_t>(plato);
  ///////////////////////
  miW = pWin->GetBufferW();
  miH = pWin->GetBufferH();
  ///////////////////////
  plato->makeCurrent();
  mFbI.SetThisBuffer(pWin);
  _GL_RENDERER = (const char*) glGetString(GL_RENDERER);
  printf( "GL_RENDERER<%s>\n", _GL_RENDERER.c_str() );
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

  auto plato = std::make_shared<GlOsxPlatformObject>();
  _impl.setShared<GlPlatformObject>(plato);

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

//void ContextGL::swapBuffers(CTXBASE* ctxbase) {
//  SwapGLContext(ctxbase);
//}

/////////////////////////////////////////////////////////////////////////

void ContextGL::initializeLoaderContext() {

  meTargetType = TargetType::LOADING;

  miW = 8;
  miH = 8;

  mCtxBase = 0;

  auto plato = std::make_shared<GlOsxPlatformObject>();
  _impl.setShared<GlPlatformObject>(plato);

  auto global_plato   = GlOsxPlatformObject::_global_plato;
  plato->_ctxbase = global_plato->_ctxbase;
  plato->_needsInit   = false;

  _defaultRTG  = new RtGroup(this, miW, miH, MsaaSamples::MSAA_1X);
  auto rtb     = _defaultRTG->createRenderTarget(EBufferFormat::RGBA8);
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
  auto plato = _impl.getShared<GlPlatformObject>();
  OrkAssert(plato);
  if (plato) {
    plato->makeCurrent();
    plato->_bindop();
  }
}

/////////////////////////////////////////////////////////////////////////

ctx_platform_handle_t ContextGL::_doClonePlatformHandle() const {
  auto glplato = _impl.getShared<GlPlatformObject>();
  auto macplato = std::dynamic_pointer_cast<GlOsxPlatformObject>(glplato);

  ctx_platform_handle_t rval;
  auto new_plato = rval.makeShared<GlOsxPlatformObject>();
  new_plato->_ctxbase = nullptr; //plato->_ctxbase;
  new_plato->_context = macplato->_context;
  new_plato->_needsInit   = false;
  // TODO : https://github.com/tweakoz/orkid/issues/139

  return rval;
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::SwapGLContext( CTXBASE *pCTFL )
{
  auto glplato = _impl.getShared<GlPlatformObject>();
  OrkAssert(glplato);
  if (glplato) {
    glplato->makeCurrent();
    glplato->swapBuffers();
  }
}

/////////////////////////////////////////////////////////////////////////

load_token_t ContextGL::_doBeginLoad() {
  load_token_t token;
  while (false == _loadTokens.try_pop(token)) {
    usleep(1 << 10);
  }
  auto loadctx    = token.getShared<GlOsxLoadContext>();
  GLFWwindow* current_window = glfwGetCurrentContext();
  loadctx->_pushedContext = current_window;
  loadctx->_global_plato->makeCurrent();
  return token;
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::_doEndLoad(load_token_t token) {
  auto loadctx = token.getShared<GlOsxLoadContext>();
  auto pushed = loadctx->_pushedContext;
  glfwMakeContextCurrent(pushed);
  _loadTokens.push(token); // return it..
}

/////////////////////////////////////////////////////////////////////////

void recomputeHIDPI(Context* ctx){
}

}}
///////////////////////////////////////////////////////////////////////////////

#endif // #if defined( ORK_CONFIG_OPENGL ) && defined(ORK_CONFIG_IX)
