////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/ncui.h>
#if defined(ENABLE_NOTCURSES_UI)
#include <thread>
#include <atomic>
#include <cstdio>
#include <errno.h>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <sstream>

namespace ork::notcurses {

////////////////////////////////////////////////////////////////
// Box implementation
////////////////////////////////////////////////////////////////

Box::Box() {
  _name = "Box";
}
void Box::_doDraw() {
  auto ctx = context();

  // Fill box area using optimized method
  drawFilledBox(_x, _y, _width, _height, _bgcolor);

  // Draw text with justification
  if (!_text.empty()) {
    // Set text colors using helper method
    _setColors(_fgcolor, _bgcolor);
    
    auto [base_x, base_y] = _calculateTextPosition(_text, _width, _height, _halign, _valign);

    // Handle multi-line text
    std::istringstream iss(_text);
    std::string line;
    int line_num = 0;

    while (std::getline(iss, line)) {
      if (line.empty()) {
        line_num++;
        continue;
      }

      int line_x = base_x;

      // Calculate horizontal position for each line
      switch (_halign) {
        case HorizontalAlign::center:
          line_x = _x + (_width - (int)line.length()) / 2;
          break;
        case HorizontalAlign::right:
          line_x = _x + _width - (int)line.length();
          break;
        default: // left
          line_x = _x;
          break;
      }

      // Ensure text stays within box bounds
      line_x = std::max(_x, std::min(line_x, _x + _width - (int)line.length()));

      //ncplane_putstr_yx(ctx->_stdplane, base_y + line_num, line_x, line.c_str());
      line_num++;
    }
  }
}

////////////////////////////////////////////////////////////////

std::pair<int, int>
Box::_calculateTextPosition(const std::string& text, int box_width, int box_height, HorizontalAlign halign, VerticalAlign valign) {
  int text_x = _x; // default left (will be overridden per-line)
  int text_y = _y; // default top

  // Count text lines
  std::istringstream iss(text);
  std::string line;
  int text_lines = 0;

  while (std::getline(iss, line)) {
    text_lines++;
  }

  // Handle empty text
  if (text_lines == 0) {
    text_lines = 1;
  }

  // Calculate vertical position
  switch (valign) {
    case VerticalAlign::center:
      text_y = _y + (box_height - text_lines) / 2;
      break;
    case VerticalAlign::bottom:
      text_y = _y + box_height - text_lines;
      break;
    default: // top
      text_y = _y;
      break;
  }

  // Ensure text stays within box bounds
  text_y = std::max(_y, std::min(text_y, _y + box_height - text_lines));

  return {text_x, text_y};
}

////////////////////////////////////////////////////////////////

void Box::_onLayoutChanged() {
  // Box doesn't need special resize handling
}

////////////////////////////////////////////////////////////////

void Box::_onInput(uint32_t c, struct ncinput ni) {
  // Box doesn't handle input by default
}

} // namespace ork::notcurses
#endif
