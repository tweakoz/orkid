////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/ui/choicelist_widget.h>
#include <ork/lev2/ui/dropdown_menu.h>
#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/style.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/util/crc.h>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////

ChoicelistWidget::ChoicelistWidget(const std::string& name, const std::string& current_value)
    : Widget(name, 0, 0, 0, 0)
    , _current_value(current_value) {
}

///////////////////////////////////////////////////////////////////////////////

void ChoicelistWidget::DoDraw(drawevent_constptr_t drwev) {
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  int ix2 = ix1 + _geometry._w;
  int iy2 = iy1 + _geometry._h;

  mtxi->PushUIMatrix();
  {
    // Draw background
    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
    tgt->PushModColor(_bg_color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    primi->RenderQuadAtZ(defmtl.get(), ix1 + 1, ix2 - 1, iy1 + 1, iy2 - 1, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    tgt->PopModColor();

    // Draw current value text
    auto font = lev2::FontMan::fontForId("i14");
    if (font) {
      lev2::FontMan::PushFont(font);
      tgt->PushModColor(_fg_color);
      int text_x = ix1 + 6;
      int text_y = iy1 + (_geometry._h - font->description().miAdvanceHeight) / 2;
      lev2::FontMan::beginTextBlock(tgt, _current_value.length() + 2);
      lev2::FontMan::DrawText(tgt, text_x, text_y, _current_value.c_str());
      lev2::FontMan::endTextBlock(tgt);
      tgt->PopModColor();

      // Draw dropdown icon on the right
      if (_uicontext && _uicontext->_theme_engine) {
        auto style = _uicontext->_theme_engine->_styledb->getStyle("box"_crcu);
        if (style && style->_icon_dropdown) {
          const int icon_size = 10;
          int icon_x = ix2 - icon_size - 6;
          int icon_y = iy1 + (_geometry._h - icon_size) / 2;
          _uicontext->_theme_engine->drawIcon(icon_x, icon_y, icon_size, icon_size, drwev, style->_icon_dropdown);
        }
      }

      lev2::FontMan::PopFont();
    }
  }
  mtxi->PopUIMatrix();
}

///////////////////////////////////////////////////////////////////////////////

HandlerResult ChoicelistWidget::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;

  if (ev->_eventcode == EventCode::PUSH) {
    if (_getChoices) {
      auto choices = _getChoices();
      if (!choices.empty()) {
        // Prepend / so DropdownMenu slash-tree works correctly
        std::vector<std::string> paths;
        for (const auto& c : choices) {
          if (c.empty()) continue;
          if (c[0] == '/') {
            paths.push_back(c);
          } else {
            paths.push_back("/" + c);
          }
        }
        auto tree = DropdownMenu::buildTreeFromPaths(paths);
        auto menu = std::make_shared<DropdownMenu>("choicelist_" + _name, tree->root());
        menu->_onSelected = [this](std::string selected) {
          if (!selected.empty() && selected[0] == '/') {
            selected = selected.substr(1);
          }
          if (_onChoiceSelected) {
            _onChoiceSelected(selected);
          }
        };
        auto sz = menu->computeSize();
        int sx = ev->miX;
        int sy = ev->miY;
        if (_uicontext) {
          _uicontext->pushOverlay(menu, sx, sy, int(sz.x), int(sz.y), true, nullptr);
        }
      }
    }
    result.setHandled(this);
  }

  return result;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
