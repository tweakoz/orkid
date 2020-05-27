#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ui::Widget, "ui::Widget");

namespace ork { namespace ui {

static Widget* gMouseFocus = nullptr;
static Widget* gFastPath   = nullptr;

/////////////////////////////////////////////////////////////////////////

HandlerResult::HandlerResult(Widget* ph)
    : mHandler(ph)
    , mHoldFocus(false) {
}

/////////////////////////////////////////////////////////////////////////

void Widget::Describe() {
}

/////////////////////////////////////////////////////////////////////////

Widget::Widget(const std::string& name, int x, int y, int w, int h)
    : msName(name)
    , mpTarget(0)
    , miX(x)
    , miY(y)
    , miLX(x)
    , miLY(y)
    , miW(w)
    , miH(h)
    , miLW(w)
    , miLH(h)
    , miX2(x + w)
    , miY2(y + h)
    , mbInit(true)
    , mbKeyboardFocus(false)
    , mParent(nullptr)
    , mDirty(true)
    , mSizeDirty(true)
    , mPosDirty(true) {
  pushEventFilter<ui::NopEventFilter>();
}
Widget::~Widget() {
  if (gFastPath == this)
    gFastPath = nullptr;
}

void Widget::Init(lev2::Context* pT) {
  DoInit(pT);
}

HandlerResult Widget::HandleUiEvent(event_constptr_t Ev) {
  HandlerResult ret;

  if (gFastPath) {
    // printf( "Widget::HandleUiEvent::FASTPATH ev<%d,%d> widget<%p:%s>\n", Ev->miX, Ev->miY, gFastPath, gFastPath->msName.c_str()
    // );
    ret = gFastPath->RouteUiEvent(Ev);
  } else {
    bool binside = IsEventInside(Ev);
    if (0)
      printf(
          "Widget::HandleUiEvent::SLOWPATH ev<%d,%d> widget<%p:%s> dim<%d %d %d %d> inside<%d>\n",
          Ev->miX,
          Ev->miY,
          this,
          msName.c_str(),
          miX,
          miY,
          miW,
          miH,
          int(binside));

    if (binside) {
      ret = RouteUiEvent(Ev);
    }
  }
  return ret;
}

bool Widget::HasMouseFocus() const {
  return (this == gMouseFocus);
}

HandlerResult Widget::OnUiEvent(event_constptr_t Ev) {
  Ev->mFilteredEvent.Reset();

  if (_eventfilterstack.size()) {
    auto top = _eventfilterstack.top();
    top->Filter(Ev);
    if (Ev->mFilteredEvent.miEventCode == 0)
      return HandlerResult();
  }
  return DoOnUiEvent(Ev);
}
HandlerResult Widget::RouteUiEvent(event_constptr_t Ev) {
  auto ret = DoRouteUiEvent(Ev);
  UpdateMouseFocus(ret, Ev);
  return ret;
}

HandlerResult Widget::DoRouteUiEvent(event_constptr_t Ev) {
  auto ret = OnUiEvent(Ev);
  // printf( "Widget::RouteUiEvent<%p:%s>\n", this, msName.c_str() );
  return ret;
}

void EventCooked::Reset() {
  miEventCode = 0;
  miKeyCode   = 0;

  miX        = 0;
  miY        = 0;
  mLastX     = 0;
  mLastY     = 0;
  mUnitX     = 0.0f;
  mUnitY     = 0.0f;
  mLastUnitX = 0.0f;
  mLastUnitY = 0.0f;

  mBut0   = false;
  mBut1   = false;
  mBut2   = false;
  mCTRL   = false;
  mALT    = false;
  mSHIFT  = false;
  mMETA   = false;
  mAction = "";
}

IWidgetEventFilter::IWidgetEventFilter(Widget& w)
    : mWidget(w)
    , mShiftDown(false)
    , mCtrlDown(false)
    , mMetaDown(false)
    , mAltDown(false)
    , mLeftDown(false)
    , mMiddleDown(false)
    , mRightDown(false)
    , mCapsDown(false)
    , mLastKeyCode(0)
    , mBut0Down(false)
    , mBut1Down(false)
    , mBut2Down(false) {
  mKeyTimer.Start();
  mDoubleTimer.Start();
  mMoveTimer.Start();
}

void IWidgetEventFilter::Filter(event_constptr_t Ev) {
  auto& fev       = Ev->mFilteredEvent;
  fev.miEventCode = Ev->miEventCode;
  fev.mBut0       = Ev->mbLeftButton;
  fev.mBut1       = Ev->mbMiddleButton;
  fev.mBut2       = Ev->mbRightButton;

  fev.miX        = Ev->miX;
  fev.miY        = Ev->miY;
  fev.mLastX     = Ev->miLastX;
  fev.mLastY     = Ev->miLastY;
  fev.mUnitX     = Ev->mfUnitX;
  fev.mUnitY     = Ev->mfUnitX;
  fev.mLastUnitX = Ev->mfLastUnitX;
  fev.mLastUnitY = Ev->mfLastUnitY;

  fev.miKeyCode = Ev->miKeyCode;
  fev.mCTRL     = Ev->mbCTRL;
  fev.mALT      = Ev->mbALT;
  fev.mSHIFT    = Ev->mbSHIFT;
  fev.mMETA     = Ev->mbMETA;

  DoFilter(Ev);
}
void NopEventFilter::DoFilter(event_constptr_t Ev) {
}

void Apple3ButtonMouseEmulationFilter::DoFilter(event_constptr_t Ev) {
  auto& fev = Ev->mFilteredEvent;

  fev.mAction = "none";

  switch (Ev->miEventCode) {
    case ui::UIEV_KEY: {
      float kt = mKeyTimer.SecsSinceStart();
      float dt = mDoubleTimer.SecsSinceStart();
      float mt = mMoveTimer.SecsSinceStart();

      bool bdouble = (kt < 0.8f) && (dt > 1.0f) && (mt > 0.5f) && (mLastKeyCode == Ev->miKeyCode);

      // printf("keydown<%d> lk<%d> kt<%f> dt<%f> mt<%f>\n", mLastKeyCode, Ev->miKeyCode, kt, dt, mt);

      auto evc = bdouble ? ui::UIEV_DOUBLECLICK : ui::UIEV_PUSH;

      mKeyTimer.Start();
      switch (Ev->miKeyCode) {
        case 'z': // synthetic left button
          fev.miEventCode = evc;
          if (fev.miEventCode == ui::UIEV_DOUBLECLICK) {
            printf("SYNTH DOUBLECLICK\n");
          } else {
            bdouble = false;
          }
          fev.mBut0   = true;
          mBut0Down   = true;
          fev.mAction = "keypush";
          break;
        case 'x': // synthetic middle button
          fev.miEventCode = mBut1Down ? 0 : evc;
          if (fev.miEventCode == ui::UIEV_DOUBLECLICK) {
          } // printf( "SYNTH DOUBLECLICK\n" );
          else {
            bdouble = false;
          }
          fev.mBut1   = true;
          mBut1Down   = true;
          fev.mAction = "keypush";
          break;
        case 'c': // synthetic right button
          fev.miEventCode = mBut2Down ? 0 : evc;
          if (fev.miEventCode == ui::UIEV_DOUBLECLICK) {
          } // printf( "SYNTH DOUBLECLICK\n" );
          else {
            bdouble = false;
          }
          fev.mBut2   = true;
          mBut2Down   = true;
          fev.mAction = "keypush";
          break;
        case Widget::keycode_shift:
          mShiftDown = true;
          break;
        case Widget::keycode_ctrl:
          mCtrlDown = true;
          break;
        case Widget::keycode_cmd:
          mMetaDown = true;
          break;
        case Widget::keycode_alt:
          mAltDown = true;
          break;
        default:
          break;
      }
      mLastKeyCode = Ev->miKeyCode;
      if (bdouble) {
        mDoubleTimer.Start();
      }
      break;
    }
    case ui::UIEV_KEYUP:
      // printf( "keyup<%d>\n", Ev->miKeyCode );
      switch (Ev->miKeyCode) {
        case 49:
        case 122: // z
          fev.miEventCode = ui::UIEV_RELEASE;
          fev.mBut0       = false;
          mBut0Down       = false;
          break;
        case 120: // x
        case 50:
          fev.miEventCode = ui::UIEV_RELEASE;
          fev.mBut1       = false;
          mBut1Down       = false;
          break;
        case 99: // c
        case 51:
          fev.miEventCode = ui::UIEV_RELEASE;
          fev.mBut2       = false;
          mBut2Down       = false;
          break;
        case Widget::keycode_shift:
          mShiftDown = false;
          break;
        case Widget::keycode_ctrl:
          mCtrlDown = false;
          break;
        case Widget::keycode_cmd:
          mMetaDown = false;
          break;
        case Widget::keycode_alt:
          mAltDown = false;
          break;
        default:
          break;
      }
      break;
    case ui::UIEV_MOVE:
      if (mBut0Down || mBut1Down || mBut2Down) {
        fev.miEventCode = ui::UIEV_DRAG;
        // printf( "SYNTH DRAG\n" );
        fev.mBut0 = mBut0Down;
        fev.mBut1 = mBut1Down;
        fev.mBut2 = mBut2Down;
      }
      mMoveTimer.Start();
      break;
    case ui::UIEV_PUSH:
      fev.mBut0   = Ev->mbLeftButton;
      fev.mBut1   = Ev->mbMiddleButton;
      fev.mBut2   = Ev->mbRightButton;
      mBut0Down   = Ev->mbLeftButton;
      mBut1Down   = Ev->mbMiddleButton;
      mBut2Down   = Ev->mbRightButton;
      mLeftDown   = Ev->mbLeftButton;
      mMiddleDown = Ev->mbMiddleButton;
      mRightDown  = Ev->mbRightButton;
      mMoveTimer.Start();
      break;
    case ui::UIEV_RELEASE:
      fev.mBut0   = Ev->mbLeftButton;
      fev.mBut1   = Ev->mbMiddleButton;
      fev.mBut2   = Ev->mbRightButton;
      mBut0Down   = Ev->mbLeftButton;
      mBut1Down   = Ev->mbMiddleButton;
      mBut2Down   = Ev->mbRightButton;
      mLeftDown   = Ev->mbLeftButton;
      mMiddleDown = Ev->mbMiddleButton;
      mRightDown  = Ev->mbRightButton;
      break;
    case ui::UIEV_DRAG:
      break;
    default:
      break;
  }
}

void Widget::UpdateMouseFocus(const HandlerResult& r, event_constptr_t Ev) {
  Widget* ponenter = nullptr;
  Widget* ponexit  = nullptr;

  const auto& filtev = Ev->mFilteredEvent;

  if (r.mHandler != gMouseFocus) {
  }

  Widget* plfp = gFastPath;

  switch (filtev.miEventCode) {
    case ui::UIEV_PUSH:
      gMouseFocus = r.mHandler;
      gFastPath   = r.mHandler;
      break;
    case ui::UIEV_RELEASE:
      if (gFastPath)
        ponexit = gFastPath;
      gFastPath   = nullptr;
      gMouseFocus = nullptr;
      break;
    case ui::UIEV_MOVE:
    case ui::UIEV_DRAG:
    default:
      break;
  }
  if (plfp != gFastPath) {
    // if( plfp )
    //	printf( "widget<%p:%s> has lost the fastpath\n", plfp, plfp->msName.c_str() );

    // if( gFastPath )
    //	printf( "widget<%p:%s> now has the fastpath\n", gFastPath, gFastPath->msName.c_str() );
  }

  if (ponexit)
    ponexit->DoOnExit();

  if (ponenter)
    ponenter->DoOnEnter();
}

HandlerResult Widget::DoOnUiEvent(event_constptr_t Ev) {
  return HandlerResult();
}

bool Widget::IsEventInside(event_constptr_t Ev) const {
  int rx = Ev->miX;
  int ry = Ev->miY;
  int ix = 0;
  int iy = 0;
  RootToLocal(rx, ry, ix, iy);

  bool inside = (ix >= 0) && (iy >= 0) && (ix <= miW) && (iy <= miH);
  return inside;
}

/////////////////////////////////////////////////////////////////////////

void Widget::LocalToRoot(int lx, int ly, int& rx, int& ry) const {
  rx = lx;
  ry = ly;

  const Widget* w = this;
  while (w) //->GetParent() )
  {
    rx += w->miX;
    ry += w->miY;

    w = w->GetParent();
  }
  bool ishidpi = mpTarget ? mpTarget->hiDPI() : false;
}

void Widget::RootToLocal(int rx, int ry, int& lx, int& ly) const {
  lx = rx;
  ly = ry;

  const Widget* w = this;
  while (w) //->GetParent() )
  {
    lx -= w->miX;
    ly -= w->miY;

    w = w->GetParent();
  }
}

/////////////////////////////////////////////////////////////////////////

void Widget::SetDirty() {
  mDirty = true;
  if (mParent)
    mParent->mDirty = true;
}

/////////////////////////////////////////////////////////////////////////

void Widget::Draw(ui::drawevent_constptr_t drwev) {
  _drawEvent = drwev;
  mpTarget   = drwev->GetTarget();

  if (mbInit) {
    ork::lev2::FontMan::GetRef();
    Init(mpTarget);
    mbInit = false;
  }

  if (mSizeDirty) {
    OnResize();
    mPosDirty  = false;
    mSizeDirty = false;
    miLX       = miX;
    miLY       = miY;
    miLW       = miW;
    miLH       = miH;
  }

  DoDraw(drwev);
  mpTarget   = 0;
  _drawEvent = nullptr;
}

/////////////////////////////////////////////////////////////////////////

float Widget::logicalWidth() const {
  bool ishidpi = mpTarget ? mpTarget->hiDPI() : false;
  return ishidpi ? miW * 2 : miW;
}
float Widget::logicalHeight() const {
  bool ishidpi = mpTarget ? mpTarget->hiDPI() : false;
  return ishidpi ? miH * 2 : miH;
}
float Widget::logicalX() const {
  bool ishidpi = mpTarget ? mpTarget->hiDPI() : false;
  return ishidpi ? miX * 2 : miX;
}
float Widget::logicalY() const {
  bool ishidpi = mpTarget ? mpTarget->hiDPI() : false;
  return ishidpi ? miY * 2 : miY;
}

/////////////////////////////////////////////////////////////////////////

void Widget::ExtDraw(lev2::Context* pTARG) {
  if (mbInit) {
    ork::lev2::FontMan::GetRef();
    Init(mpTarget);
    mbInit = false;
  }
  mpTarget = pTARG;
  auto ev  = std::make_shared<ui::DrawEvent>(pTARG);
  DoDraw(ev);
  mpTarget = 0;
}

/////////////////////////////////////////////////////////////////////////

void Widget::SetPos(int iX, int iY) {
  mPosDirty = (miLX != iX) || (miLY != iY);
  miLX      = miX;
  miLY      = miY;
  miX       = iX;
  miY       = iY;
  miX2      = miX + miW;
  miY2      = miY + miH;

  if (mPosDirty)
    ReLayout();
}

/////////////////////////////////////////////////////////////////////////

void Widget::SetSize(int iW, int iH) {
  mSizeDirty = (miLW != iW) || (miLH != iH);
  miLW       = miW;
  miLH       = miH;
  miW        = iW;
  miH        = iH;
  miX2       = miX + miW;
  miY2       = miY + miH;

  if (mSizeDirty)
    ReLayout();
}

/////////////////////////////////////////////////////////////////////////

void Widget::SetRect(int iX, int iY, int iW, int iH) {
  mPosDirty |= (miLX != iX) || (miLY != iY);
  miLX = miX;
  miLY = miY;
  miX  = iX;
  miY  = iY;

  mSizeDirty |= (miLW != iW) || (miLH != iH);
  miLW = miW;
  miLH = miH;
  miW  = iW;
  miH  = iH;

  miX2 = iX + iW;
  miY2 = iY + iH;

  if (mPosDirty || mSizeDirty)
    ReLayout();
}

/////////////////////////////////////////////////////////////////////////

void Widget::ReLayout() {
  DoLayout();
}

/////////////////////////////////////////////////////////////////////////

void Widget::OnResize(void) {
  // printf("Widget<%s>::OnResize x<%d> y<%d> w<%d> h<%d>\n", msName.c_str(), miX, miY, miW, miH);
}

/////////////////////////////////////////////////////////////////////////

bool Widget::IsKeyDepressed(int ch) {
  if (false == HasKeyboardFocus()) {
    return false;
  }

  return OldSchool::GetRef().IsKeyDepressed(ch);
}

/////////////////////////////////////////////////////////////////////////

bool Widget::IsHotKeyDepressed(const char* pact) {
  if (false == HasKeyboardFocus()) {
    return false;
  }

  return HotKeyManager::IsDepressed(pact);
}

/////////////////////////////////////////////////////////////////////////

bool Widget::IsHotKeyDepressed(const HotKey& hk) {
  if (false == HasKeyboardFocus()) {
    return false;
  }

  return HotKeyManager::IsDepressed(hk);
}

/////////////////////////////////////////////////////////////////////////

}} // namespace ork::ui
