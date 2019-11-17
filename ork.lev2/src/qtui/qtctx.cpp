////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/ctxbase.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/input/inputdevice.h>
#include <ork/pch.h>
//
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/qtui/qtui.h>
#include <ork/lev2/ui/viewport.h>

#include <QtGui/QCursor>
#include <QtWidgets/QGesture>
#include <QtWidgets/QMainWindow>
#include <ork/kernel/msgrouter.inl>
#include <ork/math/basicfilters.h>

extern "C" void StartTouchReciever(void* tr);

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

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
  show();
  activateWindow();
  setAttribute(Qt::WA_DeleteOnClose);
  // setAttribute( Qt::WA_AcceptTouchEvents );
  // grabGesture(Qt::PanGesture);
}

///////////////////////////////////////////////////////////////////////////////

QCtxWidget::~QCtxWidget() {
  if (mpCtxBase) {
    delete mpCtxBase;
    // delete
  }
}

///////////////////////////////////////////////////////////////////////////////

void QCtxWidget::SendOrkUiEvent() {
  if (UIEvent().mpGfxWin){
    UIEvent()._vpdim = fvec2(miWidth,miHeight);
    UIEvent().mpGfxWin->GetRootWidget()->HandleUiEvent(UIEvent());
  }
}

///////////////////////////////////////////////////////////////////////////////

bool QCtxWidget::event(QEvent* event) { return QWidget::event(event); }

///////////////////////////////////////////////////////////////////////////////

void QCtxWidget::showEvent(QShowEvent* event) {
  UIEvent().mpBlindEventData = (void*)event;
  QWidget::showEvent(event);
  parentWidget()->show();
}

///////////////////////////////////////////////////////////////////////////////

void QCtxWidget::resizeEvent(QResizeEvent* event) {
  UIEvent().mpBlindEventData = (void*)event;
  QWidget::resizeEvent(event);
  QSize size = event->size();
  int X      = 0;
  int Y      = 0;
  int W      = size.rwidth();
  int H      = size.rheight();
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
    UIEvent().mpBlindEventData = (void*)event;
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
  auto& uiev = UIEvent();

  uiev.mpBlindEventData = (void*)event;

  InputManager::poll();

  Qt::MouseButtons Buttons        = event->buttons();
  Qt::KeyboardModifiers modifiers = event->modifiers();

  int ix = event->x();
  int iy = event->y();

  if( _HIDPI ) {
    ix /= 2;
    iy /= 2;
  }


  uiev.mbALT          = (modifiers & Qt::AltModifier);
  uiev.mbCTRL         = (modifiers & Qt::ControlModifier);
  uiev.mbSHIFT        = (modifiers & Qt::ShiftModifier);
  uiev.mbMETA         = (modifiers & Qt::MetaModifier);
  uiev.mbLeftButton   = (Buttons & Qt::LeftButton);
  uiev.mbMiddleButton = (Buttons & Qt::MidButton);
  uiev.mbRightButton  = (Buttons & Qt::RightButton);

  uiev.miLastX = uiev.miX;
  uiev.miLastY = uiev.miY;

  uiev.miX = ix;
  uiev.miY = iy;

  float unitX = float(event->x()) / float(miWidth);
  float unitY = float(event->y()) / float(miHeight);

  uiev.mfLastUnitX = uiev.mfUnitX;
  uiev.mfLastUnitY = uiev.mfUnitY;
  uiev.mfUnitX     = unitX;
  uiev.mfUnitY     = unitY;

   printf( "UNITX<%f> UNITY<%f>\n", unitX, unitY );
    printf( "ix<%d %d>\n", ix, iy );
}

///////////////////////////////////////////////////////////////////////////////

static fvec2 gpos;
void QCtxWidget::mouseMoveEvent(QMouseEvent* event) {
  auto& uiev  = UIEvent();
  auto gfxwin = uiev.mpGfxWin;
  auto vp     = gfxwin ? gfxwin->GetRootWidget() : nullptr;

  gpos.x = event->x();
  gpos.y = event->y();
  if( _HIDPI ) {
    gpos.x /= 2.0f;
    gpos.y /= 2.0f;
  }
  MouseEventCommon(event);

  Qt::MouseButtons Buttons = event->buttons();

  uiev.miEventCode = (Buttons == Qt::NoButton) ? ork::ui::UIEV_MOVE : ork::ui::UIEV_DRAG;

  if (vp){
    uiev._vpdim = fvec2(vp->GetW(),vp->GetH());
    if( _HIDPI ){
      uiev._vpdim *= 0.5;
    }
    //gpos.x /= 2.0f;
    vp->HandleUiEvent(uiev);
  }
  if (mpCtxBase)
    mpCtxBase->SlotRepaint();
}

