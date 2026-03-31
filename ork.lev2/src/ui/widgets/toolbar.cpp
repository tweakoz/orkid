////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/txi.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/ui/toolbar.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/context.h>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////
// Image -> Texture cache (avoids redundant GPU uploads for the same image)
///////////////////////////////////////////////////////////////////////////////

static std::unordered_map<lev2::image_ptr_t, lev2::texture_ptr_t> _image_texture_cache;

static lev2::texture_ptr_t _cachedTextureForImage(lev2::Context* ctx, lev2::image_ptr_t image) {
  auto it = _image_texture_cache.find(image);
  if (it != _image_texture_cache.end()) {
    return it->second;
  }
  auto tex = std::make_shared<lev2::Texture>();
  ctx->TXI()->initTextureFromImage(tex.get(), image, false, true);
  _image_texture_cache[image] = tex;
  return tex;
}

///////////////////////////////////////////////////////////////////////////////
// ToolbarButton
///////////////////////////////////////////////////////////////////////////////

ToolbarButton::ToolbarButton(const std::string& id) {
  _id = id;
}

void ToolbarButton::updateTextures(lev2::Context* ctx) {
  // Update icon texture
  if (_icon_provider) {
    auto new_image = _icon_provider->_func();
    if (new_image != _icon_image) {
      _icon_image = new_image;
      if (_icon_image) {
        _icon_texture = _cachedTextureForImage(ctx, _icon_image);
      }
    }
  } else if (_icon_image && _icon_image != _prev_icon_image) {
    _prev_icon_image = _icon_image;
    _icon_texture = _cachedTextureForImage(ctx, _icon_image);
  }

  // Update hover texture
  if (_hover_provider) {
    auto new_image = _hover_provider->_func();
    if (new_image != _hover_image) {
      _hover_image = new_image;
      if (_hover_image) {
        _hover_texture = _cachedTextureForImage(ctx, _hover_image);
      }
    }
  } else if (_hover_image && _hover_image != _prev_hover_image) {
    _prev_hover_image = _hover_image;
    _hover_texture = _cachedTextureForImage(ctx, _hover_image);
  }

  // Update pressed texture
  if (_pressed_provider) {
    auto new_image = _pressed_provider->_func();
    if (new_image != _pressed_image) {
      _pressed_image = new_image;
      if (_pressed_image) {
        _pressed_texture = _cachedTextureForImage(ctx, _pressed_image);
      }
    }
  } else if (_pressed_image && _pressed_image != _prev_pressed_image) {
    _prev_pressed_image = _pressed_image;
    _pressed_texture = _cachedTextureForImage(ctx, _pressed_image);
  }
}

///////////////////////////////////////////////////////////////////////////////
// ToolbarSeparator
///////////////////////////////////////////////////////////////////////////////

ToolbarSeparator::ToolbarSeparator(const std::string& id) {
  _id = id;
}

///////////////////////////////////////////////////////////////////////////////
// Toolbar
///////////////////////////////////////////////////////////////////////////////

Toolbar::Toolbar(const std::string& name, int x, int y, int w, int h)
    : Widget(name, x, y, w, h) {
  _tooltip_font = lev2::FontMan::fontForId("i12");
  _label_font = lev2::FontMan::fontForId("i14");
}

Toolbar::~Toolbar() {
}

toolbar_button_ptr_t Toolbar::addButton(
    const std::string& id,
    lev2::image_ptr_t icon,
    const std::string& tooltip) {
  auto btn = std::make_shared<ToolbarButton>(id);
  btn->_icon_image = icon;
  btn->_tooltip = tooltip;
  _items.push_back(btn);
  _needs_layout = true;
  return btn;
}

toolbar_button_ptr_t Toolbar::addButtonWithProvider(
    const std::string& id,
    lev2::image_provider_ptr_t icon_provider,
    const std::string& tooltip) {
  auto btn = std::make_shared<ToolbarButton>(id);
  btn->_icon_provider = icon_provider;
  btn->_tooltip = tooltip;
  _items.push_back(btn);
  _needs_layout = true;
  return btn;
}

