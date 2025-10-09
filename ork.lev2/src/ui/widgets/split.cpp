#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/ui/split.h>
#include <ork/lev2/ui/event.h>

namespace ork::ui {

/////////////////////////////////////////////////////////////////////////
// HorizontalSplit
/////////////////////////////////////////////////////////////////////////

HorizontalSplit::HorizontalSplit(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h) {
}

/////////////////////////////////////////////////////////////////////////

HorizontalSplit::~HorizontalSplit() {
}

/////////////////////////////////////////////////////////////////////////

void HorizontalSplit::_doOnResized() {
  DoLayout();
}

/////////////////////////////////////////////////////////////////////////

void HorizontalSplit::DoLayout() {
  if (_split_ratio < 0.0f) _split_ratio = 0.0f;
  if (_split_ratio > 1.0f) _split_ratio = 1.0f;

  int split_x;
  int left_width, right_width;

  // Check for fixed width on children
  bool child0_fixed = (_children.size() > 0) && (_children[0]->_fixed_width > 0);
  bool child1_fixed = (_children.size() > 1) && (_children[1]->_fixed_width > 0);

  if (child0_fixed && !child1_fixed) {
    // First child fixed, second fills remaining
    left_width = _children[0]->_fixed_width;
    right_width = _geometry._w - left_width;
    split_x = left_width;
  } else if (!child0_fixed && child1_fixed) {
    // Second child fixed, first fills remaining
    right_width = _children[1]->_fixed_width;
    left_width = _geometry._w - right_width;
    split_x = left_width;
  } else {
    // Both fixed or neither fixed - use split_ratio
    split_x = int(_geometry._w * _split_ratio);
    left_width = split_x;
    right_width = _geometry._w - split_x;
  }

  // 0th child = left
  if (_children.size() > 0) {
    _children[0]->SetRect(0, 0, left_width, _geometry._h);
  }

  // 1st child = right
  if (_children.size() > 1) {
    _children[1]->SetRect(split_x, 0, right_width, _geometry._h);
  }
}

/////////////////////////////////////////////////////////////////////////

Widget* HorizontalSplit::doRouteUiEvent(event_constptr_t ev) {
  int localX = ev->miX - _geometry._x;
  int localY = ev->miY - _geometry._y;

  int split_x = int(_geometry._w * _split_ratio);

  // Check left child (0th)
  if (_children.size() > 0 && localX < split_x) {
    if (_children[0]->IsEventInside(ev)) {
      return _children[0]->doRouteUiEvent(ev);
    }
  }

  // Check right child (1st)
  if (_children.size() > 1 && localX >= split_x) {
    if (_children[1]->IsEventInside(ev)) {
      return _children[1]->doRouteUiEvent(ev);
    }
  }

  return nullptr;
}

/////////////////////////////////////////////////////////////////////////

HandlerResult HorizontalSplit::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;
  return result;
}

/////////////////////////////////////////////////////////////////////////

void HorizontalSplit::DoDraw(drawevent_constptr_t drwev) {
  auto tgt = drwev->GetTarget();
  auto fbi = tgt->FBI();

  // Draw children
  for (auto child : _children) {
    child->draw(drwev);
  }
}

/////////////////////////////////////////////////////////////////////////
// VerticalSplit
/////////////////////////////////////////////////////////////////////////

VerticalSplit::VerticalSplit(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h) {
}

/////////////////////////////////////////////////////////////////////////

VerticalSplit::~VerticalSplit() {
}

/////////////////////////////////////////////////////////////////////////

void VerticalSplit::_doOnResized() {
  DoLayout();
}

/////////////////////////////////////////////////////////////////////////

void VerticalSplit::DoLayout() {
  if (_split_ratio < 0.0f) _split_ratio = 0.0f;
  if (_split_ratio > 1.0f) _split_ratio = 1.0f;

  int split_y;
  int top_height, bottom_height;

  // Check for fixed height on children
  bool child0_fixed = (_children.size() > 0) && (_children[0]->_fixed_height > 0);
  bool child1_fixed = (_children.size() > 1) && (_children[1]->_fixed_height > 0);

  if (child0_fixed && !child1_fixed) {
    // First child fixed, second fills remaining
    top_height = _children[0]->_fixed_height;
    bottom_height = _geometry._h - top_height;
    split_y = top_height;
  } else if (!child0_fixed && child1_fixed) {
    // Second child fixed, first fills remaining
    bottom_height = _children[1]->_fixed_height;
    top_height = _geometry._h - bottom_height;
    split_y = top_height;
  } else {
    // Both fixed or neither fixed - use split_ratio
    split_y = int(_geometry._h * _split_ratio);
    top_height = split_y;
    bottom_height = _geometry._h - split_y;
  }

  // 0th child = top
  if (_children.size() > 0) {
    _children[0]->SetRect(0, 0, _geometry._w, top_height);
  }

  // 1st child = bottom
  if (_children.size() > 1) {
    _children[1]->SetRect(0, split_y, _geometry._w, bottom_height);
  }
}

/////////////////////////////////////////////////////////////////////////

Widget* VerticalSplit::doRouteUiEvent(event_constptr_t ev) {
  int localX = ev->miX - _geometry._x;
  int localY = ev->miY - _geometry._y;

  int split_y = int(_geometry._h * _split_ratio);

  // Check top child (0th)
  if (_children.size() > 0 && localY < split_y) {
    if (_children[0]->IsEventInside(ev)) {
      return _children[0]->doRouteUiEvent(ev);
    }
  }

  // Check bottom child (1st)
  if (_children.size() > 1 && localY >= split_y) {
    if (_children[1]->IsEventInside(ev)) {
      return _children[1]->doRouteUiEvent(ev);
    }
  }

  return nullptr;
}

/////////////////////////////////////////////////////////////////////////

HandlerResult VerticalSplit::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;
  return result;
}

/////////////////////////////////////////////////////////////////////////

void VerticalSplit::DoDraw(drawevent_constptr_t drwev) {
  auto tgt = drwev->GetTarget();
  auto fbi = tgt->FBI();

  // Draw children
  for (auto child : _children) {
    child->draw(drwev);
  }
}

/////////////////////////////////////////////////////////////////////////

} // namespace ork::ui
