#include <ork/pch.h>
#include <algorithm>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/ui/dropdown_menu.h>
#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/style.h>
#include <ork/util/crc.h>

namespace ork::ui {
///////////////////////////////////////////////////////////////////////////////
static constexpr const char* FONTNAME = "i14";
///////////////////////////////////////////////////////////////////////////////
// DropdownMenu::Content
///////////////////////////////////////////////////////////////////////////////
DropdownMenu::Content::Content(DropdownMenu* owner)
    : Widget("ddcontent", 0, 0, 0, 0)
    , _owner(owner) {
}
///////////////////////////////////////////////////////////////////////////////
int DropdownMenu::Content::desiredHeight() const {
  return _owner->_items.size() * DropdownMenu::ITEM_HEIGHT;
}
///////////////////////////////////////////////////////////////////////////////
void DropdownMenu::Content::DoDraw(drawevent_constptr_t drwev) {
  int num_items = _owner->_items.size();
  if (num_items == 0) return;

  auto tgt = drwev->GetTarget();
  auto fbi = tgt->FBI();
  auto mtxi = tgt->MTXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  mtxi->PushUIMatrix();
  {
    int ix1, iy1;
    LocalToRoot(0, 0, ix1, iy1);
    int ix2 = ix1 + _geometry._w;
    int iy2 = iy1 + _geometry._h;

    ///////////////////////////////////////////////////////////////////////
    // draw background
    ///////////////////////////////////////////////////////////////////////

    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);

    fvec4 bg_color(0.12f, 0.12f, 0.16f, 0.95f);
    tgt->PushModColor(bg_color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    primi->RenderQuadAtZ(
        defmtl.get(),
        ix1, ix2, iy1, iy2,
        0.0f,
        0.0f, 1.0f, 0.0f, 1.0f);
    tgt->PopModColor();

    ///////////////////////////////////////////////////////////////////////
    // draw highlight for hovered item
    ///////////////////////////////////////////////////////////////////////

    if (_owner->_hover_index >= 0 && _owner->_hover_index < num_items) {
      int hy1 = iy1 + _owner->_hover_index * ITEM_HEIGHT;
      int hy2 = hy1 + ITEM_HEIGHT;
      tgt->PushModColor(_owner->_hl_color);
      primi->RenderQuadAtZ(
          defmtl.get(),
          ix1 + 1, ix2 - 1, hy1, hy2,
          0.0f,
          0.0f, 1.0f, 0.0f, 1.0f);
      tgt->PopModColor();
    }

    ///////////////////////////////////////////////////////////////////////
    // draw item labels and arrows
    ///////////////////////////////////////////////////////////////////////

    fvec4 fg_color(0.85f, 0.85f, 0.9f, 1.0f);
    tgt->PushModColor(fg_color);
    auto font = ork::lev2::FontMan::PushFont(FONTNAME);
    auto& fontdesc = font->description();
    int font_h = fontdesc.miCharHeight;

    int max_chars = 0;
    for (auto& item : _owner->_items) {
      max_chars = std::max(max_chars, int(item._label.length()));
    }
    max_chars += 4;

    lev2::FontMan::beginTextBlock(tgt, num_items * (max_chars + 4));

    for (int i = 0; i < num_items; i++) {
      auto& item = _owner->_items[i];
      int item_y = iy1 + i * ITEM_HEIGHT;
      int text_y = item_y + (ITEM_HEIGHT - font_h) / 2;

      lev2::FontMan::DrawText(tgt, ix1 + PADDING_X, text_y, item._label.c_str());

    }

    lev2::FontMan::endTextBlock(tgt);
    ork::lev2::FontMan::PopFont();
    tgt->PopModColor();

    // Draw submenu arrow icons (after text block)
    if (_uicontext && _uicontext->_theme_engine) {
      auto style = _uicontext->_theme_engine->_styledb->getStyle("box"_crcu);
      if (style && style->_icon_dropdown_right) {
        const int icon_size = 10;
        for (int i = 0; i < num_items; i++) {
          auto& item = _owner->_items[i];
          if (!item._is_leaf) {
            int item_y = iy1 + i * ITEM_HEIGHT;
            int icon_x = ix2 - ARROW_WIDTH;
            int icon_y = item_y + (ITEM_HEIGHT - icon_size) / 2;
            _uicontext->_theme_engine->drawIcon(icon_x, icon_y, icon_size, icon_size, drwev, style->_icon_dropdown_right);
          }
        }
      }
    }
  }
  mtxi->PopUIMatrix();
}
///////////////////////////////////////////////////////////////////////////////
// DropdownMenu
///////////////////////////////////////////////////////////////////////////////
DropdownMenu::DropdownMenu(const std::string& name, slashnode_constptr_t node)
    : Group(name, 0, 0, 0, 0)
    , _node(node) {

  _buildItems();

  _content = std::make_shared<Content>(this);
  _scroll_container = std::make_shared<ScrollContainer>("ddscroll");
  _scroll_container->setChild(_content);
  _scroll_container->setScrollMode(ScrollMode::Y);
  _scroll_container->_draw_background = false;
  _scroll_container->_vscroller._fade_delay = 999999.0f; // always visible while dropdown is open
  addChild(_scroll_container);
}
///////////////////////////////////////////////////////////////////////////////
void DropdownMenu::_buildItems() {
  _items.clear();
  if (!_node) return;

  auto& child_map = _node->children();

  if (!_item_order.empty()) {
    // Use caller-specified order
    for (auto& key : _item_order) {
      auto it = child_map.find(key);
      if (it == child_map.end()) continue;
      auto& child_node = it->second;
      MenuItem item;
      item._label = key;
      item._node = child_node;
      item._is_leaf = child_node->isLeaf();
      if (item._is_leaf) {
        item._value = child_node->pathAsString();
      }
      _items.push_back(item);
    }
  } else {
    // Default: map iteration order (alphabetical)
    for (auto& [child_name, child_node] : child_map) {
      MenuItem item;
      item._label = child_name;
      item._node = child_node;
      item._is_leaf = child_node->isLeaf();
      if (item._is_leaf) {
        item._value = child_node->pathAsString();
      }
      _items.push_back(item);
    }
  }

  if (_sort_alphabetically) {
    std::sort(_items.begin(), _items.end(), [](const MenuItem& a, const MenuItem& b) {
      return a._label < b._label;
    });
  }
}
///////////////////////////////////////////////////////////////////////////////
void DropdownMenu::_doOnPreDestroy() {
  if (_uicontext) {
    _uicontext->unsubscribeFromTicks(this);
  }
  Group::_doOnPreDestroy();
}
///////////////////////////////////////////////////////////////////////////////
void DropdownMenu::DoLayout() {
  _scroll_container->SetRect(0, 0, width(), height());
}
///////////////////////////////////////////////////////////////////////////////
fvec2 DropdownMenu::computeSize() const {
  auto font = lev2::FontMan::PushFont(FONTNAME);
  int max_label_w = 0;
  for (auto& item : _items) {
    int sw = font->stringWidth(item._label.length());
    max_label_w = std::max(max_label_w, sw);
  }
  lev2::FontMan::PopFont();
  int w = max_label_w + PADDING_X * 2 + ARROW_WIDTH;
  int num_visible = std::min(int(_items.size()), MAX_VISIBLE);
  int h = num_visible * ITEM_HEIGHT;
  return fvec2(w, h);
}
///////////////////////////////////////////////////////////////////////////////
slashtree_ptr_t DropdownMenu::buildTreeFromPaths(const std::vector<std::string>& paths) {
  auto tree = std::make_shared<SlashTree>();
  for (auto& path : paths) {
    tree->addNode(path.c_str(), nullptr);
  }
  return tree;
}
///////////////////////////////////////////////////////////////////////////////
void DropdownMenu::_openSubmenu(int index) {
  if (index < 0 || index >= int(_items.size())) return;
  if (!_uicontext) return;
  auto& item = _items[index];
  if (item._is_leaf) return;

  // Close existing submenu if different
  if (_submenu_open_index == index) return;
  _closeSubmenu();

  // Create child dropdown menu
  auto child_menu = std::make_shared<DropdownMenu>(_name + "/" + item._label, item._node);
  child_menu->_sort_alphabetically = _sort_alphabetically;
  child_menu->_onSelected = _onSelected; // pass through selection callback

  // Position: to the right of this menu, aligned with the hovered row
  int rx, ry;
  LocalToRoot(0, 0, rx, ry);
  int sub_x = rx + width();
  int inverted_scroll = _scroll_container->maxScrollY() - _scroll_container->scrollOffsetY();
  int sub_y = ry + index * ITEM_HEIGHT - inverted_scroll;

  auto sz = child_menu->computeSize();

  // Subscribe to ticks for animation
  _uicontext->subscribeToTicks(child_menu.get(), [child_menu](updatedata_ptr_t updata) {
    float abstime = updata->_abstime;
    child_menu->_hl_color.x = 0.4f + (0.3f * sinf(abstime * 3.0f));
    child_menu->_hl_color.y = 0.4f + (0.3f * sinf(abstime * 3.1f));
    child_menu->_hl_color.z = 0.6f + (0.3f * sinf(abstime * 3.2f));
  });

  _uicontext->pushOverlay(child_menu, sub_x, sub_y, int(sz.x), int(sz.y),
                           false, // don't auto-dismiss submenu on click outside
                           nullptr);
  _submenu_open_index = index;
}
///////////////////////////////////////////////////////////////////////////////
void DropdownMenu::_closeSubmenu() {
  if (_submenu_open_index < 0) return;
  if (!_uicontext) return;

  if (_uicontext->hasOverlays()) {
    _uicontext->popOverlay();
  }
  _submenu_open_index = -1;
}
///////////////////////////////////////////////////////////////////////////////
void DropdownMenu::_selectItem(int index) {
  if (index < 0 || index >= int(_items.size())) return;
  auto& item = _items[index];
  if (item._is_leaf) {
    if (_onSelected) {
      _onSelected(item._value);
    }
    if (_uicontext) {
      _uicontext->popOverlay();
    }
  } else {
    _openSubmenu(index);
  }
}
///////////////////////////////////////////////////////////////////////////////
void DropdownMenu::_ensureItemVisible(int index) {
  if (!_scroll_container || index < 0) return;

  int item_top = index * ITEM_HEIGHT;
  int item_bottom = item_top + ITEM_HEIGHT;
  int viewport_h = _scroll_container->height();
  int max_scroll = _scroll_container->maxScrollY();
  int inverted_scroll = max_scroll - _scroll_container->scrollOffsetY();

  if (item_top < inverted_scroll) {
    // Item is above visible area — scroll up
    _scroll_container->setScrollOffsetY(max_scroll - item_top);
  } else if (item_bottom > inverted_scroll + viewport_h) {
    // Item is below visible area — scroll down
    _scroll_container->setScrollOffsetY(max_scroll - (item_bottom - viewport_h));
  }
}
///////////////////////////////////////////////////////////////////////////////
HandlerResult DropdownMenu::DoOnUiEvent(event_constptr_t cev) {
  // Propagate context to children (overlay system only sets it on us)
  _scroll_container->_uicontext = _uicontext;
  _content->_uicontext = _uicontext;

  HandlerResult rval;

  switch (cev->_eventcode) {
    case EventCode::MOVE: {
      int ly = cev->miY - y();
      int inverted_scroll = _scroll_container->maxScrollY() - _scroll_container->scrollOffsetY();
      int new_hover = (ly + inverted_scroll) / ITEM_HEIGHT;
      new_hover = std::clamp(new_hover, 0, int(_items.size()) - 1);

      if (new_hover != _hover_index) {
        _hover_index = new_hover;
        _hover_start_time = 0.0;

        // Close submenu if hovering a different item
        if (_submenu_open_index >= 0 && _submenu_open_index != _hover_index) {
          _closeSubmenu();
        }
      } else {
        // Track hover time for delayed submenu open
        _hover_start_time += 0.016; // approximate frame time
        if (_hover_start_time >= SUBMENU_DELAY && _hover_index >= 0) {
          auto& item = _items[_hover_index];
          if (!item._is_leaf && _submenu_open_index != _hover_index) {
            _openSubmenu(_hover_index);
          }
        }
      }
      rval.setHandled(this);
      break;
    }
    case EventCode::PUSH:
    case EventCode::DOUBLECLICK: {
      int ly = cev->miY - y();
      int inverted_scroll = _scroll_container->maxScrollY() - _scroll_container->scrollOffsetY();
      int click_index = (ly + inverted_scroll) / ITEM_HEIGHT;
      click_index = std::clamp(click_index, 0, int(_items.size()) - 1);
      _selectItem(click_index);
      rval.setHandled(this);
      break;
    }
    case EventCode::KEY_DOWN: {
      int key = cev->miKeyCode;
      switch (key) {
        case 256: // ESC
          if (_uicontext) {
            _uicontext->popOverlay();
          }
          rval.setHandled(this);
          break;
        case 257: // ENTER
          _selectItem(_hover_index);
          rval.setHandled(this);
          break;
        case 264: // UP
          if (_hover_index > 0) {
            _hover_index--;
            if (_submenu_open_index >= 0) _closeSubmenu();
            _ensureItemVisible(_hover_index);
          }
          rval.setHandled(this);
          break;
        case 265: // DOWN
          if (_hover_index < int(_items.size()) - 1) {
            _hover_index++;
            if (_submenu_open_index >= 0) _closeSubmenu();
            _ensureItemVisible(_hover_index);
          }
          rval.setHandled(this);
          break;
        case 262: // RIGHT - open submenu
          if (_hover_index >= 0 && _hover_index < int(_items.size())) {
            auto& item = _items[_hover_index];
            if (!item._is_leaf) {
              _openSubmenu(_hover_index);
            }
          }
          rval.setHandled(this);
          break;
        case 263: // LEFT - close this level
          if (_uicontext && _uicontext->hasOverlays()) {
            _uicontext->popOverlay();
          }
          rval.setHandled(this);
          break;
        default:
          break;
      }
      break;
    }
    case EventCode::MOUSEWHEEL: {
      // Forward to ScrollContainer for scrolling
      rval = _scroll_container->OnUiEvent(cev);
      break;
    }
    default:
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void DropdownMenu::DoDraw(drawevent_constptr_t drwev) {
  int num_items = _items.size();
  if (num_items == 0) return;

  // Propagate context to children
  _scroll_container->_uicontext = _uicontext;
  _content->_uicontext = _uicontext;

  // Draw ScrollContainer (renders Content via RTG, plus scroll indicator)
  drawChildren(drwev);

  // Draw border on top
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  mtxi->PushUIMatrix();
  {
    int ix1, iy1;
    LocalToRoot(0, 0, ix1, iy1);
    int ix2 = ix1 + _geometry._w;
    int iy2 = iy1 + _geometry._h;

    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);

    fvec4 border_color(0.3f, 0.3f, 0.4f, 0.8f);
    tgt->PushModColor(border_color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    primi->RenderQuadAtZ(defmtl.get(), ix1, ix2, iy1, iy1 + 1, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f); // top
    primi->RenderQuadAtZ(defmtl.get(), ix1, ix2, iy2 - 1, iy2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f); // bottom
    primi->RenderQuadAtZ(defmtl.get(), ix1, ix1 + 1, iy1, iy2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f); // left
    primi->RenderQuadAtZ(defmtl.get(), ix2 - 1, ix2, iy1, iy2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f); // right
    tgt->PopModColor();
  }
  mtxi->PopUIMatrix();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
