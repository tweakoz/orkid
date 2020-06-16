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
      break;
    }
    case Edge::Left: {
      int left = line._from.x + signed_offset;
      if (_layout->_right->_relative)
        geo.setLeft(left);
      else
        geo.moveLeft(left);
      break;
    }
    case Edge::Bottom: {
      int bottom = line._from.y - signed_offset;
      if (_layout->_top->_relative)
        geo.setBottom(bottom);
      else
        geo.moveBottom(bottom);
      break;
    }
    case Edge::Right: {
      int right = line._from.x - signed_offset;
      if (_layout->_left->_relative)
        geo.setRight(right);
      else
        geo.moveRight(right);
      break;
    }
    case Edge::HorizontalCenter:
      geo.moveCenter(line._from.x + signed_offset, geo.center_y());
      break;
    case Edge::VerticalCenter:
      geo.moveCenter(geo.center_x(), line._from.y + signed_offset);
      break;
    default:
      break;
  }

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
    case Edge::CustomHorizontal: {
      if (_proportion != 0.0f) {
        float y       = float(rect._y) + float(rect._h) * _proportion;
        outline._from = fvec2(rect._x, y);
        outline._to   = fvec2(rect.x2(), y);
      } else if (_fixed > 0) {
        outline._from = fvec2(rect._x, _fixed);
        outline._to   = fvec2(rect.x2(), _fixed);
      } else if (_fixed < 0) {
        outline._from = fvec2(rect._x, rect._h + _fixed);
        outline._to   = fvec2(rect.x2(), rect._h + _fixed);
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
        outline._from = fvec2(rect._w + _fixed, rect._y);
        outline._to   = fvec2(rect._w + _fixed, rect.y2());
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
} // namespace ork::ui::anchor
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
