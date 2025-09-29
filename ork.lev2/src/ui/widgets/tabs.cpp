#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/ui/tabs.h>
#include <ork/lev2/ui/event.h>

namespace ork::ui {

/////////////////////////////////////////////////////////////////////////
TabWidget::TabWidget(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h) {
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

  // Layout all children to fill the content area (but we'll only draw the active one)
  for (auto& child : _children) {
    child->SetRect(0, _tabBarHeight, _geometry._w, std::max(0, _geometry._h - _tabBarHeight));
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
  // Convert event coordinates to local space
  int localX = ev->miX - _geometry._x;
  int localY = ev->miY - _geometry._y;

  // Check if event is in tab bar area
  if (localY < _tabBarHeight) {
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

  return nullptr;
}

/////////////////////////////////////////////////////////////////////////
HandlerResult TabWidget::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;

  // Convert to local coordinates
  int localX = ev->miX - _geometry._x;
  int localY = ev->miY - _geometry._y;

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
  // Draw content area background
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  mtxi->PushUIMatrix();
  {
    // Draw content background (area below tabs)
    if (_geometry._h > _tabBarHeight) {
      int x1, y1, x2, y2;
      LocalToRoot(0, _tabBarHeight, x1, y1);
      LocalToRoot(_geometry._w, _geometry._h, x2, y2);

      defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
      defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
      tgt->PushModColor(_contentBackground);
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
      primi->RenderQuadAtZ(defmtl.get(), x1, x2, y1, y2, 0.0f,
                            0.0f, 1.0f, 0.0f, 1.0f);
      tgt->PopModColor();
    }
  }
  mtxi->PopUIMatrix();

  // Draw tab bar
  _drawTabBar(drwev);

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

  mtxi->PushUIMatrix();
  {
    // Draw tab bar background
    int x1, y1, x2, y2;
    LocalToRoot(0, 0, x1, y1);
    LocalToRoot(_geometry._w, _tabBarHeight, x2, y2);

    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
    defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
    tgt->PushModColor(_tabBarBackground);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    primi->RenderQuadAtZ(defmtl.get(), x1, x2, y1, y2, 0.0f,
                          0.0f, 1.0f, 0.0f, 1.0f);
    tgt->PopModColor();

    // Calculate tab width
    int tabWidth = _geometry._w / _children.size();

    // Draw individual tabs
    int tabIndex = 0;
    ork::lev2::FontMan::PushFont("i14");
    fontman->beginTextBlock(tgt);
    for (const auto& child : _children) {
      // Calculate tab position
      int tab_x1 = tabIndex * tabWidth;
      int tab_x2 = (tabIndex == _children.size() - 1) ? _geometry._w : (tabIndex + 1) * tabWidth;

      // Determine tab color
      fvec4 tabColor;
      if (tabIndex == _activeTabIndex) {
        tabColor = _tabColorActive;
      } else if (tabIndex == _hoveredTabIndex) {
        tabColor = _tabColorHover;
      } else {
        tabColor = _tabColorInactive;
      }

      // Draw tab button
      LocalToRoot(tab_x1, 0, x1, y1);
      LocalToRoot(tab_x2, _tabBarHeight, x2, y2);

      // Add small margin between tabs
      x1 += 1;
      x2 -= 1;

      tgt->PushModColor(tabColor);
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
      primi->RenderQuadAtZ(defmtl.get(), x1, x2, y1, y2, 0.0f,
                            0.0f, 1.0f, 0.0f, 1.0f);
      tgt->PopModColor();

      // Draw tab text (using child's name)
      if (fontman && !child->_name.empty()) {
        int textX = x1 + 5;  // 5 pixel padding from left
        int textY = y1 + (_tabBarHeight / 2);  // Center vertically

        fontman->DrawText(tgt, textX, textY, child->_name.c_str());
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