////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ez_secondary_win.h>
#include <ork/lev2/glfw/ctx_glfw.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/util/logger.h>
#include <GLFW/glfw3.h>

namespace ork::lev2 {

extern uint64_t GRAPHICS_API;
extern int GLFW_MODIFIER_OSCTRL;
void fillEventKeyboard(ui::event_ptr_t uiev, int key, int scancode, int action, int modifiers);
void fillEventCursor(ui::event_ptr_t uiev, GLFWwindow* window, GLFWmonitor* monitor,
                     double xoffset, double yoffset, double w, double h);

static logchannel_ptr_t logchan_secwin = logger()->configureChannel("SECWIN", fvec3(0.4, 0.8, 0.4), true);

///////////////////////////////////////////////////////////////////////////////
// GLFW callbacks for secondary windows
///////////////////////////////////////////////////////////////////////////////

static void _secwin_callback_mousebuttons(GLFWwindow* window, int button, int action, int modifiers);
static void _secwin_callback_cursor(GLFWwindow* window, double x, double y);
static void _secwin_callback_keyboard(GLFWwindow* window, int key, int scancode, int action, int modifiers);
static void _secwin_callback_scroll(GLFWwindow* window, double xoff, double yoff);
static void _secwin_callback_fbresized(GLFWwindow* window, int w, int h);
static void _secwin_callback_close(GLFWwindow* window);
static void _secwin_callback_focus(GLFWwindow* window, int focused);
static void _secwin_callback_enterleave(GLFWwindow* window, int entered);

///////////////////////////////////////////////////////////////////////////////
// SecondaryWinImpl - internal implementation
///////////////////////////////////////////////////////////////////////////////

struct SecondaryWinImpl {

  SecondaryWinImpl(EzSecondaryWin* owner, const EzSecondaryWinConfig& config);
  ~SecondaryWinImpl();

  void _setupEventHandlers();
  void _render();
  void _onResize(int w, int h);
  void _fireEvent(ui::event_ptr_t uiev);
  void _closeWindow();

  EzSecondaryWin* _owner = nullptr;
  EzSecondaryWinConfig _config;

  // GLFW resources
  GLFWwindow* _glfwWindow = nullptr;

  // Orkid wrappers
  Window* _orkWindow = nullptr;
  CtxGLFW* _ctxglfw = nullptr;

  // Graphics
  lev2::Context* _gfxContext = nullptr;

  // State
  int _width = 0;
  int _height = 0;
  int _buttonState = 0;
  int _mouseX = 0;
  int _mouseY = 0;
  float _mouseUnitX = 0.0f;
  float _mouseUnitY = 0.0f;

  // Clean RCFD without compositor for UI rendering
  lev2::rcfd_ptr_t _cleanRcfd;
};

///////////////////////////////////////////////////////////////////////////////

SecondaryWinImpl::SecondaryWinImpl(EzSecondaryWin* owner, const EzSecondaryWinConfig& config)
    : _owner(owner)
    , _config(config)
    , _width(config._width)
    , _height(config._height) {

  logchan_secwin->log("Creating secondary window: %s (%dx%d at %d,%d)",
                      config._title.c_str(), config._width, config._height,
                      config._x, config._y);

  // Get global context for sharing
  auto global = CtxGLFW::globalOffscreenContext();
  OrkAssert(global != nullptr && "No global offscreen context available");

  // Configure window hints
  glfwWindowHint(GLFW_DECORATED, config._decorated ? GLFW_TRUE : GLFW_FALSE);
  glfwWindowHint(GLFW_RESIZABLE, config._resizable ? GLFW_TRUE : GLFW_FALSE);
  glfwWindowHint(GLFW_FLOATING, config._floating ? GLFW_TRUE : GLFW_FALSE);
  glfwWindowHint(GLFW_FOCUS_ON_SHOW, config._focusOnShow ? GLFW_TRUE : GLFW_FALSE);

  // Transparent framebuffer (for popup styling)
  if (config._transparent) {
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
  }

  // Vulkan: no client API
  if (GRAPHICS_API == "VULKAN"_crcu) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  }

