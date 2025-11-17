////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/ui/dynagrid.h>
#include <cmath>

namespace ork::ui {

/////////////////////////////////////////////////////////////////////////
DynaGrid::DynaGrid(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h) {
}
/////////////////////////////////////////////////////////////////////////
DynaGrid::~DynaGrid() {
}
/////////////////////////////////////////////////////////////////////////
dynagrid_ptr_t DynaGrid::create(group_ptr_t parent) {
  auto grid = std::make_shared<DynaGrid>("DynaGrid");
  return grid;
}
/////////////////////////////////////////////////////////////////////////
void DynaGrid::_doOnResized() {
  DoLayout();
}
/////////////////////////////////////////////////////////////////////////
void DynaGrid::DoDraw(drawevent_constptr_t drwev) {
  // Draw background if enabled
  if (_draw_background) {
    Widget::_drawColoredBox(drwev, _bgcolor, lev2::BlendingMacro::ALPHA);
  }
  drawChildren(drwev);
}
/////////////////////////////////////////////////////////////////////////
void DynaGrid::_calculateGrid(int item_count, float container_aspect) {
  if (item_count <= 0) {
    _rows = 1;
    _cols = 1;
    return;
  }

  if (item_count == 1) {
    _rows = 1;
    _cols = 1;
    return;
  }

  if (item_count == 2) {
    // 2 items: favor horizontal on wide containers, vertical on tall
    if (container_aspect >= 1.0f) {
      _rows = 1;
      _cols = 2;  // Horizontal
    } else {
      _rows = 2;
      _cols = 1;  // Vertical
    }
    return;
  }

  // For 3+ items, find the grid that best matches the container aspect ratio
  // while minimizing wasted cells

  float best_score = 1e10f;
  int best_rows = 1;
  int best_cols = item_count;

  // Try different row/col combinations
  int max_side = (int)std::ceil(std::sqrt((float)item_count)) + 1;

  for (int rows = 1; rows <= max_side; rows++) {
    int cols = (int)std::ceil((float)item_count / (float)rows);

    // Calculate how many cells would be wasted
    int total_cells = rows * cols;
    int wasted_cells = total_cells - item_count;

    // Calculate the aspect ratio of this grid
    float grid_aspect = (float)cols / (float)rows;

    // Score: prefer grids that match the container aspect ratio
    // and minimize wasted cells
    float aspect_diff = std::abs(grid_aspect - container_aspect);
    float wasted_penalty = (float)wasted_cells * 0.5f;
    float score = aspect_diff + wasted_penalty;

    if (score < best_score) {
      best_score = score;
      best_rows = rows;
      best_cols = cols;
    }
  }

  _rows = best_rows;
  _cols = best_cols;
}
/////////////////////////////////////////////////////////////////////////
void DynaGrid::DoLayout() {
  size_t item_count = _children.size();
  if (item_count == 0) {
    return;
  }

  // Calculate container aspect ratio
  int w = _geometry._w;
  int h = _geometry._h;
  float container_aspect = (h > 0) ? ((float)w / (float)h) : 1.0f;

  // Calculate optimal grid dimensions
  _calculateGrid(item_count, container_aspect);

  // Calculate cell dimensions
  int total_margin_w = _margin * (_cols + 1);
  int total_margin_h = _margin * (_rows + 1);
  int available_w = w - total_margin_w;
  int available_h = h - total_margin_h;

  int cell_w = (_cols > 0) ? (available_w / _cols) : 0;
  int cell_h = (_rows > 0) ? (available_h / _rows) : 0;

  // Apply aspect ratio constraints
  if (cell_h > 0) {
    float cell_aspect = (float)cell_w / (float)cell_h;

    if (_aspect_min > 0.0f && cell_aspect < _aspect_min) {
      // Too tall, reduce height to meet min aspect ratio
      cell_h = (int)((float)cell_w / _aspect_min);
    }

    if (_aspect_max > 0.0f && cell_aspect > _aspect_max) {
      // Too wide, reduce width to meet max aspect ratio
      cell_w = (int)((float)cell_h * _aspect_max);
    }
  }

  // Position children in grid (using relative coordinates)
  size_t child_idx = 0;
  for (int row = 0; row < _rows && child_idx < item_count; row++) {
    // Calculate how many items are in this row
    int items_in_row = std::min((int)(_cols), (int)(item_count - child_idx));

    // Center incomplete rows
    int row_offset_x = 0;
    if (items_in_row < _cols) {
      int row_width = items_in_row * cell_w + (items_in_row - 1) * _margin;
      row_offset_x = (w - row_width) / 2;
    } else {
      row_offset_x = _margin;
    }

    for (int col = 0; col < items_in_row; col++) {
      auto child = _children[child_idx];

      // Use relative coordinates, not absolute
      int child_x = row_offset_x + col * (cell_w + _margin);
      int child_y = _margin + row * (cell_h + _margin);

      child->SetRect(child_x, child_y, cell_w, cell_h);

      child_idx++;
    }
  }
}
/////////////////////////////////////////////////////////////////////////

} // namespace ork::ui
