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
#include <ork/kernel/environment.h>
#include <ork/file/path.h>
#include <dlfcn.h>

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

#if defined(RENDERDOC_API_ENABLED)
  static RENDERDOC_API_1_6_0* _glrenderdocAPI = nullptr;
#endif

void ContextGL::_doTriggerFrameDebugCapture() {
}
void ContextGL::_doBeginFrame() {
#if defined(RENDERDOC_API_ENABLED)
  if(_glrenderdocAPI and _isFrameDebugCapture){
    printf( "RENDERDOC BEGIN CAPTURE FRAME\n");
    _glrenderdocAPI->StartFrameCapture(NULL, NULL);
  }
#endif
}
void ContextGL::_doEndFrame() {
#if defined(RENDERDOC_API_ENABLED)
  if(_glrenderdocAPI and _isFrameDebugCapture ){
    _glrenderdocAPI->EndFrameCapture(NULL, NULL);
    uint32_t numcap = _glrenderdocAPI->GetNumCaptures();
    printf( "RENDERDOC END CAPTURE FRAME numcap<%u>\n", numcap );

  }
  _isFrameDebugCapture = false;
#endif
}

void setAlwaysOnTop(GLFWwindow *window) {
    Display *display = glfwGetX11Display();
    auto x11window = glfwGetX11Window(window);

    Atom wmStateAbove = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);
    Atom wmState = XInternAtom(display, "_NET_WM_STATE", False);

    XEvent event;
    memset(&event, 0, sizeof(event));
    event.type = ClientMessage;
    event.xclient.window = x11window;
    event.xclient.message_type = wmState;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
    event.xclient.data.l[1] = wmStateAbove;
    event.xclient.data.l[2] = 0;
    event.xclient.data.l[3] = 0;
    event.xclient.data.l[4] = 0;

    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
}

extern std::atomic<int> __FIND_IT;

bool g_allow_HIDPI = false;

using x11_window_t = ::Window; // contained alias of X11 Window Class (conflicts with lev2::Window)

bool _hakHIDPI       = false;
bool _hakMixedDPI    = false;
float _hakCurrentDPI = 95.0f;

ork::MpMcBoundedQueue<void*> ContextGL::_loadTokens;

struct GlIxPlatformObject {
  static GlIxPlatformObject* _global_plato;
  static GlIxPlatformObject* _current;
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
  void makeCurrent() {
    _current = this;
    if( _ctxbase ){
      _ctxbase->makeCurrent();
    }
  }
  void swapBuffers() {
    if( _ctxbase ){
      _ctxbase->swapBuffers();
    }
  }
  /////////////////////////////////////
  CtxGLFW* _ctxbase = nullptr;
  bool _needsInit       = true;
  void_lambda_t _bindop;
  /////////////////////////////////////
};
GlIxPlatformObject* GlIxPlatformObject::_global_plato = nullptr;
GlIxPlatformObject* GlIxPlatformObject::_current      = nullptr;
/////////////////////////////////////////////////////////////////////////

struct GlxLoadContext {
  GlIxPlatformObject* _global_plato = nullptr;
  GLFWwindow* _pushedContext        = nullptr;
};

/////////////////////////////////////////////////////////////////////////

void* ContextGL::_doClonePlatformHandle() const {
  auto plato = (GlIxPlatformObject*)mPlatformHandle;
  auto new_plato = new GlIxPlatformObject;
  new_plato->_ctxbase = nullptr; //plato->_ctxbase;
  //new_plato->_context = plato->_context;
  new_plato->_needsInit   = false;
  //new_plato->_bindop = plato->_bindop;

  // TODO : https://github.com/tweakoz/orkid/issues/139
  
  return new_plato;
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

  global_ctxbase->makeCurrent();
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
  
  #if defined(RENDERDOC_API_ENABLED)
  std::string renderdoc_path;
  if( genviron.get("RENDERDOC_DIR", renderdoc_path) ){
    auto renderdoc_lib = renderdoc_path + "/lib/librenderdoc.so";
    if(void *mod = dlopen(renderdoc_lib.c_str(), RTLD_NOW | RTLD_NOLOAD)) {
      pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
      int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void **)&_glrenderdocAPI);
      OrkAssert(ret == 1);
      _glrenderdocAPI->SetCaptureFilePathTemplate(file::Path::temp_dir().c_str());
    }
  }
  else{
    auto the_error = dlerror();
    printf( "dlerror<%s>\n", the_error );
  }
  #endif

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
    , mRsI(*this)
    , mMtxI(*this)
    , mGbI(*this)
    , mFbI(*this)
    , mTxI(*this)
    , mDWI(*this)
