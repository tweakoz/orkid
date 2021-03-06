#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork::ui {
////////////////////////////////////////////////////////////////////
// Group : abstract collection of widgets
////////////////////////////////////////////////////////////////////

struct Group : public Widget {
  Group(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  /////////////////////////////
  void addChild(widget_ptr_t w);
  void removeChild(widget_ptr_t w);
  void removeChild(Widget* w);
  /////////////////////////////
  void OnResize() override;
  void DoLayout() override;
  /////////////////////////////
  Widget* doRouteUiEvent(event_constptr_t Ev) override;
  /////////////////////////////
  void drawChildren(ui::drawevent_constptr_t drwev);
  /////////////////////////////
  std::set<Widget*> _snapped;
  std::vector<widget_ptr_t> _children;
  Widget* _eventstealer = nullptr;
};

} // namespace ork::ui
