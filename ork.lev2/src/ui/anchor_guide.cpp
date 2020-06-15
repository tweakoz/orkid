#include <ork/pch.h>
#include <ork/lev2/ui/anchor.h>
#include <ork/lev2/ui/widget.h>

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
    case Edge::Horizontal:
      rval = "Horizontal";
      break;
    case Edge::Vertical:
      rval = "Vertical";
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
void Guide::setMargin(int margin) {
  _margin = margin;
  updateAssociates();
}
/////////////////////////////////////////////////////////////////////////
void Guide::updateAssociates() {
  for (auto g : _associates)
    g->updateGeometry();
}
/////////////////////////////////////////////////////////////////////////
void Guide::updateGeometry() {
  if (_relative == nullptr)
    return;

  auto rel = _relationshipWith(_relative);
  if (rel == Relationship::None)
    return;

  auto geo = _layout->_widget->geometry();

  auto relguide = (Relationship::Sibling == rel) //
                      ? _relative->line(Mode::Geometry)
                      : _relative->line(Mode::Rect);

  int signed_offset = _margin * _sign;

  int basex = relguide._from.x;
  int basey = relguide._from.y;
  printf(
      "guide<%d> relative<%d> rel<%s> edge<%s> offs<%d> base<%d,%d>\n", //
      _name,
      _relative ? _relative->_name : -1,
      rel2str(rel).c_str(),
      edge2str(_edge).c_str(),
      signed_offset,
      basex,
      basey);

  switch (_edge) {
    case Edge::Top: {
      int top = relguide._from.y + signed_offset;
      if (_layout->_bottom->_relative)
        geo.setTop(top);
      else
        geo.moveTop(top);
      break;
    }
    case Edge::Left: {
      int left = relguide._from.x + signed_offset;
      if (_layout->_right->_relative)
        geo.setLeft(left);
      else
        geo.moveLeft(left);
      break;
    }
    case Edge::Bottom: {
      int bottom = relguide._from.y - signed_offset;
      if (_layout->_top->_relative)
        geo.setBottom(bottom);
      else
        geo.moveBottom(bottom);
      break;
    }
    case Edge::Right: {
      int right = relguide._from.x - signed_offset;
      if (_layout->_left->_relative)
        geo.setRight(right);
      else
        geo.moveRight(right);
      break;
    }
    case Edge::HorizontalCenter:
      geo.moveCenter(relguide._from.x + signed_offset, geo.center_y());
      break;
    case Edge::VerticalCenter:
      geo.moveCenter(geo.center_x(), relguide._from.y + signed_offset);
      break;
    default:
      break;
  }

  _layout->_widget->setGeometry(geo);

  updateAssociates();
}
/////////////////////////////////////////////////////////////////////////
void Guide::_associate(Guide* other) {
  OrkAssert(other);
  OrkAssert(_associates.find(other) == _associates.end());
  _associates.insert(other);
  //_layout->update();
}
/////////////////////////////////////////////////////////////////////////
void Guide::_disassociate(Guide* other) {
  OrkAssert(other);
  auto it = _associates.find(other);
  OrkAssert(it != _associates.end());
  _associates.erase(it);
  //_layout->update();
}
/////////////////////////////////////////////////////////////////////////
void Guide::anchorTo(guide_ptr_t other) {
  anchorTo(other.get());
}
/////////////////////////////////////////////////////////////////////////
void Guide::anchorTo(Guide* other) {
  if (_edge == Edge::Horizontal or _edge == Edge::Vertical)
    return;

  if (other == _relative)
    return;

  if (other != nullptr and _layout == other->_layout)
    return;

  if (_relative != nullptr) {
    _relative->_disassociate(this);
    _relative = nullptr;
  }

  if (other == nullptr)
    return;

  if (this->isVertical() and not other->isVertical())
    return;

  // paranoia check
  if (this->isHorizontal() and not other->isHorizontal())
    return;

  _relative = other;
  _relative->_associate(this);
}
/////////////////////////////////////////////////////////////////////////
bool Guide::isVertical() const {
  return _edge == Edge::Left or             //
         _edge == Edge::Right or            //
         _edge == Edge::HorizontalCenter or //
         _edge == Edge::Vertical;
}
/////////////////////////////////////////////////////////////////////////
bool Guide::isHorizontal() const {
  return _edge == Edge::Top or            //
         _edge == Edge::Bottom or         //
         _edge == Edge::VerticalCenter or //
         _edge == Edge::Horizontal;
}
/////////////////////////////////////////////////////////////////////////
Relationship Guide::_relationshipWith(Guide* other) const {

  auto mywidget    = _layout->_widget;
  auto otherwidget = other->_layout->_widget;

  if (otherwidget.get() == mywidget->parent())
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
    case Edge::Horizontal: {
      float y       = float(rect._y) + float(rect._h) * _unito;
      outline._from = fvec2(rect._x, y);
      outline._to   = fvec2(rect.x2(), y);
      break;
    };
    case Edge::Vertical: {
      float x       = float(rect._x) + float(rect._w) * _unito;
      outline._from = fvec2(x, rect._y);
      outline._to   = fvec2(x, rect.y2());
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
void Guide::dump() {
  printf(
      "//   Guide<%d> edge<%s> margin<%d> relative<%d>\n", //
      _name,
      edge2str(_edge).c_str(),
      _margin,
      _relative ? _relative->_name : -1);
  for (auto g : _associates) {
    printf(
        "//     associate<%d> layout<%d> edge<%s> margin<%d>\n", //
        g->_name,
        g->_layout->_name,
        edge2str(g->_edge).c_str(),
        g->_margin);
  }
}
/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui::anchor
