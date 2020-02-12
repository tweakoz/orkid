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

#if defined(ORK_CONFIG_OPENGL) && defined(LINUX)

#include <ork/lev2/qtui/qtui.h>
#include <QtCore/QMetaObject>
#include <QtX11Extras/QX11Info>
#include <GL/glx.h>

#include <QtGui/qpa/qplatformnativeinterface.h>
extern "C" {
#include <X11/extensions/Xrandr.h>
}

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::ContextGL, "ContextGL")

extern "C" {
extern bool gbVSYNC;
}

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

typedef ::Window x11_window_t; // contained alias of X11 Window Class (conflicts with lev2::Window)

bool _hakHIDPI       = false;
float _hakCurrentDPI = 95.0f;

ork::MpMcBoundedQueue<void*> ContextGL::_loadTokens;

struct GlIxPlatformObject {
  static GLXContext gShareMaster;
  static GLXFBConfig* gFbConfigs;
  static XVisualInfo* gVisInfo;
  static Display* gDisplay;

  GLXContext mGlxContext;
  Display* mDisplay;
  int mXWindowId;
  bool mbInit;
  void_lambda_t _bindop;

  GlIxPlatformObject()
      : mbInit(true)
      , mGlxContext(nullptr)
      , mDisplay(nullptr)
      , mXWindowId(0) {
    _bindop = [=]() {};
  }
};

/////////////////////////////////////////////////////////////////////////

struct GlxLoadContext {
  GLXContext mGlxContext;
  GLXContext mPushedContext;
  x11_window_t mWindow;
  Display* mDisplay;
};

/////////////////////////////////////////////////////////////////////////

Display* GlIxPlatformObject::gDisplay       = nullptr;
GLXContext GlIxPlatformObject::gShareMaster = nullptr;
GLXFBConfig* GlIxPlatformObject::gFbConfigs = nullptr;
XVisualInfo* GlIxPlatformObject::gVisInfo   = nullptr;
static ork::atomic<int> atomic_init;
int g_rootwin = 0;
///////////////////////////////////////////////////////////////////////////////

static int g_glx_win_attrlist[] = {
    // GLX_RGBA,
    // GLX_DOUBLEBUFFER, False,
    // GLX_RED_SIZE, 1,
    // GLX_GREEN_SIZE, 1,
    // GLX_BLUE_SIZE, 1,
    // None
    GLX_X_RENDERABLE,
    True,
    GLX_RENDER_TYPE,
    GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE,
    GLX_WINDOW_BIT,
    GLX_X_VISUAL_TYPE,
    GLX_TRUE_COLOR,
    //    GLX_RED_SIZE, 8,
    //    GLX_GREEN_SIZE, 8,
    //    GLX_BLUE_SIZE, 8,
    //    GLX_ALPHA_SIZE, 8,
    //    GLX_DEPTH_SIZE, 24,
    //    GLX_STENCIL_SIZE, 8,
    GLX_SAMPLE_BUFFERS,
    1, // <-- MSAA
    GLX_SAMPLES,
    16, // <-- MSAA
    GLX_DOUBLEBUFFER,
    True,
    None};

static int g_glx_off_attrlist[] = {
    // GLX_RGBA,
    // GLX_DOUBLEBUFFER, False,
    // GLX_RED_SIZE, 1,
    // GLX_GREEN_SIZE, 1,
    // GLX_BLUE_SIZE, 1,
    // None
    GLX_X_RENDERABLE,
    True,
    GLX_RENDER_TYPE,
    GLX_RGBA_BIT,
    GLX_X_VISUAL_TYPE,
    GLX_TRUE_COLOR,
    //    GLX_RED_SIZE, 8,
    //    GLX_GREEN_SIZE, 8,
    //    GLX_BLUE_SIZE, 8,
    //    GLX_ALPHA_SIZE, 8,
    //    GLX_DEPTH_SIZE, 24,
    //    GLX_STENCIL_SIZE, 8,
    GLX_SAMPLE_BUFFERS,
    1, // <-- MSAA
    GLX_SAMPLES,
    16, // <-- MSAA
    GLX_DOUBLEBUFFER,
    True,
    None};

