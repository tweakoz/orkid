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
// ComboBox implementation
////////////////////////////////////////////////////////////////

// ComboBox constructor/destructor - registration handled externally

ComboBox::ComboBox() {
  _name = "ComboBox";
}

////////////////////////////////////////////////////////////////

bool ComboBox::wantsGlobalMouseTracking() const {
  return true;
}

////////////////////////////////////////////////////////////////

void ComboBox::onGlobalMouseEvent(
    const std::string& event_type,
    int mouse_x,
    int mouse_y,
    bool button1_down,
    uint32_t original_key,
    struct ncinput original_ni) {
  // Check if mouse is over combobox main area
  bool mouse_in_main = (mouse_x >= _x && mouse_x < _x + _width && mouse_y >= _y && mouse_y < _y + _height);

  if (_is_open) {
    // Handle dropdown interactions
    bool mouse_in_dropdown = _isMouseInDropdown(mouse_x, mouse_y);

    if (event_type == "mouse_press" && original_ni.id == NCKEY_BUTTON1) {
      if (mouse_in_main) {
        // Click on main area - close dropdown
        _closeDropdown();
      } else if (mouse_in_dropdown) {
        // Click on dropdown item
        int dropdown_y = _y + _height;
        auto ctx       = context();
        if (dropdown_y + _getDropdownHeight() > (int)ctx->_numrows && _y > _getDropdownHeight()) {
          dropdown_y = _y - _getDropdownHeight();
        }

        int item_index = (mouse_y - dropdown_y) + _scroll_offset;
        if (item_index >= 0 && item_index < (int)_items.size()) {
          _selectItem(item_index);
        }
        _closeDropdown();
      } else {
        // Click outside - close dropdown
        _closeDropdown();
      }
    } else if (event_type == "mouse_move") {
      if (mouse_in_dropdown) {
        // Update hover index
        int dropdown_y = _y + _height;
        auto ctx       = context();
        if (dropdown_y + _getDropdownHeight() > (int)ctx->_numrows && _y > _getDropdownHeight()) {
          dropdown_y = _y - _getDropdownHeight();
        }

        int item_index = (mouse_y - dropdown_y) + _scroll_offset;
        if (item_index >= 0 && item_index < (int)_items.size()) {
          _hover_index = item_index;
        } else {
          _hover_index = -1;
        }
      }
    }
  } else {
    // Handle closed combobox interactions
    if (event_type == "mouse_press" && original_ni.id == NCKEY_BUTTON1 && mouse_in_main) {
      _openDropdown();
    }
  }
}

////////////////////////////////////////////////////////////////

void ComboBox::_doDraw() {
  if (_is_open) {
    _drawOpen();
  } else {
    _drawClosed();
  }
}

////////////////////////////////////////////////////////////////

void ComboBox::_drawClosed() {
  auto ctx = context();

  // Fill combobox area using optimized method
  drawFilledBox(_x, _y, _width, _height, _bg_color);
  
  // Set colors for text drawing
  _setColors(_fg_color, _bg_color);

  // Draw selected item text
  if (_selected_index >= 0 && _selected_index < (int)_items.size()) {
    std::string selected_text = _items[_selected_index];

    // Truncate text if too long
    if ((int)selected_text.length() > _width - 3) {
      selected_text = selected_text.substr(0, _width - 6) + "...";
    }

   //ncplane_putstr_yx(ctx->_stdplane, _y, _x + 1, selected_text.c_str());
  }

  // Draw dropdown arrow
  //ncplane_putstr_yx(ctx->_stdplane, _y, _x + _width - 2, "v");
}

////////////////////////////////////////////////////////////////

void ComboBox::_drawOpen() {
  // Draw closed state first
  _drawClosed();

  auto ctx = context();

  // Calculate dropdown position and dimensions
  int dropdown_y      = _y + _height;
  int dropdown_height = _getDropdownHeight();

  // Check if dropdown should be positioned above
  if (dropdown_y + dropdown_height > (int)ctx->_numrows && _y > dropdown_height) {
    dropdown_y = _y - dropdown_height;
  }

  // Draw dropdown background
  uint32_t dropdown_fg = ((uint32_t)(_dropdown_fg_color.x * 255) << 16) | ((uint32_t)(_dropdown_fg_color.y * 255) << 8) |
                         ((uint32_t)(_dropdown_fg_color.z * 255));
  uint32_t dropdown_bg = ((uint32_t)(_dropdown_bg_color.x * 255) << 16) | ((uint32_t)(_dropdown_bg_color.y * 255) << 8) |
                         ((uint32_t)(_dropdown_bg_color.z * 255));

  //ncplane_set_fg_rgb(ctx->_stdplane, dropdown_fg);
  //ncplane_set_bg_rgb(ctx->_stdplane, dropdown_bg);

  // Clear dropdown area
  for (int y = 0; y < dropdown_height; ++y) {
    for (int x = 0; x < _width; ++x) {
      //ncplane_putchar_yx(ctx->_stdplane, dropdown_y + y, _x + x, ' ');
    }
  }

  // Draw dropdown items
  int visible_items = std::min(_max_visible_items, (int)_items.size());
  for (int i = 0; i < visible_items; ++i) {
    int item_index = i + _scroll_offset;
    if (item_index >= (int)_items.size())
      break;

    bool is_hovered  = (item_index == _hover_index);
    bool is_selected = (item_index == _selected_index);

    _drawDropdownItem(item_index, dropdown_y + i, is_hovered, is_selected);
  }
}

