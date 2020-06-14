#pragma once

// port of
// https://github.com/pnudupa/anchorlayout
// LGPL3

#include <ork/lev2/ui/event.h>
#include <functional>

namespace ork::ui {
struct Widget;
}

namespace ork::ui::anchor {

struct Guide;

/////////////////////////////////////////////////////////////////////////

enum class Edge { //
  BaseLine = 0,
  Top,
  Left,
  Bottom,
  Right,
  HorizontalCenter,
  VerticalCenter,
  Horizontal,
  Vertical
};

enum class Mode { //
  Geometry,
  Rect
};

struct Line {
  fvec2 _from, _to;
};

/////////////////////////////////////////////////////////////////////////

enum class Relationship { //
  None = 0,
  Sibling,
  ParentChild
};

/////////////////////////////////////////////////////////////////////////

struct Layout {

  void setMargin(int margin);

  int _margin = 0;

  Widget* _widget = nullptr;

  Layout* _fill   = nullptr;
  Layout* _center = nullptr;

  Guide* _top     = nullptr;
  Guide* _left    = nullptr;
  Guide* _bottom  = nullptr;
  Guide* _right   = nullptr;
  Guide* _centerH = nullptr;
  Guide* _centerV = nullptr;
};

/////////////////////////////////////////////////////////////////////////

struct Guide {

  void setOffset(int margin);
  bool isVertical() const;
  bool isHorizontal() const;
  void updateAssociates();
  void updateGeometry();
  Relationship relationshipWith(const Guide* other) const;
  Line line(Mode mode) const;

  std::set<Guide*> _associates;
  Layout* _layout = nullptr;
  Guide* _parent  = nullptr;
  Edge _edge      = Edge::Top;
  int _offset     = 0;
  int _sign       = 1; // sign of offset: -1 or 1
  float _unito    = 0.0f;
};

} // namespace ork::ui::anchor
