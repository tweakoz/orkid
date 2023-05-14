////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/orkconfig.h>

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/input/inputdevice.h>
///////////////////////////////////////////////////////////////////////////////

#include <GLFW/glfw3.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/gfx/ctxbase.h>

///////////////////////////////////////////////////////////////////////////////

struct SmtFinger;
typedef struct SmtFinger MtFinger;

namespace ork {

std::string TypeIdNameStrip(const char* name);
std::string TypeIdName(const std::type_info* ti);

namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

fvec2 logicalMousePos();

using glfw_win_lambda_t = std::function<void()>;
using glfw_win_int1_lambda_t = std::function<void(int)>;
using glfw_win_int2_lambda_t = std::function<void(int, int)>;
using glfw_win_int3_lambda_t = std::function<void(int, int, int)>;
using glfw_win_int4_lambda_t = std::function<void(int, int, int, int)>;
using glfw_win_float2_lambda_t = std::function<void(float, float)>;
using glfw_win_double2_lambda_t = std::function<void(double, double)>;

struct EventSinkGLFW{
  glfw_win_lambda_t _on_callback_refresh;
  glfw_win_int1_lambda_t _on_callback_enterleave;
  glfw_win_int2_lambda_t _on_callback_winresized;
  glfw_win_int2_lambda_t _on_callback_fbresized;
  glfw_win_int3_lambda_t _on_callback_mousebuttons;
  glfw_win_int4_lambda_t _on_callback_keyboard;
  glfw_win_double2_lambda_t _on_callback_scroll;
  glfw_win_double2_lambda_t _on_callback_cursor;
};

using eventsink_glfw_ptr_t = std::shared_ptr<EventSinkGLFW>;

struct CtxGLFW : public CTXBASE {

  static CtxGLFW* globalOffscreenContext();

  void SlotRepaint() final;
  int runloop();

  void Show() final;
  void Hide() final;

  void _setRefreshPolicy(RefreshPolicyItem epolicy) final;

  CtxGLFW(Window* pwin);
  void initWithData(appinitdata_ptr_t aid);
  ~CtxGLFW();

  void onResize(int W, int H);
  void SetAlwaysRun(bool brun);
  fvec2 MapCoordToGlobal(const fvec2& v) const override;
  void makeCurrent() final;
  void swapBuffers();
  void signalExit();
  void pollEvents();
  void _doEnqueueWindowResize( int w, int h ) final;

  void _on_callback_refresh();
  void _on_callback_winresized(int w, int h);
  void _on_callback_fbresized(int w, int h);
  void _on_callback_keyboard(int key, int scancode, int action, int mods);
  void _on_callback_mousebuttons(int button, int action, int modifiers);
  void _on_callback_scroll(double xoffset, double yoffset);
  void _on_callback_cursor(double xoffset, double yoffset);
  void _on_callback_enterleave(int entered);
  void _fire_ui_event();

  ui::event_constptr_t uievent() const;
  ui::event_ptr_t uievent();
  Context* context() const;

  GLFWwindow* _glfwWindow = nullptr;

  bool mbAlwaysRun = false;
  int _width       = 32;
  int _height      = 32;
  int mDrawLock    = 0;
  int _runstate    = 0;
  int _buttonState = 0;

  ui::event_ptr_t _uievent;
  void_lambda_t _onRunLoopIteration;
  appinitdata_ptr_t _appinitdata;
  std::function<void(Context*)> _onGpuInit;
  std::function<void(Context*)> _onGpuExit;
  GLFWmonitor* _glfwMonitor = nullptr;
  eventsink_glfw_ptr_t _eventSINK;
};

///////////////////////////////////////////////////////////////////////////////

struct AppWindow : public ork::lev2::Window {
public:
  bool mbinit;

  AppWindow(uiwidget_ptr_t root_widget);
  ~AppWindow();

  virtual void draw();
  void GotFocus() final;
  void LostFocus() final;
  virtual void Hide()  {
  }
  void OnShow() final;
};

///////////////////////////////////////////////////////////////////////////////

struct PopupWindow : public ork::lev2::Window {
public:

  svar256_t _impl;
  ui::context_ptr_t _uicontext;
  bool _useTransparency = false;

  PopupWindow(Context* pctx, int x, int y, int w, int h, bool transparent = false);
  ~PopupWindow();

  void mainThreadLoop();
};

///////////////////////////////////////////////////////////////////////////////

} // namespace lev2
} // namespace ork

///////////////////////////////////////////////////////////////////////////////
