////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/ncui.h>
#include <ork/util/logger.h>
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
  // TextLines implementation
  ////////////////////////////////////////////////////////////////

TextLines::TextLines() {
  _color = ork::fvec3(1.0f, 1.0f, 1.0f);
  _bg_color = ork::fvec3(0.0f, 0.0f, 0.0f);
  _auto_scroll = true;
  _user_has_scrolled = false;
}

/////////////////////////////////////////

void TextLines::addLine(const std::string& line) {
  _lines.push_back(line);
  
  // Limit number of lines in memory
  if (_lines.size() > _max_lines) {
    _lines.erase(_lines.begin());
    // Adjust scroll offset to maintain position
    if (_scroll_offset > 0) {
      _scroll_offset--;
    }
  }
  
  // Only auto-scroll if enabled and user hasn't manually scrolled
  if (_auto_scroll && !_user_has_scrolled) {
    // Auto-scroll to bottom if content exceeds display height
    if (_height > 0 && _lines.size() > (size_t)_height) {
      _scroll_offset = _lines.size() - _height;
    }
  }
}

/////////////////////////////////////////

void TextLines::setLines(const std::vector<std::string>& lines) {
  _lines = lines;
  
  // Limit number of lines in memory
  if (_lines.size() > _max_lines) {
    _lines.erase(_lines.begin(), _lines.begin() + (_lines.size() - _max_lines));
  }
  
  // Reset scroll position and user scroll state
  _scroll_offset = 0;
  _user_has_scrolled = false;
  
  // Auto-scroll to bottom if enabled
  if (_auto_scroll && _height > 0 && _lines.size() > (size_t)_height) {
    _scroll_offset = _lines.size() - _height;
  }
}

/////////////////////////////////////////

void TextLines::clear() {
  _lines.clear();
  _scroll_offset = 0;
  _user_has_scrolled = false;
}

/////////////////////////////////////////

void TextLines::setColor(const ork::fvec3& color) {
  _color = color;
}

/////////////////////////////////////////

void TextLines::setScrolling(bool enabled) {
  // This method can be used to enable/disable scrolling entirely
  // Currently not implemented - scrolling is always enabled
}

/////////////////////////////////////////

void TextLines::setAutoScroll(bool enabled) {
  _auto_scroll = enabled;
  
  // If auto-scroll is enabled, reset user scroll state and scroll to bottom
  if (_auto_scroll) {
    _user_has_scrolled = false;
    if (_height > 0 && _lines.size() > (size_t)_height) {
      _scroll_offset = _lines.size() - _height;
    }
  }
}

/////////////////////////////////////////

void TextLines::_scroll(int direction) {
  if (_lines.empty() || _height <= 0) {
    // Log to debug channel
    /*auto logger = ork::logger();
    auto debug_chan = logger->configureChannel("TEXTLINEDBG", ork::fvec3(1,1,0), true);
    debug_chan->log(ork::FormatString("_scroll called but early return: lines.empty=%d, height=%d", 
                    _lines.empty(), _height).c_str());
    */
    return;
  }
  
  // Mark that user has manually scrolled
  _user_has_scrolled = true;
  
  size_t max_offset = (_lines.size() > (size_t)_height) ? _lines.size() - _height : 0;
  size_t old_offset = _scroll_offset;
  
  if (direction > 0) { // Scroll down
    if (_scroll_offset < max_offset) {
      _scroll_offset++;
    }
  } else if (direction < 0) { // Scroll up
    if (_scroll_offset > 0) {
      _scroll_offset--;
    }
  }
  
  // Log scroll activity
  /*
  auto logger = ork::logger();
  auto debug_chan = logger->configureChannel("TEXTLINEDBG", ork::fvec3(1,1,0), true);
  debug_chan->log(ork::FormatString("_scroll: dir=%d, old_offset=%zu, new_offset=%zu, max_offset=%zu, lines=%zu, height=%d", 
                  direction, old_offset, _scroll_offset, max_offset, _lines.size(), _height).c_str());
                  */
}