/*int glx_attribs[] =
{
    GLX_RGBA,
    GLX_DOUBLEBUFFER, True,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    None
};*/

typedef GLXContext (*glXcca_proc_t)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
static GLXFBConfig gl_this_fb_config;
static glXcca_proc_t GLXCCA    = nullptr;
PFNGLPATCHPARAMETERIPROC GLPPI = nullptr;

static int gl46_context_attribs[] = {
    GLX_CONTEXT_MAJOR_VERSION_ARB,
    4,
    GLX_CONTEXT_MINOR_VERSION_ARB,
    6,
    GLX_CONTEXT_PROFILE_MASK_ARB,
    GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
#if 1
    GLX_CONTEXT_FLAGS_ARB,
    GLX_CONTEXT_DEBUG_BIT_ARB,
#endif
    None};

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

  // orkprintf( "INITOPENGL\n" );

  GLXFBConfig fb_config;
  // XInitThreads();
  Display* x_dpy = XOpenDisplay(0);
  assert(x_dpy != 0);
  int x_screen                   = 0; // screen?
  int inumconfigs                = 0;
  GlIxPlatformObject::gFbConfigs = glXChooseFBConfig(x_dpy, x_screen, g_glx_win_attrlist, &inumconfigs);
  // printf( "gFbConfigs<%p>\n", (void*) GlIxPlatformObject::gFbConfigs );
  // printf( "NUMCONFIGS<%d>\n", inumconfigs );
  assert(inumconfigs > 0);

  gl_this_fb_config = GlIxPlatformObject::gFbConfigs[0];

  GlIxPlatformObject::gVisInfo = glXGetVisualFromFBConfig(x_dpy, gl_this_fb_config);
  XVisualInfo* vi              = GlIxPlatformObject::gVisInfo;
  // printf( "vi<%p>\n", (void*) vi );

  // printf( "numfbconfig<%d>\n", inumconfigs );
  assert(GlIxPlatformObject::gFbConfigs != 0);

  GlIxPlatformObject::gDisplay = x_dpy;

  g_rootwin = RootWindow(x_dpy, x_screen);
  XSetWindowAttributes swa;
  swa.colormap     = XCreateColormap(x_dpy, g_rootwin, vi->visual, AllocNone);
  swa.border_pixel = 0;
  // swa.background_pixel = 0;
  swa.event_mask        = StructureNotifyMask;
  swa.override_redirect = true;

  uint32_t win_flags = CWBorderPixel;
  //    win_flags |= CWBackPixel;
  win_flags |= CWColormap;
  win_flags |= CWEventMask;
  win_flags |= CWOverrideRedirect;
  x11_window_t dummy_win = XCreateWindow(
      x_dpy,
      g_rootwin,
      0,
      0,
      1,
      1,
      0,
      GlIxPlatformObject::gVisInfo->depth,
      InputOutput,
      GlIxPlatformObject::gVisInfo->visual,
      win_flags,
      &swa);

  XMapWindow(x_dpy, dummy_win);

  ///////////////////////////////////////////////////////////////
  // create oldschool context just to determine if the newschool method
  //  is available
  ///////////////////////////////////////////////////////////////

  GLXContext old_school = glXCreateContext(x_dpy, GlIxPlatformObject::gVisInfo, nullptr, GL_TRUE);

  GLXCCA = (glXcca_proc_t)glXGetProcAddress((const GLubyte*)"glXCreateContextAttribsARB");

  GLPPI = (PFNGLPATCHPARAMETERIPROC)glXGetProcAddress((const GLubyte*)"glPatchParameteri");

  assert(GLPPI != nullptr);

  glXMakeCurrent(x_dpy, 0, 0);
  glXDestroyContext(x_dpy, old_school);

  if (GLXCCA == nullptr) {
    printf("glXCreateContextAttribsARB entry point not found. Aborting.\n");
    assert(false);
  }

  ///////////////////////////////////////////////////////////////

  GlIxPlatformObject::gShareMaster = GLXCCA(x_dpy, gl_this_fb_config, NULL, true, gl46_context_attribs);

  glXMakeCurrent(x_dpy, dummy_win, GlIxPlatformObject::gShareMaster);

  assert(GlIxPlatformObject::gShareMaster != nullptr);

  // printf( "display<%p> screen<%d> rootwin<%d> numcfgs<%d> gsharemaster<%p>\n", x_dpy, x_screen, g_rootwin, inumconfigs,
  // GlIxPlatformObject::gShareMaster );

  ///////////////////////////////////////////////////////////////

  if (!gladLoadGL()) {
    exit(-1);
  }

  printf("OpenGL Version %d.%d loaded\n", GLVersion.major, GLVersion.minor);

  // printf( "glad_glDrawMeshTasksNV<%p>\n", glad_glDrawMeshTasksNV );
  // printf( "glad_glDrawMeshTasksIndirectNV<%p>\n", glad_glDrawMeshTasksIndirectNV );
  // printf( "glad_glMultiDrawMeshTasksIndirectNV<%p>\n", glad_glMultiDrawMeshTasksIndirectNV );
  // printf( "glad_glMultiDrawMeshTasksIndirectCountNV<%p>\n", glad_glMultiDrawMeshTasksIndirectCountNV );/

  // printf( "glObjectLabel<%p>\n", glObjectLabel );
  // printf( "glPushDebugGroup<%p>\n", glPushDebugGroup );
  // printf( "glPopDebugGroup<%p>\n", glPopDebugGroup );

  // printf( "glad_glInsertEventMarkerEXT<%p>\n", glad_glInsertEventMarkerEXT );

  glInsertEventMarkerEXT = glad_glInsertEventMarkerEXT;
  // glPushGroupMarkerEXT = glad_glPushDebugGroup;
  // glPopGroupMarkerEXT = glad_glPopDebugGroup;

  // printf( "GLAD_GL_EXT_debug_label<%d>\n", int(GLAD_GL_EXT_debug_label) );
  // printf( "GLAD_GL_EXT_debug_marker<%d>\n", int(GLAD_GL_EXT_debug_marker) );
  // printf( "GLAD_GL_NV_mesh_shader<%d>\n", int(GLAD_GL_NV_mesh_shader) );
  // printf( "GLAD_GL_KHR_debug<%d>\n", int(GLAD_GL_KHR_debug));

  for (int i = 0; i < 1; i++) {
    GlxLoadContext* loadctx = new GlxLoadContext;

    GLXContext gctx = GLXCCA(x_dpy, gl_this_fb_config, GlIxPlatformObject::gShareMaster, GL_TRUE, gl46_context_attribs);

    loadctx->mGlxContext = gctx;
    loadctx->mWindow     = g_rootwin;
    loadctx->mDisplay    = GlIxPlatformObject::gDisplay;

    _loadTokens.push((void*)loadctx);
  }
}

