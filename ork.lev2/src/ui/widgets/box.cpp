#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/ui/box.h>

namespace ork::ui {
///////////////////////////////////////////////////////////////////////////////
Box::Box(
    const std::string& name, //
    fvec4 color,
    int x,
    int y,
    int w,
    int h)
    : Widget(name, x, y, w, h)
    , _color(color) {
}
///////////////////////////////////////////////////////////////////////////////
void Box::DoDraw(drawevent_constptr_t drwev) {

  auto tgt    = drwev->GetTarget();
  auto fbi    = tgt->FBI();
  auto mtxi   = tgt->MTXI();
  auto& primi = lev2::GfxPrimitives::GetRef();
  auto defmtl = lev2::defaultUIMaterial();

  mtxi->PushUIMatrix();
  {
    int ix1, iy1, ix2, iy2;
    LocalToRoot(0, 0, ix1, iy1);
    ix2 = ix1 + _geometry._w;
    iy2 = iy1 + _geometry._h;

    if (0)
      printf(
          "drawbox<%s> xy1<%d,%d> xy2<%d,%d>\n", //
          _name.c_str(),
          ix1,
          iy1,
          ix2,
          iy2);

    defmtl->_rasterstate.SetBlending(lev2::Blending::ALPHA);
    defmtl->_rasterstate.SetDepthTest(lev2::EDepthTest::OFF);
    tgt->PushModColor(_color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
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
  }
  mtxi->PopUIMatrix();
}
///////////////////////////////////////////////////////////////////////////////
HandlerResult Box::DoOnUiEvent(event_constptr_t Ev) {
  return HandlerResult();
}
///////////////////////////////////////////////////////////////////////////////
EvTestBox::EvTestBox(
    const std::string& name, //
    fvec4 colornormal,
    fvec4 colorclick,
    fvec4 colordoubleclick,
    fvec4 colordrag,
    int x,
    int y,
    int w,
    int h)
    : Widget(name, x, y, w, h)
    , _colorNormal(colornormal)
    , _colorClick(colorclick)
    , _colorDoubleClick(colordoubleclick)
    , _colorDrag(colordrag) {
}
///////////////////////////////////////////////////////////////////////////////
EvTestBox::EvTestBox(
    const std::string& name, //
    fvec4 color,
    int x,
    int y,
    int w,
    int h)
    : Widget(name, x, y, w, h) {
  _colorNormal      = (color);
  _colorClick       = (color * 0.75f);
  _colorDoubleClick = (color * 0.5f);
  _colorDrag        = color * 0.25f;
  _colorKeyDown     = fvec4(1, 1, 1, 0) - color;
  _colorKeyDown.w   = color.w;
}
///////////////////////////////////////////////////////////////////////////////
HandlerResult EvTestBox::DoOnUiEvent(event_constptr_t Ev) {
  _colorsel = Ev->_eventcode;
  //printf( "EvTestBox::DoOnUiEvent<%s> code<%d>\n", _name.c_str(), int(Ev->_eventcode) );
  return HandlerResult();
}
///////////////////////////////////////////////////////////////////////////////
void EvTestBox::DoDraw(drawevent_constptr_t drwev) {
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

    if (0)
      printf(
          "drawbox<%s> xy1<%d,%d> xy2<%d,%d>\n", //
          _name.c_str(),
          ix1,
          iy1,
          ix2,
          iy2);

    fvec4 color           = _colorNormal;
    std::string statename = "";
    switch (_colorsel) {
      case EventCode::PUSH:
        color     = _colorClick;
        statename = "PUSH";
        break;
      case EventCode::MOVE:
        statename = "MOVE";
        break;
      case EventCode::RELEASE:
        statename = "RELEASE";
        break;
      case EventCode::DOUBLECLICK:
        color     = _colorDoubleClick;
        statename = "DOUBLECLICK";
        break;
      case EventCode::BEGIN_DRAG:
        color     = _colorDrag;
        statename = "BEGIN_DRAG";
        break;
      case EventCode::DRAG:
        color     = _colorDrag;
        statename = "DRAG";
        break;
      case EventCode::END_DRAG:
        color     = _colorDrag;
        statename = "END_DRAG";
        break;
      case EventCode::KEY_DOWN:
        color     = _colorKeyDown;
        statename = "KEY";
        break;
      case EventCode::KEY_REPEAT:
        color     = _colorKeyDown;
        statename = "KEY_REPEAT";
        break;
      case EventCode::KEY_UP:
        statename = "KEYUP";
        break;
      case EventCode::MOUSEWHEEL:
        statename = "MOUSEWHEEL";
        break;
      case EventCode::MOUSE_ENTER:
        statename = "MOUSE_ENTER";
        break;
      case EventCode::MOUSE_LEAVE:
        statename = "MOUSE_LEAVE";
        break;
      default:
        statename = "---";
        break;
    }

    defmtl->_rasterstate.SetBlending(lev2::Blending::OFF);
    defmtl->_rasterstate.SetDepthTest(lev2::EDepthTest::OFF);
    ///////////////////////////////
    if (not hasMouseFocus())
      color *= 0.9f;
    tgt->PushModColor(color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
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
    ///////////////////////////////
    color   = fvec4(1, 1, 1, 1) - color;
    color.w = 1.0f;

    tgt->PushModColor(color);
    ork::lev2::FontMan::PushFont("i14");
    lev2::FontMan::beginTextBlock(tgt, 16);
    int sw = lev2::FontMan::stringWidth(statename.length());
    lev2::FontMan::DrawText(
        tgt, //
        ixc - (sw >> 1),
        iyc - 6,
        statename.c_str());
    lev2::FontMan::endTextBlock(tgt);
    ork::lev2::FontMan::PopFont();
    tgt->PopModColor();
  }
  mtxi->PopUIMatrix();
}
///////////////////////////////////////////////////////////////////////////////
LambdaBox::LambdaBox(
    const std::string& name, //
    fvec4 colornormal,
    fvec4 colorclick,
    fvec4 colordoubleclick,
    fvec4 colordrag,
    int x,
    int y,
    int w,
    int h)
    : Widget(name, x, y, w, h)
    , _colorNormal(colornormal)
    , _colorClick(colorclick)
    , _colorDoubleClick(colordoubleclick)
    , _colorDrag(colordrag) {
}
///////////////////////////////////////////////////////////////////////////////
LambdaBox::LambdaBox(
    const std::string& name, //
    fvec4 color,
    int x,
    int y,
    int w,
    int h)
    : Widget(name, x, y, w, h) {
  _colorNormal      = (color);
  _colorClick       = (color * 0.75f);
  _colorDoubleClick = (color * 0.5f);
  _colorDrag        = color * 0.25f;
  _colorKeyDown     = fvec4(1, 1, 1, 0) - color;
  _colorKeyDown.w   = color.w;
}
///////////////////////////////////////////////////////////////////////////////
HandlerResult LambdaBox::DoOnUiEvent(event_constptr_t Ev) {
  _colorsel = Ev->_eventcode;
    switch (Ev->_eventcode) {
      case EventCode::PUSH:
        if(_onPressed){
          _onPressed();
        }
        break;
      default:
        break;
      }
  return HandlerResult();
}
///////////////////////////////////////////////////////////////////////////////
void LambdaBox::DoDraw(drawevent_constptr_t drwev) {
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

    if (0)
      printf(
          "drawbox<%s> xy1<%d,%d> xy2<%d,%d>\n", //
          _name.c_str(),
          ix1,
          iy1,
          ix2,
          iy2);

    fvec4 color           = _colorNormal;
    std::string statename = "";
    switch (_colorsel) {
      case EventCode::PUSH:
        color     = _colorClick;
        statename = "PUSH";
        break;
      case EventCode::MOVE:
        statename = "MOVE";
        break;
      case EventCode::RELEASE:
        statename = "RELEASE";
        break;
      case EventCode::DOUBLECLICK:
        color     = _colorDoubleClick;
        statename = "DOUBLECLICK";
        break;
      case EventCode::BEGIN_DRAG:
        color     = _colorDrag;
        statename = "BEGIN_DRAG";
        break;
      case EventCode::DRAG:
        color     = _colorDrag;
        statename = "DRAG";
        break;
      case EventCode::END_DRAG:
        color     = _colorDrag;
        statename = "END_DRAG";
        break;
      case EventCode::KEY_DOWN:
        color     = _colorKeyDown;
        statename = "KEY";
        break;
      case EventCode::KEY_REPEAT:
        color     = _colorKeyDown;
        statename = "KEY_REPEAT";
        break;
      case EventCode::KEY_UP:
        statename = "KEYUP";
        break;
      case EventCode::MOUSEWHEEL:
        statename = "MOUSEWHEEL";
        break;
      case EventCode::MOUSE_ENTER:
        statename = "MOUSE_ENTER";
        break;
      case EventCode::MOUSE_LEAVE:
        statename = "MOUSE_LEAVE";
        break;
      default:
        statename = "---";
        break;
    }

    defmtl->_rasterstate.SetBlending(lev2::Blending::ALPHA);
    defmtl->_rasterstate.SetDepthTest(lev2::EDepthTest::OFF);
    ///////////////////////////////
    if (not hasMouseFocus())
      color *= 0.9f;
    tgt->PushModColor(color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
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
    ///////////////////////////////
    color   = fvec4(1, 1, 1, 1) - color;
    color.w = 1.0f;

    tgt->PushModColor(color);
    ork::lev2::FontMan::PushFont("i14");
    lev2::FontMan::beginTextBlock(tgt, 16);
    int sw = lev2::FontMan::stringWidth(statename.length());
    lev2::FontMan::DrawText(
        tgt, //
        ixc - (sw >> 1),
        iyc - 6,
        statename.c_str());
    lev2::FontMan::endTextBlock(tgt);
    ork::lev2::FontMan::PopFont();
    tgt->PopModColor();
  }
  mtxi->PopUIMatrix();
}///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