///////////////////////////////////////////////////////////////////////////////

void QCtxWidget::mousePressEvent(QMouseEvent* event) {
  MouseEventCommon(event);
  auto& uiev       = UIEvent();
  auto gfxwin      = uiev.mpGfxWin;
  auto vp          = gfxwin ? gfxwin->GetRootWidget() : nullptr;
  uiev.miEventCode = ork::ui::UIEV_PUSH;
  if (vp){
    uiev._vpdim = fvec2(vp->GetW(),vp->GetH());
    if( _HIDPI ){
      uiev._vpdim *= 0.5;
    }
    vp->HandleUiEvent(uiev);
  }
  if (mpCtxBase)
    mpCtxBase->SlotRepaint();
}

///////////////////////////////////////////////////////////////////////////////

void QCtxWidget::mouseDoubleClickEvent(QMouseEvent* event) {
  MouseEventCommon(event);
  auto& uiev       = UIEvent();
  auto gfxwin      = uiev.mpGfxWin;
  auto vp          = gfxwin ? gfxwin->GetRootWidget() : nullptr;
  uiev.miEventCode = ork::ui::UIEV_DOUBLECLICK;

  if (vp){
    uiev._vpdim = fvec2(vp->GetW(),vp->GetH());
    if( _HIDPI ){
      uiev._vpdim *= 0.5;
    }
    vp->HandleUiEvent(uiev);
  }
  if (mpCtxBase)
    mpCtxBase->SlotRepaint();
}

///////////////////////////////////////////////////////////////////////////////

void QCtxWidget::mouseReleaseEvent(QMouseEvent* event) {
  MouseEventCommon(event);
  auto& uiev       = UIEvent();
  auto gfxwin      = uiev.mpGfxWin;
  auto vp          = gfxwin ? gfxwin->GetRootWidget() : nullptr;
  uiev.miEventCode = ork::ui::UIEV_RELEASE;

  if (vp){
    uiev._vpdim = fvec2(vp->GetW(),vp->GetH());
    if( _HIDPI ){
      uiev._vpdim *= 0.5;
    }
    vp->HandleUiEvent(uiev);
  }

  if (mpCtxBase)
    mpCtxBase->SlotRepaint();
}

///////////////////////////////////////////////////////////////////////////////

void QCtxWidget::wheelEvent(QWheelEvent* qem) {
  auto& uiev  = UIEvent();
  auto gfxwin = uiev.mpGfxWin;
  auto vp     = gfxwin ? gfxwin->GetRootWidget() : nullptr;

  uiev.mpBlindEventData = (void*)qem;
  static avg_filter<3> gScrollFilter;

#if defined(_DARWIN) // trackpad gesture filter
  int irawdelta = qem->delta();
  int idelta    = (2 * gScrollFilter.compute(irawdelta) / 9);
#else
  int idelta = qem->delta();
#endif

  Qt::KeyboardModifiers modifiers = qem->modifiers();

  uiev.mbALT   = (modifiers & Qt::AltModifier);
  uiev.mbCTRL  = (modifiers & Qt::ControlModifier);
  uiev.mbSHIFT = (modifiers & Qt::ShiftModifier);
  uiev.mbMETA  = (modifiers & Qt::MetaModifier);

  uiev.miEventCode = ork::ui::UIEV_MOUSEWHEEL;

  uiev.miMWY = idelta;

  if (vp && idelta != 0){
    uiev._vpdim = fvec2(vp->GetW(),vp->GetH());
    if( _HIDPI ){
      uiev._vpdim *= 0.5;
    }
    vp->HandleUiEvent(uiev);
  }

  if (mpCtxBase)
    mpCtxBase->SlotRepaint();
}

///////////////////////////////////////////////////////////////////////////////