///////////////////////////////////////////////////////////////////////////////

std::string GetGlErrorString(void);

void OpenGlContextInit() {
  GfxEnv::setContextClass(ContextGL::GetClassStatic());
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
    , mCI(*this) {
  ContextGL::GLinit();

  FxInit();
}

/////////////////////////////////////////////////////////////////////////

ContextGL::~ContextGL() {
}

/////////////////////////////////////////////////////////////////////////

static QWindow* windowForWidget(const QWidget* widget) {
  QWindow* window = widget->windowHandle();
  if (window)
    return window;
  auto nativeParent = widget->nativeParentWidget();
  if (nativeParent)
    return nativeParent->windowHandle();
  return 0;
}

x11_window_t getHandleForWidget(const QWidget* widget) {
  auto window = windowForWidget(widget);
  if (window) {
    auto PNI = QGuiApplication::platformNativeInterface();
    return (x11_window_t)(PNI->nativeResourceForWindow(QByteArrayLiteral("handle"), window));
  }
  return 0;
}
void ContextGL::initializeWindowContext(Window* pWin, CTXBASE* pctxbase) {
  meTargetType = ETGTTYPE_WINDOW;

  ///////////////////////
  GlIxPlatformObject* plato = new GlIxPlatformObject;
  mCtxBase                  = pctxbase;
  mPlatformHandle           = (void*)plato;
  ///////////////////////
  CTQT* pctqt    = (CTQT*)pctxbase;
  auto pctxW     = pctqt->GetQWidget();
  Display* x_dpy = QX11Info::display();
  int x_screen   = QX11Info::appScreen();
  int x_window   = getHandleForWidget(pctxW);

  // auto pvis = (XVisualInfo*) x11info.visual();
  XVisualInfo* vinfo = GlIxPlatformObject::gVisInfo;

  plato->mGlxContext = GLXCCA(x_dpy, gl_this_fb_config, plato->gShareMaster, GL_TRUE, gl46_context_attribs);

  plato->mDisplay   = x_dpy;
  plato->mXWindowId = pctxW->winId();
  // printf( "ctx<%p>\n", plato->mGlxContext );

  makeCurrentContext();

  PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT;
  PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA;
  PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI;

  glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddress((const GLubyte*)"glXSwapIntervalEXT");
  if (glXSwapIntervalEXT != NULL) {
    glXSwapIntervalEXT(x_dpy, x_window, 0);
    // printf( "DISABLEVSYNC VIA EXT\n");
  }
  // else
  // glXSwapIntervalMESA = (PFNGLXSWAPINTERVALMESAPROC)glXGetProcAddress( (const GLubyte*)"glXSwapIntervalMESA");
  // if ( glXSwapIntervalMESA != NULL )
  // glXSwapIntervalMESA(0);
  else {
    glXSwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC)glXGetProcAddress((const GLubyte*)"glXSwapIntervalSGI");
    if (glXSwapIntervalSGI != NULL)
      glXSwapIntervalSGI(0);
    printf("DISABLEVSYNC VIA SGI\n");
  }

  mFbI.SetThisBuffer(pWin);
}

