////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/style.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// ScrollController: Shared scroll state, input, and indicator drawing.
// Widgets that scroll own one (or two) instances and delegate to them.
// This is a plain data+logic object, not a Widget.
////////////////////////////////////////////////////////////////////

struct ScrollController {

  // Scroll state
  int _scroll_offset = 0;
  int _content_size = 0;   // total content height (or width)
  int _viewport_size = 0;  // visible area height (or width)

  // Configuration
  int _scroll_speed = 3;  // pixels per mouse wheel tick

  // Indicator appearance
  fvec4 _indicator_color = fvec4(1.0f, 1.0f, 1.0f, 0.4f);
  int _indicator_width = 6;
  int _indicator_margin = 2;
  int _indicator_min_size = 20;
  int _indicator_corner_radius = 3;  // pill shape
  float _fade_delay = 1.0f;
  float _fade_duration = 0.3f;

  // Scroll indicator fade timing
  float _last_scroll_time = -10.0f;

  // Methods
  int maxScroll() const;
  void clamp();
  void applyMouseWheel(int delta, float current_time);
  bool needsIndicator() const;

  // Draw the scroll indicator (vertical by default, horizontal if horizontal=true).
  // area_x/y/w/h define the scrollable region in root coordinates.
  void drawIndicator(
      drawevent_constptr_t drwev,
      ui::Context* uictx,
      int area_x, int area_y, int area_w, int area_h,
      bool horizontal = false);
};

} // namespace ork::ui
