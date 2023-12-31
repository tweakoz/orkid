////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork::ui {
////////////////////////////////////////////////////////////////////
// Group : abstract collection of widgets
////////////////////////////////////////////////////////////////////

struct Group : public Widget {

  using visit_fn_t = std::function<void(Widget*)>;

  Group(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~Group();
  /////////////////////////////
  void addChild(widget_ptr_t w);
  void removeChild(widget_ptr_t w);
  void removeChild(Widget* w);
  void visitHeirarchy(visit_fn_t vfn);
  /////////////////////////////
  void dumpTopology(int depth = 0);
  /////////////////////////////
  void _doOnResized() override;
  void DoLayout() override;
  void _doOnPreDestroy() override;
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
