#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/ui/dial.h>
#include <ork/math/audiomath.h>

namespace ork::ui {
///////////////////////////////////////////////////////////////////////////////
Dial::Dial(const std::string& name, fvec4 color)
    : Widget(name) {
  _bgcolor   = color * 0.5;
  _fgcolor   = color * 0.75;
  _indcolor  = color;
  _textcolor = fvec4(1, 1, 1, 1);
}
///////////////////////////////////////////////////////////////////////////////
void Dial::DoDraw(drawevent_constptr_t drwev) {

  auto tgt    = drwev->GetTarget();
  auto fbi    = tgt->FBI();
  auto mtxi   = tgt->MTXI();
  auto& primi = lev2::GfxPrimitives::GetRef();
  auto defmtl = lev2::defaultUIMaterial();

  mtxi->PushUIMatrix();
  {
    int ix1, iy1, ix2, iy2, ixc, iyc;
    LocalToRoot(0, 0, ix1, iy1);
    ix2 = ix1 + _geometry._w;
    iy2 = iy1 + _geometry._h;
    ixc = ix1 + (_geometry._w >> 1);
    iyc = iy1 + (_geometry._h >> 1);

    defmtl->_rasterstate.SetBlending(lev2::EBLENDING_ALPHA);
    defmtl->_rasterstate.SetDepthTest(lev2::EDEPTHTEST_OFF);

    fvec4 color = _bgcolor;
    if (not hasMouseFocus())
      color *= 0.9f;

    tgt->PushModColor(color);
    primi.RenderQuadAtZ(
        defmtl.get(),
        tgt,
        ix1,  // x0
        ix2,  // x1
        iy1,  // y0
        iy2,  // y1
        0.0f, // z
        0.0f,
        1.0f, // u0, u1
        0.0f,
        1.0f // v0, v1
    );
    //////////////////////////////
    tgt->PopModColor();
    //////////////////////////////
    tgt->PushModColor(_textcolor);
    ork::lev2::FontMan::PushFont(_font);
    //////////////////////////////
    int lablen = _label.length();
    auto str   = FormatString("%g", _curvalue);
    //////////////////////////////
    int y = iyc - 6;
    lev2::FontMan::beginTextBlock(tgt, lablen + str.length());
    if (lablen) {
      //
      y -= 7;
      int sw = lev2::FontMan::stringWidth(lablen);
      lev2::FontMan::DrawText(
          tgt, //
          ixc - (sw >> 1),
          y,
          _label.c_str());
      //
      y += 14;
    }

    int sw = lev2::FontMan::stringWidth(str.length());
    lev2::FontMan::DrawText(
        tgt, //
        ixc - (sw >> 1),
        y,
        str.c_str());
    //
    lev2::FontMan::endTextBlock(tgt);
    ork::lev2::FontMan::PopFont();
    tgt->PopModColor();
  }
  mtxi->PopUIMatrix();
}
///////////////////////////////////////////////////////////////////////////////
HandlerResult Dial::DoOnUiEvent(event_constptr_t ev) {
  bool isshift = ev->mbSHIFT;
  switch (ev->_eventcode) {
    case EventCode::MOUSEWHEEL: {
      bool isneg = ev->miMWY < 0;
      float fy   = float(abs(ev->miMWY)) / 300.0f;
      fy         = std::clamp(powf(fy, 0.2f), 0.0f, 1.0f);
      fy *= isneg ? -1.0f : 1.0f;
      float inc    = isshift ? 40.0f : 4.0f;
      int step     = (fy * inc);
      int nextstep = _cursteps + step;
      selValFromStep(nextstep);
      // printf("wheely<%g> _curvalue<%g>\n", fy, _curvalue);
      break;
    }
    default:
      break;
  }
  return HandlerResult();
}
///////////////////////////////////////////////////////////////////////////////
constexpr int stepshift = 3;
///////////////////////////////////////////////////////////////////////////////
void Dial::selValFromStep(int step) {
  _cursteps = std::clamp(step, 0, _numsteps);
  if (_isbipolar) {
    int hcurstepsP = (_numsteps >> 1);
    if (_cursteps >= (_numsteps >> 1)) { // positive
      int hnumsteps = (_numsteps >> 1);
      int actstep   = (hcurstepsP - hnumsteps) >> stepshift;
      float fi      = float(actstep) / int(hnumsteps >> stepshift);
      _curvalue     = audiomath::lerp(_ctrval, _maxval, powf(fi, _power));
    } else { // negative
    }
  } else {
    int actstep = _cursteps >> stepshift;
    float fi    = float(actstep) / int(_numsteps >> stepshift);
    _curvalue   = audiomath::lerp(_minval, _maxval, powf(fi, _power));
  }
  if (_onupdate) {
    _onupdate(_curvalue);
  }
}
///////////////////////////////////////////////////////////////////////////////
void Dial::setParams(int numsteps, float curval, float minval, float maxval, float power) {

  _isbipolar = (_minval < 0.0f) and (_maxval > 1.0f);

  _numsteps  = numsteps << stepshift;
  _minval    = minval;
  _maxval    = maxval;
  _ctrval    = (minval + maxval * 0.5f);
  _range     = (maxval - maxval);
  _power     = power;
  _curvalue  = curval;
  float nfip = (_curvalue - _minval) / (_maxval - _minval);
  float nfi  = powf(nfip, 1.0 / _power);
  selValFromStep(int(_numsteps * nfi));
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
