#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gbi.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/ui/outliner.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/style.h>
#include <ork/lev2/ui/context.h>

namespace ork::ui {

static constexpr float PI = 3.14159265359f;

/////////////////////////////////////////////////////////////////////////
Outliner::Outliner(const std::string& name, int x, int y, int w, int h)
    : Widget(name, x, y, w, h) {
  _font = lev2::FontMan::fontForId("i14");
  // Create default empty model
  _model = std::make_shared<VarMapModel>();
  _subscribeToModel();
}

/////////////////////////////////////////////////////////////////////////
Outliner::~Outliner() {
}

/////////////////////////////////////////////////////////////////////////
void Outliner::_subscribeToModel() {
  if (_model) {
    _model->_onItemAdded = [this](const std::string& key) {
      _needs_rebuild = true;
    };
    _model->_onItemRemoved = [this](const std::string& key) {
      _needs_rebuild = true;
      // Clear selection if removed item was selected
      if (_selected_key == key || _selected_key.find(key + "/") == 0) {
        _selected_key = "";
      }
    };
    _model->_onItemChanged = [this](const std::string& key) {
      _needs_rebuild = true;
    };
    _model->_onModelReset = [this]() {
      _needs_rebuild = true;
      _expanded_keys.clear();
      _selected_key = "";
    };
  }
}

/////////////////////////////////////////////////////////////////////////
void Outliner::setModel(outliner_model_ptr_t model) {
  _model = model;
  if (!_model) {
    _model = std::make_shared<VarMapModel>();
  }
  _subscribeToModel();
  _needs_rebuild = true;
}

/////////////////////////////////////////////////////////////////////////
void Outliner::setData(varmap::varmap_ptr_t data) {
  auto varmap_model = std::make_shared<VarMapModel>(data);
  setModel(varmap_model);
}

/////////////////////////////////////////////////////////////////////////
varmap::varmap_ptr_t Outliner::getData() const {
  if (auto varmap_model = std::dynamic_pointer_cast<VarMapModel>(_model)) {
    return varmap_model->getData();
  }
  return nullptr;
}

/////////////////////////////////////////////////////////////////////////
void Outliner::setSelectedKey(const std::string& key) {
  if (_selected_key != key) {
    _selected_key = key;
    if (_onSelect) {
      _onSelect(key);
    }
  }
}

/////////////////////////////////////////////////////////////////////////
void Outliner::setExpanded(const std::string& key, bool expanded) {
  if (expanded) {
    _expanded_keys.insert(key);
  } else {
    _expanded_keys.erase(key);
  }
  _needs_rebuild = true;
}

/////////////////////////////////////////////////////////////////////////
bool Outliner::isExpanded(const std::string& key) const {
  return _expanded_keys.find(key) != _expanded_keys.end();
}

/////////////////////////////////////////////////////////////////////////
void Outliner::expandAll() {
  // Recursively add all keys that have children
  if (_model) {
    std::function<void(const std::string&)> expand_recursive;
    expand_recursive = [&](const std::string& parent_key) {
      auto children = _model->getChildren(parent_key);
      for (const auto& child_key : children) {
        if (_model->hasChildren(child_key)) {
          _expanded_keys.insert(child_key);
          expand_recursive(child_key);
        }
      }
    };
    expand_recursive("");
  }
  _needs_rebuild = true;
}

/////////////////////////////////////////////////////////////////////////
void Outliner::collapseAll() {
  _expanded_keys.clear();
  _needs_rebuild = true;
}

/////////////////////////////////////////////////////////////////////////
void Outliner::startEditing(const std::string& key) {
  if (key.empty()) return;

  // Check if the model allows renaming
  if (_model && !_model->allowRename()) {
    return;
  }

  // Find the item to get its display name
  for (const auto& item : _visible_items) {
    if (item.key == key) {
      _editing_key = key;
      _edit_value = item.display_name;
      _original_value = item.display_name;
      _cursor_pos = _edit_value.length();
      return;
    }
  }
}

/////////////////////////////////////////////////////////////////////////
void Outliner::cancelEditing() {
  _editing_key = "";
  _edit_value = "";
  _original_value = "";
  _cursor_pos = 0;
}

/////////////////////////////////////////////////////////////////////////
void Outliner::commitEditing() {
  if (_editing_key.empty()) return;

  std::string old_key = _editing_key;
  std::string new_name = _edit_value;

  // Only process if name actually changed
  if (new_name != _original_value && !new_name.empty()) {
    std::string new_key;

    // Try to rename via model first
    if (_model) {
      new_key = _model->renameItem(old_key, new_name);
    }

    // If model didn't handle it (or no model), calculate new key for callback
    if (new_key.empty()) {
      size_t last_slash = old_key.rfind('/');
      if (last_slash != std::string::npos) {
        new_key = old_key.substr(0, last_slash + 1) + new_name;
      } else {
        new_key = new_name;
      }
    }

    // Update expanded keys: replace old_key prefix with new_key prefix
    std::unordered_set<std::string> updated_expanded;
    for (const auto& expanded_key : _expanded_keys) {
      if (expanded_key == old_key) {
        updated_expanded.insert(new_key);
      } else if (expanded_key.find(old_key + "/") == 0) {
        // Child of renamed item - update prefix
        updated_expanded.insert(new_key + expanded_key.substr(old_key.length()));
      } else {
        updated_expanded.insert(expanded_key);
      }
    }
    _expanded_keys = std::move(updated_expanded);

    // Update selection if needed
    if (_selected_key == old_key) {
      _selected_key = new_key;
    } else if (_selected_key.find(old_key + "/") == 0) {
      _selected_key = new_key + _selected_key.substr(old_key.length());
    }

    // Call the rename callback (for additional handling)
    if (_onRename) {
      _onRename(old_key, new_name);
    }
  }

  cancelEditing();

  // Rebuild to reflect any changes made
  _needs_rebuild = true;
}

/////////////////////////////////////////////////////////////////////////
void Outliner::_doOnResized() {
  DoLayout();
}

/////////////////////////////////////////////////////////////////////////
void Outliner::DoLayout() {
  _rebuildVisibleItems();
}

/////////////////////////////////////////////////////////////////////////
void Outliner::_rebuildVisibleItems() {
  _visible_items.clear();
  if (_model) {
    _addItemsRecursive("", 0);
  }
  _needs_rebuild = false;
  _clampScrollOffset();
}

/////////////////////////////////////////////////////////////////////////
void Outliner::_clampScrollOffset() {
  int content_height = _visible_items.size() * _item_height;
  int max_scroll = std::max(0, content_height - _geometry._h);
  _scroll_offset = std::clamp(_scroll_offset, 0, max_scroll);
}

/////////////////////////////////////////////////////////////////////////
void Outliner::_addItemsRecursive(const std::string& parent_key, int depth) {
  if (!_model) return;

  auto children = _model->getChildren(parent_key);

  for (const auto& child_key : children) {
    VisibleItem item;
    item.key = child_key;
    item.display_name = _model->getDisplayName(child_key);
    item.depth = depth;
    item.has_children = _model->hasChildren(child_key);
    item.is_expanded = isExpanded(child_key);

    _visible_items.push_back(item);

    // Recurse if expanded
    if (item.has_children && item.is_expanded) {
      _addItemsRecursive(child_key, depth + 1);
    }
  }
}

/////////////////////////////////////////////////////////////////////////
int Outliner::_getItemIndexAt(int local_y) const {
  int adjusted_y = local_y + _scroll_offset;
  int index = adjusted_y / _item_height;
  if (index >= 0 && index < (int)_visible_items.size()) {
    return index;
  }
  return -1;
}

/////////////////////////////////////////////////////////////////////////
std::string Outliner::_getItemKeyAt(int local_y) const {
  int index = _getItemIndexAt(local_y);
  if (index >= 0) {
    return _visible_items[index].key;
  }
  return "";
}

/////////////////////////////////////////////////////////////////////////
Widget* Outliner::doRouteUiEvent(event_constptr_t ev) {
  if (IsEventInside(ev)) {
    return this;
  }
  return nullptr;
}

/////////////////////////////////////////////////////////////////////////
HandlerResult Outliner::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;

