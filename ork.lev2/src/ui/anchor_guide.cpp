#include <ork/pch.h>
#include <ork/lev2/ui/anchor.h>
#include <ork/lev2/ui/group.h>

/////////////////////////////////////////////////////////////////////////
// port of
// https://github.com/pnudupa/anchorlayout
// Original Author: Prashanth Udupa
// LGPL3
/////////////////////////////////////////////////////////////////////////

namespace ork::ui::anchor {
/////////////////////////////////////////////////////////////////////////
std::string rel2str(Relationship r) {
  std::string rval;
  switch (r) {
    case Relationship::None:
      rval = "None";
      break;
    case Relationship::Sibling:
      rval = "Sibling";
      break;
    case Relationship::ParentChild:
      rval = "ParentChild";
      break;
  }
  return rval;
}
/////////////////////////////////////////////////////////////////////////
std::string edge2str(Edge e) {
  std::string rval;
  switch (e) {
    case Edge::BaseLine:
      rval = "BaseLine";
      break;
    case Edge::Top:
      rval = "Top";
      break;
    case Edge::Left:
      rval = "Left";
      break;
    case Edge::Bottom:
      rval = "Bottom";
      break;
    case Edge::Right:
      rval = "Right";
      break;
    case Edge::HorizontalCenter:
      rval = "HorizontalCenter";
      break;
    case Edge::VerticalCenter:
      rval = "VerticalCenter";
      break;
    case Edge::CustomHorizontal:
      rval = "CustomHorizontal";
      break;
    case Edge::CustomVertical:
      rval = "CustomVertical";
      break;
  }
  return rval;
}
/////////////////////////////////////////////////////////////////////////
Guide::Guide(Layout* layout, Edge edge)
    : _layout(layout)
    , _edge(edge) {
  static int _names = 0;
  _name             = _names++;
}
/////////////////////////////////////////////////////////////////////////
Guide::~Guide() {
}
/////////////////////////////////////////////////////////////////////////
float Guide::sortKey() const {
  bool is_proportional = _type == GuideType::PROPORTIONAL;
  return is_proportional ? _proportion : float(_fixed);
}
/////////////////////////////////////////////////////////////////////////
void Guide::setMargin(int margin) {
  _margin = margin;
  visit_set vset;
  updateAssociates(vset);
}
/////////////////////////////////////////////////////////////////////////
void Guide::updateAssociates(visit_set& vset) {
  for (auto g : _associates) {
    auto it = vset.find(g->_name);
    if (it == vset.end()) {
      vset.insert(g->_name);
      g->updateGeometry();
      g->updateAssociates(vset);
    }
  }
}
/////////////////////////////////////////////////////////////////////////
void Guide::updateGeometry() {

  if (_relative == nullptr)
    return;

  int signed_offset = _margin * _sign;

  auto relationship = _relationshipWith(_relative);

  if (0)
    printf(
        "guide<%d> relative<%d> rel<%s> edge<%s> offs<%d>\n", //
        _name,
        _relative ? _relative->_name : -1,
        rel2str(relationship).c_str(),
        edge2str(_edge).c_str(),
        signed_offset);

  if (relationship == Relationship::None)
    return;

  auto geo = _layout->_widget->geometry();

  ///////////////////

  Line line;
  switch (relationship) {
    case Relationship::Sibling:
      line = _relative->line(Mode::Geometry);
      break;
    default:
      line = _relative->line(Mode::Rect);
      break;
  }

  ///////////////////

  int basex = line._from.x;
  int basey = line._from.y;

  if (0)
    printf(
        "guide<%d> base<%d,%d>\n", //
        _name,
        basex,
        basey);

  switch (_edge) {
    case Edge::Top: {
      int top = line._from.y + signed_offset;
      if (_layout->_bottom->_relative)
        geo.setTop(top);
      else
        geo.moveTop(top);
      _centerpos = geo._y;
      break;
    }
    case Edge::Left: {
      int left = line._from.x + signed_offset;
      if (_layout->_right->_relative)
        geo.setLeft(left);
      else
        geo.moveLeft(left);
      _centerpos = geo._x;
      break;
    }
    case Edge::Bottom: {
      int bottom = line._from.y - signed_offset;
      _centerpos = bottom;
      if (_layout->_top->_relative)
        geo.setBottom(bottom);
      else
        geo.moveBottom(bottom);
      _centerpos = geo.y2();
      break;
    }
    case Edge::Right: {
      int right = line._from.x - signed_offset;
      if (_layout->_left->_relative)
        geo.setRight(right);
      else
        geo.moveRight(right);
      _centerpos = geo.x2();
      break;
    }
    case Edge::HorizontalCenter:
      geo.moveCenter(line._from.x + signed_offset, geo.center_y());
      _centerpos = geo.center_x();
      break;
    case Edge::VerticalCenter:
      geo.moveCenter(geo.center_x(), line._from.y + signed_offset);
      _centerpos = geo.center_y();
      break;
    default:
      break;
  }

  if (0)
    printf("setgeo<%s> %d,%d %d,%d\n", _layout->_widget->_name.c_str(), geo._x, geo._y, geo._w, geo._h);
  _layout->_widget->setGeometry(geo);
}
/////////////////////////////////////////////////////////////////////////
void Guide::_associate(Guide* other) {
  OrkAssert(other);
  _associates.insert(other);
  //_layout->updateAll();
}
/////////////////////////////////////////////////////////////////////////
void Guide::_disassociate(Guide* other) {
  OrkAssert(other);
  auto it = _associates.find(other);
  OrkAssert(it != _associates.end());
  _associates.erase(it);
  //_layout->updateAll();
}
/////////////////////////////////////////////////////////////////////////
void Guide::anchorTo(guide_ptr_t other) {
  anchorTo(other.get());
}
/////////////////////////////////////////////////////////////////////////
void Guide::anchorTo(Guide* other) {

  //////////////////////////////////////////
  // sanity checks
  //////////////////////////////////////////

  // custom guides (by definition)
  //  are not driven by anchors

  OrkAssert(_edge != Edge::CustomHorizontal);
  OrkAssert(_edge != Edge::CustomVertical);

  //////////////////////////////////////////

  if (_relative != nullptr) {
    _relative->_disassociate(this);
    _relative = nullptr;
  }

  //////////////////////////////////////////

  if (other) {
    OrkAssert(this->isVertical() == other->isVertical());
    OrkAssert(this->isHorizontal() == other->isHorizontal());
    OrkAssert(other != _relative);
    // OrkAssert(_layout == other->_layout);

    _relative = other;
    _relative->_associate(this);

    if (0)
      printf("guide<%p> anchorTo<%p>\n", this, other);

  } else
    _relative = nullptr;
}
/////////////////////////////////////////////////////////////////////////
bool Guide::isVertical() const {
  return _edge == Edge::Left or             //
         _edge == Edge::Right or            //
         _edge == Edge::HorizontalCenter or //
         _edge == Edge::CustomVertical;
}
/////////////////////////////////////////////////////////////////////////
bool Guide::isHorizontal() const {
  return _edge == Edge::Top or            //
         _edge == Edge::Bottom or         //
         _edge == Edge::VerticalCenter or //
         _edge == Edge::CustomHorizontal;
}
/////////////////////////////////////////////////////////////////////////
Relationship Guide::_relationshipWith(Guide* other) const {

  auto mywidget    = _layout->_widget;
  auto otherwidget = other->_layout->_widget;

  if (otherwidget == mywidget->parent())
    return Relationship::ParentChild;

  if (otherwidget->parent() == mywidget->parent())
    return Relationship::Sibling;

  return Relationship::None;
}
/////////////////////////////////////////////////////////////////////////
Line Guide::line(Mode mode) const {
  auto widget_geo = _layout->_widget->geometry();

  auto rect = (mode == Mode::Geometry) //
                  ? widget_geo
                  : Rect(0, 0, widget_geo._w, widget_geo._h);

  Line outline;

  int m = _margin * _sign;

  switch (_edge) {
    case Edge::BaseLine:
      outline._from = fvec2(rect._x, rect._y);
      outline._to   = fvec2(rect.x2(), rect._y);
      break;
    case Edge::Top:
      outline._from = fvec2(rect._x, rect._y);
      outline._to   = fvec2(rect.x2(), rect._y);
      break;
    case Edge::Left:
      outline._from = fvec2(rect._x, rect._y);
      outline._to   = fvec2(rect._x, rect.y2());
      break;
    case Edge::Bottom:
      outline._from = fvec2(rect._x, rect.y2());
      outline._to   = fvec2(rect.x2(), rect.y2());
      break;
    case Edge::Right:
      outline._from = fvec2(rect.x2(), rect._y);
      outline._to   = fvec2(rect.x2(), rect.y2());
      break;
    case Edge::CustomHorizontal: {
      if (_proportion != 0.0f) {
        float y       = float(rect._y) + float(rect._h) * _proportion;
        outline._from = fvec2(rect._x, y);
        outline._to   = fvec2(rect.x2(), y);
      } else if (_fixed > 0) {
        outline._from = fvec2(rect._x, _fixed);
        outline._to   = fvec2(rect.x2(), _fixed);
      } else if (_fixed < 0) {
        outline._from = fvec2(rect._x, rect._y + rect._h + _fixed);
        outline._to   = fvec2(rect.x2(), rect._y + rect._h + _fixed);
      }
      break;
    };
    case Edge::CustomVertical: {
      if (_proportion != 0.0f) {
        float x       = float(rect._x) + float(rect._w) * _proportion;
        outline._from = fvec2(x, rect._y);
        outline._to   = fvec2(x, rect.y2());
      } else if (_fixed > 0) {
        outline._from = fvec2(_fixed, rect._y);
        outline._to   = fvec2(_fixed, rect.y2());
      } else if (_fixed < 0) {
        outline._from = fvec2(rect._x + rect._w + _fixed, rect._y);
        outline._to   = fvec2(rect._x + rect._w + _fixed, rect.y2());
      }
      break;
    };
    case Edge::HorizontalCenter:
      outline._from = fvec2(rect.center_x(), rect._y);
      outline._to   = fvec2(rect.center_x(), rect.y2());
      break;
    case Edge::VerticalCenter:
      outline._from = fvec2(rect._x, rect.center_y());
      outline._to   = fvec2(rect.x2(), rect.center_y());
      break;
  }

  return outline;
} 
/////////////////////////////////////////////////////////////////////////
void Guide::dump(int level) {
  auto indentstr = std::string(level * 2, ' ');
  printf(
      "%sGuide<%d> edge<%s> margin<%d> relative<%d>", //
      indentstr.c_str(),
      _name,
      edge2str(_edge).c_str(),
      _margin,
      _relative ? _relative->_name : -1);

  switch (_type) {
    case GuideType::NONE:
      printf(" type<NONE>");
      break;
    case GuideType::FIXED:
      printf(" type<FIXED> f<%d>", _fixed);
      break;
    case GuideType::PROPORTIONAL:
      printf(" type<PROPORTIONAL> p<%g>", _proportion);
      break;
    default:
      OrkAssert(false);
      break;
  }
  printf("\n");
  for (auto g : _associates) {
    printf(
        "%sassociate<%d> layout<%d> edge<%s> margin<%d>\n", //
        indentstr.c_str(),
        g->_name,
        g->_layout->_name,
        edge2str(g->_edge).c_str(),
        g->_margin);
  }
}
/////////////////////////////////////////////////////////////////////////
static float _distanceFromPointToLine(const fvec2& point, const fvec2& lineStart, const fvec2& lineEnd) {
  float lineLength = std::hypot(lineEnd.x - lineStart.x, lineEnd.y - lineStart.y);
  if (lineLength == 0.0f)
    return std::hypot(point.x - lineStart.x, point.y - lineStart.y);

  float t = ((point.x - lineStart.x) * (lineEnd.x - lineStart.x) + (point.y - lineStart.y) * (lineEnd.y - lineStart.y)) /
            (lineLength * lineLength);
  t = std::max(0.0f, std::min(1.0f, t));
  fvec2 projection;
  projection.x = lineStart.x + t * (lineEnd.x - lineStart.x);
  projection.y = lineStart.y + t * (lineEnd.y - lineStart.y);
  return std::hypot(point.x - projection.x, point.y - projection.y);
}
/////////////////////////////////////////////////////////////////////////
// Function to check if a point (mouse position) is over a guide
/////////////////////////////////////////////////////////////////////////
static bool _isMouseOverGuide(const Guide* guide, const fvec2& mousePos, float threshold = 5.0f) {
  if (!guide)
    return false;
  Line line = guide->line(Mode::Geometry);

  // Transform guide coordinates to root space
  Widget* widget = guide->_layout->_widget;
  if (widget && widget->parent()) {
    // Walk up the parent chain to accumulate offsets
    Widget* parent = widget->parent();
    while (parent) {
      auto parent_geo = parent->geometry();
      line._from.x += parent_geo._x;
      line._from.y += parent_geo._y;
      line._to.x += parent_geo._x;
      line._to.y += parent_geo._y;
      parent = parent->parent();
    }
  }

  float distance = _distanceFromPointToLine(mousePos, line._from, line._to);
  // The entire margin area should be draggable
  bool is_over = distance <= float(guide->_margin);
  if(is_over){
    //printf("is_over guide<%d> edge<%s> distance<%g> threshold<%g> pos<%g,%g>\n", guide->_name, edge2str(guide->_edge).c_str(), distance, threshold, mousePos.x, mousePos.y);
  }
  return is_over;
}
/////////////////////////////////////////////////////////////////////////
// Function to get all guides from a layout
/////////////////////////////////////////////////////////////////////////
static std::vector<guide_ptr_t> _getAllGuides(const Layout* layout) {
  std::vector<guide_ptr_t> guides;
  if (layout->_top)
    guides.push_back(layout->_top);
  if (layout->_left)
    guides.push_back(layout->_left);
  if (layout->_bottom)
    guides.push_back(layout->_bottom);
  if (layout->_right)
    guides.push_back(layout->_right);
  if (layout->_centerH)
    guides.push_back(layout->_centerH);
  if (layout->_centerV)
    guides.push_back(layout->_centerV);
  for (auto& custom_guide : layout->_customguides)
    guides.push_back(custom_guide);
  return guides;
}
/////////////////////////////////////////////////////////////////////////
// Find the closest draggable guide under the mouse
/////////////////////////////////////////////////////////////////////////
static guide_ptr_t _findClosestDraggableGuide(const Layout* rootLayout, const fvec2& mousePos) {
  if (!rootLayout)
    return nullptr;

  auto draggableGuides = rootLayout->getDraggableGuides();

  guide_ptr_t closestGuide = nullptr;
  float closestDistance = std::numeric_limits<float>::max();

  for (const auto& guide : draggableGuides) {
    Line line = guide->line(Mode::Geometry);

    // The line from Mode::Geometry is already in the widget's parent coordinates
    // Since all widgets are children of the root in this layout system,
    // the geometry coordinates are already in root space
    // No transformation needed

    float distance = _distanceFromPointToLine(mousePos, line._from, line._to);

    // The guide's margin extends on both sides of the guide line
    // So if margin is 3, the draggable area is 3 pixels on each side = 6 pixels total
    // The distance check should be against the full margin size since distance is from the center line
    float threshold = float(guide->_margin);

    // Debug logging for guides at different depths
    if (distance <= threshold * 2.0f) {
      Widget* widget = guide->_layout->_widget;
      printf("check guide<%d> layout<%d> widget<%s> edge<%s> distance<%g> threshold<%g> line[%g,%g - %g,%g] mouse[%g,%g]\n",
             guide->_name, guide->_layout->_name, widget->_name.c_str(),
             edge2str(guide->_edge).c_str(), distance, threshold,
             line._from.x, line._from.y, line._to.x, line._to.y,
             mousePos.x, mousePos.y);
    }

    if (distance <= threshold && distance < closestDistance) {
      closestDistance = distance;
      closestGuide = guide;
    }
  }

  return closestGuide;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t findGuidePairUnderMouse(const Layout* rootLayout, const fvec2& mousePos) {
  // For compatibility, we now return the same guide twice if found
  // This signals that we found a draggable guide
  auto guide = _findClosestDraggableGuide(rootLayout, mousePos);
  return guide;
}
/////////////////////////////////////////////////////////////////////////
static void _adjustGuidePositionVProportional(const guide_ptr_t& guide, float deltaX) {
    auto layout_dimensions = guide->_layout->_widget->geometry();
    float try_new_proportion = guide->_proportion + (deltaX / static_cast<float>(layout_dimensions._w));
    try_new_proportion = std::max(0.05f, std::min(0.95f, try_new_proportion)); 
    int try_size_left = static_cast<int>(try_new_proportion * layout_dimensions._w);
    int try_size_right = layout_dimensions._w - try_size_left;
    if (try_size_left >= 8 && try_size_right >= 8) {
        guide->_proportion = try_new_proportion;
        guide->_layout->updateAll();
    }
}
/////////////////////////////////////////////////////////////////////////
static void _adjustGuidePositionVFixed(const guide_ptr_t& guide, float deltaX) {
    auto layout_dimensions = guide->_layout->_widget->geometry();
    int new_fixed = guide->_fixed + static_cast<int>(deltaX);
    int try_size_left = new_fixed;
    int try_size_right = layout_dimensions._w - new_fixed;
    if (try_size_left >= 8 && try_size_right >= 8) {
        guide->_fixed = new_fixed;
        guide->_layout->updateAll();
    }
}
/////////////////////////////////////////////////////////////////////////
static void _adjustGuidePositionHProportional(const guide_ptr_t& guide, float deltaY) {
    auto layout_dimensions = guide->_layout->_widget->geometry();
    float try_new_proportion = guide->_proportion + (deltaY / static_cast<float>(layout_dimensions._h));
    try_new_proportion = std::max(0.05f, std::min(0.95f, try_new_proportion)); 
    int try_size_above = static_cast<int>(try_new_proportion * layout_dimensions._h);
    int try_size_below = layout_dimensions._h - try_size_above;
    if (try_size_above >= 8 && try_size_below >= 8) {
        guide->_proportion = try_new_proportion;
        guide->_layout->updateAll();
    }
}
/////////////////////////////////////////////////////////////////////////
static void _adjustGuidePositionHFixed(const guide_ptr_t& guide, float deltaY) {
    auto layout_dimensions = guide->_layout->_widget->geometry();
    int try_new_fixed = guide->_fixed + static_cast<int>(deltaY);
    int try_size_above = try_new_fixed;
    int try_size_below = layout_dimensions._h - try_new_fixed;
    if (try_size_above >= 8 && try_size_below >= 8) {
        guide->_fixed = try_new_fixed;
        guide->_layout->updateAll();
    }
}
/////////////////////////////////////////////////////////////////////////
static void _dragGuideH(const guide_ptr_t& guide, float deltaY) {
  switch (guide->_edge) {
    case Edge::CustomHorizontal:
      if(guide->_type == GuideType::FIXED)
        _adjustGuidePositionHFixed(guide, deltaY);
      else
        _adjustGuidePositionHProportional(guide, deltaY);
      break;
    default:
      // Not a horizontal guide, no action needed
      break;
  }
}
/////////////////////////////////////////////////////////////////////////
static void _dragGuideV(const guide_ptr_t& guide, float deltaX) {
  switch (guide->_edge) {
    case Edge::CustomVertical:
      if(guide->_type == GuideType::FIXED)
        _adjustGuidePositionVFixed(guide, deltaX);
      else
        _adjustGuidePositionVProportional(guide, deltaX);
      break;
    default:
      // Not a vertical guide, no action needed
      break;
  }
}
/////////////////////////////////////////////////////////////////////////
void dragGuidePairH(guide_ptr_t guide, float deltaY) {
  // Now we just drag the single guide (first and second are the same)
  _dragGuideH(guide, deltaY);
}
/////////////////////////////////////////////////////////////////////////
void dragGuidePairV(guide_ptr_t guide, float deltaX) {
  // Now we just drag the single guide (first and second are the same)
  _dragGuideV(guide, deltaX);
}
/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui::anchor