/////////////////////////////////////////////////////////////////////////
// todo :: recomputeHIDPI on window move event
/////////////////////////////////////////////////////////////////////////

void recomputeHIDPI(Context* ctx) {

  switch (ctx->meTargetType) {
    case ETGTTYPE_WINDOW:
      break;
    default:
      return;
  }

  auto ixplato = (GlIxPlatformObject*)ctx->mPlatformHandle;

  ///////////////////////
  Display* x_dpy = QX11Info::display();
  int x_screen   = QX11Info::appScreen();
  int x_window   = ixplato->mXWindowId;
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

        if ((winpos_x >= pix_left) and (winpos_x < (pix_left + pix_width)) and (winpos_y >= pix_top) and
            (winpos_y < (pix_left + pix_height))) {
          float CDPIX    = pix_width / mm_width * 25.4f;
          float CDPIY    = pix_height / mm_height * 25.4f;
          float avgdpi   = (CDPIX + CDPIY) * 0.5f;
          _hakHIDPI      = avgdpi > 180.0f;
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
    }
  }
} // namespace lev2

/////////////////////////////////////////////////////////////////////////

bool _HIDPI() {
  return _hakHIDPI;
}
float _currentDPI() {
  return _hakCurrentDPI;
}
/////////////////////////////////////////////////////////////////////////

