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
// HorizontalPack implementation
////////////////////////////////////////////////////////////////

HorizontalPack::HorizontalPack() {
  _name = "HorizontalPack";
}

////////////////////////////////////////////////////////////////

void HorizontalPack::_doDraw() {
  auto ctx = context();

    drawFilledBox(_x, _y, _width, _height, fvec3(0.25));

  // Don't allocate widths - preserve child natural widths
  int x_offset = _x;

  for (size_t i = 0; i < _children.size(); ++i) {
    auto& child = _children[i];
    child->draw();
    x_offset += child->_width; // Use existing child width

    // Draw vertical separator using optimized method
    drawVLine(x_offset, _y, _height, fvec3(0.5f, 0.5f, 0.5f));
    x_offset += int(_spacing);
  }
}

////////////////////////////////////////////////////////////////

void HorizontalPack::_onLayoutChanged() {
  if(_height_mode=="HEIGHT_FROM_TALLEST"_crcu) {
    // Calculate the tallest child
    for (auto& child : _children) {
      if(child->_height > _height) {
        _height = child->_height;
      }
    }
  }
  int x_offset = _x;
  for (auto& child : _children) {
    int W = child->_width;
    child->setPosition(x_offset, _y);
    child->resize(W, _height); // Use existing child height
    x_offset += W + int(_spacing); // Use existing child width
  }
}

////////////////////////////////////////////////////////////////

void HorizontalPack::_onInput(uint32_t c, struct ncinput ni) {
  // Forward input to children based on position
  int x_offset = _x;
  for (auto& child : _children) {
    if (ni.x >= x_offset && ni.x < x_offset + child->_width) {
      child->onInput(c, ni);
      return;
    }
    x_offset += child->_width + int(_spacing);
  }
}

////////////////////////////////////////////////////////////////

void HorizontalPack::addChild(widget_ptr_t child, widget_ptr_t parent_container) {
  if (child && parent_container) {
    _children.push_back(child);
    child->_setParent(parent_container);
    _markLayoutDirty();
  }
}

////////////////////////////////////////////////////////////////

void HorizontalPack::removeChild(widget_ptr_t child) {
  if (child) {
    auto it = std::find(_children.begin(), _children.end(), child);
    if (it != _children.end()) {
      child->_clearParent(); // Clear parent before removing
      _children.erase(it);
      _markLayoutDirty();
    }
  }
}

////////////////////////////////////////////////////////////////

widget_vect_t HorizontalPack::children() const {
  return _children;
}

////////////////////////////////////////////////////////////////
// VerticalPack implementation
////////////////////////////////////////////////////////////////

VerticalPack::VerticalPack() {
  _name = "VerticalPack";
}

////////////////////////////////////////////////////////////////

void VerticalPack::_doDraw() {
  auto ctx = context();

  // Don't allocate heights - preserve child natural heights
  int y_offset = _y;

  for (size_t i = 0; i < _children.size(); ++i) {
    auto& child = _children[i];
    child->draw();
    y_offset += child->height(); // Use existing child height

    // Draw separator (except after last child)
    if(_spacing){
      if (i < _children.size() - 1) {
        // Draw horizontal separator using optimized method
        drawHLine(_x, y_offset, _width, fvec3(0.5f, 0.5f, 0.5f));
        y_offset++;
      }
    }
  }
}

////////////////////////////////////////////////////////////////

void VerticalPack::_onLayoutChanged() {
  if(_width_mode=="WIDTH_FROM_WIDEST"_crcu) {
    // Calculate the widest child
    for (auto& child : _children) {
      if(child->_width > _width) {
        _width = child->_width;
      }
    }
  }
  int y_offset = _y;
  for (auto& child : _children) {
    int H = child->height();
    child->setPosition(_x, y_offset);
    child->resize(_width, H); // Use existing child height
    y_offset += H + int(_spacing); // Use existing child height
  }
}

////////////////////////////////////////////////////////////////

void VerticalPack::_onInput(uint32_t c, struct ncinput ni) {
  // Forward input to children based on position
  int y_offset = _y;
  for (auto& child : _children) {
    if (ni.y >= y_offset && ni.y < y_offset + child->height()) {
      child->onInput(c, ni);
      return;
    }
    y_offset += child->height() + int(_spacing);
  }
}

////////////////////////////////////////////////////////////////

void VerticalPack::addChild(widget_ptr_t child, widget_ptr_t parent_container) {
  if (child && parent_container) {
    _children.push_back(child);
    child->_setParent(parent_container);
    _markLayoutDirty();
  }
}

////////////////////////////////////////////////////////////////

void VerticalPack::removeChild(widget_ptr_t child) {
  if (child) {
    auto it = std::find(_children.begin(), _children.end(), child);
    if (it != _children.end()) {
      child->_clearParent(); // Clear parent before removing
      _children.erase(it);
      _markLayoutDirty();
    }
  }
}

////////////////////////////////////////////////////////////////

widget_vect_t VerticalPack::children() const {
  return _children;
}


} // namespace ork::notcurses
