///////////////////////////////////////////////////////////////////////////////
//
//	Orkid QT User Interface Glue
//
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/glfw/ctx_glfw.h>
#include <ork/kernel/string/StringBlock.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/ui/viewport.h>

#if defined(ORK_CONFIG_IX)
#include <cxxabi.h>
#endif

namespace ork {

file::Path SaveFileRequester(const std::string& title, const std::string& ext) {
  /*auto& gfxenv = lev2::GfxEnv::GetRef();
  gfxenv.GetGlobalLock().Lock();
  QString FileName           = QFileDialog::getSaveFileName(0, "Export ProcTexImage", 0, "PNG (*.png)");
  file::Path::NameType fname = FileName.toStdString().c_str();
  gfxenv.GetGlobalLock().UnLock();
  return file::Path(fname);
  */
  return file::Path();
}

#if defined(ORK_CONFIG_IX)
std::string GccDemangle(const std::string& inname) {
  int status;
  const char* pmangle = abi::__cxa_demangle(inname.c_str(), 0, 0, &status);
  return std::string(pmangle);
}
#endif

std::string TypeIdNameStrip(const char* name) {
  std::string strippedName(name);

#if defined(ORK_CONFIG_IX)
  strippedName = GccDemangle(strippedName);
#endif

  size_t classLength = strlen("class ");
  ;
  size_t classPosition;
  while ((classPosition = strippedName.find("class ")) != std::string::npos) {
    strippedName.swap(strippedName.erase(classPosition, classLength));
  }

  return strippedName;
}
std::string MethodIdNameStrip(const char* name) // mainly used for QT signals and slots
{

  std::string inname(name);

#if defined(ORK_CONFIG_IX)
  inname = GccDemangle(inname);
#endif

  FixedString<65536> newname = inname.c_str();
  newname.replace_in_place("std::", "");
  newname.replace_in_place("__thiscall ", "");
  return newname.c_str();
}

std::string TypeIdName(const std::type_info* ti) {
  return (ti != nullptr) ? TypeIdNameStrip(ti->name()) : "nil";
}

namespace lev2 {

fvec2 logicalMousePos() {
  fvec2 rval; // current mouse pos
  if (_HIDPI()) {
    rval.x *= 0.5f;
    rval.y *= 0.5f;
  }
  return rval;
}

void OrkGlobalDisableMousePointer() {
  //QApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
}
void OrkGlobalEnableMousePointer() {
  //QApplication::restoreOverrideCursor();
}

} // namespace lev2
} // namespace ork