////////////////////////////////////////////////////////////////

void ComboBox::_drawDropdownItem(int item_index, int draw_y, bool is_hovered, bool is_selected) {
  auto ctx = context();

  // Select colors
  ork::fvec3 fg_color, bg_color;

  if (is_hovered) {
    fg_color = _hover_fg_color;
    bg_color = _hover_bg_color;
  } else if (is_selected) {
    fg_color = _selected_fg_color;
    bg_color = _selected_bg_color;
  } else {
    fg_color = _dropdown_fg_color;
    bg_color = _dropdown_bg_color;
  }

  // Fill item area using optimized method with explicit coordinates
  drawFilledBox(_x, draw_y, _width, 1, bg_color);
  
  // Set colors for text drawing
  _setColors(fg_color, bg_color);

  // Draw item text
  if (item_index >= 0 && item_index < (int)_items.size()) {
    std::string item_text = _items[item_index];

    // Truncate text if too long
    if ((int)item_text.length() > _width - 2) {
      item_text = item_text.substr(0, _width - 5) + "...";
    }

    //ncplane_putstr_yx(ctx->_stdplane, draw_y, _x + 1, item_text.c_str());
  }
}

////////////////////////////////////////////////////////////////

void ComboBox::_onLayoutChanged() {
  // ComboBox doesn't need special resize handling
}

////////////////////////////////////////////////////////////////

void ComboBox::_onInput(uint32_t c, struct ncinput ni) {
  // Only handle keyboard events - mouse events are handled by global system
  if (_is_open) {
    // Handle dropdown keyboard interactions
    if (c == NCKEY_UP) {
      if (_hover_index > 0) {
        _hover_index--;
        if (_hover_index < _scroll_offset) {
          _scroll_offset = _hover_index;
        }
      }
    } else if (c == NCKEY_DOWN) {
      if (_hover_index < (int)_items.size() - 1) {
        _hover_index++;
        if (_hover_index >= _scroll_offset + _max_visible_items) {
          _scroll_offset = _hover_index - _max_visible_items + 1;
        }
      }
    } else if (c == '\n' || c == '\r') {
      // Enter key - select hovered item
      if (_hover_index >= 0 && _hover_index < (int)_items.size()) {
        _selectItem(_hover_index);
      }
      _closeDropdown();
    } else if (c == 27) { // Escape key
      _closeDropdown();
    } else if (ni.id == NCKEY_BUTTON4 || ni.id == NCKEY_SCROLL_UP) {
      _scrollDropdown(-1);
    } else if (ni.id == NCKEY_BUTTON5 || ni.id == NCKEY_SCROLL_DOWN) {
      _scrollDropdown(1);
    }
  } else {
    // Handle closed combobox keyboard interactions
    if (c == ' ' || c == '\n' || c == '\r') {
      // Keyboard activation
      _openDropdown();
    }
  }
}

////////////////////////////////////////////////////////////////

void ComboBox::_openDropdown() {
  if (_items.empty())
    return;

  _is_open     = true;
  _hover_index = _selected_index >= 0 ? _selected_index : 0;

  // Ensure scroll offset shows the selected/hovered item
  if (_hover_index >= 0) {
    if (_hover_index < _scroll_offset) {
      _scroll_offset = _hover_index;
    } else if (_hover_index >= _scroll_offset + _max_visible_items) {
      _scroll_offset = _hover_index - _max_visible_items + 1;
    }
  }
}

////////////////////////////////////////////////////////////////

void ComboBox::_closeDropdown() {
  _is_open     = false;
  _hover_index = -1;
}

////////////////////////////////////////////////////////////////

void ComboBox::_selectItem(int index) {
  if (index >= 0 && index < (int)_items.size() && index != _selected_index) {
    _selected_index = index;
    _invokeCallback(index, _items[index]);
  }
}

////////////////////////////////////////////////////////////////

void ComboBox::_scrollDropdown(int direction) {
  if (direction < 0) {
    // Scroll up
    if (_scroll_offset > 0) {
      _scroll_offset--;
    }
  } else {
    // Scroll down
    int max_scroll = std::max(0, (int)_items.size() - _max_visible_items);
    if (_scroll_offset < max_scroll) {
      _scroll_offset++;
    }
  }
}

////////////////////////////////////////////////////////////////

void ComboBox::_invokeCallback(int index, const std::string& value) {
  // C++ callback
  if (_on_selection_changed) {
    _on_selection_changed(index, value);
  }
}

////////////////////////////////////////////////////////////////

int ComboBox::_getDropdownHeight() const {
  return std::min(_max_visible_items, (int)_items.size());
}

////////////////////////////////////////////////////////////////

bool ComboBox::_isMouseInDropdown(int mouse_x, int mouse_y) const {
  if (!_is_open)
    return false;

  auto ctx = context();
  if (!ctx)
    return false;

  int dropdown_y      = _y + _height;
  int dropdown_height = _getDropdownHeight();

  // Check if dropdown is positioned above
  if (dropdown_y + dropdown_height > (int)ctx->_numrows && _y > dropdown_height) {
    dropdown_y = _y - dropdown_height;
  }

  return (mouse_x >= _x && mouse_x < _x + _width && mouse_y >= dropdown_y && mouse_y < dropdown_y + dropdown_height);
}

////////////////////////////////////////////////////////////////
} // namespace ork::notcurses