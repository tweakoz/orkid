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
  int getActiveTab() const;
  int getTabCount() const { return _children.size(); }

  // Customization
  void setTabBarHeight(int height) { _tabBarHeight = height; DoLayout(); }
  int getTabBarHeight() const { return _tabBarHeight; }

  // Page mode - when true, tabs are hidden and widget acts as a page/stack container
  void setShowTabs(bool show) { _showTabs = show; DoLayout(); }
  bool getShowTabs() const { return _showTabs; }

protected:
  // Override from Widget
  void DoDraw(drawevent_constptr_t drwev) override;
  void DoLayout() override;
  void _doOnResized() override;
  Widget* doRouteUiEvent(event_constptr_t ev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;

private:
  int _activeTabIndex = -1;
  int _tabBarHeight = 30;
  int _hoveredTabIndex = -1;
  bool _showTabs = true;  // When false, acts as a page/stack widget

  // Tab colors
  fvec4 _tabColorActive = fvec4(0.35, 0.35, 0.4, 1.0);
  fvec4 _tabColorInactive = fvec4(0.25, 0.25, 0.3, 1.0);
  fvec4 _tabColorHover = fvec4(0.3, 0.3, 0.35, 1.0);
  fvec4 _tabBarBackground = fvec4(0.2, 0.2, 0.25, 1.0);
  fvec4 _contentBackground = fvec4(0.15, 0.15, 0.2, 1.0);

  int _getTabIndexAt(int x, int y) const;
  void _drawTabBar(drawevent_constptr_t drwev);
};

using tabwidget_ptr_t = std::shared_ptr<TabWidget>;

} // namespace ork::ui