  int localX = 0;
  int localY = 0;
  RootToLocal(ev->miX, ev->miY, localX, localY);

  // Handle keyboard input when editing
  if (isEditing()) {
    switch (ev->_eventcode) {
      case EventCode::KEY_DOWN:
      case EventCode::KEY_REPEAT: {
        int key = ev->miKeyCode;
        switch (key) {
          case 256: // ESC - cancel editing
            cancelEditing();
            break;
          case 257: // Enter - commit editing
            commitEditing();
            result._widget_finished = true;
            break;
          case 259: // Backspace
            if (_cursor_pos > 0) {
              _edit_value.erase(_cursor_pos - 1, 1);
              _cursor_pos--;
            }
            break;
          case 261: // Delete
            if (_cursor_pos < (int)_edit_value.length()) {
              _edit_value.erase(_cursor_pos, 1);
            }
            break;
          case 263: // Left arrow
            if (_cursor_pos > 0) _cursor_pos--;
            break;
          case 262: // Right arrow
            if (_cursor_pos < (int)_edit_value.length()) _cursor_pos++;
            break;
          case 268: // Home
            _cursor_pos = 0;
            break;
          case 269: // End
            _cursor_pos = _edit_value.length();
            break;
          default:
            // Printable characters
            if (key >= 32 && key <= 126) {
              char ch = ev->mbSHIFT ? char(key) : std::tolower(key);
              _edit_value.insert(_cursor_pos, 1, ch);
              _cursor_pos++;
            }
            break;
        }
        result.setHandled(this);
        return result;
      }
      case EventCode::PASTE_TEXT: {
        _edit_value.insert(_cursor_pos, ev->_paste_text);
        _cursor_pos += ev->_paste_text.length();
        result.setHandled(this);
        return result;
      }
      case EventCode::PUSH: {
        // Click outside the editing item cancels editing
        std::string clicked_key = _getItemKeyAt(localY);
        if (clicked_key != _editing_key) {
          commitEditing();
        }
        result.setHandled(this);
        return result;
      }
      default:
        break;
    }
  }