#if defined(ENABLE_COMPUTE_SHADERS)
    , mCI(*this)
#endif
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
  GlIxPlatformObject* plato = new GlIxPlatformObject;
  plato->_ctxbase       = glfw_container;
  mCtxBase                  = pctxbase;
  mPlatformHandle           = (void*)plato;
  ///////////////////////
  plato->makeCurrent();
  mFbI.SetThisBuffer(pWin);
  _GL_RENDERER = (const char*) glGetString(GL_RENDERER);
  //printf( "GL_RENDERER<%s>\n", _GL_RENDERER.c_str() );
  bool is_virgl = _GL_RENDERER.contains("virgl");
  bool is_wsl = _GL_RENDERER.contains("D3D12");
  bool is_force = false;
  #if ! defined(OPENGL_46)
    is_force = true;
  #endif


  if(is_virgl or is_wsl or is_force){
    _ixDisableVIRGL(this);
  }

}

/////////////////////////////////////////////////////////////////////////
// todo :: recomputeHIDPI on window move event
/////////////////////////////////////////////////////////////////////////

void recomputeHIDPI(Context* ctx) {

  switch (ctx->meTargetType) {
    case TargetType::WINDOW:
      break;
    default:
      return;
  }

  auto ixplato = (GlIxPlatformObject*)ctx->mPlatformHandle;
  ///////////////////////
  auto glfw_container     = (CtxGLFW*)ctx->GetCtxBase();
  GLFWwindow* glfw_window = glfw_container->_glfwWindow;
  ///////////////////////
  Display* x_dpy = ixplato->getDisplay();
  int x_screen   = ixplato->getXscreenID();
  int x_window   = ixplato->getXwindowID();
  ///////////////////////
  int winpos_x = 0;
  int winpos_y = 0;
  x11_window_t child;
  x11_window_t root_window = DefaultRootWindow(x_dpy);
  XTranslateCoordinates(x_dpy, x_window, root_window, 0, 0, &winpos_x, &winpos_y, &child);
  XWindowAttributes xwa;
  XGetWindowAttributes(x_dpy, x_window, &xwa);
  winpos_x -= xwa.x;
  winpos_y -= xwa.y;

  int numlodpi = 0;
  int numhidpi = 0;
  // printf("winx: %d winy: %d\n", winpos_x, winpos_y);
  ///////////////////////
  // int DWMM       = DisplayWidthMM(x_dpy, x_screen);
  // int DHMM       = DisplayHeightMM(x_dpy, x_screen);
  // int RESW       = DisplayWidth(x_dpy, x_screen);
  // int RESH       = DisplayHeight(x_dpy, x_screen);
  // float CDPIX    = float(RESW) / float(DWMM) * 25.4f;
  // float CDPIY    = float(RESH) / float(DHMM) * 25.4f;
  // int DPIX       = QX11Info::appDpiX(x_screen);
  // int DPIY       = QX11Info::appDpiY(x_screen);
  // float avgdpi = (CDPIX + CDPIY) * 0.5f;
  // printf("qx11dpi<%d %d>\n", DPIX, DPIY);
  //_hakHIDPI = avgdpi > 180.0;

  if (0) { // get DPI for screen
    XRRScreenResources* xrrscreen = XRRGetScreenResources(x_dpy, x_window);

    // printf("x_dpy<%p> x_window<%d> xrrscreen<%p>\n", x_dpy, x_window, xrrscreen);

    if (xrrscreen) {
      for (int iscres = xrrscreen->noutput; iscres > 0;) {
        --iscres;
        XRROutputInfo* info = XRRGetOutputInfo(x_dpy, xrrscreen, xrrscreen->outputs[iscres]);
        double mm_width     = info->mm_width;
        double mm_height    = info->mm_height;

        RRCrtc crtcid          = info->crtc;
        XRRCrtcInfo* crtc_info = XRRGetCrtcInfo(x_dpy, xrrscreen, crtcid);

        /*printf(
            "iscres<%d> info<%p> mm_width<%g> mm_height<%g> crtcid<%lu> crtc_info<%p>\n",
            iscres,
            info,
            mm_width,
            mm_height,
            crtcid,
            crtc_info);*/

        if (crtc_info) {
          double pix_left   = crtc_info->x;
          double pix_top    = crtc_info->y;
          double pix_width  = crtc_info->width;
          double pix_height = crtc_info->height;
          int rot           = crtc_info->rotation;
          float CDPIX       = pix_width / mm_width * 25.4f;
          float CDPIY       = pix_height / mm_height * 25.4f;
          float avgdpi      = (CDPIX + CDPIY) * 0.5f;
          float is_hidpi    = avgdpi > 180.0f;

          if (not g_allow_HIDPI)
            is_hidpi = false;

          if (is_hidpi)
            numhidpi++;
          else
            numlodpi++;

          if ((winpos_x >= pix_left) and (winpos_x < (pix_left + pix_width)) and (winpos_y >= pix_top) and
              (winpos_y < (pix_left + pix_height))) {
            _hakHIDPI      = is_hidpi;
            _hakCurrentDPI = avgdpi;
            // printf("_hakHIDPI<%d>\n", int(_hakHIDPI));
          }

          switch (rot) {
            case RR_Rotate_0:
              break;
            case RR_Rotate_90: //
              break;
            case RR_Rotate_180:
              break;
            case RR_Rotate_270:
              break;
          }

          // printf("  x<%g> y<%g> w<%g> h<%g> rot<%d> avgdpi<%g>\n", pix_left, pix_top, pix_width, pix_height, rot, avgdpi);

          XRRFreeCrtcInfo(crtc_info);
        }

        XRRFreeOutputInfo(info);
      } // for each screen
      // printf("numhidpi<%d>\n", numhidpi);
      // printf("numlodpi<%d>\n", numlodpi);
      _hakMixedDPI = (numhidpi > 0) and (numlodpi > 1);
      // printf("_hakMixedDPI<%d>\n", int(_hakMixedDPI));
    }
  }
} // namespace lev2

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