toolbar_button_ptr_t Toolbar::addTextButton(
    const std::string& id,
    const std::string& label,
    const std::string& tooltip) {
  auto btn = std::make_shared<ToolbarButton>(id);
  btn->_label = label;
  btn->_tooltip = tooltip;
  _items.push_back(btn);
  _needs_layout = true;
  return btn;
}

toolbar_separator_ptr_t Toolbar::addSeparator(const std::string& id) {
  std::string sep_id = id.empty() ? ("sep_" + std::to_string(_separator_counter++)) : id;
  auto sep = std::make_shared<ToolbarSeparator>(sep_id);
  _items.push_back(sep);
  _needs_layout = true;
  return sep;
}

void Toolbar::removeItem(const std::string& id) {
  _items.erase(
      std::remove_if(_items.begin(), _items.end(),
                     [&id](const toolbar_item_ptr_t& item) { return item->_id == id; }),
      _items.end());
  _needs_layout = true;
}

toolbar_item_ptr_t Toolbar::getItem(const std::string& id) {
  for (auto& item : _items) {
    if (item->_id == id) {
      return item;
    }
  }
  return nullptr;
}

toolbar_button_ptr_t Toolbar::getButton(const std::string& id) {
  auto item = getItem(id);
  return std::dynamic_pointer_cast<ToolbarButton>(item);
}

void Toolbar::clear() {
  _items.clear();
  _needs_layout = true;
  _hovered_index = -1;
}

ToolbarOrientation Toolbar::getEffectiveOrientation() const {
  if (_orientation != ToolbarOrientation::Auto) {
    return _orientation;
  }
  // Auto: wider = horizontal, taller = vertical
  return (_geometry._w >= _geometry._h) ? ToolbarOrientation::Horizontal : ToolbarOrientation::Vertical;
}

void Toolbar::_doOnResized() {
  _needs_layout = true;
  DoLayout();
}

void Toolbar::DoLayout() {
  _rebuildLayout();
}

void Toolbar::_doGpuInit(lev2::Context* ctx) {
  _updateButtonTextures(ctx);
}

void Toolbar::_updateButtonTextures(lev2::Context* ctx) {
  for (auto& item : _items) {
    if (auto btn = std::dynamic_pointer_cast<ToolbarButton>(item)) {
      btn->updateTextures(ctx);
    }
  }
}

void Toolbar::_rebuildLayout() {
  bool horizontal = isHorizontal();
  int button_size = _icon_size + _button_padding * 2;

  // Vertically center items within toolbar height (horizontal mode)
  // or horizontally center items within toolbar width (vertical mode)
  int center_offset_h = std::max(0, (_geometry._h - button_size) / 2);
  int center_offset_w = std::max(0, (_geometry._w - button_size) / 2);

  int pos = _edge_padding;

  for (auto& item : _items) {
    if (!item->_visible) {
      continue;
    }

    if (auto btn = std::dynamic_pointer_cast<ToolbarButton>(item)) {
      int btn_w;
      if (btn->_custom_width > 0) {
        btn_w = btn->_custom_width + _button_padding * 2;
      } else if (!btn->_label.empty()) {
        // Compute width from label text
        int text_w = 0;
        if (_label_font) {
          text_w = btn->_label.length() * _label_font->description().miAdvanceWidth;
        }
        if (btn->_icon_image || btn->_icon_provider) {
          // icon + label: icon_size + spacing + text + padding
          btn_w = _button_padding + _icon_size + _label_padding + text_w + _label_padding;
        } else {
          // text only
          btn_w = _label_padding + text_w + _label_padding;
        }
      } else {
        btn_w = button_size;
      }
      if (horizontal) {
        btn->_x = pos;
        btn->_y = center_offset_h;
        btn->_width = btn_w;
        btn->_height = button_size;
        pos += btn_w + _item_spacing;
      } else {
        btn->_x = center_offset_w;
        btn->_y = pos;
        btn->_width = btn_w;
        btn->_height = button_size;
        pos += btn_w + _item_spacing;
      }
    } else if (auto sep = std::dynamic_pointer_cast<ToolbarSeparator>(item)) {
      if (horizontal) {
        sep->_x = pos + sep->_padding;
        sep->_y = center_offset_h;
        sep->_width = sep->_thickness;
        sep->_height = button_size;
        pos += sep->_thickness + sep->_padding * 2 + _item_spacing;
      } else {
        sep->_x = center_offset_w;
        sep->_y = pos + sep->_padding;
        sep->_width = button_size;
        sep->_height = sep->_thickness;
        pos += sep->_thickness + sep->_padding * 2 + _item_spacing;
      }
    }
  }

  _needs_layout = false;
}

