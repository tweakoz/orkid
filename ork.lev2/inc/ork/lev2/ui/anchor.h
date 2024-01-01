#pragma once

/////////////////////////////////////////////////////////////////////////
// port of
// https://github.com/pnudupa/anchorlayout
// Original Author: Prashanth Udupa
// LGPL3
/////////////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/event.h>
#include <functional>
#include <unordered_set>

namespace ork::ui::anchor {

/////////////////////////////////////////////////////////////////////////

enum class Edge { //
  BaseLine = 0,
  Top,
  Left,
  Bottom,
  Right,
  HorizontalCenter,
  VerticalCenter,
  CustomHorizontal,
  CustomVertical,
};

enum class Mode { //
  Geometry,
  Rect
};

struct Line {
  fvec2 _from, _to;
};

using line_ptr_t = std::shared_ptr<Line>;

/////////////////////////////////////////////////////////////////////////

enum class Relationship { //
  None = 0,
  Sibling,
  ParentChild
};

/////////////////////////////////////////////////////////////////////////

using visit_set = std::unordered_set<int>;

struct Layout {

  Layout(Widget* w);
  ~Layout();

  layout_ptr_t childLayout(Widget* w);
  void removeChild(layout_ptr_t l);

  void setMargin(int margin);

  void centerIn(Layout* other);
  bool isAnchorAllowed(guide_ptr_t guide) const;
  bool isAnchorAllowed(Layout* guide) const;
  void fill(Layout* other);

  void updateAll();

  void _doUpdateAll(visit_set& vset);

  guide_ptr_t top();
  guide_ptr_t left();
  guide_ptr_t bottom();
  guide_ptr_t right();
  guide_ptr_t centerH();
  guide_ptr_t centerV();

  guide_ptr_t proportionalHorizontalGuide(float proportion);
  guide_ptr_t proportionalVerticalGuide(float proportion);
  guide_ptr_t fixedHorizontalGuide(int fixed);
  guide_ptr_t fixedVerticalGuide(int fixed);

  void dump(int level=0);
  void prune();

  using visit_fn_t = std::function<void(Layout* l)>;
  using guide_visit_fn = std::function<void(Guide* g)>;

  void visitHierarchy(visit_fn_t vfn);
  void visitGuides(guide_visit_fn gfn);

  int _margin = 0;
  int _name   = -1;

  Widget* _widget = nullptr;

  Layout* _parent = nullptr;
  layout_ptr_t _fill   = nullptr;
  layout_ptr_t _center = nullptr;

  guide_ptr_t _top     = nullptr;
  guide_ptr_t _left    = nullptr;
  guide_ptr_t _bottom  = nullptr;
  guide_ptr_t _right   = nullptr;
  guide_ptr_t _centerH = nullptr;
  guide_ptr_t _centerV = nullptr;

  bool _locked = false;

  std::set<guide_ptr_t> _customguides;
  std::vector<layout_ptr_t> _childlayouts;
};

/////////////////////////////////////////////////////////////////////////

enum class GuideType : uint64_t {
  FIXED = 0,
  PROPORTIONAL,
  NONE
};

struct Guide {

  Guide(Layout* layout, Edge edge);
  ~Guide();

  void anchorTo(guide_ptr_t other);
  void anchorTo(Guide* other);
  void setMargin(int margin);
  bool isVertical() const;
  bool isHorizontal() const;
  void updateAssociates(visit_set& vset);
  void updateGeometry();
  Relationship _relationshipWith(Guide* other) const;
  Line line(Mode mode) const;

  void _disassociate(Guide* other);
  void _associate(Guide* other);
  float sortKey() const;
  void dump(int level=0);

  std::set<Guide*> _associates;
  int _name         = -1;
  Layout* _layout   = nullptr;
  Guide* _relative  = nullptr;
  Edge _edge        = Edge::Top;
  int _margin       = 0;
  int _sign         = 1; // sign of offset: -1 or 1
  float _proportion = 0.0f;
  int _fixed         = 0;
  int _centerpos = 0;
  bool _locked = false;
  GuideType _type = GuideType::NONE;
};

/////////////////////////////////////////////////////////////////////////

struct Bounds {
  guide_ptr_t _top;
  guide_ptr_t _left;
  guide_ptr_t _bottom;
  guide_ptr_t _right;
  int _margin = 4;
};

/////////////////////////////////////////////////////////////////////////

} // namespace ork::ui::anchor
