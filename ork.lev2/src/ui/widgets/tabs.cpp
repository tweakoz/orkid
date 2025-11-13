#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/ui/tabs.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/style.h>
#include <ork/lev2/ui/context.h>
#include <cmath>

namespace ork::ui {

/////////////////////////////////////////////////////////////////////////
TabWidget::TabWidget(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h) {
  _tabBarBackground = fvec4(0.2, 0.2, 0.25, 1.0);
  _contentBackground = fvec4(0.15, 0.15, 0.2, 1.0);
}

/////////////////////////////////////////////////////////////////////////
TabWidget::~TabWidget() {
}

/////////////////////////////////////////////////////////////////////////
void TabWidget::setActiveTab(int index) {
  if (index >= 0 && index < _children.size()) {
    _activeTabIndex = index;
    DoLayout();
  }
}

/////////////////////////////////////////////////////////////////////////
int TabWidget::getActiveTab() const {
  // If no tab is active but we have children, return 0 (first tab)
  if (_activeTabIndex < 0 && !_children.empty()) {
    return 0;
  }
  return _activeTabIndex;
}

/////////////////////////////////////////////////////////////////////////
void TabWidget::_doOnResized() {
  DoLayout();
}

/////////////////////////////////////////////////////////////////////////
void TabWidget::DoLayout() {
  // Ensure we have an active tab if there are children
  if (_activeTabIndex < 0 && !_children.empty()) {
    _activeTabIndex = 0;
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

  int tabWidth = _geometry._w / _children.size();
  int index = x / tabWidth;

  if (index >= 0 && index < _children.size()) {
    return index;
  }
  return -1;
}

/////////////////////////////////////////////////////////////////////////
Widget* TabWidget::doRouteUiEvent(event_constptr_t ev) {
  // Convert event coordinates to local space (properly!)
  int localX = 0;
  int localY = 0;
  RootToLocal(ev->miX, ev->miY, localX, localY);

  // Effective tab bar height (0 when in page mode)
  int effectiveTabBarHeight = _showTabs ? _tabBarHeight : 0;
  //printf("TabWidget::doRouteUiEvent ev<%d %d> geo<%d %d> local<%d,%d> effh<%d>\n", ev->miX, ev->miY, _geometry._x, _geometry._y, localX, localY, effectiveTabBarHeight);

  // Check if event is in tab bar area (only if tabs are shown)
  if (_showTabs && localY < effectiveTabBarHeight) {
    // Update hovered tab for visual feedback
    _hoveredTabIndex = _getTabIndexAt(localX, localY);
    // Route to self for tab selection
    return this;
  }

  // Route to active child if it exists and is visible
  if (_activeTabIndex >= 0 && _activeTabIndex < _children.size()) {
    auto& child = _children[_activeTabIndex];
    if (child->IsEventInside(ev)) {
      return child->routeUiEvent(ev);
    }
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
  RootToLocal(ev->miX, ev->miY, localX, localY);

  switch (ev->_eventcode) {
    case EventCode::PUSH: {
      if (localY < _tabBarHeight) {
        int tabIndex = _getTabIndexAt(localX, localY);
        if (tabIndex >= 0 && tabIndex != _activeTabIndex) {
          setActiveTab(tabIndex);
          result.setHandled(this);
        }
      }
      break;
    }

    case EventCode::MOVE: {
      // Update hover state
      if (localY < _tabBarHeight) {
        int oldHovered = _hoveredTabIndex;
        _hoveredTabIndex = _getTabIndexAt(localX, localY);
        if (oldHovered != _hoveredTabIndex) {
          // Trigger redraw for hover effect
          // Note: In real implementation, would mark dirty
        }
      }
      break;
    }

    case EventCode::MOUSE_ENTER:
    case EventCode::MOUSE_LEAVE: {
      _hoveredTabIndex = -1;
      break;
    }

    default:
      break;
  }

  return result;
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
  if (_activeTabIndex >= 0 && _activeTabIndex < _children.size()) {
    _children[_activeTabIndex]->draw(drwev);
  }
}

/////////////////////////////////////////////////////////////////////////
void TabWidget::_drawTabBar(drawevent_constptr_t drwev) {
  if (_children.empty()) return;

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

    // Calculate tab width
    int tabWidth = _geometry._w / _children.size();

    // Draw individual tabs using theme engine
    int tabIndex = 0;
    for (const auto& child : _children) {
      // Calculate tab position
      int tab_x1 = tabIndex * tabWidth;
      int tab_x2 = (tabIndex == _children.size() - 1) ? _geometry._w : (tabIndex + 1) * tabWidth;

      // Add small margin between tabs
      tab_x1 += 1;
      tab_x2 -= 1;

      // Convert to absolute coordinates
      int abs_x1, abs_y1;
      LocalToRoot(tab_x1, 0, abs_x1, abs_y1);

      // Calculate tab dimensions
      int tab_w = tab_x2 - tab_x1;
      int tab_h = _tabBarHeight;

      // Determine which style to use based on state
      uint64_t style_tag;

      // Check for per-tab override first
      auto it = _per_tab_style_tags.find(child);
      if (it != _per_tab_style_tags.end()) {
        style_tag = it->second;
      } else {
        // Use state-based default styles
        if (tabIndex == _activeTabIndex) {
          style_tag = _tab_active_style_tag;
        } else if (tabIndex == _hoveredTabIndex) {
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
        if (tabIndex == _activeTabIndex) {
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

      tabIndex++;
    }

    // Draw tab text
    ork::lev2::FontMan::PushFont("i14");
    fontman->beginTextBlock(tgt);
    tabIndex = 0;
    for (const auto& child : _children) {
      int tab_x1 = tabIndex * tabWidth;
      int tab_x2 = (tabIndex == _children.size() - 1) ? _geometry._w : (tabIndex + 1) * tabWidth;
      LocalToRoot(tab_x1, 0, x1, y1);
      LocalToRoot(tab_x2, _tabBarHeight, x2, y2);

      // Add small margin between tabs
      x1 += 1;
      x2 -= 1;

      // Draw tab text (using child's name)
      if (fontman && !child->_name.empty()) {
        // Center text horizontally in tab
        int text_width = fontman->stringWidth(child->_name.length());
        int text_height = fontman->stringHeight(1);
        int tab_width = x2 - x1;
        int textX = x1 + (tab_width - text_width) / 2;  // Center horizontally
        int textY = y1 + (_tabBarHeight-text_height) / 2;  // Center vertically

        // Get style for text color
        uint64_t style_tag;
        auto it = _per_tab_style_tags.find(child);
        if (it != _per_tab_style_tags.end()) {
          style_tag = it->second;
        } else {
          if (tabIndex == _activeTabIndex) {
            style_tag = _tab_active_style_tag;
          } else if (tabIndex == _hoveredTabIndex) {
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
      tabIndex++;
    }
    fontman->endTextBlock(tgt);
    ork::lev2::FontMan::PopFont();
  }
  mtxi->PopUIMatrix();
}

/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui