////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/ui/colorswatch.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/context.h>

namespace ork::ui {

/////////////////////////////////////////////////////////////////////////

ColorSwatch::ColorSwatch(const std::string& name, fvec4 color, int x, int y, int w, int h)
    : Widget(name, x, y, w, h)
    , _color(color) {
}

/////////////////////////////////////////////////////////////////////////

HandlerResult ColorSwatch::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;

  switch (ev->_eventcode) {
    case EventCode::PUSH: {
      if (_onClick) {
        _onClick();
      }
      result.setHandled(this);
      break;
    }

    case EventCode::MOUSE_ENTER: {
      _hovering = true;
      result.setHandled(this);
      break;
    }

    case EventCode::MOUSE_LEAVE: {
      _hovering = false;
      result.setHandled(this);
      break;
    }

    default:
      break;
  }

  return result;
}

/////////////////////////////////////////////////////////////////////////

void ColorSwatch::DoDraw(ui::drawevent_constptr_t drwev) {
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto primi = tgt->PRI();

  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  int ix2 = ix1 + _geometry._w;
  int iy2 = iy1 + _geometry._h;

  // Use theme engine if available for nice rounded rect
  if (_uicontext && _uicontext->_theme_engine) {
    auto theme = _uicontext->_theme_engine;

    Style style;
    style._bg_color = _color;
    style._border_color = _hovering ? _hover_border_color : _border_color;
    style._corner_radius = _corner_radius;
    style._border_width = _border_width;
    style._blend_mode = lev2::BlendingMacro::ALPHA;

    theme->drawBox(this, drwev, &style);
  } else {
    // Fallback: simple colored box
    _drawColoredBox(drwev, _color);
  }

  // Optionally show hex value
  if (_show_hex && _geometry._w > 50) {
    auto font = lev2::FontMan::fontForId("i12");
    if (font) {
      // Convert color to hex string
      int r = int(_color.x * 255.0f);
      int g = int(_color.y * 255.0f);
      int b = int(_color.z * 255.0f);
      char hex[16];
      snprintf(hex, sizeof(hex), "#%02X%02X%02X", r, g, b);

      // Choose contrasting text color
      float luminance = 0.299f * _color.x + 0.587f * _color.y + 0.114f * _color.z;
      fvec4 text_color = (luminance > 0.5f) ? fvec4(0, 0, 0, 1) : fvec4(1, 1, 1, 1);

      lev2::FontMan::PushFont(font);
      tgt->PushModColor(text_color);
      mtxi->PushUIMatrix();
      {
        lev2::FontMan::beginTextBlock(tgt, 8);
        int text_y = iy1 + (_geometry._h - font->description().miAdvanceHeight) / 2;
        int text_x = ix1 + 4;
        lev2::FontMan::DrawText(tgt, text_x, text_y, hex);
        lev2::FontMan::endTextBlock(tgt);
      }
      mtxi->PopUIMatrix();
      tgt->PopModColor();
      lev2::FontMan::PopFont();
    }
  }
}

/////////////////////////////////////////////////////////////////////////

} // namespace ork::ui
