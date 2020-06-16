#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/ui/panel.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>

namespace ork { namespace ui {

static const int kpanelw = 12;

/////////////////////////////////////////////////////////////////////////

Panel::Panel(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h)
    , _child(nullptr)
    , mDockedAtTop(false) {
}

Panel::~Panel() {
}

/////////////////////////////////////////////////////////////////////////

void Panel::setChild(widget_ptr_t w) {
  _child = w;
  addChild(w);
}

/////////////////////////////////////////////////////////////////////////

void Panel::DoDraw(ui::drawevent_constptr_t drwev) {
  auto tgt    = drwev->GetTarget();
  auto fbi    = tgt->FBI();
  auto mtxi   = tgt->MTXI();
  auto& primi = lev2::GfxPrimitives::GetRef();
  auto defmtl = lev2::defaultUIMaterial();

  lev2::SRasterState defstate;
  tgt->RSI()->BindRasterState(defstate);

  bool has_foc = hasMouseFocus();

  auto ren_quad = [&](int x, int y, int x2, int y2) {
    primi.RenderQuadAtZ(
        defmtl.get(),
        tgt,
        x,
        x2, // x0, x1
        y,
        y2,   // y0, y1
        0.0f, // z
        0.0f,
        1.0f, // u0, u1
        0.0f,
        1.0f // v0, v1
    );
  };
  auto ren_line = [&](int x, int y, int x2, int y2) {
    auto vb = &lev2::GfxEnv::GetSharedDynamicVB();
    lev2::VtxWriter<lev2::SVtxV12C4T16> vw;
    vw.Lock(tgt, vb, 2);
    vw.AddVertex(lev2::SVtxV12C4T16(x, y, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vw.AddVertex(lev2::SVtxV12C4T16(x2, y2, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vw.UnLock(tgt);
    tgt->GBI()->DrawPrimitive(defmtl.get(), vw, lev2::EPrimitiveType::LINES);
  };

  mtxi->PushUIMatrix();
  {
    int ixr, iyr;
    LocalToRoot(0, 0, ixr, iyr);

    /////////////
    // panel outline (resize/moving)
    /////////////

    fvec4 clr = fcolor4(1.0f, 0.0f, 1.0f, 0.4f);
    if (has_foc)
      clr = fcolor4::White();

    defmtl->_rasterstate.SetBlending(lev2::EBLENDING_ALPHA);
    tgt->PushModColor(clr);
    ren_quad(ixr, iyr, ixr + _geometry._w, iyr + _geometry._h);
    tgt->PopModColor();
    defmtl->_rasterstate.SetBlending(lev2::EBLENDING_OFF);

    /////////////
    // close button
    /////////////

    LocalToRoot(mCloseX, mCloseY, ixr, iyr);
    tgt->PushModColor(fcolor4(0.3f, 0.0f, 0.0f));
    ren_quad(ixr + 1, iyr + 1, ixr + kpanelw - 1, iyr + kpanelw - 1);
    tgt->PopModColor();
    tgt->PushModColor(fcolor4(1.0f, 0.3f, 0.3f));
    ren_quad(ixr + 2, iyr + 2, ixr + kpanelw - 2, iyr + kpanelw - 2);
    tgt->PopModColor();
    tgt->PushModColor(fcolor4(0.3f, 0.0f, 0.0f));
    ren_line(ixr + 1, iyr + 1, ixr + kpanelw - 1, iyr + kpanelw - 1);
    ren_line(ixr + kpanelw - 1, iyr + 1, ixr + 1, iyr + kpanelw - 1);
    tgt->PopModColor();

    if (_title.length()) {
      tgt->PushModColor(fcolor4::Yellow());
      auto font = lev2::FontMan::GetFont("i13");

      lev2::FontMan::PushFont(font);
      lev2::FontMan::beginTextBlock(tgt);
      lev2::FontMan::DrawText(tgt, ixr + kpanelw + 2, iyr + 2, _title.c_str());
      lev2::FontMan::endTextBlock(tgt);
      lev2::FontMan::PopFont();
      tgt->PopModColor();
    }
  }
  mtxi->PopUIMatrix();

  if (_child)
    _child->Draw(drwev);
}

/////////////////////////////////////////////////////////////////////////

void Panel::DoLayout() {
  mDockedAtTop = (_geometry._y == -kpanelw);
  // printf("mDockedAtTop<%d>\n", int(mDockedAtTop));

  mCloseX = kpanelw;
  mCloseY = mDockedAtTop ? _geometry._h - kpanelw : 0;

  int cw = _geometry._w - (kpanelw * 2);
  int ch = _geometry._h - (kpanelw * 2);

  // printf( "Panel<%s>::DoLayout x<%d> y<%d> w<%d> h<%d>\n", msName.c_str(), _geometry._x, _geometry._y, _geometry._w, _geometry._h
  // );
  if (_child) {
    _child->SetRect(kpanelw, kpanelw, cw, ch);
  }
}

/////////////////////////////////////////////////////////////////////////

Widget* Panel::doRouteUiEvent(event_constptr_t ev) {
  Widget* target = nullptr;
  if (_child && _child->IsEventInside(ev) && mPanelUiState == 0) {
    target = _child->routeUiEvent(ev);
  } else if (IsEventInside(ev)) {
    target = this;
  }
  return target;
}

/////////////////////////////////////////////////////////////////////////

void Panel::unsnap() {
  if (nullptr == mParent)
    return;
  mParent->_snapped.erase(this);
}

/////////////////////////////////////////////////////////////////////////

void Panel::snap() {
  if (nullptr == mParent)
    return;

  int pw = mParent->width();
  int xd = abs(x2() - pw);
  int ph = mParent->height();
  int yd = abs(y2() - ph);
  // printf( "x2<%d> pw<%d> xd<%d>\n", x2, pw, xd );
  // printf( "y2<%d> ph<%d> yd<%d>\n", y2, ph, yd );
  bool snapl = (_geometry._x < kpanelw);
  bool snapr = (xd < kpanelw);
  bool snapt = (_geometry._y < kpanelw);
  bool snapb = (yd < kpanelw);

  bool was_snapped = false;

  if (snapt && snapb) {
    SetY(-kpanelw);
    SetH(ph + 2 * kpanelw);
    was_snapped = true;
  } else if (snapt) {
    SetY(-kpanelw);
    was_snapped = true;
  } else if (snapb) {
    SetY(ph + kpanelw - height());
    was_snapped = true;
  }
  if (snapl && snapr) {
    SetX(-kpanelw);
    SetW(pw + 2 * kpanelw);
    was_snapped = true;
  }
  if (snapl) {
    SetX(-kpanelw);
    was_snapped = true;
  } else if (snapr) {
    was_snapped = true;
    SetX(pw + kpanelw - width());
  }
  if (was_snapped) {
    mParent->_snapped.insert(this);
  }
}

HandlerResult Panel::DoOnUiEvent(event_constptr_t Ev) {
  HandlerResult ret(this);

  int evx = Ev->miX;
  int evy = Ev->miY;
  // printf("Panel<%p>::OnUiEvent isshift<%d>\n", this, int(isshift));
  //////////////////////////////
  int ilocx = 0;
  int ilocy = 0;
  RootToLocal(evx, evy, ilocx, ilocy);
  //////////////////////////////
  const auto& filtev = Ev->mFilteredEvent;
  switch (filtev._eventcode) {
    case ui::EventCode::PUSH: // idle
      _downx         = evx;
      _downy         = evy;
      _prevpx        = _geometry._x;
      _prevpy        = _geometry._y;
      _prevpw        = _geometry._w;
      _prevph        = _geometry._h;
      ret.mHoldFocus = true;
      if (filtev.mBut0) {
        printf("ilocx<%d> mCloseX<%d>\n", ilocx, mCloseX);
        if ((ilocx >= mCloseX) && ((ilocx - mCloseX) < kpanelw) && (ilocy >= mCloseY) && ((ilocy - mCloseY) < kpanelw)) {
          auto lamb = [=]() {
            if (mParent) {
              mParent->removeChild(this);
            }
          };
          opq::Op(lamb).QueueASync(opq::mainSerialQueue());

        } else
          mPanelUiState = 1;
      } else if (filtev.mBut1 || filtev.mBut2) {
        if (abs(ilocy) < kpanelw) // top
          mPanelUiState = 2;
        else if (abs(ilocy - _geometry._h) < kpanelw) // bot
          mPanelUiState = 3;
        else if (abs(ilocx) < kpanelw) // lft
          mPanelUiState = 4;
        else if (abs(ilocx - _geometry._w) < kpanelw) // rht
          mPanelUiState = 5;
      }
      break;
    case ui::EventCode::RELEASE: // idle
      ret.mHoldFocus = false;

      if (mPanelUiState) // moving or sizing w
        snap();

      mPanelUiState = 0;

      break;
    case ui::EventCode::DRAG:
      ret.mHoldFocus = true;
      break;
    default:
      break;
  }

  int dx = evx - _downx;
  int dy = evy - _downy;

  switch (mPanelUiState) {
    case 0:
      break;
    case 1: // move
      SetPos(_prevpx + dx, _prevpy + dy);
      break;
    case 2: // resize h
      SetRect(_prevpx, _prevpy + dy, _prevpw, _prevph - dy);
      break;
    case 3: // resize w
      SetSize(_prevpw, _prevph + dy);
      break;
    case 4:
      SetRect(_prevpx + dx, _prevpy, _prevpw - dx, _prevph);
      break;
    case 5:
      SetSize(_prevpw + dx, _prevph);
      break;
  }

  return ret;
}
//

void Panel::DoOnEnter() {
}

void Panel::DoOnExit() {
}

}} // namespace ork::ui
