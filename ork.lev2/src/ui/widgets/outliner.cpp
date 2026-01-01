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

        // Draw item text
        lev2::FontMan::DrawText(tgt, text_x, text_y, item.display_name.c_str());

        y_pos += _item_height;
      }

      lev2::FontMan::endTextBlock(tgt);
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
