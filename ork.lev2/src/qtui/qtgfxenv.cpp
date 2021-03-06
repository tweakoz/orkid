////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/ctxbase.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/qtui/qtui.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/context.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
CQtWindow::CQtWindow(ui::Widget* prw)
    : Window(0, 0, 640, 448, "yo")
    , mbinit(true)
    , mRootWidget(prw) {
}
///////////////////////////////////////////////////////////////////////////////
CQtWindow::~CQtWindow() {
  if (mRootWidget) {
    delete mRootWidget;
  }
}
///////////////////////////////////////////////////////////////////////////////
void CQtWindow::Draw(void) {
  int iw = GetContextW();
  int ih = GetContextH();
  mRootWidget->SetRect(0, 0, iw, ih);

  auto drwev = std::make_shared<ui::DrawEvent>(context());
  mRootWidget->Draw(drwev);
}
///////////////////////////////////////////////////////////////////////////////
void CQtWindow::GotFocus(void) {
  if (mRootWidget and mRootWidget->_uicontext) {
    auto uievent        = std::make_shared<ui::Event>();
    uievent->_eventcode = ork::ui::EventCode::GOT_KEYFOCUS;
    mRootWidget->_uicontext->handleEvent(uievent);
  }
  mbHasFocus = true;
}
///////////////////////////////////////////////////////////////////////////////
void CQtWindow::LostFocus(void) {
  if (mRootWidget and mRootWidget->_uicontext) {
    auto uievent        = std::make_shared<ui::Event>();
    uievent->_eventcode = ork::ui::EventCode::LOST_KEYFOCUS;
    mRootWidget->_uicontext->handleEvent(uievent);
  }
  mbHasFocus = false;
}
///////////////////////////////////////////////////////////////////////////////
void CQtWindow::OnShow() {
  ork::lev2::Context* pTARG = context();

  // if( mbinit )
  {
    // mbinit = false;
    /////////////////////////////////////////////////////////////////////
    int iw = GetContextW();
    int ih = GetContextH();
    mRootWidget->SetRect(0, 0, iw, ih);
    SetRootWidget(mRootWidget);
  }
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
