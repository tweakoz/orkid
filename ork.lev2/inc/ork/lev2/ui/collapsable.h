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
// Collapsable Widget
//  A container with a disclosure header that can expand/collapse
//  to show/hide a single child widget.
//
//  When collapsed: only header visible (disclosure triangle + label)
//  When expanded: header + child visible
//
//  Height calculation:
//    collapsed: header_height
//    expanded:  header_height + child->desiredHeight()
////////////////////////////////////////////////////////////////////

struct Collapsable final : public Widget {
public:
  Collapsable(const std::string& name,
              int x = 0,
              int y = 0,
              int w = 0,
              int h = 0);
  ~Collapsable();

  // Child management (single child)
  void setChild(widget_ptr_t child);
  widget_ptr_t getChild() const { return _child; }

  // Expanded/collapsed state
  void setExpanded(bool expanded);
  bool isExpanded() const { return _expanded; }
  void toggle() { setExpanded(!_expanded); }

  // Callback when expansion state changes
  using on_toggle_fn_t = std::function<void(bool expanded)>;
  on_toggle_fn_t _onToggle;

  // Configuration
  int _header_height = 24;  // Height of the disclosure header
  int _content_margin = 4;  // Margin around child content (left, right, bottom)
  fvec4 _header_bg_color = fvec4(0.2f, 0.2f, 0.25f, 1.0f);
  fvec4 _header_fg_color = fvec4(0.8f, 0.8f, 0.8f, 1.0f);  // Label text color
  fvec4 _triangle_color = fvec4(0.7f, 0.7f, 0.7f, 1.0f);   // Disclosure triangle
  fvec4 _content_bg_color = fvec4(0.15f, 0.15f, 0.18f, 1.0f);  // Background around child
  bool _draw_content_background = true;
  int _indent_width = 16;  // Width reserved for disclosure triangle

  // Desired size calculation
  int desiredWidth() const override;
  int desiredHeight() const override;

protected:
  void _doGpuInit(lev2::Context* ctx) override;
  void DoDraw(drawevent_constptr_t drwev) override;
  void DoLayout() override;
  void _doOnResized() override;
  Widget* doRouteUiEvent(event_constptr_t ev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;

private:
  void _layoutChild();
  bool _isEventInHeader(event_constptr_t ev) const;

  widget_ptr_t _child;
  bool _expanded = true;
};

using collapsable_ptr_t = std::shared_ptr<Collapsable>;

} // namespace ork::ui