void ContextGL::initializeOffscreenContext(DisplayBuffer* pBuf) {

  meTargetType = TargetType::OFFSCREEN;

  ///////////////////////

  miW = pBuf->GetBufferW();
  miH = pBuf->GetBufferH();

  mCtxBase = 0;

  GlIxPlatformObject* plato = new GlIxPlatformObject;
  mPlatformHandle           = (void*)plato;
  mFbI.SetThisBuffer(pBuf);

  auto global_plato = GlIxPlatformObject::_global_plato;

  plato->_ctxbase = global_plato->_ctxbase;
  plato->_needsInit   = false;

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

  GlIxPlatformObject* plato = new GlIxPlatformObject;
  mPlatformHandle           = (void*)plato;

  auto global_plato   = GlIxPlatformObject::_global_plato;
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
      #if ! defined(OPENGL_46)
          _ixDisableVIRGL(this);
      #endif
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
  if (plato && (plato->getXwindowID() > 0)) {
    plato->makeCurrent();
    plato->swapBuffers();
  }
}


void ContextGL::swapBuffers(CTXBASE* ctxbase) {
  SwapGLContext(ctxbase);
}

/////////////////////////////////////////////////////////////////////////

void* ContextGL::_doBeginLoad() {
  void* pvoiddat = nullptr;

  while (false == _loadTokens.try_pop(pvoiddat)) {
    ork::usleep(1000);
  }
  GlxLoadContext* loadctx    = (GlxLoadContext*)pvoiddat;
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