int Toolbar::_getItemAt(int local_x, int local_y) const {
  for (size_t i = 0; i < _items.size(); i++) {
    const auto& item = _items[i];
    if (!item->_visible || !item->_enabled) {
      continue;
    }

    // Only buttons are interactive
    if (!std::dynamic_pointer_cast<ToolbarButton>(item)) {
      continue;
    }

    if (local_x >= item->_x && local_x < item->_x + item->_width &&
        local_y >= item->_y && local_y < item->_y + item->_height) {
      return (int)i;
    }
  }
  return -1;
}

Widget* Toolbar::doRouteUiEvent(event_constptr_t ev) {
  if (IsEventInside(ev)) {
    return this;
  }
  return nullptr;
}

HandlerResult Toolbar::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;

  int localX = 0;
  int localY = 0;
  RootToLocal(ev->miX, ev->miY, localX, localY, true);

  int item_index = _getItemAt(localX, localY);

  switch (ev->_eventcode) {
    case EventCode::PUSH: {
      if (item_index >= 0) {
        if (auto btn = std::dynamic_pointer_cast<ToolbarButton>(_items[item_index])) {
          btn->_pressed = true;
          _focused_index = item_index;
          result.setHandled(this);
        }
      }
      break;
    }

    case EventCode::KEY_DOWN:
    case EventCode::KEY_REPEAT: {
      if (_focused_index >= 0 && _focused_index < (int)_items.size()) {
        if (auto btn = std::dynamic_pointer_cast<ToolbarButton>(_items[_focused_index])) {
          if (btn->_onKeyEvent) {
            btn->_onKeyEvent(ev->miKeyCode);
            result.setHandled(this);
          }
        }
      }
      break;
    }

    case EventCode::RELEASE: {
      // Find the pressed button and release it
      for (auto& item : _items) {
        if (auto btn = std::dynamic_pointer_cast<ToolbarButton>(item)) {
          if (btn->_pressed) {
            btn->_pressed = false;

            // Check if release is still over this button
            if (item_index >= 0 && _items[item_index] == item) {
              if (btn->_toggle_mode) {
                btn->_toggled = !btn->_toggled;
                if (btn->_onToggled) {
                  btn->_onToggled(btn->_toggled);
                }
              }
              if (btn->_onPressed) {
                btn->_onPressed();
              }
            }
            result.setHandled(this);
            break;
          }
        }
      }
      break;
    }

    case EventCode::MOVE: {
      // Update hover state
      int old_hovered = _hovered_index;
      _hovered_index = item_index;

      // Clear hover on all buttons first
      for (auto& item : _items) {
        if (auto btn = std::dynamic_pointer_cast<ToolbarButton>(item)) {
          btn->_hovered = false;
        }
      }

      // Set hover on current button
      if (_hovered_index >= 0) {
        if (auto btn = std::dynamic_pointer_cast<ToolbarButton>(_items[_hovered_index])) {
          btn->_hovered = true;
        }
      }

      // Reset tooltip timer if hovered item changed
      if (_hovered_index != old_hovered) {
        if (_uicontext) {
          _hover_start_time = _uicontext->_uitimer.SecsSinceStart();
        }
        _tooltip_visible = false;
      }
      break;
    }

    case EventCode::MOUSE_LEAVE: {
      _hovered_index = -1;
      _tooltip_visible = false;
      for (auto& item : _items) {
        if (auto btn = std::dynamic_pointer_cast<ToolbarButton>(item)) {
          btn->_hovered = false;
          btn->_pressed = false;
        }
      }
      break;
    }

    default:
      break;
  }

  return result;
}

