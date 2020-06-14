#include <ork/pch.h>
#include <ork/lev2/ui/anchor.h>
#include <ork/lev2/ui/widget.h>

// port of
// https://github.com/pnudupa/anchorlayout
// LGPL3

namespace ork::ui::anchor {

/////////////////////////////////////////////////////////////////////////

void Layout::setMargin(int margin) {

  _margin = margin;

  if (_left)
    _left->setOffset(margin);

  if (_top)
    _top->setOffset(margin);

  if (_right)
    _right->setOffset(margin);

  if (_bottom)
    _bottom->setOffset(margin);
}

/////////////////////////////////////////////////////////////////////////

void Guide::setOffset(int offset) {
  _offset = offset;
  updateAssociates();
}

/////////////////////////////////////////////////////////////////////////

void Guide::updateAssociates() {
  for (auto g : _associates)
    g->updateGeometry();
}

void Guide::updateGeometry() {
  if (_parent == nullptr)
    return;

  auto rel = relationshipWith(_parent);
  if (rel == Relationship::None)
    return;

  auto geo = _layout->_widget->geometry();

  auto relguide = (Relationship::Sibling == rel) //
                      ? _parent->line(Mode::Geometry)
                      : _parent->line(Mode::Rect);

  int signed_offset = _offset * _sign;

  switch (_edge) {
    case Edge::Top: {
      int top = relguide._from.y + signed_offset;
      if (_layout->_bottom->_parent)
        geo.setTop(top);
      else
        geo.moveTop(top);
      break;
    }
    case Edge::Left: {
      int left = relguide._from.x + signed_offset;
      if (_layout->_right->_parent)
        geo.setLeft(left);
      else
        geo.moveLeft(left);
      break;
    }
    case Edge::Bottom: {
      int bottom = relguide._from.y + signed_offset;
      if (_layout->_top->_parent)
        geo.setBottom(bottom);
      else
        geo.moveBottom(bottom);
      break;
    }
    case Edge::Right: {
      int right = relguide._from.x + signed_offset;
      if (_layout->_left->_parent)
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

bool Guide::isVertical() const {
  return _edge == Edge::Left ||             //
         _edge == Edge::Right ||            //
         _edge == Edge::HorizontalCenter || //
         _edge == Edge::Vertical;
}

/////////////////////////////////////////////////////////////////////////

bool Guide::isHorizontal() const {
  return _edge == Edge::Top ||            //
         _edge == Edge::Bottom ||         //
         _edge == Edge::VerticalCenter || //
         _edge == Edge::Horizontal;
}

/////////////////////////////////////////////////////////////////////////

Relationship Guide::relationshipWith(const Guide* other) const {

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
      outline._from = fvec2(rect._x, rect._y);
      outline._to   = fvec2(rect._x, rect.y2());
      break;
    case Edge::VerticalCenter:
      outline._from = fvec2(rect._x, rect.center_y());
      outline._to   = fvec2(rect.x2(), rect.center_y());
      break;
  }

  return outline;
}
/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui::anchor
