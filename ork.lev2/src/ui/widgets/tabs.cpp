#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/ui/tabs.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/style.h>
#include <ork/lev2/ui/context.h>
#include <ork/util/crc.h>
#include <cmath>
#include <cctype>

namespace ork::ui {

/////////////////////////////////////////////////////////////////////////
// Natural sort comparison - handles embedded numbers correctly
// "item2" < "item10", "a1b2" < "a1b10", etc.
/////////////////////////////////////////////////////////////////////////
static bool naturalSortCompare(const std::string& a, const std::string& b) {
  size_t i = 0, j = 0;
  while (i < a.size() && j < b.size()) {
    if (std::isdigit(a[i]) && std::isdigit(b[j])) {
      // Both are digits - compare numerically
      size_t num_start_a = i, num_start_b = j;
      while (i < a.size() && std::isdigit(a[i])) i++;
      while (j < b.size() && std::isdigit(b[j])) j++;

      // Extract numeric substrings
      std::string num_a = a.substr(num_start_a, i - num_start_a);
      std::string num_b = b.substr(num_start_b, j - num_start_b);

      // Compare by length first (longer = larger), then lexicographically
      if (num_a.size() != num_b.size()) {
        return num_a.size() < num_b.size();
      }
      if (num_a != num_b) {
        return num_a < num_b;
      }
      // Numbers are equal, continue comparing rest of string
    } else {
      // At least one is not a digit - compare as characters
      if (a[i] != b[j]) {
        return a[i] < b[j];
      }
      i++;
      j++;
    }
  }
  // Shorter string comes first if one is prefix of other
  return a.size() < b.size();
}

/////////////////////////////////////////////////////////////////////////
TabWidget::TabWidget(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h) {
  _tabBarBackground = fvec4(0.2, 0.2, 0.25, 1.0);
  _contentBackground = fvec4(0.15, 0.15, 0.2, 1.0);
  _tab_font = lev2::FontMan::fontForId("i14");
}

/////////////////////////////////////////////////////////////////////////
TabWidget::~TabWidget() {
}

/////////////////////////////////////////////////////////////////////////
void TabWidget::setTabCloseable(widget_ptr_t tab, bool closeable) {
  if (closeable)
    _closeable_tabs.insert(tab);
  else
    _closeable_tabs.erase(tab);
  _needs_layout_recalc = true;
}

bool TabWidget::isTabCloseable(widget_ptr_t tab) const {
  return _closeable_tabs.count(tab) > 0;
}

/////////////////////////////////////////////////////////////////////////
void TabWidget::_onChildrenChanged() {
  _needs_layout_recalc = true;
}

/////////////////////////////////////////////////////////////////////////
void TabWidget::_ensureSorted() {
  if (!_needs_layout_recalc) return;

  if (_sort_tabs) {
    // Sort _children using natural sort order (1, 2, 10 instead of 1, 10, 2)
    std::sort(_children.begin(), _children.end(),
      [](const widget_ptr_t& a, const widget_ptr_t& b) {
        return naturalSortCompare(a->_name, b->_name);
      });
  }

  // Active tab pointer is still valid - no adjustment needed!
}

/////////////////////////////////////////////////////////////////////////
void TabWidget::_recalculateTabLayout() {
  if (!_needs_layout_recalc) return;

  _tab_widths.clear();
  _tab_positions.clear();

  auto fontman = lev2::FontMan::instance();

  int current_x = 0;
  for (const auto& child : _children) {
    // Measure label width
    int label_width = 0;
    if (fontman && _tab_font) {
      label_width = _tab_font->stringWidth(child->_name.length());
    } else {
      // Fallback estimate
      label_width = child->_name.length() * 8;
    }

    int tab_width = label_width + _tab_padding;
    if (_closeable_tabs.count(child))
      tab_width += _close_button_size + 4;

    _tab_widths.push_back(tab_width);
    _tab_positions.push_back(current_x);
    current_x += tab_width;
  }

  _needs_layout_recalc = false;
}

/////////////////////////////////////////////////////////////////////////
void TabWidget::setActiveTab(int index) {
  if (index >= 0 && index < _children.size()) {
    _active_tab = _children[index];  // Index refers to sorted order
    DoLayout();
  }
}

/////////////////////////////////////////////////////////////////////////
void TabWidget::setActiveTabByName(const std::string& name) {
  // Find first child with matching name
  for (const auto& child : _children) {
    if (child->_name == name) {
      _active_tab = child;
      DoLayout();
      return;
    }
  }
}

/////////////////////////////////////////////////////////////////////////
int TabWidget::getActiveTab() const {
  if (!_active_tab && !_children.empty()) {
    return 0;  // Default to first tab (sorted)
  }

  // Find index of active tab in sorted children
  auto it = std::find(_children.begin(), _children.end(), _active_tab);
  if (it != _children.end()) {
    return std::distance(_children.begin(), it);
  }

  return -1;
}

/////////////////////////////////////////////////////////////////////////
void TabWidget::_doOnResized() {
  DoLayout();
}

/////////////////////////////////////////////////////////////////////////
void TabWidget::DoLayout() {
  // Ensure sorted and layout calculated
  _ensureSorted();
  _recalculateTabLayout();

  // Ensure we have an active tab
  if (!_active_tab && !_children.empty()) {
    _active_tab = _children[0];  // Default to first (alphabetically)
  }

  // Validate active tab is still a child
  if (_active_tab) {
    auto it = std::find(_children.begin(), _children.end(), _active_tab);
    if (it == _children.end()) {
      _active_tab = _children.empty() ? nullptr : _children[0];
    }
  }

  // Effective tab bar height (0 when in page mode)
  int effectiveTabBarHeight = _showTabs ? _tabBarHeight : 0;

  // Layout all children to fill the content area (but we'll only draw the active one)
  for (auto& child : _children) {
    child->SetRect(0, effectiveTabBarHeight, _geometry._w, std::max(0, _geometry._h - effectiveTabBarHeight));
  }
}

/////////////////////////////////////////////////////////////////////////
int TabWidget::_getTabIndexAt(int x, int y) const {
  if (y >= _tabBarHeight) return -1;
  if (_children.empty()) return -1;

  // Find which tab was clicked using cached positions
  for (size_t i = 0; i < _tab_positions.size(); i++) {
    int tab_x1 = _tab_positions[i];
    int tab_x2 = tab_x1 + _tab_widths[i];

    if (x >= tab_x1 && x < tab_x2) {
      return i;  // Return index in sorted order
    }
  }

  return -1;
}

/////////////////////////////////////////////////////////////////////////
Widget* TabWidget::doRouteUiEvent(event_constptr_t ev) {
  // Convert event coordinates to local space (properly!)
  int localX = 0;
  int localY = 0;
  RootToLocal(ev->miX, ev->miY, localX, localY, true);

  // Effective tab bar height (0 when in page mode)
  int effectiveTabBarHeight = _showTabs ? _tabBarHeight : 0;

  // Check if event is in tab bar area (only if tabs are shown)
  if (_showTabs && localY < effectiveTabBarHeight) {
    // Update hovered tab for visual feedback
    int tabIndex = _getTabIndexAt(localX, localY);
    _hovered_tab = (tabIndex >= 0) ? _children[tabIndex] : nullptr;
    // Route to self for tab selection
    return this;
  }

  // Route to active child if it exists
  if (_active_tab && _active_tab->IsEventInside(ev)) {
    return _active_tab->routeUiEvent(ev);
  }

  // If event is inside this widget, route to self
  if (IsEventInside(ev))
    return this;

  return nullptr;
}

/////////////////////////////////////////////////////////////////////////
HandlerResult TabWidget::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;