/*
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW_Window::MouseEventCommon(QMouseEvent* event) {
  auto uiev = uievent();

  uiev->mpBlindEventData = (void*)event;

  // InputManager::instance()->poll();

  Qt::MouseButtons Buttons        = event->buttons();
  Qt::KeyboardModifiers modifiers = event->modifiers();

  int ix = event->x();
  int iy = event->y();

  if (_HIDPI()) {
    ix /= 2;
    iy /= 2;
  }

  uiev->mbALT          = (modifiers & Qt::AltModifier);
  uiev->mbCTRL         = (modifiers & Qt::ControlModifier);
  uiev->mbSHIFT        = (modifiers & Qt::ShiftModifier);
  uiev->mbMETA         = (modifiers & Qt::MetaModifier);
  uiev->mbLeftButton   = (Buttons & Qt::LeftButton);
  uiev->mbMiddleButton = (Buttons & Qt::MidButton);
  uiev->mbRightButton  = (Buttons & Qt::RightButton);

  uiev->miLastX = uiev->miX;
  uiev->miLastY = uiev->miY;

  uiev->miX = ix;
  uiev->miY = iy;

  float unitX = float(event->x()) / float(_width);
  float unitY = float(event->y()) / float(_height);

  uiev->mfLastUnitX = uiev->mfUnitX;
  uiev->mfLastUnitY = uiev->mfUnitY;
  uiev->mfUnitX     = unitX;
  uiev->mfUnitY     = unitY;

  //   printf( "UNITX<%f> UNITY<%f>\n", unitX, unitY );
  //    printf( "ix<%d %d>\n", ix, iy );
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW_Window::mouseMoveEvent(QMouseEvent* event) {
  auto uiev        = uievent();
  auto gfxwin      = uiev->mpGfxWin;
  auto root        = gfxwin ? gfxwin->GetRootWidget() : nullptr;
  uiev->_uicontext = root ? root->_uicontext : nullptr;

  gpos.x = event->x();
  gpos.y = event->y();
  if (_HIDPI()) {
    gpos.x /= 2.0f;
    gpos.y /= 2.0f;
  }
  MouseEventCommon(event);

  Qt::MouseButtons Buttons = event->buttons();

  bool isbutton = (Buttons != Qt::NoButton);

  uiev->_eventcode = (isbutton) //
                         ? ork::ui::EventCode::DRAG
                         : ork::ui::EventCode::MOVE;

  if (root) {
    uiev->setvpDim(root);
    ui::Event::sendToContext(uiev);
  }
  if (_ctxglfw)
    _ctxglfw->SlotRepaint();
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW_Window::mousePressEvent(QMouseEvent* event) {
  MouseEventCommon(event);
  auto uiev        = uievent();
  auto gfxwin      = uiev->mpGfxWin;
  auto root        = gfxwin ? gfxwin->GetRootWidget() : nullptr;
  uiev->_uicontext = root ? root->_uicontext : nullptr;
  uiev->_eventcode = ork::ui::EventCode::PUSH;
  if (root) {
    uiev->setvpDim(root);
    ui::Event::sendToContext(uiev);
    _pushTimer.Start();
  }
  if (_ctxglfw)
    _ctxglfw->SlotRepaint();
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW_Window::mouseDoubleClickEvent(QMouseEvent* event) {
  MouseEventCommon(event);
  auto uiev        = uievent();
  auto gfxwin      = uiev->mpGfxWin;
  auto root        = gfxwin ? gfxwin->GetRootWidget() : nullptr;
  uiev->_uicontext = root ? root->_uicontext : nullptr;
  uiev->_eventcode = ork::ui::EventCode::DOUBLECLICK;

  if (root) {
    uiev->setvpDim(root);
    ui::Event::sendToContext(uiev);
  }
  if (_ctxglfw)
    _ctxglfw->SlotRepaint();
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW_Window::mouseReleaseEvent(QMouseEvent* event) {
  //printf("gotrelease\n");
  MouseEventCommon(event);
  auto uiev        = uievent();
  auto gfxwin      = uiev->mpGfxWin;
  auto root        = gfxwin ? gfxwin->GetRootWidget() : nullptr;
  uiev->_uicontext = root ? root->_uicontext : nullptr;
  uiev->_eventcode = ork::ui::EventCode::RELEASE;

  //////////////////////////////////////

  if (root) {
    uiev->setvpDim(root);
    ui::Event::sendToContext(uiev);
  }

  _evstealwidget = nullptr;

  if (_ctxglfw)
    _ctxglfw->SlotRepaint();
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW_Window::wheelEvent(QWheelEvent* qem) {
  auto uiev        = uievent();
  auto gfxwin      = uiev->mpGfxWin;
  auto root        = gfxwin ? gfxwin->GetRootWidget() : nullptr;
  uiev->_uicontext = root ? root->_uicontext : nullptr;

  uiev->mpBlindEventData = (void*)qem;
  static avg_filter<3> gScrollFilter;

#if defined(ORK_CONFIG_DARWIN) // trackpad gesture filter
  int irawdelta = qem->delta();
  int idelta    = (2 * gScrollFilter.compute(irawdelta) / 9);
#else
  int idelta = qem->delta();
#endif

  Qt::KeyboardModifiers modifiers = qem->modifiers();

  uiev->mbALT   = (modifiers & Qt::AltModifier);
  uiev->mbCTRL  = (modifiers & Qt::ControlModifier);
  uiev->mbSHIFT = (modifiers & Qt::ShiftModifier);
  uiev->mbMETA  = (modifiers & Qt::MetaModifier);

  uiev->_eventcode = ork::ui::EventCode::MOUSEWHEEL;

  uiev->miMWY = idelta;

  if (root && idelta != 0) {
    uiev->setvpDim(root);
    ui::Event::sendToContext(uiev);
  }

  if (_ctxglfw)
    _ctxglfw->SlotRepaint();
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW_Window::keyPressEvent(QKeyEvent* event) {

  auto uiev        = uievent();
  auto gfxwin      = uiev->mpGfxWin;
  auto root        = gfxwin ? gfxwin->GetRootWidget() : nullptr;
  uiev->_uicontext = root ? root->_uicontext : nullptr;

  uiev->mpBlindEventData = (void*)event;
  uiev->_eventcode       = event->isAutoRepeat() //
                         ? ork::ui::EventCode::KEY_REPEAT
                         : ork::ui::EventCode::KEY;

  auto ikeyUNI = event->key();

  Qt::KeyboardModifiers modifiers = event->modifiers();

  uiev->mbALT   = (modifiers & Qt::AltModifier);
  uiev->mbCTRL  = (modifiers & Qt::ControlModifier);
  uiev->mbSHIFT = (modifiers & Qt::ShiftModifier);
  uiev->mbMETA  = (modifiers & Qt::MetaModifier);

  auto keyc       = _keymap.find(Qt::Key(ikeyUNI));
  uiev->miKeyCode = int(ikeyUNI);
  if (keyc != _keymap.end()) {
    uiev->miKeyCode = keyc->second;
    msgrouter::content_t c;
    c.set<int>(uiev->miKeyCode);
    msgrouter::channel("qtkeyboard.down")->post(c);
  }

  if (root) {
    uiev->setvpDim(root);
    ui::Event::sendToContext(uiev);
  }

  if (_ctxglfw)
    _ctxglfw->SlotRepaint();
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW_Window::keyReleaseEvent(QKeyEvent* event) {
  if (event->isAutoRepeat())
    return;

  auto uiev        = uievent();
  auto gfxwin      = uiev->mpGfxWin;
  auto root        = gfxwin ? gfxwin->GetRootWidget() : nullptr;
  uiev->_uicontext = root ? root->_uicontext : nullptr;

  uiev->mpBlindEventData = (void*)event;
  uiev->_eventcode       = ork::ui::EventCode::KEYUP;

  auto ikeyUNI    = event->key();
  uiev->miKeyCode = int(ikeyUNI);
  auto keyc       = _keymap.find(Qt::Key(ikeyUNI));
  if (keyc != _keymap.end()) {
    uiev->miKeyCode = keyc->second;
    msgrouter::content_t c;
    c.set<int>(uiev->miKeyCode);
    msgrouter::channel("qtkeyboard.up")->post(c);
  }

  if (root) {
    uiev->setvpDim(root);
    ui::Event::sendToContext(uiev);
  }

  if (_ctxglfw)
    _ctxglfw->SlotRepaint();
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW_Window::focusInEvent(QFocusEvent* event) {
  auto uiev              = uievent();
  auto gfxwin            = uiev->mpGfxWin;
  auto root              = gfxwin ? gfxwin->GetRootWidget() : nullptr;
  uiev->_uicontext       = root ? root->_uicontext : nullptr;
  uiev->_eventcode       = ork::ui::EventCode::GOT_KEYFOCUS;
  uiev->mpBlindEventData = (void*)event;
  if (Target()) {
    ui::Event::sendToContext(uiev);
  }
  // orkprintf( "CtxGLFW %08x got keyboard focus\n", this );
  QWidget::focusInEvent(event);
  if (GetWindow())
    GetWindow()->GotFocus();
  //////////////////////////////////////////
  if (_ctxglfw)
    _ctxglfw->SlotRepaint();
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW_Window::focusOutEvent(QFocusEvent* event) {
  auto uiev              = uievent();
  auto gfxwin            = uiev->mpGfxWin;
  auto root              = gfxwin ? gfxwin->GetRootWidget() : nullptr;
  uiev->_uicontext       = root ? root->_uicontext : nullptr;
  uiev->_eventcode       = ork::ui::EventCode::LOST_KEYFOCUS;
  uiev->mpBlindEventData = (void*)event;
  if (root) {
    uiev->setvpDim(root);
    ui::Event::sendToContext(uiev);
  }
  // orkprintf( "CtxGLFW %08x lost keyboard focus\n", this );
  QWidget::focusOutEvent(event);
  if (GetWindow())
    GetWindow()->LostFocus();
  //////////////////////////////////////////
  if (_ctxglfw)
    _ctxglfw->SlotRepaint();
}*/