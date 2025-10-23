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
// TabGroup implementation
////////////////////////////////////////////////////////////////

TabGroup::TabGroup() {
}

////////////////////////////////////////////////////////////////

TabGroup::~TabGroup() {
  // Clear parent pointers for all tab content
  for (auto& tab : _tabs) {
    tab->content->_clearParent();
  }

  // Clean up ncplane
  if (_tabbar) {
    //ncplane_destroy(_tabbar);
    _tabbar = nullptr;
  }
}

////////////////////////////////////////////////////////////////

void TabGroup::_doDraw() {
  if (!_validateContext())
    return;
  
  auto ctx = context();

  // Create tab bar if needed
  if (!_tabbar) {
    ncplane_options nopts = {};
    nopts.y               = _y;
    nopts.x               = _x;
    nopts.rows            = 3;
    nopts.cols            = _width;
    //_tabbar               = ncplane_create(ctx->_stdplane, &nopts);
  }

  // Fill entire tab bar area with black background first
  //ncplane_erase(_tabbar);
  //ncplane_set_fg_rgb(_tabbar, 0xFFFFFF);
  //ncplane_set_bg_rgb(_tabbar, 0x000000);
  for (unsigned y = 0; y < 3; ++y) {
    for (unsigned x = 0; x < _width; ++x) {
      //ncplane_putchar_yx(_tabbar, y, x, ' ');
    }
  }

  // Draw tabs
  int x = 2;
  for (size_t i = 0; i < _tabs.size(); ++i) {
    auto& tab = _tabs[i];

    // Set colors based on active tab and channel color
    if (tab->_name == _active_tab) {
      //ncplane_set_fg_rgb(_tabbar, 0x000000);
      //ncplane_set_bg_rgb(_tabbar, 0xFFFFFF);
    } else {
      // Use channel color for inactive tabs
      auto color  = tab->_color;
      uint32_t fg = _colorToUint32(color);
      //ncplane_set_fg_rgb(_tabbar, fg);
      //ncplane_set_bg_rgb(_tabbar, 0x000000);
    }

    std::string tab_text = " " + tab->_name + " ";
    //ncplane_putstr_yx(_tabbar, 1, x, tab_text.c_str());
    x += tab_text.length() + 1;
  }

  // Draw border using optimized method
  //ncplane_set_fg_rgb(_tabbar, 0xFFFFFF);
  //ncplane_set_bg_rgb(_tabbar, 0x000000);
  
  // Use optimized horizontal line drawing with Unicode
  std::string border_line;
  border_line.reserve(_width * 3); // Unicode chars are typically 3 bytes
  for (unsigned i = 0; i < _width; ++i) {
    border_line += "─";
  }
  //ncplane_putstr_yx(_tabbar, 2, 0, border_line.c_str());

  // Clear content area before drawing new tab content
  clearContentArea();

  // Draw active tab content
  auto active_content = getActiveContent();
  if (active_content) {
    // active_content->setPosition(_x, _y + 3); // Below tab bar
    // active_content->resize(_width, _height - 3);
    active_content->draw();
  }
}

////////////////////////////////////////////////////////////////

void TabGroup::_onLayoutChanged() {
  auto ctx = context();
  int x = 2;
  for (size_t i = 0; i < _tabs.size(); ++i) {
    auto& tab = _tabs[i];
    tab->content->setPosition(_x, _y + 3); // Below tab bar
    tab->content->resize(_width, _height - 3);
  }
}

////////////////////////////////////////////////////////////////

void TabGroup::_onInput(uint32_t c, struct ncinput ni) {
  if (c == '\t') {
    // Next tab
    if (!_tabs.empty()) {
      auto it = std::find_if(_tabs.begin(), _tabs.end(), [this](const tab_item_ptr_t& tab) { return tab->_name == _active_tab; });
      if (it != _tabs.end()) {
        ++it;
        if (it == _tabs.end()) {
          it = _tabs.begin();
        }
        _active_tab = (*it)->_name;
      }
    }
  } else if (ni.id == NCKEY_BUTTON1 && ni.y < 3) {
    // Click on tab bar
    int x = 2;
    for (const auto& tab : _tabs) {
      std::string tab_text = " " + tab->_name + " ";
      if (ni.x >= x && ni.x < x + (int)tab_text.length()) {
        _active_tab = tab->_name;
        break;
      }
      x += tab_text.length() + 1;
    }
  } else {
    // Forward to active content
    auto active_content = getActiveContent();
    if (active_content) {
      active_content->onInput(c, ni);
    }
  }
}

////////////////////////////////////////////////////////////////

void TabGroup::switchToTab(const std::string& name) {
  _active_tab = name;
}

////////////////////////////////////////////////////////////////

void TabGroup::addTab(const std::string& name, widget_ptr_t content, widget_ptr_t parent_container, const ork::fvec3& color) {
  auto tab     = std::make_shared<TabItem>();
  tab->_name   = name;
  tab->content = content;
  tab->_color  = color;
  _tabs.push_back(tab);

  // Set parent for tab content
  if (content && parent_container) {
    content->_setParent(parent_container);
  }

  // If this is the first tab, make it active
  if (_tabs.size() == 1) {
    _active_tab = name;
  }
}

////////////////////////////////////////////////////////////////

widget_ptr_t TabGroup::getActiveContent() const {
  for (const auto& tab : _tabs) {
    if (tab->_name == _active_tab) {
      return tab->content;
    }
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////

void TabGroup::clearContentArea() {
  // Clear the content area below tab bar
  int content_y      = _y + 3;
  int content_height = _height - 3;
  clearRectangle(_x, content_y, _width, content_height);
}

////////////////////////////////////////////////////////////////

widget_vect_t TabGroup::children() const {
  auto active = getActiveContent();
  if (active) {
    return {active};
  }
  return {};
}

} // namespace ork::notcurses