/////////////////////////////////////////

void TextLines::_onInput(uint32_t c, struct ncinput ni) {
  // Log all input received
  /*auto logger = ork::logger();
  auto debug_chan = logger->configureChannel("TEXTLINEDBG", ork::fvec3(1,1,0), true);
  debug_chan->log(ork::FormatString("TextLines::_onInput received key: c=%u (char='%c'), name='%s'", 
                  c, (c >= 32 && c <= 126) ? (char)c : '?', _name.c_str()).c_str());
  */
  // Handle keyboard and mouse scrolling
  switch (c) {
    case NCKEY_UP:
      //debug_chan->log("Processing NCKEY_UP - scrolling up");
      _scroll(-1);
      break;
    case NCKEY_DOWN:
      //debug_chan->log("Processing NCKEY_DOWN - scrolling down");
      _scroll(1);
      break;
    // Page navigation using different key combinations
    // Note: NCKEY_PGUP and NCKEY_PGDN might not be available
    // We'll use the working arrow keys for now
    case 'j':
      //debug_chan->log("Processing 'j' key - scrolling down");
      _scroll(1);
      break;
    case 'k':
      //debug_chan->log("Processing 'k' key - scrolling up");
      _scroll(-1);
      break;
    case NCKEY_HOME:
      //debug_chan->log("Processing NCKEY_HOME - jumping to top");
      _user_has_scrolled = true;
      _scroll_offset = 0;
      break;
    case NCKEY_END:
      //debug_chan->log("Processing NCKEY_END - jumping to bottom");
      _user_has_scrolled = false; // Reset user scroll state - END key acts like auto-scroll
      if (_height > 0 && _lines.size() > (size_t)_height) {
        _scroll_offset = _lines.size() - _height;
      }
      break;
    default:
      // Check for mouse wheel events using ni.id
      if (ni.id == NCKEY_BUTTON4 || ni.id == NCKEY_SCROLL_UP) {
        //debug_chan->log("Processing mouse wheel up - scrolling up");
        _scroll(-1);
      } else if (ni.id == NCKEY_BUTTON5 || ni.id == NCKEY_SCROLL_DOWN) {
        //debug_chan->log("Processing mouse wheel down - scrolling down");
        _scroll(1);
      } else {
        //debug_chan->log(ork::FormatString("Input not handled by TextLines: c=%u, ni.id=%u", c, ni.id).c_str());
      }
      break;
  }
}

/////////////////////////////////////////

void TextLines::_doDraw() {
  if (!_validateContext()) return;
  auto ctx = context();
    
  // Clear the area with background color
  drawFilledBox(_x, _y, _width, _height, _bg_color);
      
  // Draw visible lines
  _setColors(_color, _bg_color);
  int parh = _parent.lock()->_height;
  //auto dbgtxt = ork::FormatString("x=%d y=%d width=%d height=%d parh=%d soff=%zu nlines=%zu",
  //                    _x, _y, _width, _height, parh, _scroll_offset, _lines.size());
  for (size_t i=0; i<_height; i++) {
    int y = _y + i;
    size_t line_index = _scroll_offset + i;
    if(line_index<_lines.size()) {
      std::string line = _lines[line_index];
      if (static_cast<int>(line.length()) > _width) {
        line = line.substr(0, _width);
      }
      if(i==0){
        //ncplane_putstr_yx(ctx->_stdplane, y, _x+4, dbgtxt.c_str());
        //ncplane_putstr_yx(ctx->_stdplane, y, _x, line.c_str());
      }
      else{
        //ncplane_putstr_yx(ctx->_stdplane, y, _x, line.c_str());
      }
    }
    
  }
}

/////////////////////////////////////////

void TextLines::_onLayoutChanged() {
  // Adjust scroll offset if layout changed
  if (_height > 0 && _lines.size() > (size_t)_height) {
    size_t max_offset = _lines.size() - _height;
    if (_scroll_offset > max_offset) {
      _scroll_offset = max_offset;
    }
  }
}

/////////////////////////////////////////

}