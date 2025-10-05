#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/ui/button.h>

namespace ork::ui {
///////////////////////////////////////////////////////////////////////////////
Button::Button(
    const std::string& name, //
    fvec4 color,
    int x,
    int y,
    int w,
    int h)
    : Widget(name, x, y, w, h)
    , _bg_color(color) {
      _fg_color = fvec4(1,1,1,1);
      _down_color = fvec4(color.xyz() * 0.5f, 1);
      _draw_label = true;
}
///////////////////////////////////////////////////////////////////////////////
HandlerResult Button::DoOnUiEvent(event_constptr_t cev) {
  HandlerResult rval;

  switch (cev->_eventcode) {
    case EventCode::PUSH: {
      _pressed = true;
      rval.setHandled(this);
      break;
    }

    case EventCode::RELEASE: {
      if (_pressed) {
        _pressed = false;
        if (_onPressed) {
          _onPressed();
        }
        rval.setHandled(this);
      }
      break;
    }

    case EventCode::MOUSE_LEAVE: {
      _pressed = false;
      break;
    }

    default:
      break;
  }

  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void Button::DoDraw(drawevent_constptr_t drwev) {

  auto tgt    = drwev->GetTarget();
  auto fbi    = tgt->FBI();
  auto mtxi   = tgt->MTXI();
  auto primi  = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int label_w = labelWidth();
  auto content = contentRect();

  mtxi->PushUIMatrix();
  {
    int ix1, iy1, ix2, iy2, ixc, iyc;
    LocalToRoot(0, 0, ix1, iy1);
    ix2 = ix1 + _geometry._w;
    iy2 = iy1 + _geometry._h;
    ixc = ix1 + (_geometry._w >> 1);
    iyc = iy1 + (_geometry._h >> 1);

    // Compute content area position
    int content_x1 = ix1 + label_w;
    int content_x2 = ix2 - 2;
    int content_y1 = iy1 + 2;
    int content_y2 = iy2 - 2;

    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);

    ///////////////////////////////
    // draw background
    ///////////////////////////////

    tgt->PushModColor(_bg_color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    primi->RenderQuadAtZ(
        defmtl.get(),
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
    // draw button area (texture or color)
    ///////////////////////////////

    // Choose texture based on pressed state
    lev2::texture_ptr_t active_texture = _pressed ? _down_texture : _up_texture;

    if (active_texture) {
      // Draw with texture
      defmtl->SetUIColorMode(lev2::UiColorMode::VTX);
      //defmtl->setTexture(lev2::ETEXDEST_DIFFUSE, active_texture.get());
      defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);

      tgt->PushModColor(fvec4(1, 1, 1, 1));
      primi->RenderQuadAtZ(
          defmtl.get(),
          content_x1,   // x0
          content_x2,   // x1
          content_y1,   // y0
          content_y2,   // y1
          0.0f,         // z
          0.0f,
          1.0f, // u0, u1
          0.0f,
          1.0f // v0, v1
      );
      tgt->PopModColor();

      //defmtl->setTexture(lev2::ETEXDEST_DIFFUSE, nullptr);
    } else {
      // Draw with color
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);

      fvec4 button_color = _pressed ? _down_color : fvec4(_bg_color.xyz() * 0.7f, 1);

      tgt->PushModColor(button_color);
      primi->RenderQuadAtZ(
          defmtl.get(),
          content_x1,   // x0
          content_x2,   // x1
          content_y1,   // y0
          content_y2,   // y1
          0.0f,         // z
          0.0f,
          1.0f, // u0, u1
          0.0f,
          1.0f // v0, v1
      );
      tgt->PopModColor();
    }

    ///////////////////////////////
    // draw label (using Widget base class)
    ///////////////////////////////

    _drawLabel(drwev);
  }
  mtxi->PopUIMatrix();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
