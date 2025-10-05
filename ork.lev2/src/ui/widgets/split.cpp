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

  int split_x = int(_geometry._w * _split_ratio);

  // 0th child = left
  if (_children.size() > 0) {
    _children[0]->SetRect(0, 0, split_x, _geometry._h);
  }

  // 1st child = right
  if (_children.size() > 1) {
    _children[1]->SetRect(split_x, 0, _geometry._w - split_x, _geometry._h);
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

  int split_y = int(_geometry._h * _split_ratio);

  // 0th child = top
  if (_children.size() > 0) {
    _children[0]->SetRect(0, 0, _geometry._w, split_y);
  }

  // 1st child = bottom
  if (_children.size() > 1) {
    _children[1]->SetRect(0, split_y, _geometry._w, _geometry._h - split_y);
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
