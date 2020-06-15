#pragma once

/////////////////////////////////////////////////////////////////////////
// port of
// https://github.com/pnudupa/anchorlayout
// Original Author: Prashanth Udupa
// LGPL3
/////////////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/event.h>
#include <functional>

namespace ork::ui {
struct Widget;
}

namespace ork::ui::anchor {

struct Guide;
struct Layout;

using guide_ptr_t       = std::shared_ptr<Guide>;
using guide_constptr_t  = std::shared_ptr<const Guide>;
using layout_ptr_t      = std::shared_ptr<Layout>;
using layout_constptr_t = std::shared_ptr<const Layout>;

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

  Layout(widget_ptr_t w);
  ~Layout();

  void setMargin(int margin);
  void centerIn(Layout* other);
  bool isAnchorAllowed(guide_ptr_t guide) const;
  bool isAnchorAllowed(Layout* guide) const;
  void fill(Layout* other);

  guide_ptr_t top();
  guide_ptr_t left();
  guide_ptr_t bottom();
  guide_ptr_t right();
  guide_ptr_t centerH();
  guide_ptr_t centerV();

  int _margin = 0;

  widget_ptr_t _widget;

  layout_ptr_t _fill   = nullptr;
  layout_ptr_t _center = nullptr;

  guide_ptr_t _top     = nullptr;
  guide_ptr_t _left    = nullptr;
  guide_ptr_t _bottom  = nullptr;
  guide_ptr_t _right   = nullptr;
  guide_ptr_t _centerH = nullptr;
  guide_ptr_t _centerV = nullptr;
};

/////////////////////////////////////////////////////////////////////////

struct Guide {

  Guide(Layout* layout, Edge edge);

  void anchorTo(guide_ptr_t other);
  void anchorTo(Guide* other);
  void setMargin(int margin);
  bool isVertical() const;
  bool isHorizontal() const;
  void updateAssociates();
  void updateGeometry();
  Relationship _relationshipWith(Guide* other) const;
  Line line(Mode mode) const;

  void _disassociate(Guide* other);
  void _associate(Guide* other);

  std::set<Guide*> _associates;
  Layout* _layout = nullptr;
  Guide* _parent  = nullptr;
  Edge _edge      = Edge::Top;
  int _margin     = 0;
  int _sign       = 1; // sign of offset: -1 or 1
  float _unito    = 0.0f;
};

} // namespace ork::ui::anchor
