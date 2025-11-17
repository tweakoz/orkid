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

enum class Edge : crc_enum_t {
  CrcEnum(BaseLine),
  CrcEnum(Top),
  CrcEnum(Left),
  CrcEnum(Bottom),
  CrcEnum(Right),
  CrcEnum(HorizontalCenter),
  CrcEnum(VerticalCenter),
  CrcEnum(CustomHorizontal),
  CrcEnum(CustomVertical),
};

enum class ELayoutSplitPlacement : crc_enum_t {
  CrcEnum(TOP),
  CrcEnum(BOTTOM),
  CrcEnum(LEFT),
  CrcEnum(RIGHT),
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
  void setProportionalRect(Layout* parent, float x, float y, float w, float h, bool locked = false);
  void setFixedRect(Layout* parent, int x, int y, int w, int h, bool locked = false);

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
  guide_ptr_t offsetHorizontalGuide(guide_ptr_t base, int offset);
  guide_ptr_t offsetVerticalGuide(guide_ptr_t base, int offset);
  void setRect(guide_ptr_t top, guide_ptr_t left, guide_ptr_t right, guide_ptr_t bottom);
  void lockAllGuides();
  void dump(int level=0);
  void prune();

  // Find guide between two layouts (returns nullptr if not found or ambiguous)
  guide_ptr_t findGuideBetween(layout_ptr_t layout_a, layout_ptr_t layout_b);

  using visit_fn_t = std::function<void(Layout* l)>;
  using guide_visit_fn = std::function<void(Guide* g)>;

  void visitHierarchy(visit_fn_t vfn);
  void visitGuides(guide_visit_fn gfn);
  std::vector<guide_ptr_t> getDraggableGuides() const;

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

enum class GuideType : crc_enum_t {
  CrcEnum(FIXED),
  CrcEnum(PROPORTIONAL),
  CrcEnum(OFFSET),
  CrcEnum(NONE)
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

  // Programmatic control API
  void setProportion(float new_proportion);
  void setFixed(int new_fixed);
  void lock() { _locked = true; }
  void unlock() { _locked = false; }

  // Query API
  float getProportion() const { return _proportion; }
  int getFixed() const { return _fixed; }
  GuideType getType() const { return _type; }
  bool isLocked() const { return _locked; }
  bool isClamped() const { return _clamped; }
  Edge getEdge() const { return _edge; }
  int getMargin() const { return _margin; }

  // Clamping control
  void clamp() { _clamped = true; }
  void unclamp() { _clamped = false; }

  std::set<Guide*> _associates;
  int _name         = -1;
  Layout* _layout   = nullptr;
  Guide* _relative  = nullptr;
  Edge _edge        = Edge::Top;
  int _margin       = 0;
  int _sign         = 1; // sign of offset: -1 or 1
  float _proportion = 0.0f;
  int _fixed         = 0;
  int _offset        = 0;  // offset in pixels from _offset_base (for OFFSET type)
  int _centerpos = 0;
  bool _locked = false;
  bool _clamped = true;  // clamp offset guides to layout bounds (default true)
  GuideType _type = GuideType::NONE;
  Guide* _offset_base = nullptr;  // base guide for OFFSET type
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