  // Create window (shares GL context with global if OpenGL)
  _glfwWindow = glfwCreateWindow(
    config._width,
    config._height,
    config._title.c_str(),
    nullptr,              // No monitor (windowed)
    global->_glfwWindow   // Share with global
  );
  OrkAssert(_glfwWindow != nullptr && "Failed to create GLFW window");

  glfwSetWindowPos(_glfwWindow, config._x, config._y);

  // Create orkid Window wrapper
  _orkWindow = new Window(config._x, config._y, config._width, config._height, config._title);

  // Create CtxGLFW for this window
  _ctxglfw = new CtxGLFW(_orkWindow);
  _ctxglfw->_glfwWindow = _glfwWindow;
  _ctxglfw->_width = config._width;
  _ctxglfw->_height = config._height;
  _orkWindow->mpCTXBASE = _ctxglfw;

  // Set user pointer for event routing (point to this impl)
  glfwSetWindowUserPointer(_glfwWindow, this);

  // Set up GLFW callbacks
  glfwSetMouseButtonCallback(_glfwWindow, _secwin_callback_mousebuttons);
  glfwSetCursorPosCallback(_glfwWindow, _secwin_callback_cursor);
  glfwSetKeyCallback(_glfwWindow, _secwin_callback_keyboard);
  glfwSetScrollCallback(_glfwWindow, _secwin_callback_scroll);
  glfwSetFramebufferSizeCallback(_glfwWindow, _secwin_callback_fbresized);
  glfwSetWindowCloseCallback(_glfwWindow, _secwin_callback_close);
  glfwSetWindowFocusCallback(_glfwWindow, _secwin_callback_focus);
  glfwSetCursorEnterCallback(_glfwWindow, _secwin_callback_enterleave);

  // Show window first - on macOS, framebuffer size is 0 until window is shown
  glfwShowWindow(_glfwWindow);

  // Poll events to ensure window system processes the show request
  // This is needed on macOS to properly initialize the Metal layer
  glfwPollEvents();

  // Use logical window size (config dimensions)
  // Note: glfwGetFramebufferSize() may return 2x on Retina, but when _allowHIDPI=false
  // the actual Metal surface is the logical size. Trust config, not GLFW.
  _width = config._width;
  _height = config._height;
  _ctxglfw->_width = config._width;
  _ctxglfw->_height = config._height;

  logchan_secwin->log("Secondary window init: %dx%d", _width, _height);

  // Initialize graphics context
  // This creates a VkContext (or GLContext) that shares the device with the main window
  _orkWindow->initContext();
  _gfxContext = _orkWindow->context();

  // CRITICAL: Initialize the context's main surface dimensions
  // Without this, mainSurfaceWidth()/mainSurfaceHeight() return 0/garbage,
  // which corrupts the MVP matrix in PushUIMatrix()
  if (_gfxContext) {
    _gfxContext->resizeMainSurface(_width, _height);
    logchan_secwin->log("Initialized context main surface: %dx%d", _width, _height);

    // Create clean RCFD without compositor for UI rendering
    // (The default context RCFD has a shared static compositor with uninitialized CPD)
    _cleanRcfd = std::make_shared<lev2::RenderContextFrameData>(_gfxContext);
  }

  logchan_secwin->log("Secondary window created successfully");
}

///////////////////////////////////////////////////////////////////////////////

SecondaryWinImpl::~SecondaryWinImpl() {
  logchan_secwin->log("Destroying secondary window: %s", _config._title.c_str());

  if (_glfwWindow) {
    glfwHideWindow(_glfwWindow);
    glfwDestroyWindow(_glfwWindow);
    _glfwWindow = nullptr;
  }

  delete _orkWindow;
  _orkWindow = nullptr;
  // Note: _ctxglfw is owned by _orkWindow->mpCTXBASE
}

