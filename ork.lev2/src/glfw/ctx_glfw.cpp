////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/ctxbase.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/pch.h>
////////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/glfw/ctx_glfw.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/context.h>
#include <ork/lev2/imgui/imgui_impl_glfw.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/msgrouter.inl>
#include <ork/math/basicfilters.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/util/logger.h>
#include <ork/profiling.inl>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
static logchannel_ptr_t logchan_glfw = logger()->createChannel("GLFW", fvec3(0.8, 0.2, 0.6), true);
void setAlwaysOnTop(GLFWwindow* window);
///////////////////////////////////////////////////////////////////////////////
float content_scale_x = 1.0f;
float content_scale_y = 1.0f;
#if defined(__APPLE__)
extern bool _macosUseHIDPI;
const int GLFW_MODIFIER_OSCTRL = GLFW_MOD_SUPER;
#else
const int GLFW_MODIFIER_OSCTRL = GLFW_MOD_CONTROL;
#endif
bool _mouseCursorDisabled = false;

int _g_post_swap_wait_time = 0;

///////////////////////////////////////////////////////////////////////////////
static fvec2 gpos;
///////////////////////////////////////////////////////////////////////////////
ui::event_ptr_t CtxGLFW::uievent() {
  return _uievent;
}
void CtxGLFW::hideMouseCursor() {
  glfwSetInputMode(_glfwWindow, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
}
void CtxGLFW::disableMouseCursor() {
  glfwSetInputMode(_glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetInputMode(_glfwWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
  _mouseCursorDisabled = true;
}

///////////////////////////////////////////////////////////////////////////////
ui::event_constptr_t CtxGLFW::uievent() const {
  return _uievent;
}
///////////////////////////////////////////////////////////////////////////////
static GLFWmonitor* monitorForWindow(GLFWwindow* window) {
  int winX, winY;                         // window position
  glfwGetWindowPos(window, &winX, &winY); // get window position

  int monitorCount;
  GLFWmonitor** monitors = glfwGetMonitors(&monitorCount); // get all available monitors

  for (int i = 0; i < monitorCount; i++) {
    int monitorX, monitorY;
    glfwGetMonitorPos(monitors[i], &monitorX, &monitorY); // get monitor position

    int monitorWidth, monitorHeight;
    glfwGetMonitorWorkarea(monitors[i], NULL, NULL, &monitorWidth, &monitorHeight); // get monitor size

    if (winX >= monitorX && winX < monitorX + monitorWidth && winY >= monitorY && winY < monitorY + monitorHeight) {
      // The window is located on this monitor
      // ...
      return monitors[i];
    }
  }
  return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
inline int to_qtmillis(RefreshPolicyItem policy) {
  int user_millis = 0;

  if (policy._fps >= 0)
    user_millis = (policy._fps <= 0) ? 2000 : int(1000.0f / float(policy._fps));

  int qt_millis = 0;

  switch (policy._policy) {
    case EREFRESH_FASTEST:
      qt_millis = 0;
      break;
    case EREFRESH_WHENDIRTY:
      qt_millis = -1;
      break;
    case EREFRESH_FIXEDFPS:
      qt_millis = user_millis + 1;
      break;
    default:
      break;
  }
  return qt_millis;
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_refresh(GLFWwindow* window) {
  auto sink = (EventSinkGLFW*)glfwGetWindowUserPointer(window);
  if (nullptr == sink)
    return;
  sink->_on_callback_refresh();
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_winresized(GLFWwindow* window, int w, int h) {
#if defined(__APPLE__)
  if (_macosUseHIDPI) {
    w *= content_scale_x;
    h *= content_scale_y;
  }
#endif

int x, y;
glfwGetWindowPos(window, &x, &y);

logchan_glfw->log("WIN RESIZED x<%d> y<%d> w<%d> h<%d>", x, y, w, h );


  auto sink = (EventSinkGLFW*)glfwGetWindowUserPointer(window);
  if (nullptr == sink)
    return;
  sink->_on_callback_winresized(w, h);
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_fbresized(GLFWwindow* window, int w, int h) {
#if defined(__APPLE__)
  if (_macosUseHIDPI) {
    w *= content_scale_x;
    h *= content_scale_y;
  }
#endif
  auto sink = (EventSinkGLFW*)glfwGetWindowUserPointer(window);
  if (nullptr == sink)
    return;
  sink->_on_callback_fbresized(w, h);
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_contentScaleChanged(GLFWwindow* window, float sw, float sh) {
  logchan_glfw->log("fb contentscale<%p %f %f>", window, sw, sh);
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_focusChanged(GLFWwindow* window, int focus) {
  bool has_focus = (focus == GLFW_TRUE);
  ////////////////////////
  // TODO - resolve where to send input, IMGUI - or ork::lev2::ui ?
  ////////////////////////
  // ImGui_ImplGlfw_WindowFocusCallback(window, focus);
  ////////////////////////
  // printf("fb focus<%p %d>", window, focus);
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_keyboard(GLFWwindow* window, int key, int scancode, int action, int modifiers) {
  auto sink = (EventSinkGLFW*)glfwGetWindowUserPointer(window);
  if (nullptr == sink)
    return;
  sink->_on_callback_keyboard(key, scancode, action, modifiers);
}
void fillEventKeyboard(ui::event_ptr_t uiev, int key, int scancode, int action, int modifiers) {
  uiev->miKeyCode = key;
  uiev->mbALT     = (modifiers & GLFW_MOD_ALT);
  uiev->mbCTRL    = (modifiers & GLFW_MOD_CONTROL);
  uiev->mbSHIFT   = (modifiers & GLFW_MOD_SHIFT);
  uiev->mbSUPER   = (modifiers & GLFW_MOD_SUPER);
  switch (action) {
    case GLFW_PRESS:
      uiev->_eventcode = ui::EventCode::KEY_DOWN;
      break;
    case GLFW_RELEASE:
      uiev->_eventcode = ui::EventCode::KEY_UP;
      break;
    case GLFW_REPEAT:
      uiev->_eventcode = ui::EventCode::KEY_REPEAT;
      break;
  }
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_mousebuttons(GLFWwindow* window, int button, int action, int modifiers) {
  auto sink = (EventSinkGLFW*)glfwGetWindowUserPointer(window);
  if (nullptr == sink)
    return;
  sink->_on_callback_mousebuttons(button, action, modifiers);
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_scroll(GLFWwindow* window, double xoffset, double yoffset) {
  auto sink = (EventSinkGLFW*)glfwGetWindowUserPointer(window);
  if (nullptr == sink)
    return;
  sink->_on_callback_scroll(xoffset, yoffset);
}
void CtxGLFW::_on_callback_scroll(double xoffset, double yoffset) {
  auto uiev        = this->uievent();
  uiev->_eventcode = ui::EventCode::MOUSEWHEEL;

  uiev->miMWY = int(yoffset);
  uiev->miMWX = int(xoffset);

  _fire_ui_event();
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_cursor(GLFWwindow* window, double xoffset, double yoffset) {
  auto sink = (EventSinkGLFW*)glfwGetWindowUserPointer(window);
  if (nullptr == sink)
    return;
  sink->_on_callback_cursor(xoffset, yoffset);
}
void fillEventCursor(
    ui::event_ptr_t uiev,
    GLFWwindow* window,
    GLFWmonitor* monitor,
    double xoffset,
    double yoffset,
    double w,
    double h) {
  uiev->mpBlindEventData = nullptr;

  // InputManager::instance()->poll();

  // int ix = event->x();
  // int iy = event->y();
  // if (_HIDPI()) {
  // ix /= 2;
  // iy /= 2;
  //}

#if defined(__APPLE__)
  if (false and _macosUseHIDPI) {
    xoffset *= 2;
    yoffset *= 2;
  }
#endif

  uiev->miLastX = uiev->miX;
  uiev->miLastY = uiev->miY;

  uiev->miX = int(xoffset);
  uiev->miY = int(yoffset);

  float unitX = xoffset / float(w);
  float unitY = yoffset / float(h);

  uiev->mfLastUnitX = uiev->mfUnitX;
  uiev->mfLastUnitY = uiev->mfUnitY;
  uiev->mfUnitX     = unitX;
  uiev->mfUnitY     = unitY;
  uiev->miScreenWidth = w;
  uiev->miScreenHeight = h;

  //printf("CURSOR x<%d> y<%d> unitx<%g> unity<%g>\n", uiev->miX, uiev->miY, unitX, unitY);

  if (monitor) {
    int winX, winY;                         // window position
    glfwGetWindowPos(window, &winX, &winY); // get window position
    int screenX = 0;
    int screenY = 0;
    glfwGetMonitorPos(monitor, &screenX, &screenY); // get monitor position
    uiev->miScreenPosX = winX + screenX + int(xoffset);
    uiev->miScreenPosY = winY + screenY + int(yoffset);
  }
  // int winX, winY; // window coordinate to convert
  // glfwGetWindowPos(window, &winX, &winY); // get window position
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_enterleave(GLFWwindow* window, int entered) {
  auto sink = (EventSinkGLFW*)glfwGetWindowUserPointer(window);
  if (nullptr == sink)
    return;
  sink->_on_callback_enterleave(entered);
}
///////////////////////////////////////////////////////////////////////////////
CtxGLFW::CtxGLFW(Window* ork_win)
    : CTXBASE(ork_win) {

  _onRunLoopIteration = []() {};

  _uievent = std::make_shared<ui::Event>();

  _runstate = 1;

  _eventSINK = std::make_shared<EventSinkGLFW>();

  _eventSINK->_on_callback_refresh      = [=]() { _on_callback_refresh(); };
  _eventSINK->_on_callback_enterleave   = [=](int entered) { _on_callback_enterleave(entered); };
  _eventSINK->_on_callback_winresized   = [=](int w, int h) { _on_callback_winresized(w, h); };
  _eventSINK->_on_callback_fbresized    = [=](int w, int h) { _on_callback_fbresized(w, h); };
  _eventSINK->_on_callback_mousebuttons = [=](int button, int action, int modifiers) {
    _on_callback_mousebuttons(button, action, modifiers);
  };
  _eventSINK->_on_callback_keyboard = [=](int key, int scancode, int action, int modifiers) {
    _on_callback_keyboard(key, scancode, action, modifiers);
  };
  _eventSINK->_on_callback_scroll = [=](double xoffset, double yoffset) { _on_callback_scroll(xoffset, yoffset); };
  _eventSINK->_on_callback_cursor = [=](double xoffset, double yoffset) { _on_callback_cursor(xoffset, yoffset); };

  // printf("CtxGLFW created<%p> glfw_win<%p> isglobal<%d>", this, _glfwWindow, int(isGlobal()));
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::initWithData(appinitdata_ptr_t aid) {
  _appinitdata = aid;
  logchan_glfw->log( "set swap interval to<%d>", aid->_swap_interval );
  glfwSwapInterval(aid->_swap_interval);
  glfwWindowHint(GLFW_SAMPLES, aid->_msaa_samples);
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::Show() {

  GLFWmonitor* fullscreen_monitor = nullptr;
  GLFWmonitor* selected_monitor   = nullptr;

  glfwWindowHint(GLFW_RED_BITS, 10);
  glfwWindowHint(GLFW_GREEN_BITS, 10);
  glfwWindowHint(GLFW_BLUE_BITS, 10);
  glfwWindowHint(GLFW_ALPHA_BITS, 2);

  if (_orkwindow) {
    _orkwindow->SetDirty(true);

    int l = _appinitdata->_left;
    int t = _appinitdata->_top;

    if (_appinitdata->_fullscreen) {

      fullscreen_monitor = glfwGetPrimaryMonitor();


      int monitor_count = 0;
      auto monitors     = glfwGetMonitors(&monitor_count);

      int idiff = 100000;

      for (int i = 0; i < monitor_count; i++) {
        GLFWmonitor* monitor = monitors[i];
        int mon_x            = 0;
        int mon_y            = 0;
        glfwGetMonitorPos(monitor, &mon_x, &mon_y);

        /////////////////////////////////
        // select monitor whose left edge is the closest to the appinitdata's left
        /////////////////////////////////

        int d = abs(mon_x - l);

        if (d < idiff) {
          fullscreen_monitor = monitor;
          idiff              = d;
          const char* monitorName = glfwGetMonitorName(fullscreen_monitor);
          logchan_glfw->log( "USING FULLSCREEN MONITOR<%p:%s> ", fullscreen_monitor, monitorName );
        }

        /////////////////////////////////
      }

      //////////////////////////////////////
      // technically "windowed fullscreen"
      //////////////////////////////////////
      const GLFWvidmode* mode = glfwGetVideoMode(fullscreen_monitor);
      glfwWindowHint(GLFW_RED_BITS, 10);
      glfwWindowHint(GLFW_GREEN_BITS, 10);
      glfwWindowHint(GLFW_BLUE_BITS, 10);
      glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
      glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

      float contentScaleX = 1.0f;
      float contentScaleY = 1.0f;
      // fetch content scale
      glfwGetMonitorContentScale(fullscreen_monitor, &contentScaleX, &contentScaleY);


      _width  = mode->width*contentScaleX;
      _height = mode->height*contentScaleY;
      logchan_glfw->log("USING GLFW_REFRESH_RATE<%d> ", int(mode->refreshRate));
      logchan_glfw->log("USING GLFW _width<%d> ", _width);
      logchan_glfw->log("USING GLFW _height<%d> ", _height);
      logchan_glfw->log("USING GLFW redbits<%d> ", mode->redBits);
      logchan_glfw->log("USING GLFW greenbits<%d> ", mode->greenBits);
      logchan_glfw->log("USING GLFW bluebits<%d> ", mode->blueBits);
      logchan_glfw->log("USING GLFW contentScaleX<%f> ", contentScaleX);
      logchan_glfw->log("USING GLFW contentScaleY<%f> ", contentScaleY);

      _appinitdata->_width  = _width;
      _appinitdata->_height = _height;
      
      //////////////////////////////////////
      selected_monitor = fullscreen_monitor;


      this->onResize(_width, _height);

    }

#if defined(__APPLE__)
    glfwWindowHint(
        GLFW_COCOA_RETINA_FRAMEBUFFER, //
        _appinitdata->_allowHIDPI ? GLFW_TRUE : GLFW_FALSE);
#endif

    if (_appinitdata->_offscreen) {
      glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
      glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
      glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
    } else {
      glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
      glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
      glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
    }

    auto global = globalOffscreenContext();

    _glfwWindow = glfwCreateWindow(
        _width,             //
        _height,            //
        "OrkWindow",        //
        fullscreen_monitor, // monitor
        global->_glfwWindow // sharegroup
    );

    OrkAssert(_glfwWindow != nullptr);
    glfwSetWindowUserPointer(_glfwWindow, (void*)_eventSINK.get());
    glfwSetWindowAttrib(_glfwWindow, GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    if (not _appinitdata->_offscreen) {
      glfwSetWindowAttrib(_glfwWindow, GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
      // glfwSetInputMode(_glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      glfwFocusWindow(_glfwWindow);
    }

    glfwSetWindowRefreshCallback(_glfwWindow, _glfw_callback_refresh);
    glfwSetFramebufferSizeCallback(_glfwWindow, _glfw_callback_fbresized);
    glfwSetWindowSizeCallback(_glfwWindow, _glfw_callback_winresized);
    glfwSetWindowContentScaleCallback(_glfwWindow, _glfw_callback_contentScaleChanged);
    glfwSetWindowFocusCallback(_glfwWindow, _glfw_callback_focusChanged);
    glfwSetKeyCallback(_glfwWindow, _glfw_callback_keyboard);
    glfwSetMouseButtonCallback(_glfwWindow, _glfw_callback_mousebuttons);
    glfwSetScrollCallback(_glfwWindow, _glfw_callback_scroll);
    glfwSetCursorPosCallback(_glfwWindow, _glfw_callback_cursor);
    glfwSetCursorEnterCallback(_glfwWindow, _glfw_callback_enterleave);
  }

  if (_appinitdata->_fullscreen) {

    _glfw_callback_winresized(_glfwWindow, _width, _height);
    _glfw_callback_fbresized(_glfwWindow, _width, _height);

  } else {
    logchan_glfw->log("WINDOWEDMODE T<%d> L<%d> W<%d> H<%d>", //
                      _appinitdata->_top, //
                      _appinitdata->_left, //
                      _appinitdata->_width, //
                      _appinitdata->_height);

    glfwSetWindowPos(
        _glfwWindow,
        _appinitdata->_left, //
        _appinitdata->_top);
    glfwSetWindowSize(
        _glfwWindow,
        _appinitdata->_width, //
        _appinitdata->_height);
  }

  if (_needsInitialize) {
    // printf("CreateCONTEXT");
    _orkwindow->initContext();
      if( _appinitdata->_fullscreen ){
          _target->resizeMainSurface(_width, _height);
      }
    _orkwindow->OnShow();
    _needsInitialize = false;
  }
  if (not _appinitdata->_offscreen) {
    glfwShowWindow(_glfwWindow);
  }

  if (selected_monitor == nullptr) {
    selected_monitor = monitorForWindow(_glfwWindow);
    //OrkAssert(selected_monitor != nullptr);
  }

  _glfwMonitor = selected_monitor;

  glfwGetWindowContentScale(_glfwWindow, &content_scale_x, &content_scale_y);

  logchan_glfw->log( "content_scale_x<%f> content_scale_y<%f>\n", content_scale_x, content_scale_y );
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::Hide() {
  glfwHideWindow(_glfwWindow);
}
///////////////////////////////////////////////////////////////////////////////
CtxGLFW::~CtxGLFW() {
  _runstate = 2;
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::makeCurrent() {
  // printf( "CtxGLFW makeCurrent<%p> glfw_win<%p> isglobal<%d>", this, _glfwWindow, int(isGlobal()) );
  glfwMakeContextCurrent(_glfwWindow);
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::swapBuffers() {
  glfwSwapBuffers(_glfwWindow);
  if(_g_post_swap_wait_time>0){
    usleep(_g_post_swap_wait_time);
  }
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::SetAlwaysRun(bool brun) {
  mbAlwaysRun = brun;
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::signalExit() {
  _runstate = 2;
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::pollEvents() {
  glfwPollEvents();
}
///////////////////////////////////////////////////////////////////////////////
int CtxGLFW::runloop() {
  int rval = 0;

  OrkAssert(_target);

  lev2::ThreadGfxContext l2ctx_track(_target);

  _target->makeCurrentContext();

  if (_onGpuInit) {
    FontMan::gpuInit(_target);
    _onGpuInit(_target);
  }

  while (_runstate == 1) {

    EASY_BLOCK("ctx_glfw::render::pollev", profiler::colors::Red);

    //////////////////////////////
    // poll UI/windowing system events
    //////////////////////////////

    // glfwWaitEvents();
    glfwPollEvents();
    
    EASY_END_BLOCK;

    //////////////////////////////
    // run main thread app logic
    //////////////////////////////

    EASY_BLOCK("ctx_glfw::render::rli", profiler::colors::Red);

    _onRunLoopIteration();

    EASY_END_BLOCK;

    //////////////////////////////
    // redraw ?
    //////////////////////////////

    EASY_BLOCK("ctx_glfw::render::gpuu", profiler::colors::Red);

    if (_onGpuUpdate) {
      _onGpuUpdate(_target);
    }

    EASY_END_BLOCK;

    EASY_BLOCK("ctx_glfw::render::gpupre", profiler::colors::Red);

    if (_onGpuPreFrame) {
      _onGpuPreFrame(_target);
    }

    EASY_END_BLOCK;

    for( auto fn : _gpu_misc_updates ){
      fn(_target);
    }

    EASY_BLOCK("ctx_glfw::render::repaint", profiler::colors::Red);
    SlotRepaint();
    EASY_END_BLOCK;

    EASY_BLOCK("ctx_glfw::render::gpupos", profiler::colors::Red);
    if (_onGpuPostFrame) {
      _onGpuPostFrame(_target);
    }
    EASY_END_BLOCK;

    //////////////////////////////
    // check for closed window
    //////////////////////////////

    if (_glfwWindow) {
      bool window_should_close = glfwWindowShouldClose(_glfwWindow);
      if (window_should_close) {
        _runstate = 2;
      }
    }

  } //  while (_runstate == 1) {

  //////////////////////////////
  // run main thread app logic (one last time..)
  //////////////////////////////

  //_onRunLoopIteration();

  //////////////////////////////

  if (_onGpuExit) {
    _onGpuExit(_target);
  }

  //////////////////////////////

  glfwDestroyWindow(_glfwWindow);
  _runstate = 3;
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
fvec2 CtxGLFW::MapCoordToGlobal(const fvec2& v) const {
  return v; // QPoint p(v.x, v.y);
  // QPoint p2 = mpQtWidget->mapToGlobal(p);
  // return fvec2(p2.x(), p2.y());
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::onResize(int W, int H) {
  _uievent->mpBlindEventData = nullptr;
  //////////////////////////////////////////////////////////
  lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
  //////////////////////////////////////////////////////////

  if (_target) {
    _target->resizeMainSurface(W, H);
    _uievent->mpGfxWin = (Window*)_target->FBI()->GetThisBuffer();
    if (_uievent->mpGfxWin)
      _uievent->mpGfxWin->Resize(0, 0, W, H);
  }
  lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();

  _width  = W;
  _height = H;
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::_doEnqueueWindowResize(int w, int h) {
  auto op = [=]() { glfwSetWindowSize(_glfwWindow, w, h); };
  opq::mainSerialQueue()->enqueue(op);
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::SlotRepaint() {
  // OrkAssert(opq::TrackCurrent::is(opq::mainSerialQueue()));

  // auto lamb = [&]() {
  if (not GfxEnv::initialized())
    return;

  ork::PerfMarkerPush("ork.viewport.draw.begin");

  // this->mDrawLock++;
  // if (this->mDrawLock == 1) {
  //  printf( "CtxGLFW::SlotRepaint() _target<%p>", _target );
  if (this->_target) {
    _target->makeCurrentContext();
    auto gfxwin        = _uievent->mpGfxWin;
    _uievent->mpGfxWin = (Window*)_target->FBI()->GetThisBuffer();
    auto drwev         = std::make_shared<ui::DrawEvent>(this->_target);

    auto widget = gfxwin ? gfxwin->GetRootWidget() : nullptr;
    if (widget)
      widget->draw(drwev);
  }
  //}
  // this->mDrawLock--;
  ork::PerfMarkerPush("ork.viewport.draw.end");
  // glFinish();
  //};

  // lamb();
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::_setRefreshPolicy(RefreshPolicyItem newpolicy) { // final

  auto prev         = _curpolicy;
  int prev_qtmillis = to_qtmillis(prev);
  int next_qtmillis = to_qtmillis(newpolicy);

  if (next_qtmillis != prev_qtmillis) {
    if (next_qtmillis == -1) {
      // Timer().stop();
    } else {
      // Timer().start();
      // Timer().setInterval(next_qtmillis);
    }
  }

  _curpolicy = newpolicy;
}
///////////////////////////////////////////////////////////////////////////////
void error_callback(int error, const char* msg) {
  logchan_glfw->log("GLFW ERROR<%d:%s>", error, msg);
}
///////////////////////////////////////////////////////////////////////////////
CtxGLFW* CtxGLFW::globalOffscreenContext() {
  static CtxGLFW* gctx = nullptr;
  if (nullptr == gctx) {

    glfwSetErrorCallback(error_callback);

    gctx = new CtxGLFW(nullptr);

    bool ok = glfwInit();
    assert(ok);

    auto primary_monitor = glfwGetPrimaryMonitor();
    if (primary_monitor) {
      const GLFWvidmode* mode = glfwGetVideoMode(primary_monitor);

      // printf( "primary_monitor<%p>", primary_monitor );
      // printf( "mode<%p>", mode );
      // printf( "mode redbits<%d>", mode->redBits );
      // printf( "mode grnbits<%d>", mode->greenBits );
      // printf( "mode blubits<%d>", mode->blueBits );
      // printf( "mode refreshRate<%d>", mode->refreshRate );
      // glfwWindowHint(GLFW_RED_BITS, mode->redBits);
      // glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
      // glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
      // glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    std::set<int> _try_minors;
    _try_minors.insert(0);

#if defined(OPENGL_46)
    _try_minors.insert(6);
    _try_minors.insert(5);
    _try_minors.insert(3);
#elif defined(OPENGL_41)
    _try_minors.insert(1);
#endif

    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // this can fail on nvidia aarch64 devices
    //  see: https://github.com/isl-org/Open3D/issues/2549

    GLFWwindow* offscreen_window = nullptr;

    bool done = false;

    auto it_minor = _try_minors.rbegin();

    auto ctx_vars = std::make_shared<varmap::VarMap>();

    int MINOR = 0;

    while (not done) {

      int this_minor = *it_minor;

      ctx_vars->makeValueForKey<int>("GL_API_MAJOR_VERSION") = 4;
      ctx_vars->makeValueForKey<int>("GL_API_MINOR_VERSION") = this_minor;

      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, this_minor);
      #if defined(__APPLE__)
      glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
      #endif
      MINOR = this_minor;

      offscreen_window = glfwCreateWindow(
          32,      //
          32,      //
          "",      //
          nullptr, //
          nullptr);

      it_minor++;
      done |= (offscreen_window != nullptr);
      done |= (it_minor == _try_minors.rend());

      logchan_glfw->log("try minor<%d> offscreen_window<%p> done<%d>", this_minor, (void*) offscreen_window, int(done));
    }

    gctx->_vars       = ctx_vars;
    gctx->_glfwWindow = offscreen_window;

    int minor_api_version = gctx->_vars->typedValueForKey<int>("GL_API_MINOR_VERSION").value();
    logchan_glfw->log("offscreen_window<%p>", offscreen_window);
    logchan_glfw->log("global_ctxbase<%p> vars<%p> minor version<%d : %d>", gctx, (void*)ctx_vars.get(), MINOR, minor_api_version);
    OrkAssert(offscreen_window != nullptr);
    glfwSetWindowAttrib(offscreen_window, GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwSetWindowUserPointer(offscreen_window, (void*)gctx);
    glfwSwapInterval(0);
  }
  return gctx;
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::_on_callback_mousebuttons(int button, int action, int modifiers) {
  // printf("_glfw_callback_mousebuttons<%p>", window);

  ////////////////////////
  // TODO - resolve where to send input, IMGUI - or ork::lev2::ui ?
  ////////////////////////

  // ImGui_ImplGlfw_MouseButtonCallback(window, button, action, modifiers);

  ////////////////////////
  opq::mainSerialQueue()->enqueue( [=](){

  auto uiev = this->uievent();

  bool DOWN = (action == GLFW_PRESS);

  switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT:
      uiev->mbLeftButton = DOWN;
      this->_buttonState = (this->_buttonState & 6) | int(DOWN);
      break;
    case GLFW_MOUSE_BUTTON_MIDDLE:
      uiev->mbMiddleButton = DOWN;
      this->_buttonState   = (this->_buttonState & 5) | (int(DOWN) << 1);
      break;
    case GLFW_MOUSE_BUTTON_RIGHT:
      uiev->mbRightButton = DOWN;
      this->_buttonState  = (this->_buttonState & 3) | (int(DOWN) << 2);
      break;
  }

  uiev->mbALT   = (modifiers & GLFW_MOD_ALT);
  uiev->mbCTRL  = (modifiers & GLFW_MOD_CONTROL);
  uiev->mbSHIFT = (modifiers & GLFW_MOD_SHIFT);
  uiev->mbSUPER  = (modifiers & GLFW_MOD_SUPER);

  uiev->_eventcode = DOWN                           //
                         ? ork::ui::EventCode::PUSH //
                         : ork::ui::EventCode::RELEASE;

  _fire_ui_event();
  });

  /////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::_on_callback_refresh() {
  auto orkwin           = this->GetWindow();
  static int gistackctr = 0;
  static int gictr      = 0;

  gistackctr++;
  if ((1 == gistackctr) && (gictr > 0)) {
    this->uievent()->mpBlindEventData = (void*)nullptr;
    // ctx->SlotRepaint();
  }
  gistackctr--;
  gictr++;
}
void CtxGLFW::_on_callback_winresized(int w, int h) {
  opq::mainSerialQueue()->enqueue( [=](){
    this->onResize(w, h);
    auto uiev = this->uievent();
    uiev->_eventcode  = ui::EventCode::RESIZED;
    uiev->miScreenWidth = w;
    uiev->miScreenHeight = h;
    _fire_ui_event();
  });
}
void CtxGLFW::_on_callback_fbresized(int w, int h) {
  opq::mainSerialQueue()->enqueue( [=](){
  this->onResize(w, h);
  });
}
void CtxGLFW::_on_callback_keyboard(int key, int scancode, int action, int modifiers) {
  opq::mainSerialQueue()->enqueue( [=](){
  auto uiev = this->uievent();
  if (action == GLFW_PRESS && key == GLFW_KEY_V && (modifiers & GLFW_MODIFIER_OSCTRL)) {
    const char* clipboardText = glfwGetClipboardString(_glfwWindow);
    if (clipboardText) {
      uiev->_eventcode  = ui::EventCode::PASTE_TEXT;
      uiev->_paste_text = clipboardText;
      _fire_ui_event();
      return;
    }
  }
  fillEventKeyboard(uiev, key, scancode, action, modifiers);
  _fire_ui_event();
  });
}
void CtxGLFW::_on_callback_cursor(double xoffset, double yoffset) {
  EASY_BLOCK("ctx_glfw::render::OCC", profiler::colors::Red);
  opq::mainSerialQueue()->enqueue( [=](){
    auto uiev = this->uievent();
    fillEventCursor(uiev, _glfwWindow, _glfwMonitor, xoffset, yoffset, _width, _height);
    //printf( "xoffset<%d> yoffset<%d>\n", xoffset, yoffset);
    if (this->_buttonState == 0) {
      uiev->_eventcode = ui::EventCode::MOVE; //
      _fire_ui_event();
    } else {
      uiev->_eventcode = ui::EventCode::DRAG; //
      _fire_ui_event();
    }
  });
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::_on_callback_enterleave(int entered) {
  // printf("_glfw_callback_enterleave<%p> entered<%d>", window, entered);

  // ImGui_ImplGlfw_CursorEnterCallback(window, entered);
  opq::mainSerialQueue()->enqueue( [=](){

  bool was_entered = bool(entered);

  auto uiev = this->uievent();

  uiev->_eventcode = was_entered                       //
                         ? ui::EventCode::GOT_KEYFOCUS //
                         : ui::EventCode::LOST_KEYFOCUS;

  auto ork_window = this->GetWindow();

  if (ork_window) {
    if (was_entered) {
      ork_window->GotFocus();
    } else {
      ork_window->LostFocus();
    }
  }
  _fire_ui_event();
  });
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::_fire_ui_event() {
  EASY_BLOCK("ctx_glfw::render::FUE", profiler::colors::Red);
  auto uiev        = this->uievent();
  auto gfxwin      = uiev->mpGfxWin;
  auto root        = gfxwin ? gfxwin->GetRootWidget() : nullptr;
  uiev->_uicontext = root ? root->_uicontext : nullptr;
  auto op = [=]() {
    if (root) {
      uiev->setvpDim(root);
      ui::Event::sendToContext(uiev);
      //_pushTimer.Start();
    }
  };
  op();
  //opq::mainSerialQueue()->enqueue(op);
  // this->SlotRepaint(); // refresh UI after button event
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct PopupImpl {
  //////////////////////////////////////////////////
  PopupImpl(PopupWindow* win, lev2::Context* ctx, int x, int y, int w, int h) {

    _parent_context = ctx;
    _uicontext      = win->_uicontext;

    _x = x;
    _y = y;
    _w = w;
    _h = h;

    _window = win;

    auto global = CtxGLFW::globalOffscreenContext();

    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    if (win->_useTransparency) {
      glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    }
    _glfwPopupWindow = glfwCreateWindow(w, h, "Popup", NULL, global->_glfwWindow);
    glfwSetWindowPos(_glfwPopupWindow, x, y);
    setAlwaysOnTop(_glfwPopupWindow);
    //////////////////////////////////////////////////
    _eventSINK                            = std::make_shared<EventSinkGLFW>();
    _eventSINK->_on_callback_mousebuttons = [=](int button, int action, int modifiers) {
      auto uiev = std::make_shared<ui::Event>();

      bool DOWN = (action == GLFW_PRESS);

      switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT:
          uiev->mbLeftButton = DOWN;
          this->_buttonState = (this->_buttonState & 6) | int(DOWN);
          break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
          uiev->mbMiddleButton = DOWN;
          this->_buttonState   = (this->_buttonState & 5) | (int(DOWN) << 1);
          break;
        case GLFW_MOUSE_BUTTON_RIGHT:
          uiev->mbRightButton = DOWN;
          this->_buttonState  = (this->_buttonState & 3) | (int(DOWN) << 2);
          break;
      }

      uiev->mbALT   = (modifiers & GLFW_MOD_ALT);
      uiev->mbCTRL  = (modifiers & GLFW_MOD_CONTROL);
      uiev->mbSHIFT = (modifiers & GLFW_MOD_SHIFT);
      uiev->mbSUPER  = (modifiers & GLFW_MOD_SUPER);

      uiev->_eventcode = DOWN                           //
                             ? ork::ui::EventCode::PUSH //
                             : ork::ui::EventCode::RELEASE;

      uiev->miX = _mouseX;
      uiev->miY = _mouseY;

      _fireEvent(uiev);
    };
    //////////////////////////////////////////////////
    _eventSINK->_on_callback_keyboard = [=](int key, int scancode, int action, int modifiers) { //
      if (_uicontext->_top) {
        auto uiev = std::make_shared<ui::Event>();
        if (action == GLFW_PRESS && key == GLFW_KEY_V && (modifiers & GLFW_MODIFIER_OSCTRL)) {
          const char* clipboardText = glfwGetClipboardString(_glfwPopupWindow);
          if (clipboardText) {
            uiev->_eventcode  = ui::EventCode::PASTE_TEXT;
            uiev->_paste_text = clipboardText;
            _fireEvent(uiev);
            return;
          }
        } else {
          fillEventKeyboard(uiev, key, scancode, action, modifiers);
          _fireEvent(uiev);
        }
      }
    };
    //////////////////////////////////////////////////
    _eventSINK->_on_callback_cursor = [=](double xoffset, double yoffset) { //
      auto uiev = std::make_shared<ui::Event>();
      fillEventCursor(uiev, nullptr, nullptr, xoffset, yoffset, _w, _h);

      _mouseX = uiev->miX;
      _mouseY = uiev->miY;

      if (this->_buttonState == 0) {
        uiev->_eventcode = ui::EventCode::MOVE; //
        _fireEvent(uiev);
      } else {
        uiev->_eventcode = ui::EventCode::DRAG; //
        _fireEvent(uiev);
      }
    };
    //////////////////////////////////////////////////
    _eventSINK->_on_callback_scroll = [=](double xoffset, double yoffset) {
      auto uiev        = std::make_shared<ui::Event>();
      uiev->_eventcode = ui::EventCode::MOUSEWHEEL;

      uiev->miMWY = int(yoffset);
      uiev->miMWX = int(xoffset);

      _fireEvent(uiev);
    };
    ///////////////////////////////////////////////////////////////////////////////
    _eventSINK->_on_callback_enterleave = [=](int entered) {
      auto uiev        = std::make_shared<ui::Event>();
      bool was_entered = bool(entered);
      uiev->_eventcode = was_entered                       //
                             ? ui::EventCode::GOT_KEYFOCUS //
                             : ui::EventCode::LOST_KEYFOCUS;

      if (was_entered) {
        _window->GotFocus();
      } else {
        _window->LostFocus();
      }
      _fireEvent(uiev);
    };
    //////////////////////////////////////////////////
    glfwSetWindowUserPointer(_glfwPopupWindow, (void*)_eventSINK.get());
    glfwSetMouseButtonCallback(_glfwPopupWindow, _glfw_callback_mousebuttons);
    glfwSetCursorPosCallback(_glfwPopupWindow, _glfw_callback_cursor);
    glfwSetKeyCallback(_glfwPopupWindow, _glfw_callback_keyboard);
    glfwSetScrollCallback(_glfwPopupWindow, _glfw_callback_scroll);
    glfwSetCursorEnterCallback(_glfwPopupWindow, _glfw_callback_enterleave);
    glfwSetWindowAttrib(_glfwPopupWindow, GLFW_FOCUS_ON_SHOW, GLFW_TRUE);

    glfwShowWindow(_glfwPopupWindow);

    _rtgroup             = std::make_shared<lev2::RtGroup>(_parent_context, _w, _h);
    _rtgroup->_pseudoRTG = true;
    _rtgroup->mNumMrts   = 1;
    _rtgroup->_autoclear = true;

    _cloned_plato = _parent_context->clonePlatformHandle();
  }
  //////////////////////////////////////////////////
  ~PopupImpl() {
    glfwDestroyWindow(_glfwPopupWindow);
  }
  //////////////////////////////////////////////////
  void _fireEvent(ui::event_ptr_t uiev) {
    uiev->_uicontext = _uicontext.get();
    uiev->setvpDim(_uicontext->_top.get());
    auto handled = ui::Event::sendToContext(uiev);
    if (handled._widget_finished) {
      _terminate = true;
    }
  }
  //////////////////////////////////////////////////
  void mainThreadLoop() {
    auto ctxbase = dynamic_cast<CtxGLFW*>(_parent_context->mCtxBase);
    OrkAssert(ctxbase != nullptr);

    _terminate = false;

    if (_uicontext->_top) {
      _uicontext->_top->gpuInit(_parent_context);
      _uicontext->_top->SetRect(0, 0, _w, _h);
    }

    ork::Timer timer;
    timer.Start();
    double prev_time = timer.SecsSinceStart();
    ui::updatedata_ptr_t updata = std::make_shared<ui::UpdateData>();

    while (not _terminate) {

      double this_time = timer.SecsSinceStart();
      double dt        = this_time - prev_time;
      prev_time = this_time;
      updata->_dt = dt;
      updata->_abstime = this_time;

      glfwPollEvents();
      glfwMakeContextCurrent(_glfwPopupWindow);

      _rtgroup->_clearColor = fvec4(0, 0, 0, 0);

      _parent_context->FBI()->pushViewport(0, 0, _w, _h);
      _parent_context->FBI()->pushScissor(0, 0, _w, _h);
      _parent_context->FBI()->PushRtGroup(_rtgroup.get());
      glfwMakeContextCurrent(_glfwPopupWindow);

      void* plato = (void*)_parent_context->GetPlatformHandle();

      _parent_context->SetPlatformHandle(_cloned_plato);

      _uicontext->tick(updata);

      if (_uicontext->_top) {
        auto drwev = std::make_shared<ui::DrawEvent>(_parent_context);
        _uicontext->draw(drwev);
      }

      _parent_context->SetPlatformHandle(plato);

      _parent_context->FBI()->PopRtGroup();
      _parent_context->FBI()->popScissor();
      _parent_context->FBI()->popViewport();
      glfwMakeContextCurrent(_glfwPopupWindow);

      glFinish();

      glfwSwapBuffers(_glfwPopupWindow);

      usleep(1000 * 16);
    }

    glfwMakeContextCurrent(ctxbase->_glfwWindow);
    glfwFocusWindow(ctxbase->_glfwWindow);

  }
  //////////////////////////////////////////////////
  GLFWwindow* _glfwPopupWindow = nullptr;
  PopupWindow* _window;
  eventsink_glfw_ptr_t _eventSINK;
  lev2::Context* _parent_context = nullptr;
  ui::context_ptr_t _uicontext;
  rtgroup_ptr_t _rtgroup;
  int _x, _y, _w, _h;
  bool _terminate     = false;
  void* _cloned_plato = nullptr;
  int _buttonState    = 0;
  int _mouseX         = 0;
  int _mouseY         = 0;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

PopupWindow::PopupWindow(Context* pctx, int x, int y, int w, int h, bool transparent)
    : Window(x, y, w, h, "Popup")
    , _useTransparency(transparent) {
  _uicontext = std::make_shared<ui::Context>();
  auto impl  = _impl.makeShared<PopupImpl>(this, pctx, x, y, w, h);
}

///////////////////////////////////////////////////////////////////////////////

void PopupWindow::mainThreadLoop() {
  auto impl = _impl.getShared<PopupImpl>();
  impl->mainThreadLoop();
}

///////////////////////////////////////////////////////////////////////////////

PopupWindow::~PopupWindow() {
  _impl = 0;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
