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
  void setMargin(int margin);
  int margin() const;
  /////////////////////////////
  void addChild(widget_ptr_t w, bool relayout = true);
  void removeChild(widget_ptr_t w, bool relayout = true);
  void removeChild(Widget* w, bool relayout = true);
  widget_ptr_t findChildPtr(const Widget* w) const;
  void visitHeirarchy(visit_fn_t vfn);
  /////////////////////////////
  void dumpTopology(int depth = 0);
  /////////////////////////////
  void _doOnResized() override;
  void DoLayout() override;
  void _doOnPreDestroy() override;
  void _doOnParentChanged(Group* parent) override;
  /////////////////////////////
  Widget* doRouteUiEvent(event_constptr_t Ev) override;
  /////////////////////////////
  void drawChildren(ui::drawevent_constptr_t drwev);
  size_t numChildren() const;
  /////////////////////////////
protected:
  virtual void _onChildrenChanged() {}  // Hook for subclasses to respond to child add/remove
  /////////////////////////////
public:
  std::set<Widget*> _snapped;
  std::vector<widget_ptr_t> _children;
  Widget* _eventstealer = nullptr;
  int _margin = 2;
  bool _propagate_on_parent_change = false;
};

} // namespace ork::ui
