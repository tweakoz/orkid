////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/ncui.h>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <climits>
#include <cfloat>
#include <iomanip>

namespace ork::notcurses {

////////////////////////////////////////////////////////////////
// TextEdit implementation
////////////////////////////////////////////////////////////////

TextEdit::TextEdit() {
  _name = "TextEdit";
  _width = 20;
  _height = 1;
}

////////////////////////////////////////////////////////////////

bool TextEdit::wantsGlobalMouseTracking() const {
  return true;
}

////////////////////////////////////////////////////////////////

void TextEdit::onGlobalMouseEvent(
    const std::string& event_type,
    int mouse_x,
    int mouse_y,
    bool button1_down,
    uint32_t original_key,
    struct ncinput original_ni) {
  
  if (event_type == "mouse_press") {
    // Set keyboard focus when clicked
    auto ctx = context();
    if (ctx) {
      ctx->setKeyboardFocus(shared_from_this());
      
      // Position cursor based on mouse click
      int text_x = mouse_x - (_x + 1); // Account for border
      _cursor_pos = std::min((size_t)std::max(0, text_x), _text.length());
    }
  }
}

////////////////////////////////////////////////////////////////

void TextEdit::_doDraw() {
  auto ctx = context();
  if (!ctx || !ctx->_stdplane) return;
  
  // Update validation state
  _is_valid = _isValidContent();
  
  // Select colors based on state
  ork::fvec3 bg_color, fg_color, border_color;
  
  if (!_is_valid) {
    bg_color = _invalid_bg_color;
    fg_color = _invalid_fg_color;
    border_color = _invalid_border_color;
  } else if (_has_focus) {
    bg_color = _focused_bg_color;
    fg_color = _focused_fg_color;
    border_color = _focused_border_color;
  } else {
    bg_color = _normal_bg_color;
    fg_color = _normal_fg_color;
    border_color = _border_color;
  }
  
  // Draw background
  drawFilledBox(_x, _y, _width, _height, bg_color);
  
  // Draw border (simple outline)
  drawOutlineBox(_x, _y, _width, _height, border_color);
  
  // Draw text content
  _setColors(fg_color, bg_color);
  
  std::string display_text = _text;
  int content_width = _width - 2; // Account for borders
  
  // Truncate text if too long
  if (static_cast<int>(display_text.length()) > content_width) {
    display_text = display_text.substr(0, content_width);
  }
  
  // Draw text
  //ncplane_putstr_yx(ctx->_stdplane, _y, _x + 1, display_text.c_str());
  
  // Draw cursor if focused
  if (_has_focus && _cursor_pos <= display_text.length()) {
    int cursor_x = _x + 1 + (int)_cursor_pos;
    if (cursor_x < _x + _width - 1) {
      char cursor_char = (_cursor_pos < _text.length()) ? _text[_cursor_pos] : ' ';
      _setColors(bg_color, fg_color); // Inverted colors for cursor
      //ncplane_putchar_yx(ctx->_stdplane, _y, cursor_x, cursor_char);
    }
  }
}

////////////////////////////////////////////////////////////////

void TextEdit::_onLayoutChanged() {
  // TextEdit doesn't need special resize handling
}

////////////////////////////////////////////////////////////////

void TextEdit::_onInput(uint32_t c, struct ncinput ni) {
  if (!_has_focus) return;
  
  // Handle keyboard input
  if (c == '\n' || c == '\r' || c == NCKEY_ENTER) {
    // Enter key releases focus
    auto ctx = context();
    if (ctx) {
      ctx->setKeyboardFocus(nullptr);
    }
  } else if (c >= 32 && c <= 126) {
    // Printable ASCII characters
    _insertChar((char)c);
  } else if (c == NCKEY_BACKSPACE || c == '\b' || c == 127) {
    _backspace();
  } else if (c == NCKEY_DEL) {
    _deleteChar();
  } else if (c == NCKEY_LEFT) {
    _moveCursor(-1);
  } else if (c == NCKEY_RIGHT) {
    _moveCursor(1);
  } else if (c == NCKEY_HOME) {
    _cursor_pos = 0;
  } else if (c == NCKEY_END) {
    _cursor_pos = _text.length();
  }
}

////////////////////////////////////////////////////////////////

void TextEdit::_insertChar(char c) {
  if (_cursor_pos <= _text.length()) {
    _text.insert(_cursor_pos, 1, c);
    _cursor_pos++;
  }
}

void TextEdit::_deleteChar() {
  if (_cursor_pos < _text.length()) {
    _text.erase(_cursor_pos, 1);
  }
}

void TextEdit::_backspace() {
  if (_cursor_pos > 0) {
    _cursor_pos--;
    _text.erase(_cursor_pos, 1);
  }
}

void TextEdit::_moveCursor(int delta) {
  if (delta < 0) {
    _cursor_pos = (_cursor_pos >= (size_t)(-delta)) ? _cursor_pos + delta : 0;
  } else {
    _cursor_pos = std::min(_cursor_pos + delta, _text.length());
  }
}

bool TextEdit::_isValidContent() const {
  return true; // Base TextEdit always valid
}

////////////////////////////////////////////////////////////////
// IntEdit implementation
////////////////////////////////////////////////////////////////

IntEdit::IntEdit() {
  _name = "IntEdit";
  _text = "0";
}

////////////////////////////////////////////////////////////////

bool IntEdit::_isValidContent() const {
  if (_text.empty()) return false;
  
  try {
    int value = std::stoi(_text);
    return value >= _min_value && value <= _max_value;
  } catch (...) {
    return false;
  }
}

int IntEdit::getValue() const {
  try {
    return std::stoi(_text);
  } catch (...) {
    return 0;
  }
}

void IntEdit::setValue(int value) {
  value = std::clamp(value, _min_value, _max_value);
  _text = std::to_string(value);
  _cursor_pos = _text.length();
}

////////////////////////////////////////////////////////////////
// FloatEdit implementation
////////////////////////////////////////////////////////////////

FloatEdit::FloatEdit() {
  _name = "FloatEdit";
  _text = "0.0";
}

////////////////////////////////////////////////////////////////

bool FloatEdit::_isValidContent() const {
  if (_text.empty()) return false;
  
  try {
    float value = std::stof(_text);
    return value >= _min_value && value <= _max_value;
  } catch (...) {
    return false;
  }
}

float FloatEdit::getValue() const {
  try {
    return std::stof(_text);
  } catch (...) {
    return 0.0f;
  }
}

void FloatEdit::setValue(float value) {
  value = std::clamp(value, _min_value, _max_value);
  
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(_decimal_places) << value;
  _text = oss.str();
  _cursor_pos = _text.length();
}

////////////////////////////////////////////////////////////////
// BoolEdit implementation
////////////////////////////////////////////////////////////////

BoolEdit::BoolEdit() {
  _name = "BoolEdit";
  _width = 15; // "[X] Checkbox"
  _height = 1;
}

////////////////////////////////////////////////////////////////

bool BoolEdit::wantsGlobalMouseTracking() const {
  return true;
}

////////////////////////////////////////////////////////////////

void BoolEdit::onGlobalMouseEvent(
    const std::string& event_type,
    int mouse_x,
    int mouse_y,
    bool button1_down,
    uint32_t original_key,
    struct ncinput original_ni) {
  
