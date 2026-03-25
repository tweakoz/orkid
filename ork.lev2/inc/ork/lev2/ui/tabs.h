////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/group.h>
#include <map>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// TabWidget: A container that displays children as tabs
// - Tab bar at top shows all child names
// - Only active child is visible and receives events
// - Click on tab to switch active child
////////////////////////////////////////////////////////////////////

struct TabWidget : public Group {
  TabWidget(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~TabWidget();

  // Tab management
  template <typename T, typename... Args>
  std::shared_ptr<T> makeChild(Args&&... args) {
    auto child = std::make_shared<T>(std::forward<Args>(args)...);
    addChild(child);
    return child;
  }

  void setActiveTab(int index);
  void setActiveTabByName(const std::string& name);  // Find first tab with this name
  int getActiveTab() const;
  int getTabCount() const { return _children.size(); }

  // Customization
  void setTabBarHeight(int height) { _tabBarHeight = height; DoLayout(); }
  int getTabBarHeight() const { return _tabBarHeight; }

  // Page mode - when true, tabs are hidden and widget acts as a page/stack container
  void setShowTabs(bool show) { _showTabs = show; DoLayout(); }
  bool getShowTabs() const { return _showTabs; }

  // Style system integration
  uint64_t _default_tab_style_tag = "tab"_crcu;  // Default tab style
  uint64_t _tab_active_style_tag = "tab_active"_crcu;
  uint64_t _tab_hover_style_tag = "tab_hover"_crcu;

  // Per-tab style overrides (keyed by widget pointer)
  std::unordered_map<widget_ptr_t, uint64_t> _per_tab_style_tags;

  // Per-tab closeable flag (opt-in)
  std::unordered_set<widget_ptr_t> _closeable_tabs;
  void setTabCloseable(widget_ptr_t tab, bool closeable);
  bool isTabCloseable(widget_ptr_t tab) const;
  std::function<void(widget_ptr_t)> _onTabClose;
  static constexpr int _close_button_size = 14;

  // Widget-level colors
  fvec4 _tabBarBackground;
  fvec4 _contentBackground;
  bool _draw_background = true;
  lev2::font_ptr_t _tab_font;

  // Tab layout configuration
  int _tab_padding = 16;  // Constant padding around label text
  bool _sort_tabs = true; // When true, tabs are sorted by name (natural sort)
  void setSortTabs(bool b) { _sort_tabs = b; _needs_layout_recalc = true; }

  protected:
  // Override from Widget
  void DoDraw(drawevent_constptr_t drwev) override;
  void DoLayout() override;
  void _doOnResized() override;
  Widget* doRouteUiEvent(event_constptr_t ev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;

  // Override from Group
  void _onChildrenChanged() override;

private:
  // Pointer-based tracking (stable across sorts)
  widget_ptr_t _active_tab;
  widget_ptr_t _hovered_tab;
  widget_ptr_t _pendingClose;  // deferred close to avoid destroying during event routing

  // Cached layout data
  std::vector<int> _tab_widths;     // Width of each tab (in current sorted order)
  std::vector<int> _tab_positions;  // X position of each tab (in current sorted order)
  bool _needs_layout_recalc = true;

  // Legacy members
  int _tabBarHeight = 30;
  bool _showTabs = true;  // When false, acts as a page/stack widget
  float _pulsation_phase = 0.0f;  // Phase accumulator for active tab pulsation

  // Private methods
  void _ensureSorted();
  void _recalculateTabLayout();
  int _getTabIndexAt(int x, int y) const;
  void _drawTabBar(drawevent_constptr_t drwev);
};

using tabwidget_ptr_t = std::shared_ptr<TabWidget>;

} // namespace ork::ui