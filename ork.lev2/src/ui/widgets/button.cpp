#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/ui/button.h>
#include <ork/lev2/ui/context.h>

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
      _down_color = fvec4(color.xyz() * 0.6f, 1);
      _hover_color = fvec4(color.xyz() * 1.3f, 1);
      _draw_label = false; // we render text ourselves via ThemeEngine
}
///////////////////////////////////////////////////////////////////////////////
HandlerResult Button::DoOnUiEvent(event_constptr_t cev) {
  HandlerResult rval;

  switch (cev->_eventcode) {
    case EventCode::PUSH: {
      _pressed = true;
      if (_onPushed) _onPushed();
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

    case EventCode::MOUSE_ENTER: {
      _hovering = true;
      break;
    }

    case EventCode::MOUSE_LEAVE: {
      _hovering = false;
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

  // Determine fill color based on state
  fvec4 fill_color = _bg_color;
  if (_pressed) {
    fill_color = _down_color;
  } else if (_hovering) {
    fill_color = _hover_color;
  }

  // Brighten border slightly relative to fill
  fvec4 border_color = fvec4(fill_color.xyz() * 1.4f, 1.0f);

  // Use ThemeEngine if available
  if (_uicontext && _uicontext->_theme_engine) {
    Style style;
    style._bg_color = fill_color;
    style._border_color = border_color;
    style._text_color = _fg_color;
    style._corner_radius = 4;
    style._border_width = 1;
    style._blend_mode = lev2::BlendingMacro::ALPHA;

    _uicontext->_theme_engine->drawBox(this, drwev, &style);
    _uicontext->_theme_engine->drawText(this, drwev, &style, _name);
    return;
  }

  // Fallback: simple flat quad + label (legacy path)
  auto tgt    = drwev->GetTarget();
  auto mtxi   = tgt->MTXI();
  auto primi  = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  mtxi->PushUIMatrix();
  {
    int ix1, iy1;
    LocalToRoot(0, 0, ix1, iy1);
    int ix2 = ix1 + _geometry._w;
    int iy2 = iy1 + _geometry._h;

    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);

    tgt->PushModColor(fill_color);
    primi->RenderQuadAtZ(
        defmtl.get(),
        ix1, ix2, iy1, iy2,
        0.0f,
        0.0f, 1.0f,
        0.0f, 1.0f
    );
    tgt->PopModColor();

    // Draw centered label text
    tgt->PushModColor(_fg_color);
    lev2::FontMan::PushFont("i14");
    lev2::FontMan::beginTextBlock(tgt, 16);
    int sw = lev2::FontMan::stringWidth(_name.length());
    int ixc = ix1 + (_geometry._w >> 1);
    int iyc = iy1 + (_geometry._h >> 1);
    lev2::FontMan::DrawText(tgt, ixc - (sw >> 1), iyc - 6, _name.c_str());
    lev2::FontMan::endTextBlock(tgt);
    lev2::FontMan::PopFont();
    tgt->PopModColor();
  }
  mtxi->PopUIMatrix();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
