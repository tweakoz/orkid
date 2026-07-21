////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/group.h>

namespace ork::ui {

struct DockSpace;
struct DockPanel;
using dockpanel_ptr_t = std::shared_ptr<DockPanel>;

////////////////////////////////////////////////////////////////////
// DockPanel: a container with a titlebar displaying the child's name.
//  The unit that a DockSpace moves between dock nodes. Successor to the
//  earlier DockablePanel stub.
////////////////////////////////////////////////////////////////////

struct DockPanel : public Group {

  DockPanel(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~DockPanel();

  void setChild(widget_ptr_t w);
  widget_ptr_t child() const { return _child; }

  HandlerResult DoOnUiEvent(event_constptr_t ev) override;
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  void DoLayout() override;
  Widget* doRouteUiEvent(event_constptr_t ev) override;

  // Titlebar properties
  int _titlebar_height = 40;
  fvec4 _titlebar_color = fvec4(0.2f, 0.2f, 0.25f, 1.0f);
  fvec4 _title_color = fvec4(0.9f, 0.9f, 0.9f, 1.0f);
  fvec4 _border_color = fvec4(0.3f, 0.3f, 0.35f, 1.0f);
  std::string _title_override; // if non-empty, use this instead of child name
  bool _title_center = false;  // center-justify title text

private:
  DockSpace* _findDockSpace();       // walk up to the owning dock space
  dockpanel_ptr_t _selfPtr();        // shared_ptr to this (from the hosting TabWidget)

  widget_ptr_t _child;
  lev2::font_ptr_t _font;
  // titlebar-drag state
  bool _push_on_titlebar = false;
  DockSpace* _drag_owner = nullptr;  // dock space driving the active drag (if any)
};

} // namespace ork::ui
