////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/group.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////////////////
// DynaGrid: Smart grid layout widget
//
// Intelligently arranges children in a grid based on item count and container
// aspect ratio. Minimizes wasted space and keeps cells reasonably square.
//
// Usage:
//   auto grid = DynaGrid::create(parent);
//   grid->setMargin(4);
//   // Add children as needed
//   parent->addChild(grid);
////////////////////////////////////////////////////////////////////////////////

struct DynaGrid : public Group {

  DynaGrid(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~DynaGrid();

  // Static factory
  static dynagrid_ptr_t create(group_ptr_t parent);

  // Configuration
  void setMargin(int margin);
  int margin() const { return _margin; }

  // Public members for aspect ratio constraints
  float _aspect_min = 0.0f;  // Minimum cell aspect ratio (0.0 = ignore)
  float _aspect_max = 0.0f;  // Maximum cell aspect ratio (0.0 = ignore)

  // Visual style
  fvec4 _bgcolor = fvec4(0.0f, 0.0f, 0.0f, 0.0f);  // Background color (transparent by default)
  bool _draw_background = false;

private:
  void DoLayout() override;
  void _doOnResized() override;
  void DoDraw(drawevent_constptr_t drwev) override;
  void _calculateGrid(int item_count, float container_aspect);

  int _rows = 1;
  int _cols = 1;
  int _margin = 4;  // Pixels between cells
};

} // namespace ork::ui
