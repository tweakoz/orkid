////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/ncui_perfviz.h>
#include <ork/kernel/string/deco.inl>
#include <notcurses/notcurses.h>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace ork::notcurses {

////////////////////////////////////////////////////////////////
// RootWidget implementation
////////////////////////////////////////////////////////////////

RootWidget::RootWidget() {
  _name    = "RootWidget";
  _content = nullptr; // Initially no content
}

void RootWidget::_doDraw() {
  if (_content) {
    _content->draw();
  }
}

void RootWidget::_onLayoutChanged() {
  if (_content) {
    _content->resize(_width, _height);
  }
}

void RootWidget::_onInput(uint32_t c, struct ncinput ni) {
  if (_content) {
    _content->onInput(c, ni);
  }
}

void RootWidget::_setContentParent(widget_ptr_t content, widget_ptr_t parent_widget) {
  if (content && parent_widget) {
    content->_setParent(parent_widget);
  }
}

void RootWidget::_clearContentParent(widget_ptr_t content) {
  if (content) {
    content->_clearParent();
  }
}



bool RootWidget::_isQuitButtonHit(int x, int y) {
  if (_width > 2 && _height > 0) {
    int quit_x = 0;
    int quit_y = 1;
    return (x== quit_x && y == quit_y);
  }
  return false;
}

void RootWidget::setContent(widget_ptr_t content, widget_ptr_t parent_container) {
  // Clear old content parent
  if (_content) {
    _clearContentParent(_content);
  }

  // Set new content
  _content = content;

  // Set new content parent
  if (_content && parent_container) {
    _setContentParent(_content, parent_container);
    _content->resize(_width, _height);
  }
}

widget_ptr_t RootWidget::swapContent(widget_ptr_t new_content, widget_ptr_t parent_container) {
  widget_ptr_t old_content = _content;

  // Clear old content parent
  if (old_content) {
    _clearContentParent(old_content);
  }

  // Set new content with proper parent
  setContent(new_content, parent_container);

  return old_content;
}

std::vector<std::shared_ptr<Widget>> RootWidget::children() const {
  if (_content) {
    return {_content};
  }
  return {};
}

} //namespace ork::notcurses {