void Toolbar::DoDraw(drawevent_constptr_t drwev) {
  if (_needs_layout) {
    _rebuildLayout();
  }

  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto fxi = tgt->FXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  // Update textures
  _updateButtonTextures(tgt);

  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  int ix2 = ix1 + _geometry._w;
  int iy2 = iy1 + _geometry._h;

  mtxi->PushUIMatrix();
  {
    // Draw background
    if (_draw_background) {
      auto rs = defmtl->_rasterstate;
      rs->setBlendingMacro(lev2::BlendingMacro::OFF);
      rs->setDepthTest(lev2::EDepthTest::OFF);
      fxi->pushRasterState(rs);
      tgt->PushModColor(_bgcolor);
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
      primi->RenderQuadAtZ(defmtl.get(), ix1, ix2, iy1, iy2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      tgt->PopModColor();
      fxi->popRasterState();
    }

    // Draw items
    for (auto& item : _items) {
      if (!item->_visible) {
        continue;
      }

      int abs_x = ix1 + item->_x;
      int abs_y = iy1 + item->_y;

      if (auto btn = std::dynamic_pointer_cast<ToolbarButton>(item)) {
        _drawButton(drwev, btn, abs_x, abs_y);
      } else if (auto sep = std::dynamic_pointer_cast<ToolbarSeparator>(item)) {
        _drawSeparator(drwev, sep, abs_x, abs_y);
      }
    }

    // Draw tooltip
    if (_show_tooltips) {
      _drawTooltip(drwev);
    }
  }
  mtxi->PopUIMatrix();
}

void Toolbar::_drawButton(drawevent_constptr_t drwev, toolbar_button_ptr_t btn, int abs_x, int abs_y) {
  auto tgt = drwev->GetTarget();
  auto fxi = tgt->FXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int bx1 = abs_x;
  int by1 = abs_y;
  int bx2 = abs_x + btn->_width;
  int by2 = abs_y + btn->_height;

  // Draw button background (hover/pressed/toggled state)
  fvec4 bg_color;
  bool draw_bg = false;

  if (btn->_pressed) {
    bg_color = _button_pressed_color;
    draw_bg = true;
  } else if (btn->_toggle_mode && btn->_toggled) {
    bg_color = _button_toggled_color;
    draw_bg = true;
  } else if (btn->_hovered) {
    bg_color = _button_hover_color;
    draw_bg = true;
  }

  if (draw_bg && bg_color.w > 0.001f) {
    auto rs = defmtl->_rasterstate;
    rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    rs->setDepthTest(lev2::EDepthTest::OFF);
    fxi->pushRasterState(rs);
    tgt->PushModColor(bg_color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    primi->RenderQuadAtZ(defmtl.get(), bx1, bx2, by1, by2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    tgt->PopModColor();
    fxi->popRasterState();
  }

  // Draw icon
  lev2::texture_ptr_t tex = nullptr;
  if (btn->_pressed && btn->_pressed_texture) {
    tex = btn->_pressed_texture;
  } else if (btn->_hovered && btn->_hover_texture) {
    tex = btn->_hover_texture;
  } else {
    tex = btn->_icon_texture;
  }

  int icon_right = bx1 + _button_padding;  // track where icon ends for label positioning

  if (tex) {
    auto texmtl = lev2::defaultUITextureMaterial();

    int icon_x1 = bx1 + _button_padding;
    int icon_y1 = by1 + _button_padding;
    int icon_w = (btn->_custom_width > 0) ? btn->_custom_width : _icon_size;
    int icon_x2 = icon_x1 + icon_w;
    int icon_y2 = icon_y1 + _icon_size;

    fvec4 tint = btn->_enabled ? fvec4(1, 1, 1, 1) : _disabled_tint;

    tgt->PushModColor(tint);
    texmtl->SetTexture(lev2::ETEXDEST_DIFFUSE, tex.get());

    auto rs = texmtl->_rasterstate;
    rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    rs->setDepthTest(lev2::EDepthTest::OFF);
    int prev_pri = rs->_priority;
    rs->_priority = 1 << 16;
    fxi->pushRasterState(rs);
    primi->RenderQuadAtZ(texmtl.get(), icon_x1, icon_x2, icon_y1, icon_y2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    fxi->popRasterState();
    rs->_priority = prev_pri;

    tgt->PopModColor();
    texmtl->SetTexture(lev2::ETEXDEST_DIFFUSE, nullptr);

    icon_right = icon_x2;
  }

  // Draw text label
  if (!btn->_label.empty() && _label_font) {
    int text_x = tex ? (icon_right + _label_padding) : (bx1 + _label_padding);
    int text_y = by1 + _button_padding;

    fvec4 text_color = (btn->_toggle_mode && btn->_toggled) ? _label_toggled_color : _label_color;
    if (!btn->_enabled) {
      text_color = _disabled_tint;
    }

    lev2::FontMan::PushFont(_label_font);
    tgt->PushModColor(text_color);
    lev2::FontMan::beginTextBlock(tgt, btn->_label.length());
    lev2::FontMan::DrawText(tgt, text_x, text_y, btn->_label.c_str());
    lev2::FontMan::endTextBlock(tgt);
    tgt->PopModColor();
    lev2::FontMan::PopFont();
  }
}

void Toolbar::_drawSeparator(drawevent_constptr_t drwev, toolbar_separator_ptr_t sep, int abs_x, int abs_y) {
  auto tgt = drwev->GetTarget();
  auto fxi = tgt->FXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  auto rs = defmtl->_rasterstate;
  rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
  rs->setDepthTest(lev2::EDepthTest::OFF);
  fxi->pushRasterState(rs);
  tgt->PushModColor(_separator_color);
  defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
  primi->RenderQuadAtZ(defmtl.get(),
                       abs_x, abs_x + sep->_width,
                       abs_y, abs_y + sep->_height,
                       0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
  tgt->PopModColor();
  fxi->popRasterState();
}

void Toolbar::_drawTooltip(drawevent_constptr_t drwev) {
  if (_hovered_index < 0) {
    return;
  }

  auto btn = std::dynamic_pointer_cast<ToolbarButton>(_items[_hovered_index]);
  if (!btn || btn->_tooltip.empty()) {
    return;
  }

  // Check if enough time has passed
  if (!_uicontext) {
    return;
  }
  double current_time = _uicontext->_uitimer.SecsSinceStart();
  double elapsed_ms = (current_time - _hover_start_time) * 1000.0;
  if (elapsed_ms < _tooltip_delay_ms) {
    return;
  }

  _tooltip_visible = true;

  auto tgt = drwev->GetTarget();
  auto fxi = tgt->FXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  if (!_tooltip_font) {
    return;
  }

  // Calculate tooltip position (below button)
  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  int abs_x = ix1 + btn->_x;
  int abs_y = iy1 + btn->_y + btn->_height + 4;

  // Measure text
  int text_width = btn->_tooltip.length() * _tooltip_font->description().miAdvanceWidth;
  int text_height = _tooltip_font->description().miAdvanceHeight;
  int padding = 4;

  int tip_x1 = abs_x;
  int tip_y1 = abs_y;
  int tip_x2 = tip_x1 + text_width + padding * 2;
  int tip_y2 = tip_y1 + text_height + padding * 2;

  // Draw tooltip background
  auto rs = defmtl->_rasterstate;
  rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
  rs->setDepthTest(lev2::EDepthTest::OFF);
  fxi->pushRasterState(rs);
  tgt->PushModColor(fvec4(0.1f, 0.1f, 0.1f, 0.9f));
  defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
  primi->RenderQuadAtZ(defmtl.get(), tip_x1, tip_x2, tip_y1, tip_y2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
  tgt->PopModColor();
  fxi->popRasterState();

  // Draw tooltip text
  lev2::FontMan::PushFont(_tooltip_font);
  tgt->PushModColor(fvec4(1.0f, 1.0f, 1.0f, 1.0f));
  lev2::FontMan::beginTextBlock(tgt, btn->_tooltip.length());
  lev2::FontMan::DrawText(tgt, tip_x1 + padding, tip_y1 + padding, btn->_tooltip.c_str());
  lev2::FontMan::endTextBlock(tgt);
  tgt->PopModColor();
  lev2::FontMan::PopFont();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