  switch (ev->_eventcode) {
    case EventCode::PUSH: {
      std::string clicked_key = _getItemKeyAt(localY);
      if (!clicked_key.empty()) {
        // Check if click is on expand/collapse arrow area
        int index = _getItemIndexAt(localY);
        if (index >= 0) {
          const auto& item = _visible_items[index];
          int arrow_x = item.depth * _indent_width;

          if (item.has_children && localX >= arrow_x && localX < arrow_x + _indent_width) {
            // Toggle expand/collapse
            setExpanded(item.key, !item.is_expanded);
            _rebuildVisibleItems();
          } else {
            // Select item
            setSelectedKey(clicked_key);
          }
          result.setHandled(this);
        }
      }
      break;
    }

    case EventCode::DOUBLECLICK: {
      // Shift+Double-click to start editing
      if (ev->mbSHIFT) {
        std::string clicked_key = _getItemKeyAt(localY);
        if (!clicked_key.empty()) {
          int index = _getItemIndexAt(localY);
          if (index >= 0) {
            const auto& item = _visible_items[index];
            int arrow_x = item.depth * _indent_width;
            // Don't start editing if clicking on disclosure triangle
            if (!(item.has_children && localX >= arrow_x && localX < arrow_x + _indent_width)) {
              startEditing(clicked_key);
            }
          }
          result.setHandled(this);
        }
      }
      break;
    }

    case EventCode::KEY_DOWN: {
      int key = ev->miKeyCode;
      printf("Outliner::DoOnUiEvent key<%d>\n", key);
      // F2 to start editing selected item
      if (key == 291 && !_selected_key.empty()) { // F2 = 291
        startEditing(_selected_key);
        result.setHandled(this);
      }
      // Shift+Delete to delete selected item
      else if (key == 259 && ev->mbSHIFT && !_selected_key.empty()) { // Delete = 261
        if (_model && _model->allowDelete()) {
          std::string key_to_delete = _selected_key;

          // Clear selection before delete
          _selected_key = "";

          // Remove from model
          _model->removeItem(key_to_delete);

          // Call callback
          if (_onDelete) {
            _onDelete(key_to_delete);
          }

          // Remove from expanded keys
          _expanded_keys.erase(key_to_delete);
          // Also remove any children from expanded keys
          std::string prefix = key_to_delete + "/";
          for (auto it = _expanded_keys.begin(); it != _expanded_keys.end(); ) {
            if (it->find(prefix) == 0) {
              it = _expanded_keys.erase(it);
            } else {
              ++it;
            }
          }

          _needs_rebuild = true;
          result.setHandled(this);
        }
      }
      break;
    }

    case EventCode::MOVE: {
      std::string hovered_key = _getItemKeyAt(localY);
      if (_hovered_key != hovered_key) {
        _hovered_key = hovered_key;
        // Could trigger redraw here
      }
      break;
    }

    case EventCode::MOUSE_LEAVE: {
      _hovered_key = "";
      break;
    }

    case EventCode::MOUSEWHEEL: {
      // Scroll by wheel delta (negative = scroll down, positive = scroll up)
      _scroll_offset -= ev->miMWY * 3; // multiply for faster scrolling
      _clampScrollOffset();
      result.setHandled(this);
      break;
    }

    default:
      break;
  }

