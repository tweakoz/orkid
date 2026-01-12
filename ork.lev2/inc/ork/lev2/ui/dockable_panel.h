////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/group.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// DockablePanel: A container with a titlebar displaying the child's name
// Foundation for future docking widget system
////////////////////////////////////////////////////////////////////

struct DockablePanel : public Group {

  DockablePanel(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~DockablePanel();

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

private:
  widget_ptr_t _child;
  lev2::font_ptr_t _font;
};

using dockablepanel_ptr_t = std::shared_ptr<DockablePanel>;

} // namespace ork::ui