  // Skip tab interaction if tabs are hidden (page mode)
  if (!_showTabs) {
    return result;
  }

  // Convert to local coordinates (properly!)
  int localX = 0;
  int localY = 0;
  RootToLocal(ev->miX, ev->miY, localX, localY, true);

  switch (ev->_eventcode) {
    case EventCode::PUSH: {
      _push_tab_index = -1;
      if (localY < _tabBarHeight) {
        int tabIndex = _getTabIndexAt(localX, localY);
        if (tabIndex >= 0) {
          _push_tab_index = tabIndex;   // record for a possible header drag
          auto& child = _children[tabIndex];
          // Check if click is on close button
          if (_closeable_tabs.count(child)) {
            int tab_x2 = _tab_positions[tabIndex] + _tab_widths[tabIndex] - 2;
            int close_x1 = tab_x2 - _close_button_size - 2;
            if (localX >= close_x1 && localX < tab_x2) {
              // Close button clicked — defer removal to the context's structural
              // mutation queue so the widget (which may hold Python refs) is not
              // destroyed mid-dispatch.
              auto tab_to_close = child;
              _closeTabDeferred(tab_to_close);
              result.setHandled(this);
              break;
            }
          }
          if (child != _active_tab) {
            setActiveTab(tabIndex);
            result.setHandled(this);
          }
        }
      }
      break;
    }

    case EventCode::MOVE: {
      // Update hover state
      if (localY < _tabBarHeight) {
        int tabIndex = _getTabIndexAt(localX, localY);
        widget_ptr_t old_hovered = _hovered_tab;
        _hovered_tab = (tabIndex >= 0) ? _children[tabIndex] : nullptr;
        if (old_hovered != _hovered_tab) {
          // Trigger redraw for hover effect
          // Note: In real implementation, would mark dirty
        }
      }
      break;
    }

    case EventCode::MOUSE_ENTER:
    case EventCode::MOUSE_LEAVE: {
      _hovered_tab = nullptr;
      break;
    }

    case EventCode::BEGIN_DRAG: {
      // a header PUSH promoted to a drag -> start a tab-header drag
      if (_push_tab_index >= 0 && _push_tab_index < int(_children.size())) {
        _drag_tab        = _children[_push_tab_index];
        _tab_drag_active = true;
        _tab_dragged_out = false;
        result.setHandled(this);
      }
      break;
    }

    case EventCode::DRAG: {
      if (_tab_drag_active && _drag_tab) {
        bool in_bar = (localY >= 0 && localY < _tabBarHeight &&
                       localX >= 0 && localX < _geometry._w);
        if (in_bar && not _tab_dragged_out) {
          // in-bar reorder — a structural mutation; ride the deferred queue when
          // event-driven so it never runs mid-dispatch.
          int target = _getTabIndexAt(localX, localY);
          if (target >= 0) {
            auto tab = _drag_tab;
            if (_uicontext)
              _uicontext->enqueueDeferredMutation([this, tab, target]() { reorderTab(tab, target); });
            else
              reorderTab(tab, target);
          }
        } else {
          // left the bar -> hand off to the host drag session (drag-out)
          if (not _tab_dragged_out) {
            _tab_dragged_out = true;
            if (_onTabDetach)
              _onTabDetach(_drag_tab, ev->miX, ev->miY);
          } else if (_onTabDragMove) {
            _onTabDragMove(ev->miX, ev->miY);
          }
        }
        result.setHandled(this);
      }
      break;
    }

    case EventCode::END_DRAG: {
      if (_tab_drag_active) {
        // Forward BOTH release and cancel to the host session (which began on
        // detach) so a canceled drag-out still tears down the host's drag state;
        // the host skips the commit when canceled. Tab state clears either way.
        if (_tab_dragged_out && _onTabDragCommit)
          _onTabDragCommit(ev->miX, ev->miY, ev->_dragCanceled);
        _tab_drag_active = false;
        _tab_dragged_out = false;
        _drag_tab        = nullptr;
        _push_tab_index  = -1;
        result.setHandled(this);
      }
      break;
    }

    default:
      break;
  }