  if (event_type == "mouse_press") {
    // Set keyboard focus and toggle value
    auto ctx = context();
    if (ctx) {
      ctx->setKeyboardFocus(shared_from_this());
      _toggle();
    }
  }
}

////////////////////////////////////////////////////////////////

void BoolEdit::_doDraw() {
  auto ctx = context();
  if (!ctx || !ctx->_stdplane) return;
  
  // Select colors
  ork::fvec3 bg_color = _has_focus ? _focused_bg_color : _normal_bg_color;
  ork::fvec3 fg_color = _has_focus ? _focused_fg_color : _normal_fg_color;
  ork::fvec3 border_color = _has_focus ? _focused_border_color : _border_color;
  
  // Draw background
  drawFilledBox(_x, _y, _width, _height, bg_color);
  
  // Draw checkbox
  _setColors(fg_color, bg_color);
  
  std::string checkbox = _value ? "[X] " : "[ ] ";
  std::string display = checkbox + _label;
  
  // Truncate if too long
  if (static_cast<int>(display.length()) > _width) {
    display = display.substr(0, _width);
  }
  
  //ncplane_putstr_yx(ctx->_stdplane, _y, _x, display.c_str());
  
  // Highlight the checkbox part if checked
  if (_value) {
    _setColors(_checked_color, bg_color);
    //ncplane_putchar_yx(ctx->_stdplane, _y, _x + 1, 'X');
  }
  
  // Draw focus indicator
  if (_has_focus) {
    drawHLine(_x, _y + _height, _width, _focused_border_color);
  }
}

////////////////////////////////////////////////////////////////

void BoolEdit::_onLayoutChanged() {
  // BoolEdit doesn't need special resize handling
}

////////////////////////////////////////////////////////////////

void BoolEdit::_onInput(uint32_t c, struct ncinput ni) {
  if (!_has_focus) return;
  
  if (c == ' ') {
    _toggle();
  } else if (c == '\n' || c == '\r' || c == NCKEY_ENTER) {
    // Enter key toggles and releases focus
    _toggle();
    auto ctx = context();
    if (ctx) {
      ctx->setKeyboardFocus(nullptr);
    }
  }
}

////////////////////////////////////////////////////////////////

void BoolEdit::_toggle() {
  _value = !_value;
}

} // namespace ork::notcurses 