void ContextGL::initializeOffscreenContext(OffscreenBuffer* pBuf) {

  meTargetType = ETGTTYPE_OFFSCREEN;

  ///////////////////////

  miW = pBuf->GetBufferW();
  miH = pBuf->GetBufferH();

  mCtxBase = 0;

  GlIxPlatformObject* plato = new GlIxPlatformObject;
  mPlatformHandle           = (void*)plato;
  mFbI.SetThisBuffer(pBuf);

  plato->mGlxContext = GlIxPlatformObject::gShareMaster;
  plato->mbInit      = false;
  plato->mDisplay    = GlIxPlatformObject::gDisplay;
  plato->mXWindowId  = g_rootwin;

  _defaultRTG = new RtGroup(this, miW, miH, 1);
  auto rtb    = new RtBuffer(ERTGSLOT0, EBUFFMT_RGBA8, miW, miH);
  _defaultRTG->SetMrt(0, rtb);
  auto texture = _defaultRTG->GetMrt(0)->texture();
  FBI()->SetBufferTexture(texture);

  ///////////////////////////////////////////
  // create material

  GfxMaterialUITextured* pmtl = new GfxMaterialUITextured(this);
  pBuf->SetMaterial(pmtl);
  pmtl->SetTexture(ETEXDEST_DIFFUSE, texture);
  pBuf->SetTexture(texture);

  //	makeCurrentContext();

  //////////////////////////////////////////////

  // mFbI.InitializeContext( pBuf );
  //////////////////////////////////////////////

  pBuf->SetContext(this);
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::initializeLoaderContext() {

  meTargetType = ETGTTYPE_LOADING;

  miW = 8;
  miH = 8;

  mCtxBase = 0;

  GlIxPlatformObject* plato = new GlIxPlatformObject;
  mPlatformHandle           = (void*)plato;

  plato->mGlxContext = GlIxPlatformObject::gShareMaster;
  plato->mbInit      = false;
  plato->mDisplay    = GlIxPlatformObject::gDisplay;
  plato->mXWindowId  = g_rootwin;

  _defaultRTG = new RtGroup(this, miW, miH, 1);
  auto rtb    = new RtBuffer(ERTGSLOT0, EBUFFMT_RGBA8, miW, miH);
  _defaultRTG->SetMrt(0, rtb);
  auto texture = _defaultRTG->GetMrt(0)->texture();
  FBI()->SetBufferTexture(texture);

  plato->_bindop = [=]() {
    if (this->mTargetDrawableSizeDirty) {
      int w = mainSurfaceWidth();
      int h = mainSurfaceHeight();
      printf("resizing defaultRTG<%p>\n", _defaultRTG);
      _defaultRTG->Resize(w, h);
      mTargetDrawableSizeDirty = false;
    }
  };
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::makeCurrentContext(void) {
  GlIxPlatformObject* plato = (GlIxPlatformObject*)mPlatformHandle;
  OrkAssert(plato);
  if (plato) {
    bool bOK = glXMakeCurrent(plato->mDisplay, plato->mXWindowId, plato->mGlxContext);
    plato->_bindop();
    //	OrkAssert(bOK);
  }
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::SwapGLContext(CTXBASE* pCTFL) {
  GlIxPlatformObject* plato = (GlIxPlatformObject*)mPlatformHandle;
  OrkAssert(plato);
  if (plato && (plato->mXWindowId > 0)) {
    glXMakeCurrent(plato->mDisplay, plato->mXWindowId, plato->mGlxContext);
    glXSwapBuffers(plato->mDisplay, plato->mXWindowId);
  }
}

/////////////////////////////////////////////////////////////////////////

void* ContextGL::_doBeginLoad() {
  void* pvoiddat = nullptr;

  while (false == _loadTokens.try_pop(pvoiddat)) {
    usleep(1 << 10);
  }

  GlxLoadContext* loadctx = (GlxLoadContext*)pvoiddat;

  loadctx->mPushedContext = glXGetCurrentContext();

  bool bOK = glXMakeCurrent(loadctx->mDisplay, loadctx->mWindow, loadctx->mGlxContext);

  // printf("BEGINLOAD loadctx<%p> glx<%p> OK<%d>\n", loadctx, loadctx->mGlxContext, int(bOK));

  OrkAssert(bOK);

  return pvoiddat;
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::_doEndLoad(void* ploadtok) {
  GlxLoadContext* loadctx = (GlxLoadContext*)ploadtok;
  // printf("ENDLOAD loadctx<%p> glx<%p>\n", loadctx, loadctx->mGlxContext);
  _loadTokens.push(ploadtok);
}

/////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////

#endif // #if defined( ORK_CONFIG_OPENGL ) && defined(IX)
