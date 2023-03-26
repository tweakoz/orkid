////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/ctxbase.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/glfw/ctx_glfw.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/context.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
AppWindow::AppWindow(uiwidget_ptr_t prw)
    : Window(0, 0, 1280, 720, "yo")
    , mbinit(true){

    _rootWidget = prw;

    if(_HIDPI()){
      SetBufferWidth(1280*2);
      SetBufferHeight(720*2);
    }
}
///////////////////////////////////////////////////////////////////////////////
AppWindow::~AppWindow() {
}
///////////////////////////////////////////////////////////////////////////////
void AppWindow::draw(void) {
  int iw = GetContextW();
  int ih = GetContextH();
  _rootWidget->SetRect(0, 0, iw, ih);
  auto drwev = std::make_shared<ui::DrawEvent>(context());
  _rootWidget->draw(drwev);
}
///////////////////////////////////////////////////////////////////////////////
void AppWindow::GotFocus(void) {
  if (_rootWidget and _rootWidget->_uicontext) {
    auto uievent        = std::make_shared<ui::Event>();
    uievent->_eventcode = ork::ui::EventCode::GOT_KEYFOCUS;
    _rootWidget->_uicontext->handleEvent(uievent);
  }
  mbHasFocus = true;
}
///////////////////////////////////////////////////////////////////////////////
void AppWindow::LostFocus(void) {
  if (_rootWidget and _rootWidget->_uicontext) {
    auto uievent        = std::make_shared<ui::Event>();
    uievent->_eventcode = ork::ui::EventCode::LOST_KEYFOCUS;
    _rootWidget->_uicontext->handleEvent(uievent);
  }
  mbHasFocus = false;
}
///////////////////////////////////////////////////////////////////////////////
void AppWindow::OnShow() {
  ork::lev2::Context* pTARG = context();

  // if( mbinit )
  {
    // mbinit = false;
    /////////////////////////////////////////////////////////////////////
    int iw = GetContextW();
    int ih = GetContextH();
    _rootWidget->SetRect(0, 0, iw, ih);
  }
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
