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
HandlerResult Box::DoOnUiEvent(event_constptr_t Ev) {
  return HandlerResult();
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

    if (1)
      printf(
          "drawbox<%s> xy1<%d,%d> xy2<%d,%d>\n", //
          msName.c_str(),
          ix1,
          iy1,
          ix2,
          iy2);

    defmtl->_rasterstate.SetBlending(lev2::EBLENDING_ALPHA);
    defmtl->_rasterstate.SetDepthTest(lev2::EDEPTHTEST_OFF);
    tgt->PushModColor(_color);
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
  _colorDrag        = fvec4(1, 1, 1, 0) - color;
  _colorDrag.w      = color.w;
}

///////////////////////////////////////////////////////////////////////////////

HandlerResult EvTestBox::DoOnUiEvent(event_constptr_t Ev) {

  switch (Ev->_eventcode) {
    case EventCode::PUSH:
    case EventCode::DOUBLECLICK:
    case EventCode::RELEASE:
    case EventCode::DRAG:
      _colorsel = Ev->_eventcode;
      break;
  }
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
    int ix1, iy1, ix2, iy2;
    LocalToRoot(0, 0, ix1, iy1);
    ix2 = ix1 + _geometry._w;
    iy2 = iy1 + _geometry._h;

    if (0)
      printf(
          "drawbox<%s> xy1<%d,%d> xy2<%d,%d>\n", //
          msName.c_str(),
          ix1,
          iy1,
          ix2,
          iy2);

    fvec4 color = _colorNormal;
    switch (_colorsel) {
      case EventCode::PUSH:
        color = _colorClick;
        break;
      case EventCode::DOUBLECLICK:
        color = _colorDoubleClick;
        break;
      case EventCode::DRAG:
        color = _colorDrag;
        break;
      default:
        break;
    }

    defmtl->_rasterstate.SetBlending(lev2::EBLENDING_ALPHA);
    defmtl->_rasterstate.SetDepthTest(lev2::EDEPTHTEST_OFF);
    ///////////////////////////////
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
    tgt->PopModColor();
    ///////////////////////////////
    if (not hasMouseFocus()) {
      tgt->PushModColor(color * 0.5f);
      primi.RenderQuadAtZ(
          defmtl.get(),
          tgt,
          ix1 + 1, // x0
          ix2 - 2, // x1
          iy1 + 1, // y0
          iy2 - 2, // y1
          0.0f,    // z
          0.0f,
          1.0f, // u0, u1
          0.0f,
          1.0f // v0, v1
      );
    }
    tgt->PopModColor();
  }
  mtxi->PopUIMatrix();
}
} // namespace ork::ui
