////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/ctxbase.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/pch.h>
////////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/qtui/qtui.h>
#include <ork/lev2/ui/viewport.h>
///////////////////////////////////////////////////////////////////////////////
#include <QtGui/QCursor>
#include <QtWidgets/QGesture>
#include <QtWidgets/QMainWindow>
#include <ork/kernel/msgrouter.inl>
#include <ork/math/basicfilters.h>
///////////////////////////////////////////////////////////////////////////////
extern "C" void StartTouchReciever(void* tr);
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
QCtxWidget::QCtxWidget(CTQT* pctxbase, QWidget* parent)
    : QWidget(parent)
    , mpCtxBase(pctxbase)
    , mbSignalConnected(false)
    , mQtTimer(this)
    , mbEnabled(false)
    , miWidth(1)
    , miHeight(1) {
  setMouseTracking(true);
  setAutoFillBackground(false);
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_OpaquePaintEvent);
  setFocusPolicy(Qt::StrongFocus);
  connect(&mQtTimer, SIGNAL(timeout()), this, SLOT(repaint()));
  // parent->activateWindow();
  // activateWindow();
  setAttribute(Qt::WA_DeleteOnClose);
  // show();
  // setAttribute( Qt::WA_AcceptTouchEvents );
  // grabGesture(Qt::PanGesture);
  _evstealwidget = nullptr;

  _pushTimer.Start();

  auto assignkey = [&](int from, int to) { _keymap[Qt::Key(from)] = ERawTriggerNames(to); };

  assignkey(Qt::Key_Left, ETRIG_RAW_KEY_LEFT);
  assignkey(Qt::Key_Right, ETRIG_RAW_KEY_RIGHT);
  assignkey(Qt::Key_Up, ETRIG_RAW_KEY_UP);
  assignkey(Qt::Key_Down, ETRIG_RAW_KEY_DOWN);
  assignkey(Qt::Key_Enter, ETRIG_RAW_KEY_RETURN);
  for (int k = Qt::Key_A; k <= Qt::Key_Z; k++) {
    assignkey(k, (k - Qt::Key_A) + ETRIG_RAW_ALPHA_A);
  }
  for (int k = Qt::Key_0; k <= Qt::Key_9; k++) {
    assignkey(k, (k - Qt::Key_0) + ETRIG_RAW_NUMBER_0);
  }
}
///////////////////////////////////////////////////////////////////////////////
QCtxWidget::~QCtxWidget() {
  if (mpCtxBase) {
    delete mpCtxBase;
    // delete
  }
}
///////////////////////////////////////////////////////////////////////////////
bool QCtxWidget::event(QEvent* event) {
  return QWidget::event(event);
}
///////////////////////////////////////////////////////////////////////////////
void QCtxWidget::showEvent(QShowEvent* event) {
  uievent()->mpBlindEventData = (void*)event;
  QWidget::showEvent(event);
  parentWidget()->show();
}
///////////////////////////////////////////////////////////////////////////////
#if defined(__APPLE__)
extern bool _macosUseHIDPI;
#endif
///////////////////////////////////////////////////////////////////////////////
void QCtxWidget::resizeEvent(QResizeEvent* event) {
  if (nullptr == event)
    return;

  uievent()->mpBlindEventData = (void*)event;
  QWidget::resizeEvent(event);
  QSize size = event->size();
  int X      = 0;
  int Y      = 0;
  int W      = size.rwidth();
  int H      = size.rheight();
#if defined(__APPLE__)
  if (_macosUseHIDPI) {
    W *= 2;
    H *= 2;
  }
#endif
  if (0)
    printf("W<%d> H<%d>\n", W, H);
  miWidth  = W;
  miHeight = H;
  if (mpCtxBase)
    mpCtxBase->Resize(X, Y, W, H);
}
///////////////////////////////////////////////////////////////////////////////
void QCtxWidget::paintEvent(QPaintEvent* event) {
  static int gistackctr = 0;
  static int gictr      = 0;

  gistackctr++;
  if ((1 == gistackctr) && (gictr > 0)) {
    uievent()->mpBlindEventData = (void*)event;
    if (IsEnabled()) {
      if (mpCtxBase)
        mpCtxBase->SlotRepaint();
    }
  }
  gistackctr--;
  gictr++;
}
///////////////////////////////////////////////////////////////////////////////
void QCtxWidget::MouseEventCommon(QMouseEvent* event) {
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

  float unitX = float(event->x()) / float(miWidth);
  float unitY = float(event->y()) / float(miHeight);

  uiev->mfLastUnitX = uiev->mfUnitX;
  uiev->mfLastUnitY = uiev->mfUnitY;
  uiev->mfUnitX     = unitX;
  uiev->mfUnitY     = unitY;

  //   printf( "UNITX<%f> UNITY<%f>\n", unitX, unitY );
  //    printf( "ix<%d %d>\n", ix, iy );
}
///////////////////////////////////////////////////////////////////////////////
static fvec2 gpos;
void QCtxWidget::mouseMoveEvent(QMouseEvent* event) {
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
  if (mpCtxBase)
    mpCtxBase->SlotRepaint();
}
///////////////////////////////////////////////////////////////////////////////
void QCtxWidget::mousePressEvent(QMouseEvent* event) {
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
  if (mpCtxBase)
    mpCtxBase->SlotRepaint();
}
///////////////////////////////////////////////////////////////////////////////
void QCtxWidget::mouseDoubleClickEvent(QMouseEvent* event) {
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
  if (mpCtxBase)
    mpCtxBase->SlotRepaint();
}
///////////////////////////////////////////////////////////////////////////////
void QCtxWidget::mouseReleaseEvent(QMouseEvent* event) {
  printf("gotrelease\n");
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

  if (mpCtxBase)
    mpCtxBase->SlotRepaint();
}
///////////////////////////////////////////////////////////////////////////////
void QCtxWidget::wheelEvent(QWheelEvent* qem) {
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

  if (mpCtxBase)
    mpCtxBase->SlotRepaint();
}
///////////////////////////////////////////////////////////////////////////////
void QCtxWidget::keyPressEvent(QKeyEvent* event) {

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
    c.Set<int>(uiev->miKeyCode);
    msgrouter::channel("qtkeyboard.down")->post(c);
  }

  if (root) {
    uiev->setvpDim(root);
    ui::Event::sendToContext(uiev);
  }

  if (mpCtxBase)
    mpCtxBase->SlotRepaint();
}
///////////////////////////////////////////////////////////////////////////////
void QCtxWidget::keyReleaseEvent(QKeyEvent* event) {
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
    c.Set<int>(uiev->miKeyCode);
    msgrouter::channel("qtkeyboard.up")->post(c);
  }

  if (root) {
    uiev->setvpDim(root);
    ui::Event::sendToContext(uiev);
  }

  if (mpCtxBase)
    mpCtxBase->SlotRepaint();
}
///////////////////////////////////////////////////////////////////////////////
void QCtxWidget::focusInEvent(QFocusEvent* event) {
  auto uiev              = uievent();
  auto gfxwin            = uiev->mpGfxWin;
  auto root              = gfxwin ? gfxwin->GetRootWidget() : nullptr;
  uiev->_uicontext       = root ? root->_uicontext : nullptr;
  uiev->_eventcode       = ork::ui::EventCode::GOT_KEYFOCUS;
  uiev->mpBlindEventData = (void*)event;
  if (Target()) {
    ui::Event::sendToContext(uiev);
  }
  // orkprintf( "CTQT %08x got keyboard focus\n", this );
  QWidget::focusInEvent(event);
  if (GetWindow())
    GetWindow()->GotFocus();
  //////////////////////////////////////////
  if (mpCtxBase)
    mpCtxBase->SlotRepaint();
}
///////////////////////////////////////////////////////////////////////////////
void QCtxWidget::focusOutEvent(QFocusEvent* event) {
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
  // orkprintf( "CTQT %08x lost keyboard focus\n", this );
  QWidget::focusOutEvent(event);
  if (GetWindow())
    GetWindow()->LostFocus();
  //////////////////////////////////////////
  if (mpCtxBase)
    mpCtxBase->SlotRepaint();
}
///////////////////////////////////////////////////////////////////////////////
ui::event_ptr_t QCtxWidget::uievent() {
  return mpCtxBase->_uievent;
}
///////////////////////////////////////////////////////////////////////////////
ui::event_constptr_t QCtxWidget::uievent() const {
  return mpCtxBase->_uievent;
}
///////////////////////////////////////////////////////////////////////////////
Context* QCtxWidget::Target() const {
  return mpCtxBase->_target;
}
///////////////////////////////////////////////////////////////////////////////
Window* QCtxWidget::GetWindow() const {
  return mpCtxBase->mpWindow;
}
///////////////////////////////////////////////////////////////////////////////
bool QCtxWidget::AlwaysRun() const {
  return mpCtxBase->mbAlwaysRun;
}
///////////////////////////////////////////////////////////////////////////////
QTimer& CTQT::Timer() const {
  return mpQtWidget->mQtTimer;
}
///////////////////////////////////////////////////////////////////////////////
void CTQT::Show() {
  _parent->show();
  if (mbInitialize) {
    printf("CreateCONTEXT\n");
    mpWindow->initContext();
    mpWindow->OnShow();
    mbInitialize = false;
  }
}
///////////////////////////////////////////////////////////////////////////////
void CTQT::Hide() {
  _parent->hide();
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
void CTQT::_setRefreshPolicy(RefreshPolicyItem newpolicy) { // final

  auto prev         = _curpolicy;
  int prev_qtmillis = to_qtmillis(prev);
  int next_qtmillis = to_qtmillis(newpolicy);

  if (next_qtmillis != prev_qtmillis) {
    if (next_qtmillis == -1) {
      Timer().stop();
    } else {
      Timer().start();
      Timer().setInterval(next_qtmillis);
    }
  }

  _curpolicy = newpolicy;
}
///////////////////////////////////////////////////////////////////////////////
CTQT::CTQT(Window* pwin, QWidget* pparent)
    : CTXBASE(pwin)
    , mbAlwaysRun(false)
    , mix(0)
    , miy(0)
    , miw(0)
    , mih(0)
    , _parent(0)
    , mDrawLock(0) {
  this->SetThisXID(CTFLXID(0));
  this->SetTopXID(CTFLXID(0));

  SetParent(pparent);
}
///////////////////////////////////////////////////////////////////////////////
CTQT::~CTQT() {
}
///////////////////////////////////////////////////////////////////////////////
void CTQT::SetParent(QWidget* pparent) {
  printf("CTQT::SetParent() pparent<%p>\n", pparent);
  QMainWindow* mainwin = 0;
  if (0 == pparent) {
    mainwin = new QMainWindow;
    pparent = mainwin;
  }

  _parent    = pparent;
  mpQtWidget = new QCtxWidget(this, pparent);
  if (mainwin) {
    mainwin->setCentralWidget(mpQtWidget);
  }

  this->SetThisXID((CTFLXID)winId());
  this->SetTopXID((CTFLXID)pparent->winId());

  mpWindow->SetDirty(true);
}
///////////////////////////////////////////////////////////////////////////////
fvec2 CTQT::MapCoordToGlobal(const fvec2& v) const {
  QPoint p(v.GetX(), v.GetY());
  QPoint p2 = mpQtWidget->mapToGlobal(p);
  return fvec2(p2.x(), p2.y());
}
///////////////////////////////////////////////////////////////////////////////
void CTQT::Resize(int X, int Y, int W, int H) {
  //////////////////////////////////////////////////////////
  lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
  //////////////////////////////////////////////////////////

  this->SetThisXID((CTFLXID)winId());
  // printf( "CTQT::Resize() _target<%p>\n", _target );
  if (_target) {
    _target->resizeMainSurface(W, H);
    _uievent->mpGfxWin = (Window*)_target->FBI()->GetThisBuffer();
    if (_uievent->mpGfxWin)
      _uievent->mpGfxWin->Resize(X, Y, W, H);
  }
  lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
}
///////////////////////////////////////////////////////////////////////////////
void CTQT::SlotRepaint() {
  auto lamb = [&]() {
    if (not GfxEnv::initialized())
      return;

    auto pos = ork::lev2::logicalMousePos();

    msgrouter::content_t c;

    float fx = (gpos.x / 1440.0) * 2.0f - 1.0f;
    float fy = (gpos.y / 900.0) * 2.0f - 1.0f;

    c.Set<fvec2>(fvec2(fx, fy));

    msgrouter::channel("qtmousepos")->post(c);

    ork::PerfMarkerPush("ork.viewport.draw.begin");

    this->mDrawLock++;
    if (this->mDrawLock == 1) {
      // printf( "CTQT::SlotRepaint() _target<%p>\n", _target );
      if (this->_target) {
        _target->makeCurrentContext();
        auto gfxwin = _uievent->mpGfxWin;
        auto vp     = gfxwin ? gfxwin->GetRootWidget() : nullptr;

        // this->UIEvent->mpGfxWin = (Window*) this->_target->FBI()->GetThisBuffer();
        auto drwev = std::make_shared<ui::DrawEvent>(this->_target);

        if (vp)
          vp->Draw(drwev);
      }
    }
    this->mDrawLock--;
    ork::PerfMarkerPush("ork.viewport.draw.end");
  };

  if (opq::TrackCurrent::is(opq::mainSerialQueue())) {
    // already on main Q
    lamb();
  } else {
    opq::mainSerialQueue()->enqueue(lamb);
  }
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
