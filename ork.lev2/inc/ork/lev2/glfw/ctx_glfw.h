////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
};

///////////////////////////////////////////////////////////////////////////////

struct AppWindow : public ork::lev2::Window {
public:
  bool mbinit;

  AppWindow(uiwidget_ptr_t root_widget);
  ~AppWindow();

  virtual void Draw(void);
  virtual void GotFocus(void);
  virtual void LostFocus(void);
  virtual void Hide(void) {
  }
  virtual void OnShow();
};

///////////////////////////////////////////////////////////////////////////////

} // namespace lev2
} // namespace ork

///////////////////////////////////////////////////////////////////////////////