  return result;
}
/////////////////////////////////////////////////////////////////////////
int TabWidget::tabIndexOf(widget_ptr_t tab) const {
  auto it = std::find(_children.begin(), _children.end(), tab);
  return (it == _children.end()) ? -1 : int(std::distance(_children.begin(), it));
}
/////////////////////////////////////////////////////////////////////////
void TabWidget::reorderTab(widget_ptr_t tab, int index) {
  auto it = std::find(_children.begin(), _children.end(), tab);
  if (it == _children.end())
    return;
  int n = int(_children.size());
  if (index < 0) index = 0;
  if (index >= n) index = n - 1;
  int cur = int(std::distance(_children.begin(), it));
  if (cur == index)
    return;
  _children.erase(it);
  _children.insert(_children.begin() + index, tab);
  _needs_layout_recalc = true;
  DoLayout();
}

/////////////////////////////////////////////////////////////////////////
void TabWidget::_closeTabDeferred(widget_ptr_t tab_to_close) {
  auto do_close = [this, tab_to_close]() {
    _closeable_tabs.erase(tab_to_close);
    _per_tab_style_tags.erase(tab_to_close);
    if (_active_tab == tab_to_close)
      _active_tab = nullptr;
    removeChild(tab_to_close);
    if (_onTabClose) _onTabClose(tab_to_close);
  };
  if (_uicontext)
    _uicontext->enqueueDeferredMutation(do_close);
  else
    do_close();  // no active dispatch — safe to close immediately
}
/////////////////////////////////////////////////////////////////////////
void TabWidget::DoDraw(drawevent_constptr_t drwev) {
  // Update pulsation phase for active tab animation
  _pulsation_phase += 0.01f;

  // Effective tab bar height (0 when in page mode)
  int effectiveTabBarHeight = _showTabs ? _tabBarHeight : 0;

  // Draw content area background
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto primi = tgt->PRI();
  auto fxi = tgt->FXI();
  auto defmtl = lev2::defaultUIMaterial();

  mtxi->PushUIMatrix();
  if(_draw_background){
    // Draw content background (area below tabs)
    if (_geometry._h > effectiveTabBarHeight) {
      int x1, y1, x2, y2;
      LocalToRoot(0, effectiveTabBarHeight, x1, y1);
      LocalToRoot(_geometry._w, _geometry._h, x2, y2);

      auto rs = defmtl->_rasterstate;
      auto omacro = rs->_blendingMacro;
      auto omode = defmtl->meUIColorMode;
      rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
      rs->setDepthTest(lev2::EDepthTest::OFF);
      int prev_pri = rs->_priority;
      rs->_priority = 1<<17; 
      fxi->pushRasterState(rs);
      tgt->PushModColor(_contentBackground);
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
      if(0)primi->RenderQuadAtZ(defmtl.get(), x1, x2, y1, y2, 0.0f,
                            0.0f, 1.0f, 0.0f, 1.0f);
      fxi->popRasterState();
      rs->_priority = prev_pri;
      rs->_blendingMacro = omacro;
      defmtl->meUIColorMode = omode;
      tgt->PopModColor();
    }
  }
  mtxi->PopUIMatrix();

  // Draw tab bar (only if tabs are shown)
  if (_showTabs) {
    _drawTabBar(drwev);
  }

  // Draw active child
  if (_active_tab) {
    _active_tab->draw(drwev);
  }
}

