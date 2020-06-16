#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/ui/split_panel.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/kernel/opq.h>

namespace ork { namespace ui {

static const int kpanelw = 12;
static const int ksplith = 7;

/////////////////////////////////////////////////////////////////////////

SplitPanel::SplitPanel(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h)
    , _child1(nullptr)
    , _child2(nullptr)
    , mSplitVal(0.5f)
    , mDockedAtTop(false)
    , mEnableCloseButton(false)
    , mPanelUiState(0) {
}

SplitPanel::~SplitPanel() {
  if (_child1) {
    _child1->SetParent(nullptr);
    _child1 = nullptr;
  }
  if (_child2) {
    _child2->SetParent(nullptr);
    _child2 = nullptr;
  }
  if (mParent)
    mParent->removeChild(this);
}

/////////////////////////////////////////////////////////////////////////

void SplitPanel::setChild1(widget_ptr_t w) {
  _child1 = w;
  addChild(w);
}
void SplitPanel::setChild2(widget_ptr_t w) {
  _child2 = w;
  addChild(w);
}

/////////////////////////////////////////////////////////////////////////

void SplitPanel::DoDraw(ui::drawevent_constptr_t drwev) {
  auto tgt     = drwev->GetTarget();
  bool is_hdpi = tgt->hiDPI();
  auto fbi     = tgt->FBI();
  auto mtxi    = tgt->MTXI();
  auto& primi  = lev2::GfxPrimitives::GetRef();
  auto defmtl  = lev2::defaultUIMaterial();

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

  lev2::SRasterState defstate;
  tgt->RSI()->BindRasterState(defstate);

  bool has_foc = hasMouseFocus();
  tgt->PushModColor(has_foc ? fcolor4::White() : fcolor4::Red());
  mtxi->PushUIMatrix();
  {
    int ixr, iyr;
    LocalToRoot(0, 0, ixr, iyr);

    /////////////
    // panel outline (resize/moving)
    /////////////

    fvec4 clr = fcolor4(1.0f, 0.0f, 1.0f, 0.4f);

    switch (mPanelUiState) {
      case 0:
        break;
      case 1: // move
        break;
      case 2: // resize h
        break;
      case 3: // resize w
        break;
      case 4: // ?
        break;
      case 5: // ?
        break;
      case 6: // set splitter
        clr = fcolor4(1, 1, 1, 1);
        break;
    }

    defmtl->_rasterstate.SetBlending(lev2::EBLENDING_ALPHA);
    tgt->PushModColor(clr);
    ren_quad(ixr, iyr, ixr + _geometry._w, iyr + _geometry._h);
    tgt->PopModColor();
    defmtl->_rasterstate.SetBlending(lev2::EBLENDING_OFF);

    /////////////
    // close button
    /////////////

    if (mEnableCloseButton) {
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
    }
  }
  mtxi->PopUIMatrix();
  tgt->PopModColor();

  if (_child1)
    _child1->Draw(drwev);

  if (_child2)
    _child2->Draw(drwev);
}

/////////////////////////////////////////////////////////////////////////

void SplitPanel::DoLayout() {
  mDockedAtTop = (_geometry._y == -kpanelw);
  mCloseX      = kpanelw;
  mCloseY      = mDockedAtTop ? _geometry._h - kpanelw : 0;

  int cw = _geometry._w - (kpanelw * 2);

  int ch  = _geometry._h / 2;
  int p1y = kpanelw;
  int p1h = int(float(_geometry._h) * mSplitVal) - ksplith;
  int p2y = p1y + p1h + ksplith;
  int p2h = _geometry._h - kpanelw - p2y;

  if (0)
    printf(
        "SplitPanel<%s>::DoLayout x<%d> y<%d> w<%d> h<%d> p1y<%d> p1h<%d> p2y<%d> p2h<%d>\n", //
        msName.c_str(),
        _geometry._x,
        _geometry._y,
        _geometry._w,
        _geometry._h,
        p1y,
        p1h,
        p2y,
        p2h);

  if (_child1) {
    _child1->SetRect(kpanelw, p1y, cw, p1h);
  }
  if (_child2) {
    _child2->SetRect(kpanelw, p2y, cw, p2h);
  }
}

/////////////////////////////////////////////////////////////////////////

Widget* SplitPanel::doRouteUiEvent(event_constptr_t ev) {
  Widget* target = nullptr;
  switch (mPanelUiState) {
    case 0:
      if (_child1 and _child1->IsEventInside(ev)) {
        target = _child1->routeUiEvent(ev);
      } else if (_child2 and _child2->IsEventInside(ev)) {
        target = _child2->routeUiEvent(ev);
      }
      break;
    default:
      break;
  }
  if (nullptr == target and IsEventInside(ev)) {
    target = this;
  }
  return target;
}

/////////////////////////////////////////////////////////////////////////

void SplitPanel::snap() {
  if (nullptr == mParent)
    return;
  if (not _moveEnabled)
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
  if (snapt and snapb) {
    SetY(-kpanelw);
    SetH(ph + 2 * kpanelw);
  } else if (snapt)
    SetY(-kpanelw);
  else if (snapb)
    SetY(ph + kpanelw - height());
  if (snapl and snapr) {
    SetX(-kpanelw);
    SetW(pw + 2 * kpanelw);
  }
  if (snapl)
    SetX(-kpanelw);
  else if (snapr)
    SetX(pw + kpanelw - width());
}

HandlerResult SplitPanel::DoOnUiEvent(event_constptr_t Ev) {

  HandlerResult ret(this);

  int evx      = Ev->miX;
  int evy      = Ev->miY;
  bool isshift = Ev->mbSHIFT;
  // printf("Panel<%p>::OnUiEvent isshift<%d>\n", this, int(isshift));
  //////////////////////////////
  int ilocx = 0;
  int ilocy = 0;
  RootToLocal(evx, evy, ilocx, ilocy);
  //////////////////////////////
  const auto& filtev = Ev->mFilteredEvent;
  switch (filtev._eventcode) {
    case ui::EventCode::PUSH: // idle
    {
      _downx         = evx;
      _downy         = evy;
      _prevpx        = _geometry._x;
      _prevpy        = _geometry._y;
      _prevpw        = _geometry._w;
      _prevph        = _geometry._h;
      ret.mHoldFocus = true;

      float funity  = float(ilocy) / float(_geometry._h);
      float funitks = float(kpanelw) / float(_geometry._h);

      bool is_splitter = (fabs(funity - mSplitVal) < funitks) and (ilocy < (_geometry._h - kpanelw * 2));

      is_splitter &= (ilocx > kpanelw) and (ilocx < (_geometry._w - kpanelw)); // x check

      // printf( "ilocy<%d> funity<%f> funitks<%f> mSplitVal<%f> is_splitter<%d> b0<%d>\n", ilocy, funity, funitks, mSplitVal,
      // int(is_splitter), int(filtev.mBut0) );

      if (filtev.mBut0) {
        if (mEnableCloseButton and (ilocx >= mCloseX) and ((ilocx - mCloseX) < kpanelw) and (ilocy >= mCloseY) and
            ((ilocy - mCloseY) < kpanelw)) {
          auto lamb = [=]() { delete this; };
          opq::Op(lamb).QueueASync(opq::mainSerialQueue());

        } else {
          if (is_splitter)
            mPanelUiState = 6;
          else if (_moveEnabled)
            mPanelUiState = 1;
        }
      } else if (filtev.mBut1 || filtev.mBut2) {
        if (is_splitter)
          mPanelUiState = 6;
        else if (_moveEnabled) {
          if (abs(ilocy) < kpanelw) // top
            mPanelUiState = 2;
          else if (abs(ilocy - _geometry._h) < kpanelw) // bot
            mPanelUiState = 3;
          else if (abs(ilocx) < kpanelw) // lft
            mPanelUiState = 4;
          else if (abs(ilocx - _geometry._w) < kpanelw) // rht
            mPanelUiState = 5;
        }
      }
      break;
    }
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
    case 6: // set splitter
    {
      mSplitVal = float(ilocy - ksplith) / float(_geometry._h);

      mSplitVal = std::clamp(mSplitVal, 0.1f, 0.9f);

      DoLayout();
      SetDirty();
      break;
    }
  }

  return ret;
}
//

void SplitPanel::DoOnEnter() {
}

void SplitPanel::DoOnExit() {
}

}} // namespace ork::ui
