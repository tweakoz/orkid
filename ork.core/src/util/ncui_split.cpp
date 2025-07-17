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
// HorizSplit and VerticalSplit - stub implementations
////////////////////////////////////////////////////////////////

HorizSplit::HorizSplit() {
  _name = "HorizSplit";
}

////////////////////////////////////////////////////////////////

void HorizSplit::_doDraw() {
  auto ctx = context();

  // Calculate split position
  int split_x = _x + (int)(_width * _split_position);

  // Draw left child
  if (_left) {
    _left->setPosition(_x, _y);
    _left->resize(split_x - _x, _height);
    _left->draw();
  }

  // Draw vertical separator using optimized method
  drawVLine(split_x, _y, _height, _split_color);

  // Draw right child
  if (_right) {
    _right->setPosition(split_x + 1, _y);
    _right->resize(_x + _width - (split_x + 1), _height);
    _right->draw();
  }
}

////////////////////////////////////////////////////////////////

void HorizSplit::_onLayoutChanged() {
  // Splits handle resizing in _doDraw based on split position
  // Just store the new dimensions
  int split_x = _x + (int)(_width * _split_position);

  if (_left) {
    _left->resize(split_x - _x, _height);
  }
  if (_right) {
    _right->resize(_x + _width - (split_x + 1), _height);
  }
}

////////////////////////////////////////////////////////////////

void VerticalSplit::_doDraw() {
  auto ctx = context();

  // Calculate split position
  int split_y = _y + (int)(_height * _split_position);

  // Draw top child
  if (_top) {
    _top->draw();
  }

  // Draw horizontal separator using optimized method
  drawHLine(_x, split_y, _width, _split_color);

  // Draw bottom child
  if (_bottom) {
    _bottom->draw();
  }
}

////////////////////////////////////////////////////////////////

void HorizSplit::_onInput(uint32_t c, struct ncinput ni) {
  // Forward to appropriate child based on position
  if (_left && ni.x < _x + (_width * _split_position)) {
    _left->onInput(c, ni);
  } else if (_right) {
    _right->onInput(c, ni);
  }
}

////////////////////////////////////////////////////////////////

widget_vect_t HorizSplit::children() const {
  widget_vect_t _children;
  if (_left)
    _children.push_back(_left);
  if (_right)
    _children.push_back(_right);
  return _children;
}

////////////////////////////////////////////////////////////////

void HorizSplit::setLeft(widget_ptr_t widget, widget_ptr_t parent_container) {
  // Clear old child parent
  if (_left) {
    _left->_clearParent();
  }

  // Set new child
  _left = widget;

  // Set new child parent
  if (_left && parent_container) {
    _left->_setParent(parent_container);
  }

  _markLayoutDirty();
}

////////////////////////////////////////////////////////////////

void HorizSplit::setRight(widget_ptr_t widget, widget_ptr_t parent_container) {
  // Clear old child parent
  if (_right) {
    _right->_clearParent();
  }

  // Set new child
  _right = widget;

  // Set new child parent
  if (_right && parent_container) {
    _right->_setParent(parent_container);
  }

  _markLayoutDirty();
}

////////////////////////////////////////////////////////////////

VerticalSplit::VerticalSplit() {
  _name = "VerticalSplit";
}

////////////////////////////////////////////////////////////////

void VerticalSplit::_onLayoutChanged() {
  // Splits handle resizing in _doDraw based on split position
  // Just store the new dimensions
  int split_y = _y + (int)(_height * _split_position);

  if (_top) {
    _top->setPosition(_x, _y);
    _top->resize(_width, split_y - _y);
  }
  if (_bottom) {
    _bottom->setPosition(_x, split_y + 1);
    _bottom->resize(_width, _y + _height - (split_y + 1));
  }
}


////////////////////////////////////////////////////////////////

void VerticalSplit::_onInput(uint32_t c, struct ncinput ni) {
  // Forward to appropriate child based on position
  if (_top && ni.y < _y + (_height * _split_position)) {
    _top->onInput(c, ni);
  } else if (_bottom) {
    _bottom->onInput(c, ni);
  }
}

////////////////////////////////////////////////////////////////

widget_vect_t VerticalSplit::children() const {
  widget_vect_t _children;
  if (_top)
    _children.push_back(_top);
  if (_bottom)
    _children.push_back(_bottom);
  return _children;
}

////////////////////////////////////////////////////////////////

void VerticalSplit::setTop(widget_ptr_t widget, widget_ptr_t parent_container) {
  // Clear old child parent
  if (_top) {
    _top->_clearParent();
  }

  // Set new child
  _top = widget;

  // Set new child parent
  if (_top && parent_container) {
    _top->_setParent(parent_container);
  }

  _markLayoutDirty();
}

////////////////////////////////////////////////////////////////

void VerticalSplit::setBottom(widget_ptr_t widget, widget_ptr_t parent_container) {
  // Clear old child parent
  if (_bottom) {
    _bottom->_clearParent();
  }

  // Set new child
  _bottom = widget;

  // Set new child parent
  if (_bottom && parent_container) {
    _bottom->_setParent(parent_container);
  }

  _markLayoutDirty();
}


} // namespace ork::notcurses