void QCtxWidget::keyPressEvent(QKeyEvent* event) {
  if (event->isAutoRepeat())
    return;

  auto& uiev  = UIEvent();
  auto gfxwin = uiev.mpGfxWin;
  auto vp     = gfxwin ? gfxwin->GetRootWidget() : nullptr;

  uiev.mpBlindEventData = (void*)event;
  uiev.miEventCode      = ork::ui::UIEV_KEY;

  int ikeyUNI = event->key();

  uiev.miKeyCode                  = ikeyUNI;
  Qt::KeyboardModifiers modifiers = event->modifiers();

  uiev.mbALT   = (modifiers & Qt::AltModifier);
  uiev.mbCTRL  = (modifiers & Qt::ControlModifier);
  uiev.mbSHIFT = (modifiers & Qt::ShiftModifier);
  uiev.mbMETA  = (modifiers & Qt::MetaModifier);

  if ((ikeyUNI >= Qt::Key_A) && (ikeyUNI <= Qt::Key_Z)) {
    uiev.miKeyCode = (ikeyUNI - Qt::Key_A) + int('a');

    msgrouter::content_t c;
    c.Set<int>(uiev.miKeyCode);
    msgrouter::channel("qtkeyboard.down")->post(c);
  }
  if (ikeyUNI == 0x01000004) // enter != (Qt::Key_Enter)
  {
    uiev.miKeyCode = 13;
  }

  if (vp){
    uiev._vpdim = fvec2(vp->GetW(),vp->GetH());
    if( _HIDPI ){
      uiev._vpdim *= 0.5;
    }
    vp->HandleUiEvent(uiev);
  }

  if (mpCtxBase)
    mpCtxBase->SlotRepaint();
}

///////////////////////////////////////////////////////////////////////////////

void QCtxWidget::keyReleaseEvent(QKeyEvent* event) {
  if (event->isAutoRepeat())
    return;

  auto& uiev  = UIEvent();
  auto gfxwin = uiev.mpGfxWin;
  auto vp     = gfxwin ? gfxwin->GetRootWidget() : nullptr;

  uiev.mpBlindEventData = (void*)event;
  uiev.miEventCode      = ork::ui::UIEV_KEYUP;

  int ikeyUNI = event->key();

  uiev.miKeyCode = ikeyUNI;

  if ((ikeyUNI >= Qt::Key_A) && (ikeyUNI <= Qt::Key_Z)) {

    uiev.miKeyCode = (ikeyUNI - Qt::Key_A) + int('a');
    msgrouter::content_t c;
    c.Set<int>(uiev.miKeyCode);
    msgrouter::channel("qtkeyboard.up")->post(c);
  }
  if (ikeyUNI == 0x01000004) // enter != (Qt::Key_Enter)
  {
    uiev.miKeyCode = 13;
  }

  if (vp){
    uiev._vpdim = fvec2(vp->GetW(),vp->GetH());
    if( _HIDPI ){
      uiev._vpdim *= 0.5;
    }
    vp->HandleUiEvent(uiev);
  }

  if (mpCtxBase)
    mpCtxBase->SlotRepaint();
}

///////////////////////////////////////////////////////////////////////////////
void QCtxWidget::focusInEvent(QFocusEvent* event) {
  auto& uiev            = UIEvent();
  auto gfxwin           = uiev.mpGfxWin;
  auto vp               = gfxwin ? gfxwin->GetRootWidget() : nullptr;
  uiev.miEventCode      = ork::ui::UIEV_GOT_KEYFOCUS;
  uiev.mpBlindEventData = (void*)event;
  if (Target()) {
    if (vp)
      vp->HandleUiEvent(UIEvent());
  }
  // orkprintf( "CTQT %08x got keyboard focus\n", this );
  QWidget::focusInEvent(event);
  if (GetGfxWindow())
    GetGfxWindow()->GotFocus();
  //////////////////////////////////////////
  if (mpCtxBase)
    mpCtxBase->SlotRepaint();
}

