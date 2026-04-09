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
// BorderFrame: A container that draws a solid border around a single child
////////////////////////////////////////////////////////////////////

struct BorderFrame : public Group {

  BorderFrame(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~BorderFrame();

  void setChild(widget_ptr_t w);
  widget_ptr_t child() const { return _child; }

  HandlerResult DoOnUiEvent(event_constptr_t ev) override;
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  void DoLayout() override;
  Widget* doRouteUiEvent(event_constptr_t ev) override;

  int _border_width = 2;
  int _border_edge_width = 1;
  fvec4 _border_color = fvec4(0.0f, 0.0f, 0.0f, 1.0f);
  fvec4 _border_outer_color = fvec4(0.0f, 0.0f, 0.0f, 1.0f);
  fvec4 _border_inner_color = fvec4(0.0f, 0.0f, 0.0f, 1.0f);

private:
  widget_ptr_t _child;
};

using borderframe_ptr_t = std::shared_ptr<BorderFrame>;

} // namespace ork::ui
