////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/ncui.h>
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
// Button implementation
////////////////////////////////////////////////////////////////

// Button constructor/destructor - registration handled externally
Button::Button() {
  _name = "Button";
}

////////////////////////////////////////////////////////////////

bool Button::wantsGlobalMouseTracking() const {
  return true;
}

////////////////////////////////////////////////////////////////

void Button::onGlobalMouseEvent(
    const std::string& event_type,
    int mouse_x,
    int mouse_y,
    bool button1_down,
    uint32_t original_key,
    struct ncinput original_ni) {
  if (!_enabled)
    return;

  if (event_type == "mouse_enter") {
    _mouse_over = true;
    _updateState();
  } else if (event_type == "mouse_leave") {
    _mouse_over = false;
    _updateState();
  } else if (event_type == "mouse_press") {
    _mouse_down = true;
    _updateState();
  } else if (event_type == "mouse_release") {
    if (_mouse_down) {
      _mouse_down = false;
      _updateState();

      // If button is released while mouse is over the button, invoke callback
      if (_mouse_over) {
        _invokeCallback();
      }
    }
  } else if (event_type == "mouse_move") {
    // Mouse position updates are handled by enter/leave events
    // No additional action needed for move events
  }
}

////////////////////////////////////////////////////////////////

void Button::_doDraw() {
  auto ctx = context();

  // Select colors based on state
  ork::fvec3 bg_color, fg_color;
  switch (_state) {
    case State::Hover:
      bg_color = _hover_bg_color;
      fg_color = _hover_fg_color;
      break;
    case State::Pressed:
      bg_color = _pressed_bg_color;
      fg_color = _pressed_fg_color;
      break;
    case State::Disabled:
      bg_color = _disabled_bg_color;
      fg_color = _disabled_fg_color;
      break;
    default: // Normal
      bg_color = _normal_bg_color;
      fg_color = _normal_fg_color;
      break;
  }

  // Fill button area using optimized method
  drawFilledBox(_x, _y, _width, _height, bg_color);

  // Draw text with alignment
  if (!_text.empty()) {
    // Set text colors using helper method
    _setColors(fg_color, bg_color);
    
    auto [text_x, text_y] = _calculateTextPosition();

    // Handle multi-line text
    std::istringstream iss(_text);
    std::string line;
    int line_num = 0;

    while (std::getline(iss, line)) {
      if (line.empty()) {
        line_num++;
        continue;
      }

      int line_x = text_x;

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

      // Ensure text stays within button bounds
      line_x = std::max(_x, std::min(line_x, _x + _width - (int)line.length()));

      //ncplane_putstr_yx(ctx->_stdplane, text_y + line_num, line_x, line.c_str());
      line_num++;
    }
  }
}

////////////////////////////////////////////////////////////////

void Button::_onLayoutChanged() {
  // Button doesn't need special resize handling
}

////////////////////////////////////////////////////////////////

void Button::_onInput(uint32_t c, struct ncinput ni) {
  if (!_enabled)
    return;

  // Only handle keyboard events - mouse events are handled by global system
  if (c == ' ' || c == '\n' || c == '\r') {
    // Keyboard activation
    _invokeCallback();
  }
}

////////////////////////////////////////////////////////////////

void Button::_updateState() {
  if (!_enabled) {
    _state = State::Disabled;
  } else if (_mouse_down && _mouse_over) {
    _state = State::Pressed;
  } else if (_mouse_over) {
    _state = State::Hover;
  } else {
    _state = State::Normal;
  }
}

////////////////////////////////////////////////////////////////

void Button::_invokeCallback() {
  if (!_enabled)
    return;
  if (_on_click) {
    _on_click();
  }
}

////////////////////////////////////////////////////////////////

std::pair<int, int> Button::_calculateTextPosition() {
  int text_x = _x; // default left (will be overridden per-line)
  int text_y = _y; // default top

  // Count text lines
  std::istringstream iss(_text);
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
  switch (_valign) {
    case VerticalAlign::center:
      text_y = _y + (_height - text_lines) / 2;
      break;
    case VerticalAlign::bottom:
      text_y = _y + _height - text_lines;
      break;
    default: // top
      text_y = _y;
      break;
  }

  // Ensure text stays within button bounds
  text_y = std::max(_y, std::min(text_y, _y + _height - text_lines));

  return {text_x, text_y};
}

} // namespace ork::notcurses