void QCtxWidget::focusOutEvent(QFocusEvent* event) {
  auto& uiev            = UIEvent();
  auto gfxwin           = uiev.mpGfxWin;
  auto vp               = gfxwin ? gfxwin->GetRootWidget() : nullptr;
  uiev.miEventCode      = ork::ui::UIEV_LOST_KEYFOCUS;
  uiev.mpBlindEventData = (void*)event;
  if (vp) {
    vp->HandleUiEvent(UIEvent());
  }
  // orkprintf( "CTQT %08x lost keyboard focus\n", this );
  QWidget::focusOutEvent(event);
  if (GetGfxWindow())
    GetGfxWindow()->LostFocus();
  //////////////////////////////////////////
  if (mpCtxBase)
    mpCtxBase->SlotRepaint();
}

ui::Event& QCtxWidget::UIEvent() { return mpCtxBase->mUIEvent; }
const ui::Event& QCtxWidget::UIEvent() const { return mpCtxBase->mUIEvent; }

GfxTarget* QCtxWidget::Target() const { return mpCtxBase->mpTarget; }

GfxWindow* QCtxWidget::GetGfxWindow() const { return mpCtxBase->mpGfxWindow; }

bool QCtxWidget::AlwaysRun() const { return mpCtxBase->mbAlwaysRun; }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

QTimer& CTQT::Timer() const { return mpQtWidget->mQtTimer; }

///////////////////////////////////////////////////////////////////////////////

void CTQT::Show() {
  mParent->show();
  if (mbInitialize) {
    printf("CreateCONTEXT\n");
    mpGfxWindow->CreateContext();
    mpGfxWindow->OnShow();
    mbInitialize = false;
  }
}

void CTQT::Hide() { mParent->hide(); }

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
      qt_millis = user_millis+1;
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

CTQT::CTQT(GfxWindow* pwin, QWidget* pparent)
    : CTXBASE(pwin)
    , mbAlwaysRun(false)
    , mix(0)
    , miy(0)
    , miw(0)
    , mih(0)
    , mParent(0)
    , mDrawLock(0) {
  this->SetThisXID(CTFLXID(0));
  this->SetTopXID(CTFLXID(0));

  SetParent(pparent);
}

CTQT::~CTQT() {
  if (mpGfxWindow)
    delete mpGfxWindow;
}

///////////////////////////////////////////////////////////////////////////////

void CTQT::SetParent(QWidget* pparent) {
  printf("CTQT::SetParent() pparent<%p>\n", pparent);
  QMainWindow* mainwin = 0;
  if (0 == pparent) {
    mainwin = new QMainWindow;
    pparent = mainwin;
  }

  mParent    = pparent;
  mpQtWidget = new QCtxWidget(this, pparent);
  if (mainwin) {
    mainwin->setCentralWidget(mpQtWidget);
  }

  this->SetThisXID((CTFLXID)winId());
  this->SetTopXID((CTFLXID)pparent->winId());

  mpGfxWindow->SetDirty(true);
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
  printf( "CTQT::Resize() mpTarget<%p>\n", mpTarget );
  if (mpTarget) {
    mpTarget->SetSize(X, Y, W, H);
    mUIEvent.mpGfxWin = (GfxWindow*)mpTarget->FBI()->GetThisBuffer();
    if (mUIEvent.mpGfxWin)
      mUIEvent.mpGfxWin->Resize(X, Y, W, H);
  }
  lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
}

///////////////////////////////////////////////////////////////////////////////

void CTQT::SlotRepaint() {
  auto lamb = [&]() {
    if (nullptr == GfxEnv::GetRef().GetDefaultUIMaterial())
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
      // printf( "CTQT::SlotRepaint() mpTarget<%p>\n", mpTarget );
      if (this->mpTarget) {
        mpTarget->makeCurrentContext();
        auto& uiev  = this->mUIEvent;
        auto gfxwin = uiev.mpGfxWin;
        auto vp     = gfxwin ? gfxwin->GetRootWidget() : nullptr;

        // this->UIEvent->mpGfxWin = (GfxWindow*) this->mpTarget->FBI()->GetThisBuffer();
        ui::DrawEvent drwev(this->mpTarget);

        if (vp)
          vp->Draw(drwev);
      }
    }
    this->mDrawLock--;
    ork::PerfMarkerPush("ork.viewport.draw.end");
  };

  if (OpqTest::GetContext()->mOPQ == &MainThreadOpQ()) {
    // already on main Q
    lamb();
  } else {
    MainThreadOpQ().push(Op(lamb));
  }
}

}} // namespace ork::lev2