///////////////////////////////////////////////////////////////////////////////

void SecondaryWinImpl::_closeWindow() {
  if (_glfwWindow) {
    logchan_secwin->log("Closing secondary window: %s", _config._title.c_str());
    glfwHideWindow(_glfwWindow);
    glfwDestroyWindow(_glfwWindow);
    _glfwWindow = nullptr;

    // Call callback only once (when window is actually closed)
    if (_owner->_onClosed) {
      _owner->_onClosed();
    }
  }
  _owner->_shouldClose = true;
}

///////////////////////////////////////////////////////////////////////////////

void SecondaryWinImpl::_fireEvent(ui::event_ptr_t uiev) {
  uiev->_uicontext = _owner->_uicontext.get();
  if (_owner->_uicontext && _owner->_uicontext->_top) {
    uiev->setvpDim(_owner->_uicontext->_top.get());
  } else {
    uiev->_vpdim = fvec2(_width, _height);
  }

  // First try user callback
  if (_owner->_onUiEvent) {
    auto result = _owner->_onUiEvent(uiev);
    if (result._widget_finished) {
      _owner->_shouldClose = true;
    }
  }
  // Then route to ui::Context if present
  else if (_owner->_uicontext && _owner->_uicontext->_top) {
    auto result = ui::Event::sendToContext(uiev);
    if (result._widget_finished) {
      _owner->_shouldClose = true;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void SecondaryWinImpl::_render() {
  if (!_gfxContext || !_glfwWindow) return;

  // Check for window close
  if (glfwWindowShouldClose(_glfwWindow)) {
    _closeWindow();
    return;
  }

  // GPU init on first render
  if (!_owner->_gpuInitialized) {
    if (_owner->_onGpuInit) {
      _owner->_onGpuInit(_gfxContext);
    }
    if (_owner->_uicontext && _owner->_uicontext->_top) {
      _owner->_uicontext->_top->gpuInit(_gfxContext);
    }
    _owner->_gpuInitialized = true;
  }

  // Make this window's context current
  _gfxContext->makeCurrentContext();

  // Render
  if (_owner->_onDraw) {
    logchan_secwin->log("_render: using custom onDraw");
    auto drwev = std::make_shared<ui::DrawEvent>(_gfxContext);
    _owner->_onDraw(drwev);
  } else if (_owner->_uicontext && _owner->_uicontext->_top) {
    // Default: draw UI context
    int ctx_w = _gfxContext->mainSurfaceWidth();
    int ctx_h = _gfxContext->mainSurfaceHeight();

    auto fbi = _gfxContext->FBI();
    auto mtxi = _gfxContext->MTXI();
    auto tgtrect = _gfxContext->mainSurfaceRectAtOrigin();

    _gfxContext->beginFrame();

    if (fbi->_main_rtg) {
      fbi->pushViewport(tgtrect);
      fbi->pushScissor(tgtrect);

      // Push clean RCFD without compositor so PushUIMatrix() uses viewport dimensions
      _gfxContext->pushRenderContextFrameData(_cleanRcfd);

      mtxi->PushUIMatrix(tgtrect._w, tgtrect._h);

      auto drwev = std::make_shared<ui::DrawEvent>(_gfxContext);
      _owner->_uicontext->draw(drwev);
      mtxi->PopUIMatrix();

      _gfxContext->popRenderContextFrameData();

      fbi->popScissor();
      fbi->popViewport();
    } else {
      logchan_secwin->log("  WARNING: _main_rtg is null, skipping draw");
    }

    _gfxContext->endFrame();
  } else {
    // Default: just clear
    logchan_secwin->log("_render: no ui, just clearing (uicontext=%p, top=%p)",
                        _owner->_uicontext.get(),
                        _owner->_uicontext ? _owner->_uicontext->_top.get() : nullptr);
    _gfxContext->beginFrame();
    _gfxContext->endFrame();
  }

  // Swap buffers to display the rendered frame
  _gfxContext->swapBuffers(_ctxglfw);
}

///////////////////////////////////////////////////////////////////////////////

void SecondaryWinImpl::_onResize(int w, int h) {
  logchan_secwin->log("_onResize: %dx%d (was %dx%d)", w, h, _width, _height);
  _width = w;
  _height = h;
  _ctxglfw->_width = w;
  _ctxglfw->_height = h;

  if (_gfxContext) {
    _gfxContext->resizeMainSurface(w, h);
    logchan_secwin->log("  resizeMainSurface done, ctx w/h now: %d/%d",
                        _gfxContext->mainSurfaceWidth(), _gfxContext->mainSurfaceHeight());
  }

  if (_owner->_uicontext && _owner->_uicontext->_top) {
    auto top = _owner->_uicontext->_top;
    auto geo_before = top->geometry();
    logchan_secwin->log("  top widget geo BEFORE SetRect: %d,%d,%d,%d",
                        geo_before._x, geo_before._y, geo_before._w, geo_before._h);
    top->SetRect(0, 0, w, h);
    auto geo_after = top->geometry();
    logchan_secwin->log("  top widget geo AFTER SetRect: %d,%d,%d,%d",
                        geo_after._x, geo_after._y, geo_after._w, geo_after._h);
  }

  // Fire RESIZED event (matches primary window behavior)
  auto uiev = std::make_shared<ui::Event>();
  uiev->_eventcode = ui::EventCode::RESIZED;
  uiev->miScreenWidth = w;
  uiev->miScreenHeight = h;
  uiev->miX = _mouseX;
  uiev->miY = _mouseY;
  _fireEvent(uiev);

  if (_owner->_onResize) {
    _owner->_onResize(w, h);
  }
}

///////////////////////////////////////////////////////////////////////////////
// GLFW Callbacks
///////////////////////////////////////////////////////////////////////////////

static void _secwin_callback_mousebuttons(GLFWwindow* window, int button, int action, int modifiers) {
  auto impl = static_cast<SecondaryWinImpl*>(glfwGetWindowUserPointer(window));
  if (!impl) return;

  auto uiev = std::make_shared<ui::Event>();
  bool DOWN = (action == GLFW_PRESS);

  switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT:
      uiev->mbLeftButton = DOWN;
      impl->_buttonState = (impl->_buttonState & 6) | int(DOWN);
      break;
    case GLFW_MOUSE_BUTTON_MIDDLE:
      uiev->mbMiddleButton = DOWN;
      impl->_buttonState = (impl->_buttonState & 5) | (int(DOWN) << 1);
      break;
    case GLFW_MOUSE_BUTTON_RIGHT:
      uiev->mbRightButton = DOWN;
      impl->_buttonState = (impl->_buttonState & 3) | (int(DOWN) << 2);
      break;
  }

  uiev->mbALT = (modifiers & GLFW_MOD_ALT);
  uiev->mbCTRL = (modifiers & GLFW_MOD_CONTROL);
  uiev->mbSHIFT = (modifiers & GLFW_MOD_SHIFT);
  uiev->mbSUPER = (modifiers & GLFW_MOD_SUPER);
  uiev->_eventcode = DOWN ? ui::EventCode::PUSH : ui::EventCode::RELEASE;
  uiev->miX = impl->_mouseX;
  uiev->miY = impl->_mouseY;

  impl->_fireEvent(uiev);
}

///////////////////////////////////////////////////////////////////////////////

static void _secwin_callback_cursor(GLFWwindow* window, double x, double y) {
  auto impl = static_cast<SecondaryWinImpl*>(glfwGetWindowUserPointer(window));
  if (!impl) return;

  auto uiev = std::make_shared<ui::Event>();
  // Pre-set the current mouse position so fillEventCursor can copy it to miLastX/miLastY
  // (fillEventCursor does: miLast* = mi*; mfLast* = mf*; then sets new values)
  uiev->miX = impl->_mouseX;
  uiev->miY = impl->_mouseY;
  uiev->mfUnitX = impl->_mouseUnitX;
  uiev->mfUnitY = impl->_mouseUnitY;
  fillEventCursor(uiev, window, nullptr, x, y, impl->_width, impl->_height);

  impl->_mouseX = uiev->miX;
  impl->_mouseY = uiev->miY;
  impl->_mouseUnitX = uiev->mfUnitX;
  impl->_mouseUnitY = uiev->mfUnitY;

  uiev->_eventcode = (impl->_buttonState == 0) ? ui::EventCode::MOVE : ui::EventCode::DRAG;
  impl->_fireEvent(uiev);
}

///////////////////////////////////////////////////////////////////////////////

static void _secwin_callback_keyboard(GLFWwindow* window, int key, int scancode, int action, int modifiers) {
  auto impl = static_cast<SecondaryWinImpl*>(glfwGetWindowUserPointer(window));
  if (!impl) return;

  const char* action_str = (action == GLFW_PRESS) ? "PRESS" : (action == GLFW_RELEASE) ? "RELEASE" : "REPEAT";
  logchan_secwin->log("[SECWIN-KEY] key=%d scancode=%d action=%s mods=%d", key, scancode, action_str, modifiers);

  auto uiev = std::make_shared<ui::Event>();

  // Handle paste
  if (action == GLFW_PRESS && key == GLFW_KEY_V && (modifiers & GLFW_MODIFIER_OSCTRL)) {
    const char* clipboardText = glfwGetClipboardString(window);
    if (clipboardText) {
      uiev->_eventcode = ui::EventCode::PASTE_TEXT;
      uiev->_paste_text = clipboardText;
      impl->_fireEvent(uiev);
      return;
    }
  }

  // Handle ESC to close (for popups)
  if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE && !impl->_config._decorated) {
    impl->_owner->requestClose();
    return;
  }

  fillEventKeyboard(uiev, key, scancode, action, modifiers);
  // Set mouse position so IsEventInside() can route to correct widget
  uiev->miX = impl->_mouseX;
  uiev->miY = impl->_mouseY;
  impl->_fireEvent(uiev);
}

///////////////////////////////////////////////////////////////////////////////

static void _secwin_callback_scroll(GLFWwindow* window, double xoff, double yoff) {
  auto impl = static_cast<SecondaryWinImpl*>(glfwGetWindowUserPointer(window));
  if (!impl) return;

  auto uiev = std::make_shared<ui::Event>();
  uiev->_eventcode = ui::EventCode::MOUSEWHEEL;
  // Scale by 10.0 to match primary window behavior
  uiev->miMWX = int(xoff * 10.0);
  uiev->miMWY = int(yoff * 10.0);
  uiev->miX = impl->_mouseX;
  uiev->miY = impl->_mouseY;

  impl->_fireEvent(uiev);
}

///////////////////////////////////////////////////////////////////////////////

static void _secwin_callback_fbresized(GLFWwindow* window, int fb_w, int fb_h) {
  auto impl = static_cast<SecondaryWinImpl*>(glfwGetWindowUserPointer(window));
  if (!impl) return;

  // GLFW reports framebuffer size which may be 2x on Retina even when _allowHIDPI=false.
  // Get actual logical window size instead.
  int win_w, win_h;
  glfwGetWindowSize(window, &win_w, &win_h);
  logchan_secwin->log("_secwin_callback_fbresized: fb=%dx%d win=%dx%d (using win)", fb_w, fb_h, win_w, win_h);

  impl->_onResize(win_w, win_h);
}

///////////////////////////////////////////////////////////////////////////////

static void _secwin_callback_close(GLFWwindow* window) {
  auto impl = static_cast<SecondaryWinImpl*>(glfwGetWindowUserPointer(window));
  if (!impl) return;

  impl->_owner->requestClose();
}

///////////////////////////////////////////////////////////////////////////////

static void _secwin_callback_focus(GLFWwindow* window, int focused) {
  auto impl = static_cast<SecondaryWinImpl*>(glfwGetWindowUserPointer(window));
  if (!impl) return;

  auto uiev = std::make_shared<ui::Event>();
  uiev->_eventcode = focused
      ? ui::EventCode::GOT_KEYFOCUS
      : ui::EventCode::LOST_KEYFOCUS;
  uiev->miX = impl->_mouseX;
  uiev->miY = impl->_mouseY;

  // Notify Window object (matches primary window behavior)
  if (impl->_orkWindow) {
    if (focused) {
      impl->_orkWindow->GotFocus();
    } else {
      impl->_orkWindow->LostFocus();
    }
  }

  logchan_secwin->log("Focus %s", focused ? "gained" : "lost");
  impl->_fireEvent(uiev);
}

///////////////////////////////////////////////////////////////////////////////

static void _secwin_callback_enterleave(GLFWwindow* window, int entered) {
  auto impl = static_cast<SecondaryWinImpl*>(glfwGetWindowUserPointer(window));
  if (!impl) return;

  auto uiev = std::make_shared<ui::Event>();
  uiev->_eventcode = entered
      ? ui::EventCode::MOUSE_ENTER
      : ui::EventCode::MOUSE_LEAVE;
  uiev->miX = impl->_mouseX;
  uiev->miY = impl->_mouseY;

  impl->_fireEvent(uiev);
}

///////////////////////////////////////////////////////////////////////////////
// EzSecondaryWin implementation using SecondaryWinImpl
///////////////////////////////////////////////////////////////////////////////

EzSecondaryWin::EzSecondaryWin(const EzSecondaryWinConfig& config) {
  _uicontext = std::make_shared<ui::Context>();
  _impl.makeShared<SecondaryWinImpl>(this, config);
}

///////////////////////////////////////////////////////////////////////////////

EzSecondaryWin::~EzSecondaryWin() {
  _impl.clear();
}

///////////////////////////////////////////////////////////////////////////////

bool EzSecondaryWin::shouldClose() const {
  return _shouldClose;
}

///////////////////////////////////////////////////////////////////////////////

void EzSecondaryWin::requestClose() {
  if (auto impl = _impl.tryAsShared<SecondaryWinImpl>()) {
    impl.value()->_closeWindow();
  }
  _shouldClose = true;
}

///////////////////////////////////////////////////////////////////////////////

int EzSecondaryWin::width() const {
  if (auto impl = _impl.tryAsShared<SecondaryWinImpl>()) {
    return impl.value()->_width;
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////////////

int EzSecondaryWin::height() const {
  if (auto impl = _impl.tryAsShared<SecondaryWinImpl>()) {
    return impl.value()->_height;
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////////////

ui::Context* EzSecondaryWin::uiContext() {
  return _uicontext.get();
}

///////////////////////////////////////////////////////////////////////////////

ui::context_ptr_t EzSecondaryWin::uiContextPtr() {
  return _uicontext;
}

///////////////////////////////////////////////////////////////////////////////

lev2::Context* EzSecondaryWin::gfxContext() {
  if (auto impl = _impl.tryAsShared<SecondaryWinImpl>()) {
    return impl.value()->_gfxContext;
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void EzSecondaryWin::_render() {
  if (auto impl = _impl.tryAsShared<SecondaryWinImpl>()) {
    impl.value()->_render();
  }
}

///////////////////////////////////////////////////////////////////////////////

void EzSecondaryWin::_handleResize(int w, int h) {
  if (auto impl = _impl.tryAsShared<SecondaryWinImpl>()) {
    impl.value()->_onResize(w, h);
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
