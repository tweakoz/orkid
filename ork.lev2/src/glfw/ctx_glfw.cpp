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
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/context.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/msgrouter.inl>
#include <ork/math/basicfilters.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/util/logger.h>
///////////////////////////////////////////////////////////////////////////////
#include "../gfx/vulkan/headers/vulkan_ctx.h"
#if defined(__APPLE__)
#include <dlfcn.h>
#endif
///////////////////////////////////////////////////////////////////////////////
#if defined(ENABLE_GLFW)
#include <ork/lev2/glfw/ctx_glfw.h>
#include <ork/lev2/ezapp.h>
#include <GLFW/glfw3native.h>
namespace ork::lev2 {
int _g_post_swap_wait_time = 0;
extern int GLFW_MODIFIER_OSCTRL;
extern bool _macosUseHIDPI;
extern uint64_t GRAPHICS_API;
extern appinitdata_ptr_t _ginitdata;
static logchannel_ptr_t logchan_glfw = logger()->configureChannel("GLFW", fvec3(0.8, 0.2, 0.6), true);
void setAlwaysOnTop(GLFWwindow* window);
void recomputeHIDPI(GLFWwindow* window);
void windowToFront(GLFWwindow* window);
void activateWindow(GLFWwindow *window);
void enableFocusFollowsMouse(GLFWwindow* window);

///////////////////////////////////////////////////////////////////////////////
static CtxGLFW* _gctx = nullptr;
///////////////////////////////////////////////////////////////////////////////
struct ApiImpl {};
using apiimpl_ptr_t = std::shared_ptr<ApiImpl>;
///////////////////////////////////////////////////////////////////////////////
struct ApiImpl_GL : public ApiImpl {};
///////////////////////////////////////////////////////////////////////////////
struct ApiImpl_VK : public ApiImpl {};
///////////////////////////////////////////////////////////////////////////////
static fvec2 gpos;
///////////////////////////////////////////////////////////////////////////////
ui::event_ptr_t CtxGLFW::uievent() {
  return _uievent;
}
void CtxGLFW::disableMouseCursor() {
  glfwSetInputMode(_glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}
void CtxGLFW::hideMouseCursor() {
  glfwSetInputMode(_glfwWindow, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
}
void CtxGLFW::showMouseCursor() {
  glfwSetInputMode(_glfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}
void CtxGLFW::warpCursor(int x, int y) {
  glfwSetCursorPos(_glfwWindow, double(x), double(y));
}
///////////////////////////////////////////////////////////////////////////////
ui::event_constptr_t CtxGLFW::uievent() const {
  return _uievent;
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::setClipboardText(const std::string& text) {
  if (_glfwWindow) {
    glfwSetClipboardString(_glfwWindow, text.c_str());
  }
}
std::string CtxGLFW::getClipboardText() const {
  if (_glfwWindow) {
    const char* txt = glfwGetClipboardString(_glfwWindow);
    if (txt) return txt;
  }
  return "";
}
///////////////////////////////////////////////////////////////////////////////
static GLFWmonitor* monitorForWindow(GLFWwindow* window) {
  // Wayland does not expose window positions; fall back to primary monitor
  if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
    return glfwGetPrimaryMonitor();
  }

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
  auto ctxbase = (CtxGLFW*)glfwGetWindowUserPointer(window);
  if (nullptr == ctxbase)
    return;
  auto sink = ctxbase->_eventSINK;
  if (nullptr == sink)
    return;
  sink->_on_callback_refresh();
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_winresized(GLFWwindow* window, int w, int h) {
  auto ctxbase = (CtxGLFW*)glfwGetWindowUserPointer(window);
  if (nullptr == ctxbase)
    return;

  auto sink = ctxbase->_eventSINK;
  if (nullptr == sink)
    return;
  w = int(w * ctxbase->_contentScaleX);
  h = int(h * ctxbase->_contentScaleY);

  if(0)logchan_glfw->status("WIN RESIZED", "w<%d> h<%d>", w, h);
  sink->_on_callback_winresized(w, h);
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_fbresized(GLFWwindow* window, int w, int h) {
  auto ctxbase = (CtxGLFW*)glfwGetWindowUserPointer(window);
  if (nullptr == ctxbase)
    return;
  auto sink = ctxbase->_eventSINK;
  if (nullptr == sink)
    return;
  if(0)logchan_glfw->status("FB RESIZED", "w<%d> h<%d> cs<%g %g>", w, h, ctxbase->_contentScaleX, ctxbase->_contentScaleY);
  sink->_on_callback_fbresized(w, h);
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_contentScaleChanged(GLFWwindow* window, float sw, float sh) {
  logchan_glfw->status("CONTENTSCALE", "<%p %f %f>", window, sw, sh);
  auto ctxbase = (CtxGLFW*)glfwGetWindowUserPointer(window);
  if (nullptr == ctxbase)
    return;
  ctxbase->_contentScaleX = sw;
  ctxbase->_contentScaleY = sh;
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_focusChanged(GLFWwindow* window, int focus) {
  bool has_focus = (focus == GLFW_TRUE);
  //printf("fb focus<%p %d>", window, focus);
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_keyboard(GLFWwindow* window, int key, int scancode, int action, int modifiers) {
  auto ctxbase = (CtxGLFW*)glfwGetWindowUserPointer(window);
  if (nullptr == ctxbase)
    return;
  auto sink = ctxbase->_eventSINK;
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
  auto ctxbase = (CtxGLFW*)glfwGetWindowUserPointer(window);
  if (nullptr == ctxbase)
    return;
  auto sink = ctxbase->_eventSINK;
  if (nullptr == sink)
    return;
  sink->_on_callback_mousebuttons(button, action, modifiers);
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_scroll(GLFWwindow* window, double xoffset, double yoffset) {
  auto ctxbase = (CtxGLFW*)glfwGetWindowUserPointer(window);
  if (nullptr == ctxbase)
    return;
  auto sink = ctxbase->_eventSINK;
  if (nullptr == sink)
    return;
  sink->_on_callback_scroll(xoffset, yoffset);
}
void CtxGLFW::_on_callback_scroll(double xoffset, double yoffset) {
  auto uiev        = this->uievent();
  uiev->_eventcode = ui::EventCode::MOUSEWHEEL;
  //printf("scroll xoffset<%f> yoffset<%f>\n", xoffset, yoffset);
  uiev->miMWY = int(yoffset*10.0);
  uiev->miMWX = int(xoffset*10.0);

  _fire_ui_event();
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_cursor(GLFWwindow* window, double xoffset, double yoffset) {
  auto ctxbase = (CtxGLFW*)glfwGetWindowUserPointer(window);
  //printf("cursor<%p %d %d>\n", window, xoffset, yoffset);
  if (nullptr == ctxbase)
    return;
  auto sink = ctxbase->_eventSINK;
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
  if (_macosUseHIDPI) {
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

  uiev->mfLastUnitX    = uiev->mfUnitX;
  uiev->mfLastUnitY    = uiev->mfUnitY;
  uiev->mfUnitX        = unitX;
  uiev->mfUnitY        = unitY;
  uiev->miScreenWidth  = w;
  uiev->miScreenHeight = h;

  if (monitor && glfwGetPlatform() != GLFW_PLATFORM_WAYLAND) {
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
  auto ctxbase = (CtxGLFW*)glfwGetWindowUserPointer(window);
  //printf("enterleave<%p %d>\n", window, entered);
  if (nullptr == ctxbase)
    return;
  auto sink = ctxbase->_eventSINK;
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
  glfwWindowHint(GLFW_SAMPLES, aid->_msaa_samples);
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::Show() {

  //logchan_glfw->log("CtxGLFW::Show");
  fflush(stdout);
  GLFWmonitor* fullscreen_monitor = nullptr;
  GLFWmonitor* selected_monitor   = nullptr;

  if (_orkwindow) {
    _orkwindow->SetDirty(true);

    int l = _appinitdata->_left;
    int t = _appinitdata->_top;

#if defined(__APPLE__)
    glfwWindowHint(
        GLFW_COCOA_RETINA_FRAMEBUFFER, //
        _appinitdata->_allowHIDPI ? GLFW_TRUE : GLFW_FALSE);
#endif

    if (_appinitdata->_fullscreen) {

      std::string desired_monitor_name = _appinitdata->_fullscreen_monitor;
      fullscreen_monitor               = glfwGetPrimaryMonitor();

      int monitor_count = 0;
      auto monitors     = glfwGetMonitors(&monitor_count);
      logchan_glfw->log("desired_monitor_name<%s> ", desired_monitor_name.c_str());

      for (int i = 0; i < monitor_count; i++) {
        GLFWmonitor* monitor = monitors[i];
        int mon_x            = 0;
        int mon_y            = 0;
        glfwGetMonitorPos(monitor, &mon_x, &mon_y);
        const char* monitorName = glfwGetMonitorName(monitor);
        logchan_glfw->log("have monitor<%d:%s> monx<%d> mony<%d>", i, monitorName, mon_x, mon_y);
      }

      if (desired_monitor_name != "none") {
        for (int i = 0; i < monitor_count; i++) {
          GLFWmonitor* monitor    = monitors[i];
          const char* monitorName = glfwGetMonitorName(monitor);
          if (desired_monitor_name == std::string(monitorName)) {
            fullscreen_monitor = monitor;
            logchan_glfw->log("1: USING FULLSCREEN MONITOR<%p:%s> ", fullscreen_monitor, monitorName);
          }
        }
      } else { // by position
        int idiff = 100000;
        for (int i = 0; i < monitor_count; i++) {
          GLFWmonitor* monitor    = monitors[i];
          const char* monitorName = glfwGetMonitorName(monitor);
          int mon_x               = 0;
          int mon_y               = 0;
          glfwGetMonitorPos(monitor, &mon_x, &mon_y);

          /////////////////////////////////
          // select monitor whose left edge is the closest to the appinitdata's left
          /////////////////////////////////

          int d = abs(mon_x - l);

          logchan_glfw->log(
              "diffmode: monitor<%d> %s mon_x<%d> mon_y<%d> dist<%d> idiff<%d>\n", i, monitorName, mon_x, mon_y, d, idiff);

          if (d < idiff) {
            fullscreen_monitor = monitor;
            idiff              = d;
            logchan_glfw->log("2: USING FULLSCREEN MONITOR<%p:%s> ", fullscreen_monitor, monitorName);
          }

          /////////////////////////////////
        }
      }

      //////////////////////////////////////
      // "windowed fullscreen" — borderless window covering some/all of the
      // selected monitor. Two sub-modes (EFullScreenMode):
      //   Windowed  — sized to the monitor's WORKAREA (excludes menu bar /
      //               dock). Default. Framebuffer ends at menu-bar bottom
      //               so the OS UI doesn't composite on top of pixels we
      //               render. The "respectful" behavior; correct for most
      //               apps on macOS Tahoe (where the menu bar no longer
      //               auto-hides for borderless windows).
      //   Immersive — sized to the full vidmode dimensions. The OS menu
      //               bar / dock stay visible and overlap the top/edges
      //               of our framebuffer, but the rendering surface is
      //               the entire panel. Use for VR mirror windows, kiosk,
      //               game-style takeover, fullscreen video, etc.
      //////////////////////////////////////
      const GLFWvidmode* mode = glfwGetVideoMode(fullscreen_monitor);
      const char* monitorName = glfwGetMonitorName(fullscreen_monitor);
      if (monitorName == nullptr) {
        monitorName = "";
      }
      const bool immersive =
        (_appinitdata->_fullscreen_mode == AppInitData::EFullScreenMode::Immersive);

      int win_x, win_y, win_w, win_h;
      if (immersive) {
        // Full-panel coverage
        glfwGetMonitorPos(fullscreen_monitor, &win_x, &win_y);
        win_w = mode->width;
        win_h = mode->height;
      } else {
        // Workarea (excludes menu bar / dock)
        glfwGetMonitorWorkarea(fullscreen_monitor, &win_x, &win_y, &win_w, &win_h);
      }
      _width  = win_w;
      _height = win_h;
      logchan_glfw->log("USING GLFW 'windowed fullscreen on monitor<%s>' mode<%s>",
                       monitorName, immersive ? "immersive" : "windowed");
      logchan_glfw->log("USING GLFW_REFRESH_RATE<%d> ", int(mode->refreshRate));
      logchan_glfw->log("USING GLFW _width<%d> ", _width);
      logchan_glfw->log("USING GLFW _height<%d> ", _height);
      _appinitdata->_width  = _width;
      _appinitdata->_height = _height;
      //////////////////////////////////////
      selected_monitor = fullscreen_monitor;
      glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
      glfwWindowHint(GLFW_RED_BITS, mode->redBits);
      glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
      glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
      glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
      glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
      glfwWindowHint(GLFW_FLOATING, GLFW_TRUE); // Changed from TRUE to avoid positioning issues
      glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
      glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
      glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);

      _appinitdata->_left = win_x;
      _appinitdata->_top  = win_y;
      logchan_glfw->log("Setting window position to: x<%d> y<%d>", win_x, win_y);

      this->onResize(_width, _height);
      fullscreen_monitor = nullptr; // disable actual fullscreen
    } // fullscreen

    switch (GRAPHICS_API) {
      case "VULKAN"_crcu:
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        break;
      default:
        break;
    }

    auto global = globalOffscreenContext();

    logchan_glfw->log("glfwCreateWindow _width<%d> _height<%d>", _width, _height);

    // Set window hints for offscreen mode to prevent focus stealing
    if (_appinitdata->_offscreen) {
      glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
      glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
      glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
    }
    else{
      glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
      glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);
      glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
    }

    _glfwWindow = glfwCreateWindow(
        _width,             //
        _height,            //
        _appinitdata->_application_name.c_str(),        //
        fullscreen_monitor, // monitor
        global->_glfwWindow // sharegroup
    );

    OrkAssert(_glfwWindow != nullptr);

    if (not _appinitdata->_offscreen) {
      glfwSetWindowUserPointer(_glfwWindow, (void*)this);
      glfwSetWindowAttrib(_glfwWindow, GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
      // glfwSetInputMode(_glfwWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
      // glfwSetInputMode(_glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
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
  if (selected_monitor == nullptr) {
    selected_monitor = monitorForWindow(_glfwWindow);
    // In headless/offscreen mode, skip HIDPI computation (requires display server)
    if (not _appinitdata->_offscreen) {
      recomputeHIDPI(_glfwWindow);
    }
    // OrkAssert(selected_monitor != nullptr);
  }
  _glfwMonitor = selected_monitor;

  // if(_appinitdata->_allowHIDPI){
  // glfwGetWindowContentScale(_glfwWindow, &_contentScaleX, &_contentScaleY);
  //}
  // else{
  //  _contentScaleX = 1.0f;
  //  _contentScaleY = 1.0f;
  //}

  if (_appinitdata->_fullscreen) {

    // _glfw_callback_winresized(_glfwWindow, _width, _height);
    //_glfw_callback_fbresized(_glfwWindow, _width, _height);
    glfwGetWindowSize(_glfwWindow, &_width, &_height);
    _appinitdata->_width  = _width;
    _appinitdata->_height = _height;

    // Check for fullscreen mouse mode (hide hardware cursor, render virtual)
    // Can be enabled via kwarg fsmouse=True or env var ORKID_FSMOUSEMODE=1
    const char* fsmousemode_env = getenv("ORKID_FSMOUSEMODE");
    bool fsmousemode_envvar = fsmousemode_env && std::string(fsmousemode_env) == "1";
    if (_appinitdata->_fsMouseMode || fsmousemode_envvar) {
      _fsMouseMode = true;
      hideMouseCursor();
      logchan_glfw->log("Fullscreen mouse mode enabled: hardware cursor hidden");
    }

  } else if( not _appinitdata->_offscreen ) {
    if(0)logchan_glfw->log(
        "WINDOWEDMODE T<%d> L<%d> W<%d> H<%d>", //
        _appinitdata->_top,                     //
        _appinitdata->_left,                    //
        _appinitdata->_width,                   //
        _appinitdata->_height);

    glfwSetWindowPos(
        _glfwWindow,
        _appinitdata->_left, //
        _appinitdata->_top);
    glfwSetWindowSize(
        _glfwWindow,
        _appinitdata->_width, //
        _appinitdata->_height);

    // Query actual window and framebuffer sizes
    // On HiDPI displays (Retina), framebuffer != window size
    int actual_win_w, actual_win_h;
    int actual_fb_w, actual_fb_h;
    glfwGetWindowSize(_glfwWindow, &actual_win_w, &actual_win_h);
    glfwGetFramebufferSize(_glfwWindow, &actual_fb_w, &actual_fb_h);

    // Detect if we have HiDPI scaling
    bool has_hidpi_scaling = (actual_fb_w != actual_win_w) || (actual_fb_h != actual_win_h);

    if (has_hidpi_scaling) {
      // HiDPI display (e.g., MacOS Retina): Use window size for UI layout, framebuffer for rendering
      // The window size is the logical size we should use for UI dimensions
      if (actual_win_w != _appinitdata->_width || actual_win_h != _appinitdata->_height) {
        logchan_glfw->log("HiDPI: Window size clamped by monitor: requested %dx%d, actual window %dx%d (framebuffer %dx%d)",
                          _appinitdata->_width, _appinitdata->_height, actual_win_w, actual_win_h, actual_fb_w, actual_fb_h);
        _width = actual_win_w;
        _height = actual_win_h;
        _appinitdata->_width = actual_win_w;
        _appinitdata->_height = actual_win_h;

        if (_orkwindow) {
          _orkwindow->miWidth = actual_win_w;
          _orkwindow->miHeight = actual_win_h;
          logchan_glfw->log("Updated Window object dimensions to %dx%d (window coordinates)", actual_win_w, actual_win_h);
        }
      }
    } else {
      // Non-HiDPI display (e.g., Linux): Window size == framebuffer size
      // Use framebuffer size which may have been clamped by monitor constraints
      if (actual_fb_w != _appinitdata->_width || actual_fb_h != _appinitdata->_height) {
        logchan_glfw->log("Framebuffer size clamped by monitor: requested %dx%d, actual %dx%d",
                          _appinitdata->_width, _appinitdata->_height, actual_fb_w, actual_fb_h);
        _width = actual_fb_w;
        _height = actual_fb_h;
        _appinitdata->_width = actual_fb_w;
        _appinitdata->_height = actual_fb_h;

        if (_orkwindow) {
          _orkwindow->miWidth = actual_fb_w;
          _orkwindow->miHeight = actual_fb_h;
          logchan_glfw->log("Updated Window object dimensions to %dx%d", actual_fb_w, actual_fb_h);
        }
      }
    }
  }

  if (_needsInitialize) {
    // printf("CreateCONTEXT");
    _orkwindow->initContext();
    if (_appinitdata->_fullscreen) {
      _target->resizeMainSurface(_width, _height);
    }
    _orkwindow->OnShow();
    _needsInitialize = false;
  }
  if (not _appinitdata->_offscreen) {
    glfwSetWindowAttrib(_glfwWindow, GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
    glfwShowWindow(_glfwWindow);

    // Re-apply position after window is shown for fullscreen mode
    if (_appinitdata->_fullscreen) {
      glfwSetWindowPos(_glfwWindow, _appinitdata->_left, _appinitdata->_top);
      logchan_glfw->log("Re-positioning window after show to: x<%d> y<%d>", _appinitdata->_left, _appinitdata->_top);
    }
  }
  _appinitdata->_width  = (_appinitdata->_width * _contentScaleX);
  _appinitdata->_height = (_appinitdata->_height * _contentScaleY);
  _width                = _appinitdata->_width;
  _height               = _appinitdata->_height;

  onResize(_width, _height);
  if (_appinitdata->_canalwaysontop) {
    setAlwaysOnTop(_glfwWindow);
  }

  glfwPollEvents();
#ifdef __APPLE__
  if (not _appinitdata->_offscreen) {
    windowToFront(_glfwWindow);
    activateWindow(_glfwWindow);
    enableFocusFollowsMouse(_glfwWindow);
  }
#endif

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
void CtxGLFW::present() {
  // todo remove
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
void CtxGLFW::_runloopBegin() {
  OrkAssert(_target);

  lev2::ThreadGfxContext l2ctx_track(_target);

  _target->makeCurrentContext();

  if (_onGpuInit) {
    FontMan::gpuInit(_target);
    _target->gpuPreInit(); // Initialize Context GPU resources
    _onGpuInit(_target);
    _target->gpuPostInit(); // Initialize Context GPU resources
  }

  if(not _appinitdata->_offscreen ){
    activateWindow(_glfwWindow);
  }
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::_runloopIter(bool pollevents) {

  lev2::ThreadGfxContext l2ctx_track(_target);

  //////////////////////////////
  // poll UI/windowing system events
  //////////////////////////////

  // glfwWaitEvents();
  if(pollevents){
    glfwPollEvents();
  }

  //////////////////////////////
  // run main thread app logic
  //////////////////////////////

  _onRunLoopIteration();

  //////////////////////////////
  // redraw ?
  //////////////////////////////

  if (_onGpuUpdate) {
    _onGpuUpdate(_target);
  }

  SlotRepaint();

  //////////////////////////////
  // check for closed window
  //////////////////////////////

  if (_glfwWindow) {
    bool window_should_close = glfwWindowShouldClose(_glfwWindow);
    if (window_should_close) {
      _runstate = 2;
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::_runloopEnd() {

  lev2::ThreadGfxContext l2ctx_track(_target);

  //////////////////////////////

  if (_onGpuExit) {
    _onGpuExit(_target);
  }

  //////////////////////////////

  glfwDestroyWindow(_glfwWindow);
  _runstate = 3;
}
///////////////////////////////////////////////////////////////////////////////
int CtxGLFW::runloop() {
  int rval = 0;
  _runloopBegin();
  while (_runstate == 1) {
    _runloopIter();
  }
  _runloopEnd();
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
  auto op = [=]() {
    glfwSetWindowSize(_glfwWindow, w, h);
    glfwFocusWindow(_glfwWindow);
 };
  //opq::mainSerialQueue()->enqueue(op);
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::queryFramebufferSize(int& w, int& h) const {
  if (_glfwWindow) {
    glfwGetFramebufferSize(_glfwWindow, &w, &h);
  } else {
    w = 0;
    h = 0;
  }
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::SlotRepaint() {
  // OrkAssert(opq::TrackCurrent::is(opq::mainSerialQueue()));

  if (not GfxEnv::initialized()){
    printf( "CtxGLFW::SlotRepaint() earlyret1\n" );
    return;
  }

  OrkProfilerSampleScope(CHANNEL_MAIN, "viewport.draw");

  if (this->_target) {
    _target->makeCurrentContext();
    auto gfxwin        = _uievent->mpGfxWin;
    _uievent->mpGfxWin = (Window*)_target->FBI()->GetThisBuffer();
    auto drwev         = std::make_shared<ui::DrawEvent>(this->_target);

    auto widget = gfxwin ? gfxwin->GetRootWidget() : nullptr;

      //printf( "CtxGLFW::SlotRepaint() _target<%p> widget<%p>\n", _target, widget );

    if (widget) {
      widget->draw(drwev);
    }
    else{
      _target->beginFrame(false);  // false = non-visual frame
      _target->endFrame();
    }
  }

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
  // GLFW_FEATURE_UNAVAILABLE (0x1000C) is a non-fatal warning on Wayland
  // (e.g. glfwGetWindowPos is not supported by the Wayland compositor)
  if (error == 0x1000C) return;
  OrkAssert(false);
}

///////////////////////////////////////////////////////////////////////////////

GLFWwindow* CtxGLFW::_apiInitVK() {
  OrkAssertI(glfwVulkanSupported(), "glfwVulkanSupported == false! Might need to set VK_ICD_FILENAMES env var on linux!");
  // OrkAssert(vulkan::_GVI);
  // OrkAssert(vulkan::_GVI->_instance);
  auto ctx_vars = std::make_shared<varmap::VarMap>();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  // Hide the window and prevent focus for offscreen mode
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);
  glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
  GLFWwindow* offscreen_window = glfwCreateWindow(
      32,      //
      32,      //
      "",      //
      nullptr, //
      nullptr);
  logchan_glfw->log("VK: offscreen_window<%p>", offscreen_window);
  // Reset hints for future windows
  glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);
  glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
  return offscreen_window;
}

///////////////////////////////////////////////////////////////////////////////

CtxGLFW* CtxGLFW::globalOffscreenContext() {
  if (nullptr == _gctx) {

    glfwSetErrorCallback(error_callback);

    _gctx = new CtxGLFW(nullptr);

    //printf("<<<glfwInit>>> HERE!!!\n");

#if defined(LINUX) || defined(ORK_CONFIG_IX)
    // On Linux, if no display server is available, use GLFW NULL platform for headless operation
    // This allows Vulkan-based offscreen rendering without X11/Wayland
    // Otherwise, let GLFW auto-detect the available platform (X11, Wayland, etc.)
    const char* display = getenv("DISPLAY");
    const char* wayland = getenv("WAYLAND_DISPLAY");
    if ((display == nullptr || display[0] == '\0') &&
        (wayland == nullptr || wayland[0] == '\0')) {
      logchan_glfw->log("No display server detected, using GLFW NULL platform for headless operation");
      glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_NULL);
    }
    // Otherwise let GLFW auto-select between X11/Wayland based on availability
#endif

#if defined(__APPLE__)
    // On macOS, GLFW's Cocoa backend calls dlopen("libvulkan.1.dylib") during glfwInit().
    // SIP strips DYLD_LIBRARY_PATH, so the bare-name dlopen can't find our staging copy,
    // and may instead find homebrew's (causing dual-load crashes).
    // Fix: dlopen our Vulkan loader by absolute path, extract vkGetInstanceProcAddr,
    // and hand it to GLFW via glfwInitVulkanLoader() — GLFW then skips its own dlopen entirely.
    {
      const char* stage = getenv("OBT_STAGE");
      if (stage) {
        std::string vk_path = std::string(stage) + "/lib/libvulkan.1.dylib";
        void* h = dlopen(vk_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
        if (h) {
          auto procAddr = (PFN_vkGetInstanceProcAddr)dlsym(h, "vkGetInstanceProcAddr");
          if (procAddr) {
            glfwInitVulkanLoader(procAddr);
            logchan_glfw->log("Initialized GLFW Vulkan loader from: %s", vk_path.c_str());
          } else {
            logchan_glfw->log("WARNING: dlsym vkGetInstanceProcAddr failed: %s", dlerror());
          }
        } else {
          logchan_glfw->log("WARNING: failed to load Vulkan loader from %s: %s", vk_path.c_str(), dlerror());
        }
      }
    }
#endif

    bool ok = glfwInit();
    assert(ok);

    // Log which platform GLFW is using
    int platform = glfwGetPlatform();
    const char* platform_name = "UNKNOWN";
    switch(platform) {
      case GLFW_PLATFORM_WIN32: platform_name = "WIN32"; break;
      case GLFW_PLATFORM_COCOA: platform_name = "COCOA"; break;
      case GLFW_PLATFORM_WAYLAND: platform_name = "WAYLAND"; break;
      case GLFW_PLATFORM_X11: platform_name = "X11"; break;
      case GLFW_PLATFORM_NULL: platform_name = "NULL"; break;
    }
    logchan_glfw->log("GLFW platform: %s", platform_name);

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

    GLFWwindow* offscreen_window = nullptr;

    switch (GRAPHICS_API) {
      case "VULKAN"_crcu: {
        offscreen_window = _gctx->_apiInitVK();
        break;
      }
      default: {
        OrkAssert(false);
        break;
      }
    }

    glfwSetWindowUserPointer(offscreen_window, (void*)_gctx);
  }
  return _gctx;
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::_on_callback_mousebuttons(int button, int action, int modifiers) {
  // printf("_glfw_callback_mousebuttons<%p>", window);

  ////////////////////////

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
  uiev->mbSUPER = (modifiers & GLFW_MOD_SUPER);

  uiev->_eventcode = DOWN                           //
                         ? ork::ui::EventCode::PUSH //
                         : ork::ui::EventCode::RELEASE;

  _fire_ui_event();

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
  this->onResize(w, h);
  auto uiev            = this->uievent();
  uiev->_eventcode     = ui::EventCode::RESIZED;
  uiev->miScreenWidth  = w;
  uiev->miScreenHeight = h;
  _fire_ui_event();
}
void CtxGLFW::_on_callback_fbresized(int w, int h) {
  this->onResize(w, h);
}
void CtxGLFW::_on_callback_keyboard(int key, int scancode, int action, int modifiers) {
  const char* action_str = (action == GLFW_PRESS) ? "PRESS" : (action == GLFW_RELEASE) ? "RELEASE" : "REPEAT";
  //logchan_glfw->log("[PRIMARY-KEY] key=%d scancode=%d action=%s mods=%d", key, scancode, action_str, modifiers);

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
}
void CtxGLFW::_on_callback_cursor(double xoffset, double yoffset) {
  auto uiev = this->uievent();
  fillEventCursor(uiev, _glfwWindow, _glfwMonitor, xoffset, yoffset, _width, _height);
  if (this->_buttonState == 0) {
    if(0)printf( "move _width<%d> _height<%d>\n", int(xoffset), int(yoffset));
    uiev->_eventcode = ui::EventCode::MOVE; //
    _fire_ui_event();
  } else {
    uiev->_eventcode = ui::EventCode::DRAG; //
    _fire_ui_event();
  }
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::_on_callback_enterleave(int entered) {
  bool was_entered = bool(entered);

  //logchan_glfw->log("[PRIMARY] enterleave: entered=%d window=%p", entered, _glfwWindow);

  // Focus follows mouse: give this window keyboard focus when mouse enters
  if (was_entered && _glfwWindow) {
    //logchan_glfw->log("[PRIMARY] calling glfwFocusWindow(%p)", _glfwWindow);
    glfwFocusWindow(_glfwWindow);
  }

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
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::_fire_ui_event() {
  auto uiev        = this->uievent();
  auto gfxwin      = uiev->mpGfxWin;
  auto root        = gfxwin ? gfxwin->GetRootWidget() : nullptr;
  uiev->_uicontext = root ? root->_uicontext : nullptr;
  if (root) {
    uiev->setvpDim(root);
    if (auto app = dynamic_cast<OrkEzApp*>(OrkEzAppBase::get())) {
      app->_fireGlobalEvent(uiev);
    }
    ui::Event::sendToContext(uiev);
    //_pushTimer.Start();
  }
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

    _window            = win;
    auto ctx_glfw      = new CtxGLFW(_window);
    _window->mpCTXBASE = ctx_glfw;

    auto global = CtxGLFW::globalOffscreenContext();

    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    switch (GRAPHICS_API) {
      case "VULKAN"_crcu:
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        break;
      default:
        break;
    }
    if (win->_useTransparency) {
      glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    }
    _glfwPopupWindow = glfwCreateWindow(w, h, "Popup", NULL, global->_glfwWindow);
    glfwSetWindowPos(_glfwPopupWindow, x, y);
    setAlwaysOnTop(_glfwPopupWindow);
    //////////////////////////////////////////////////
    auto eventSINK                       = ctx_glfw->_eventSINK;
    eventSINK->_on_callback_mousebuttons = [=](int button, int action, int modifiers) {
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
      uiev->mbSUPER = (modifiers & GLFW_MOD_SUPER);

      uiev->_eventcode = DOWN                           //
                             ? ork::ui::EventCode::PUSH //
                             : ork::ui::EventCode::RELEASE;

      uiev->miX = _mouseX;
      uiev->miY = _mouseY;

      _fireEvent(uiev);
    };
    //////////////////////////////////////////////////
    eventSINK->_on_callback_keyboard = [=](int key, int scancode, int action, int modifiers) { //
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
    eventSINK->_on_callback_cursor = [=](double xoffset, double yoffset) { //
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
    eventSINK->_on_callback_scroll = [=](double xoffset, double yoffset) {
      auto uiev        = std::make_shared<ui::Event>();
      uiev->_eventcode = ui::EventCode::MOUSEWHEEL;
      
      uiev->miMWY = int(yoffset);
      uiev->miMWX = int(xoffset);

      _fireEvent(uiev);
    };
    ///////////////////////////////////////////////////////////////////////////////
    eventSINK->_on_callback_enterleave = [=](int entered) {
      bool was_entered = bool(entered);

      // Focus follows mouse: give this window keyboard focus when mouse enters
      if (was_entered && _glfwPopupWindow) {
        glfwFocusWindow(_glfwPopupWindow);
      }

      auto uiev        = std::make_shared<ui::Event>();
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
    glfwSetWindowUserPointer(_glfwPopupWindow, (void*)this);
    glfwSetMouseButtonCallback(_glfwPopupWindow, _glfw_callback_mousebuttons);
    glfwSetCursorPosCallback(_glfwPopupWindow, _glfw_callback_cursor);
    glfwSetKeyCallback(_glfwPopupWindow, _glfw_callback_keyboard);
    glfwSetScrollCallback(_glfwPopupWindow, _glfw_callback_scroll);
    glfwSetCursorEnterCallback(_glfwPopupWindow, _glfw_callback_enterleave);
    glfwSetWindowAttrib(_glfwPopupWindow, GLFW_FOCUS_ON_SHOW, GLFW_TRUE);

    glfwShowWindow(_glfwPopupWindow);

    _rtgroup             = std::make_shared<lev2::RtGroup>(_parent_context, _w, _h);
    _rtgroup->_usage     = "popup"_crcu;
    _rtgroup->mNumMrts   = 1;
    _rtgroup->_autoclear = false;

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
      // OrkAssert(false);
    }

    ork::Timer timer;
    timer.Start();
    double prev_time            = timer.SecsSinceStart();
    ui::updatedata_ptr_t updata = std::make_shared<ui::UpdateData>();

    while (not _terminate) {

      double this_time = timer.SecsSinceStart();
      double dt        = this_time - prev_time;
      prev_time        = this_time;
      updata->_dt      = dt;
      updata->_abstime = this_time;

      glfwPollEvents();

      auto plato_saved       = _parent_context->_impl;
      _parent_context->_impl = _cloned_plato;
      //_parent_context->bindPlatformHandle(_cloned_plato);
      _rtgroup->buffer(0)->_clearColor = fvec4(0, 0, 0, 0);

      _parent_context->FBI()->pushViewport(0, 0, _w, _h);
      _parent_context->FBI()->pushScissor(0, 0, _w, _h);
      _parent_context->FBI()->PushRtGroup(_rtgroup.get());

      _uicontext->tick(updata);

      if (_uicontext->_top) {
        auto drwev = std::make_shared<ui::DrawEvent>(_parent_context);
        _uicontext->draw(drwev);
      }

      _parent_context->_impl = plato_saved;
      //_parent_context->bindPlatformHandle(plato_saved);

      _parent_context->FBI()->PopRtGroup();
      _parent_context->FBI()->popScissor();
      _parent_context->FBI()->popViewport();

      usleep(1000 * 16);
    }

    glfwFocusWindow(ctxbase->_glfwWindow);
  }
  //////////////////////////////////////////////////
  GLFWwindow* _glfwPopupWindow = nullptr;
  PopupWindow* _window;
  lev2::Context* _parent_context = nullptr;
  ui::context_ptr_t _uicontext;
  rtgroup_ptr_t _rtgroup;
  int _x, _y, _w, _h;
  bool _terminate = false;
  ctx_platform_handle_t _cloned_plato;
  int _buttonState = 0;
  int _mouseX      = 0;
  int _mouseY      = 0;
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
#endif // #if defined(ENABLE_GLFW)
