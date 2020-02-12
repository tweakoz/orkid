////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/ctxbase.h>
//
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/qtui/qtui.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/widget.h>

#if !defined(_CYGWIN)

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

CQtWindow::CQtWindow(ui::Widget* prw)
    : Window(0, 0, 640, 448, "yo")
    , mbinit(true)
    , mRootWidget(prw) {
}

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

  ui::DrawEvent drwev(context());
  mRootWidget->Draw(drwev);
}

///////////////////////////////////////////////////////////////////////////////

void CQtWindow::GotFocus(void) {
  if (mRootWidget) {
    mRootWidget->GotKeyboardFocus();

    ui::Event uievent;
    uievent.mEventCode = ork::ui::UIEV_GOT_KEYFOCUS;
    mRootWidget->HandleUiEvent(uievent);
  }
  mbHasFocus = true;
}

///////////////////////////////////////////////////////////////////////////////

void CQtWindow::LostFocus(void) {
  if (mRootWidget) {
    mRootWidget->LostKeyboardFocus();

    ui::Event uievent;
    uievent.mEventCode = ork::ui::UIEV_LOST_KEYFOCUS;
    mRootWidget->HandleUiEvent(uievent);
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

}} // namespace ork::lev2

#endif