  return result;
}

/////////////////////////////////////////////////////////////////////////
void Outliner::DoDraw(drawevent_constptr_t drwev) {
  if (_needs_rebuild) {
    _rebuildVisibleItems();
  }

  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto fbi = tgt->FBI();
  auto fxi = tgt->FXI();
  auto gbi = tgt->GBI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  // Get absolute position
  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  int ix2 = ix1 + _geometry._w;
  int iy2 = iy1 + _geometry._h;

  // Setup scissor
  fbi->pushScissor(lev2::ViewportRect(ix1, iy1, _geometry._w, _geometry._h));

  mtxi->PushUIMatrix();
  {
    // Draw background
    if (_draw_background) {
      auto rs = defmtl->_rasterstate;
      auto omacro = rs->_blendingMacro;
      auto omode = defmtl->meUIColorMode;
      rs->setBlendingMacro(lev2::BlendingMacro::OFF);
      rs->setDepthTest(lev2::EDepthTest::OFF);
      tgt->PushModColor(_bgcolor);
      int prev_pri = rs->_priority;
      rs->_priority = 1<<16;
      fxi->pushRasterState(rs);
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
      primi->RenderQuadAtZ(defmtl.get(), ix1, ix2, iy1, iy2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      fxi->popRasterState();
      rs->_priority = prev_pri;
      rs->_blendingMacro = omacro;
      defmtl->meUIColorMode = omode;
      tgt->PopModColor();
    }

    // Count visible items and characters for allocation
    size_t visible_chars = 0;
    int num_triangles = 0;
    int y_pos = -_scroll_offset;
    for (const auto& item : _visible_items) {
      if (y_pos + _item_height >= 0 && y_pos < _geometry._h) {
        visible_chars += item.display_name.length();
        if (item.has_children) {
          num_triangles++;
        }
      }
      y_pos += _item_height;
    }

    // Draw selection/hover backgrounds and text
    if (_font && visible_chars > 0) {
      lev2::FontMan::PushFont(_font);
      tgt->PushModColor(_text_color);
      lev2::FontMan::beginTextBlock(tgt, visible_chars);

      y_pos = -_scroll_offset;
      for (const auto& item : _visible_items) {
        if (y_pos + _item_height < 0) {
          y_pos += _item_height;
          continue;
        }
        if (y_pos >= _geometry._h) {
          break;
        }

        int item_abs_x, item_abs_y;
        LocalToRoot(0, y_pos, item_abs_x, item_abs_y);

        // Draw alternating row background
        int item_index = &item - &_visible_items[0];
        if (item_index % 2 == 1) {
          fvec4 alt_color = _bgcolor * 0.85f; // slightly darker
          alt_color.w = 1.0f; // keep full alpha
          auto rs = defmtl->_rasterstate;
          auto omacro = rs->_blendingMacro;
          auto omode = defmtl->meUIColorMode;
          rs->setBlendingMacro(lev2::BlendingMacro::OFF);
          rs->setDepthTest(lev2::EDepthTest::OFF);
          fxi->pushRasterState(rs);
          tgt->PushModColor(alt_color);
          defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
          primi->RenderQuadAtZ(defmtl.get(),
                               item_abs_x, item_abs_x + _geometry._w,
                               item_abs_y, item_abs_y + _item_height,
                               0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
          tgt->PopModColor();
          fxi->popRasterState();
          rs->_blendingMacro = omacro;
          defmtl->meUIColorMode = omode;
        }

        // Draw selection/hover background
        if (item.key == _selected_key || item.key == _hovered_key) {
          fvec4 bg_color = (item.key == _selected_key) ? _selected_color : _hover_color;
          auto rs = defmtl->_rasterstate;
          auto omacro = rs->_blendingMacro;
          auto omode = defmtl->meUIColorMode;
          rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
          rs->setDepthTest(lev2::EDepthTest::OFF);
          fxi->pushRasterState(rs);
          tgt->PushModColor(bg_color);
          defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
          primi->RenderQuadAtZ(defmtl.get(),
                               item_abs_x, item_abs_x + _geometry._w,
                               item_abs_y, item_abs_y + _item_height,
                               0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
          tgt->PopModColor();
          fxi->popRasterState();
          rs->_blendingMacro = omacro;
          defmtl->meUIColorMode = omode;
        }

        // Calculate text position with indentation
        int text_x = ix1 + (item.depth + 1) * _indent_width;
        int text_y = item_abs_y + (_item_height - _font->description().miAdvanceHeight) / 2;

        // Check if this item is being edited
        if (item.key == _editing_key) {
          // End text block temporarily to draw edit box
          lev2::FontMan::endTextBlock(tgt);
          tgt->PopModColor();

          // Draw edit box background
          int edit_x1 = text_x - 2;
          int edit_x2 = ix2 - 4;
          int edit_y1 = item_abs_y + 2;
          int edit_y2 = item_abs_y + _item_height - 2;

          auto rs = defmtl->_rasterstate;
          auto omacro = rs->_blendingMacro;
          auto omode = defmtl->meUIColorMode;
          rs->setBlendingMacro(lev2::BlendingMacro::OFF);
          rs->setDepthTest(lev2::EDepthTest::OFF);
          fxi->pushRasterState(rs);

          // Draw highlight border
          tgt->PushModColor(fvec4(0.4f, 0.6f, 1.0f, 1.0f));
          defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
          primi->RenderQuadAtZ(defmtl.get(), edit_x1 - 1, edit_x2 + 1, edit_y1 - 1, edit_y2 + 1,
                               0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
          tgt->PopModColor();

          // Draw inner background
          tgt->PushModColor(fvec4(0.05f, 0.05f, 0.1f, 1.0f));
          primi->RenderQuadAtZ(defmtl.get(), edit_x1, edit_x2, edit_y1, edit_y2,
                               0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
          tgt->PopModColor();

          fxi->popRasterState();
          rs->_blendingMacro = omacro;
          defmtl->meUIColorMode = omode;

          // Draw edit text
          tgt->PushModColor(_text_color);
          lev2::FontMan::beginTextBlock(tgt, _edit_value.length() + 1);
          lev2::FontMan::DrawText(tgt, text_x, text_y, _edit_value.c_str());
          lev2::FontMan::endTextBlock(tgt);

          // Draw cursor
          int cursor_x = text_x;
          if (_cursor_pos > 0) {
            // Measure text width up to cursor
            auto& desc = _font->description();
            for (int i = 0; i < _cursor_pos && i < (int)_edit_value.length(); i++) {
              cursor_x += desc.miAdvanceWidth; // approximate
            }
          }
          rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
          fxi->pushRasterState(rs);
          tgt->PushModColor(fvec4(1.0f, 1.0f, 1.0f, 0.8f));
          defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
          primi->RenderQuadAtZ(defmtl.get(), cursor_x, cursor_x + 2, edit_y1 + 2, edit_y2 - 2,
                               0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
          tgt->PopModColor();
          fxi->popRasterState();
          rs->_blendingMacro = omacro;

          tgt->PopModColor();

          // Restart text block for remaining items
          tgt->PushModColor(_text_color);
          size_t remaining_chars = 0;
          int temp_y = y_pos + _item_height;
          for (size_t j = item_index + 1; j < _visible_items.size(); j++) {
            if (temp_y + _item_height >= 0 && temp_y < _geometry._h) {
              remaining_chars += _visible_items[j].display_name.length();
            }
            temp_y += _item_height;
          }
          if (remaining_chars > 0) {
            lev2::FontMan::beginTextBlock(tgt, remaining_chars);
          }
        } else {
          // Draw normal item text
          lev2::FontMan::DrawText(tgt, text_x, text_y, item.display_name.c_str());
        }

        y_pos += _item_height;
      }

      if (!isEditing() || _visible_items.back().key != _editing_key) {
        lev2::FontMan::endTextBlock(tgt);
      }
      tgt->PopModColor();
      lev2::FontMan::PopFont();
    }

    // Draw disclosure triangles using SDF
    if (num_triangles > 0 && _uicontext && _uicontext->_theme_engine) {
      auto theme = _uicontext->_theme_engine;

      // Create a style for disclosure triangles
      Style tri_style;
      tri_style._bg_color = fvec4(1.0f, 1.0f, 1.0f, 1.0f); // white fill
      tri_style._border_color = fvec4(1.0f, 1.0f, 1.0f, 0.0f); // no border
      tri_style._corner_radius = 0;
      tri_style._border_width = 0;
      tri_style._blend_mode = lev2::BlendingMacro::ALPHA;

      const int tri_size = 12; // triangle size in pixels

      y_pos = -_scroll_offset;
      for (const auto& item : _visible_items) {
        if (y_pos + _item_height < 0) {
          y_pos += _item_height;
          continue;
        }
        if (y_pos >= _geometry._h) {
          break;
        }

        if (item.has_children) {
          int item_abs_x, item_abs_y;
          LocalToRoot(0, y_pos, item_abs_x, item_abs_y);

          // Position of disclosure triangle
          int tri_x = ix1 + item.depth * _indent_width + (_indent_width - tri_size) / 2;
          int tri_y = item_abs_y + (_item_height - tri_size) / 2;

          // Rotation: 0 = point down, PI/2 = point right
          float rotation = item.is_expanded ? 0.0f : PI / 2.0f;

          theme->drawTriangle(tri_x, tri_y, tri_size, tri_size, drwev, &tri_style, rotation);
        }
        y_pos += _item_height;
      }
    }
  }
  mtxi->PopUIMatrix();

  fbi->popScissor();
}

/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
