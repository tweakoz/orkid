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
// Alignment enum for positioning child widget
////////////////////////////////////////////////////////////////////

enum class Alignment : crc_enum_t {
  CrcEnum(TOP_LEFT),
  CrcEnum(TOP_CENTER),
  CrcEnum(TOP_RIGHT),
  CrcEnum(CENTER_LEFT),
  CrcEnum(CENTER),
  CrcEnum(CENTER_RIGHT),
  CrcEnum(BOTTOM_LEFT),
  CrcEnum(BOTTOM_CENTER),
  CrcEnum(BOTTOM_RIGHT),
};

////////////////////////////////////////////////////////////////////
// AlignmentGroup: A container that aligns and sizes a single child
// - Draws background
// - Positions single child based on alignment (9-point grid)
// - Supports proportional sizing and pixel min/max constraints
////////////////////////////////////////////////////////////////////

struct AlignmentGroup : public Group {
  AlignmentGroup(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~AlignmentGroup();

  // Child management
  template <typename T, typename... Args>
  std::shared_ptr<T> makeChild(Args&&... args) {
    auto child = std::make_shared<T>(std::forward<Args>(args)...);
    addChild(child);
    return child;
  }

  // Alignment
  Alignment _alignment = Alignment::CENTER;

  // Proportional sizing (0.0-1.0 relative to parent, -1 = disabled/natural size)
  float _width_proportional = -1.0f;
  float _height_proportional = -1.0f;

  // Pixel constraints (-1 = disabled)
  int _min_width_pixels = -1;
  int _max_width_pixels = -1;
  int _min_height_pixels = -1;
  int _max_height_pixels = -1;

  // Aspect ratio maintenance (0 = disabled, >0 = width/height ratio)
  // Maintains ratio while respecting max constraints and available space
  // May violate min constraints to maintain ratio
  float _maintain_aspect_ratio = 0.0f;

  // Styling
  fvec4 _bgcolor = fvec4(0.1f, 0.1f, 0.1f, 1.0f);
  bool _draw_background = true;

protected:
  // Override from Widget
  void DoDraw(drawevent_constptr_t drwev) override;
  void DoLayout() override;
  void _doOnResized() override;
  Widget* doRouteUiEvent(event_constptr_t ev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;
};

using alignmentgroup_ptr_t = std::shared_ptr<AlignmentGroup>;

} // namespace ork::ui
