#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/context.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
///////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ui::Widget, "ui::Widget");
///////////////////////////////////////////////////////////
namespace ork::ui {
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
    : _name(name)
    , _target(0)
    , _needsinit(true)
    , _parent(nullptr)
    , _dirty(true)
    , mSizeDirty(true)
    , mPosDirty(true) {

  _geometry._x  = x;
  _geometry._y  = y;
  _geometry._w  = w;
  _geometry._h  = h;
  _prevGeometry = _geometry;

  pushEventFilter<ui::NopEventFilter>();
}
///////////////////////////////////////////////////////////
Widget::~Widget() {
}
///////////////////////////////////////////////////////////
void Widget::Init(lev2::Context* pT) {
  DoInit(pT);
}
///////////////////////////////////////////////////////////
void Widget::setGeometry(Rect newgeo) {
  _prevGeometry = _geometry;
  _geometry     = newgeo;
  mSizeDirty    = true;
  ReLayout();
}
///////////////////////////////////////////////////////////
HandlerResult Widget::handleUiEvent(event_constptr_t ev) {
  Widget* target = doRouteUiEvent(ev);
  if (0)
    printf("handleuiev target<%s>\n", target ? target->_name.c_str() : "none");
  return target ? target->OnUiEvent(ev) : HandlerResult();
}
///////////////////////////////////////////////////////////
Widget* Widget::routeUiEvent(event_constptr_t ev) {
  auto ret = doRouteUiEvent(ev);
  return ret;
}
///////////////////////////////////////////////////////////
Widget* Widget::doRouteUiEvent(event_constptr_t ev) {
  bool inside    = IsEventInside(ev);
  Widget* target = inside ? this : nullptr;
  if (0)
    printf("Widget::doRouteUiEvent w<%s> inside<%d> target<%p>\n", _name.c_str(), int(inside), target);
  return target;
}
///////////////////////////////////////////////////////////
bool Widget::hasMouseFocus() const {
  return _uicontext ? _uicontext->hasMouseFocus(this) : false;
}
///////////////////////////////////////////////////////////
HandlerResult Widget::OnUiEvent(event_constptr_t ev) {
  ev->mFilteredEvent.Reset();
  // printf("Widget<%p>::OnUiEvent\n", this);

  if (_eventfilterstack.size()) {
    auto top = _eventfilterstack.top();
    top->Filter(ev);
    if (ev->mFilteredEvent._eventcode == EventCode::UNKNOWN)
      return HandlerResult();
  }
  return DoOnUiEvent(ev);
}
///////////////////////////////////////////////////////////
void NopEventFilter::DoFilter(event_constptr_t ev) {
}
///////////////////////////////////////////////////////////
void Apple3ButtonMouseEmulationFilter::DoFilter(event_constptr_t Ev) {
  auto& fev = Ev->mFilteredEvent;

  fev.mAction = "none";

  switch (Ev->_eventcode) {
    case ui::EventCode::KEY: {
      float kt = mKeyTimer.SecsSinceStart();
      float dt = mDoubleTimer.SecsSinceStart();
      float mt = mMoveTimer.SecsSinceStart();

      bool bdouble = (kt < 0.8f) && (dt > 1.0f) && (mt > 0.5f) && (mLastKeyCode == Ev->miKeyCode);

      // printf("keydown<%d> lk<%d> kt<%f> dt<%f> mt<%f>\n", mLastKeyCode, Ev->miKeyCode, kt, dt, mt);

      auto evc = bdouble ? ui::EventCode::DOUBLECLICK : ui::EventCode::PUSH;

      mKeyTimer.Start();
      switch (Ev->miKeyCode) {
        case 'z': // synthetic left button
          fev._eventcode = evc;
          if (fev._eventcode == ui::EventCode::DOUBLECLICK) {
            printf("SYNTH DOUBLECLICK\n");
          } else {
            bdouble = false;
          }
          fev.mBut0   = true;
          mBut0Down   = true;
          fev.mAction = "keypush";
          break;
        case 'x': // synthetic middle button
          fev._eventcode = mBut1Down ? EventCode::UNKNOWN : evc;
          if (fev._eventcode == ui::EventCode::DOUBLECLICK) {
          } // printf( "SYNTH DOUBLECLICK\n" );
          else {
            bdouble = false;
          }
          fev.mBut1   = true;
          mBut1Down   = true;
          fev.mAction = "keypush";
          break;
        case 'c': // synthetic right button
          fev._eventcode = mBut2Down ? EventCode::UNKNOWN : evc;
          if (fev._eventcode == ui::EventCode::DOUBLECLICK) {
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
    case ui::EventCode::KEYUP:
      // printf( "keyup<%d>\n", Ev->miKeyCode );
      switch (Ev->miKeyCode) {
        case 49:
        case 122: // z
          fev._eventcode = ui::EventCode::RELEASE;
          fev.mBut0      = false;
          mBut0Down      = false;
          break;
        case 120: // x
        case 50:
          fev._eventcode = ui::EventCode::RELEASE;
          fev.mBut1      = false;
          mBut1Down      = false;
          break;
        case 99: // c
        case 51:
          fev._eventcode = ui::EventCode::RELEASE;
          fev.mBut2      = false;
          mBut2Down      = false;
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
    case ui::EventCode::MOVE:
      if (mBut0Down or mBut1Down or mBut2Down) {
        fev._eventcode = ui::EventCode::DRAG;
        // printf( "SYNTH DRAG\n" );
        fev.mBut0 = mBut0Down;
        fev.mBut1 = mBut1Down;
        fev.mBut2 = mBut2Down;
      }
      mMoveTimer.Start();
      break;
    case ui::EventCode::PUSH:
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
    case ui::EventCode::RELEASE:
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
    case ui::EventCode::DRAG:
      break;
    default:
      break;
  }
}
///////////////////////////////////////////////////////////
HandlerResult Widget::DoOnUiEvent(event_constptr_t Ev) {
  return HandlerResult();
}
///////////////////////////////////////////////////////////
bool Widget::IsEventInside(event_constptr_t Ev) const {
  int rx = Ev->miX;
  int ry = Ev->miY;
  int ix = 0;
  int iy = 0;
  RootToLocal(rx, ry, ix, iy);
  auto ngeo   = _geometry;
  ngeo._x     = 0;
  ngeo._y     = 0;
  bool inside = ngeo.isPointInside(ix, iy);
  return inside;
}
/////////////////////////////////////////////////////////////////////////
void Widget::LocalToRoot(int lx, int ly, int& rx, int& ry) const {
  bool ishidpi    = _target ? _target->hiDPI() : false;
  rx              = lx;
  ry              = ly;
  const Widget* w = this;
  while (w) {
    rx += w->x();
    ry += w->y();
    w = w->parent();
  }
}
///////////////////////////////////////////////////////////
void Widget::RootToLocal(int rx, int ry, int& lx, int& ly) const {
  bool ishidpi    = _target ? _target->hiDPI() : false;
  lx              = rx;
  ly              = ry;
  const Widget* w = this;
  while (w) {
    lx -= w->x();
    ly -= w->y();
    w = w->parent();
  }
}
/////////////////////////////////////////////////////////////////////////
void Widget::SetDirty() {
  _dirty = true;
  if (_parent)
    _parent->_dirty = true;
}
/////////////////////////////////////////////////////////////////////////
void Widget::Draw(ui::drawevent_constptr_t drwev) {
  _drawEvent = drwev;
  _target    = drwev->GetTarget();

  if (_needsinit) {
    ork::lev2::FontMan::GetRef();
    Init(_target);
    _needsinit = false;
  }

  if (mSizeDirty) {
    OnResize();
    mPosDirty     = false;
    mSizeDirty    = false;
    _prevGeometry = _geometry;
  }

  DoDraw(drwev);
  _target    = 0;
  _drawEvent = nullptr;
}
/////////////////////////////////////////////////////////////////////////
float Widget::logicalWidth() const {
  bool ishidpi = _target ? _target->hiDPI() : false;
  return ishidpi ? width() * 2 : width();
}
///////////////////////////////////////////////////////////
float Widget::logicalHeight() const {
  bool ishidpi = _target ? _target->hiDPI() : false;
  return ishidpi ? height() * 2 : height();
}
///////////////////////////////////////////////////////////
float Widget::logicalX() const {
  bool ishidpi = _target ? _target->hiDPI() : false;
  return ishidpi ? x() * 2 : x();
}
///////////////////////////////////////////////////////////
float Widget::logicalY() const {
  bool ishidpi = _target ? _target->hiDPI() : false;
  return ishidpi ? y() * 2 : y();
}
/////////////////////////////////////////////////////////////////////////
void Widget::ExtDraw(lev2::Context* pTARG) {
  if (_needsinit) {
    ork::lev2::FontMan::GetRef();
    Init(_target);
    _needsinit = false;
  }
  _target = pTARG;
  auto ev = std::make_shared<ui::DrawEvent>(pTARG);
  DoDraw(ev);
  _target = 0;
}
/////////////////////////////////////////////////////////////////////////
void Widget::SetPos(int iX, int iY) {
  mPosDirty |= (x() != iX) or (y() != iY);
  _prevGeometry = _geometry;
  _geometry._x  = iX;
  _geometry._y  = iY;

  if (mPosDirty)
    ReLayout();
}
/////////////////////////////////////////////////////////////////////////
void Widget::SetSize(int iW, int iH) {
  mSizeDirty |= (width() != iW) or (height() != iH);
  _prevGeometry = _geometry;
  _geometry._w  = iW;
  _geometry._h  = iH;
  if (mSizeDirty)
    ReLayout();
}
/////////////////////////////////////////////////////////////////////////
void Widget::SetRect(int iX, int iY, int iW, int iH) {
  mPosDirty |= (x() != iX) or (y() != iY);
  mSizeDirty |= (width() != iW) or (height() != iH);
  _prevGeometry = _geometry;
  _geometry._x  = iX;
  _geometry._y  = iY;
  _geometry._w  = iW;
  _geometry._h  = iH;

  if (mPosDirty or mSizeDirty)
    ReLayout();
}
/////////////////////////////////////////////////////////////////////////
void Widget::ReLayout() {
  DoLayout();
}
/////////////////////////////////////////////////////////////////////////
void Widget::OnResize(void) {
  // printf("Widget<%s>::OnResize x<%d> y<%d> w<%d> h<%d>\n", _name.c_str(), miX, miY, miW, miH);
}
/////////////////////////////////////////////////////////////////////////
bool Widget::IsKeyDepressed(int ch) {
  if (_uicontext and false == _uicontext->hasKeyboardFocus()) {
    return false;
  }
  return OldSchool::GetRef().IsKeyDepressed(ch);
}
/////////////////////////////////////////////////////////////////////////
bool Widget::IsHotKeyDepressed(const char* pact) {
  if (_uicontext and false == _uicontext->hasKeyboardFocus()) {
    return false;
  }
  return HotKeyManager::IsDepressed(pact);
}
/////////////////////////////////////////////////////////////////////////
bool Widget::IsHotKeyDepressed(const HotKey& hk) {
  if (_uicontext and false == _uicontext->hasKeyboardFocus()) {
    return false;
  }
  return HotKeyManager::IsDepressed(hk);
}
/////////////////////////////////////////////////////////////////////////
Group* Widget::root() const {
  Group* node = _parent;
  while (node) {
    if (node->_parent == nullptr)
      return node;
    else
      node = node->_parent;
  }
  return nullptr;
}
/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