/////////////////////////////////////////////////////////////////////////
void TabWidget::_drawTabBar(drawevent_constptr_t drwev) {
  if (_children.empty()) return;

  // Ensure sorted and layout calculated
  const_cast<TabWidget*>(this)->_ensureSorted();
  const_cast<TabWidget*>(this)->_recalculateTabLayout();

  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();
  auto fontman = lev2::FontMan::instance();

  // Get theme engine
  auto theme_engine = _uicontext ? _uicontext->_theme_engine : nullptr;
  if (!theme_engine) {
    return;  // Can't render without theme engine
  }

  int x1, y1, x2, y2;
  LocalToRoot(0, 0, x1, y1);
  LocalToRoot(_geometry._w, _tabBarHeight, x2, y2);

  mtxi->PushUIMatrix();
  {
    // Draw tab bar background
    auto rs = defmtl->_rasterstate;
    auto omacro = rs->_blendingMacro;
    auto omode = defmtl->meUIColorMode;
    rs->setBlendingMacro(lev2::BlendingMacro::OFF);
    rs->setDepthTest(lev2::EDepthTest::OFF);
    tgt->PushModColor(_tabBarBackground);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    //primi->RenderQuadAtZ(defmtl.get(), x1, x2, y1, y2, 0.0f,
      //                    0.0f, 1.0f, 0.0f, 1.0f);
    tgt->PopModColor();
    rs->_blendingMacro = omacro;
    defmtl->meUIColorMode = omode;

    // Draw individual tabs using cached layout (in sorted order)
    for (size_t i = 0; i < _children.size(); i++) {
      auto& child = _children[i];

      // Get cached tab position and width
      int tab_x1 = _tab_positions[i];
      int tab_w = _tab_widths[i];

      // Add small margin between tabs
      tab_x1 += 1;
      tab_w -= 2;

      // Convert to absolute coordinates
      int abs_x1, abs_y1;
      LocalToRoot(tab_x1, 0, abs_x1, abs_y1);

      int tab_h = _tabBarHeight;

      // Determine which style to use based on state (pointer comparison)
      uint64_t style_tag;

      // Check for per-tab override first
      auto it = _per_tab_style_tags.find(child);
      if (it != _per_tab_style_tags.end()) {
        style_tag = it->second;
      } else {
        // Use state-based default styles (compare pointers)
        if (child == _active_tab) {
          style_tag = _tab_active_style_tag;
        } else if (child == _hovered_tab) {
          style_tag = _tab_hover_style_tag;
        } else {
          style_tag = _default_tab_style_tag;
        }
      }

      // Get style from theme database
      auto style = theme_engine->_styledb->getStyle(style_tag);
      if (style) {
        Style pulsating_style = *style;
        // Apply pulsation to active tab outline
        if (child == _active_tab) {
          // Calculate pulsation multiplier (1.0 ± 0.3)
          float pulsation = 0.85f + 0.15f * sinf(_pulsation_phase);

          // Create a modified style with pulsating border color
          pulsating_style._border_color = style->_border_color * pulsation;

          // Draw tab with pulsating outline
          theme_engine->drawTab(abs_x1, abs_y1, tab_w, tab_h, drwev, &pulsating_style);
        } else {
          // Draw tab normally
          pulsating_style._border_color = style->_border_color * 0.7;
          theme_engine->drawTab(abs_x1, abs_y1, tab_w, tab_h, drwev, &pulsating_style);
        }
      }
    }

    // Draw tab text (in sorted order using cached layout)
    ork::lev2::FontMan::PushFont(_tab_font);
    fontman->beginTextBlock(tgt);
    for (size_t i = 0; i < _children.size(); i++) {
      auto& child = _children[i];

      // Get cached tab position and width
      int tab_x1 = _tab_positions[i];
      int tab_w = _tab_widths[i];

      // Add small margin between tabs
      tab_x1 += 1;
      tab_w -= 2;

      LocalToRoot(tab_x1, 0, x1, y1);
      int x2 = x1 + tab_w;
      int y2 = y1 + _tabBarHeight;

      // Draw tab text (using child's name)
      if (fontman && !child->_name.empty()) {
        // Center text horizontally in tab (account for close icon on closeable tabs)
        int text_width = _tab_font->stringWidth(child->_name.length());
        int text_height = _tab_font->stringHeight(1);
        int avail_w = _closeable_tabs.count(child) ? (tab_w - _close_button_size - 4) : tab_w;
        int textX = x1 + (avail_w - text_width) / 2;  // Center horizontally
        int textY = y1 + (_tabBarHeight - text_height) / 2;  // Center vertically

        // Get style for text color (pointer comparison)
        uint64_t style_tag;
        auto it = _per_tab_style_tags.find(child);
        if (it != _per_tab_style_tags.end()) {
          style_tag = it->second;
        } else {
          if (child == _active_tab) {
            style_tag = _tab_active_style_tag;
          } else if (child == _hovered_tab) {
            style_tag = _tab_hover_style_tag;
          } else {
            style_tag = _default_tab_style_tag;
          }
        }

        auto style = theme_engine->_styledb->getStyle(style_tag);
        fvec4 text_color = style ? style->_text_color : fvec4(1, 1, 1, 1);

        tgt->PushModColor(text_color);
        fontman->DrawText(tgt, textX, textY, child->_name.c_str());
        tgt->PopModColor();
      }
    }
    fontman->endTextBlock(tgt);
    ork::lev2::FontMan::PopFont();

    // Draw close button icons on closeable tabs
    {
      auto style = theme_engine->_styledb->getStyle("box"_crcu);
      if (style && style->_icon_close) {
        for (size_t i = 0; i < _children.size(); i++) {
          auto& child = _children[i];
          if (!_closeable_tabs.count(child)) continue;

          int tab_x1 = _tab_positions[i] + 1;
          int tab_w = _tab_widths[i] - 2;
          int abs_x1, abs_y1;
          LocalToRoot(tab_x1, 0, abs_x1, abs_y1);

          int icon_size = _close_button_size;
          int ix = abs_x1 + tab_w - icon_size - 4;
          int iy = abs_y1 + (_tabBarHeight - icon_size) / 2;
          theme_engine->drawIcon(ix, iy, icon_size, icon_size, drwev, style->_icon_close);
        }
      }
    }
  }
  mtxi->PopUIMatrix();
}

/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui