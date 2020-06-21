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
    tgt->PushModColor(_bgcolor);
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
    tgt->PopModColor();
    tgt->PushModColor(_textcolor);
    ork::lev2::FontMan::PushFont(_font);
    lev2::FontMan::beginTextBlock(tgt, 256);
    //
    int y  = iyc - 6 - 7;
    int sw = lev2::FontMan::stringWidth(_label.length());
    lev2::FontMan::DrawText(
        tgt, //
        ixc - (sw >> 1),
        y,
        _label.c_str());
    //
    y += 14;
    auto str = FormatString("%g", _curvalue);
    sw       = lev2::FontMan::stringWidth(str.length());
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
  switch (ev->_eventcode) {
    case EventCode::MOUSEWHEEL: {
      bool isneg = ev->miMWY < 0;
      float fy   = float(abs(ev->miMWY)) / 300.0f;
      fy         = std::clamp(powf(fy, 0.2f), 0.0f, 1.0f);
      fy *= isneg ? -1.0f : 1.0f;
      int step     = (fy * 4.0);
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
void Dial::selValFromStep(int step) {
  _cursteps = std::clamp(step, 0, _numsteps);
  float fi  = float(_cursteps) / float(_numsteps);
  _curvalue = audiomath::lerp(_minval, _maxval, powf(fi, _power));
  if (_onupdate) {
    _onupdate(_curvalue);
  }
}
///////////////////////////////////////////////////////////////////////////////
void Dial::setParams(int numsteps, float curval, float minval, float maxval, float power) {
  _numsteps  = numsteps;
  _minval    = minval;
  _maxval    = maxval;
  _power     = power;
  _curvalue  = curval;
  float nfip = (_curvalue - _minval) / (_maxval - _minval);
  float nfi  = powf(nfip, 1.0 / _power);
  selValFromStep(int(_numsteps * nfi));
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
