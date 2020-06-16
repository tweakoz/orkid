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
Layout::Layout(Widget* w)
    : _widget(w) {
  static int _names = 0;
  _name             = _names++;
}
Layout::~Layout() {
}
/////////////////////////////////////////////////////////////////////////
layout_ptr_t Layout::childLayout(Widget* w) {
  auto l = std::make_shared<Layout>(w);
  _childlayouts.push_back(l);
  return l;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::top() {
  if (_top == nullptr) {
    _top = std::make_shared<Guide>(this, Edge::Top);
    _top->setMargin(_margin);
  }
  return _top;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::left() {
  if (_left == nullptr) {
    _left = std::make_shared<Guide>(this, Edge::Left);
    _left->setMargin(_margin);
  }
  return _left;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::bottom() {
  if (_bottom == nullptr) {
    _bottom = std::make_shared<Guide>(this, Edge::Bottom);
    _bottom->setMargin(_margin);
  }
  return _bottom;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::right() {
  if (_right == nullptr) {
    _right = std::make_shared<Guide>(this, Edge::Right);
    _right->setMargin(_margin);
  }
  return _right;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::centerH() {
  if (_centerH == nullptr) {
    _centerH = std::make_shared<Guide>(this, Edge::HorizontalCenter);
    _centerH->setMargin(_margin);
  }
  return _centerH;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::centerV() {
  if (_centerV == nullptr) {
    _centerV = std::make_shared<Guide>(this, Edge::VerticalCenter);
    _centerV->setMargin(_margin);
  }
  return _centerV;
}
/////////////////////////////////////////////////////////////////////////
void Layout::updateAll() {
  visit_set vset;
  _doUpdateAll(vset);
}
/////////////////////////////////////////////////////////////////////////
void Layout::_doUpdateAll(visit_set& vset) {
  if (_top)
    _top->updateAssociates(vset);
  if (_left)
    _left->updateAssociates(vset);
  if (_bottom)
    _bottom->updateAssociates(vset);
  if (_right)
    _right->updateAssociates(vset);
  if (_centerH)
    _centerH->updateAssociates(vset);
  if (_centerV)
    _centerV->updateAssociates(vset);
  for (auto g : _customguides)
    g->updateAssociates(vset);

  for (auto l : _childlayouts) {
    l->_doUpdateAll(vset);
  }
}
/////////////////////////////////////////////////////////////////////////
void Layout::setMargin(int margin) {

  _margin = margin;

  if (_top)
    _top->setMargin(margin);

  if (_left)
    _left->setMargin(margin);

  if (_bottom)
    _bottom->setMargin(margin);

  if (_right)
    _right->setMargin(margin);

  for (auto g : _customguides) {
    g->setMargin(margin);
  }
}
/////////////////////////////////////////////////////////////////////////
void Layout::centerIn(Layout* other) {
  if (not isAnchorAllowed(other))
    return;

  if (_top)
    _top->anchorTo(nullptr);

  if (_left)
    _left->anchorTo(nullptr);

  if (_bottom)
    _bottom->anchorTo(nullptr);

  if (_right)
    _right->anchorTo(nullptr);

  if (other != nullptr) {
    this->centerH()->anchorTo(other->centerH().get());
    this->centerV()->anchorTo(other->centerV().get());
  } else {
    if (_centerH)
      _centerH->anchorTo(nullptr);

    if (_centerV)
      _centerV->anchorTo(nullptr);
  }
}
/////////////////////////////////////////////////////////////////////////
void Layout::fill(Layout* other) {
  if (!this->isAnchorAllowed(other))
    return;

  if (other != nullptr) {
    this->left()->anchorTo(other->left());
    this->right()->anchorTo(other->right());
    this->top()->anchorTo(other->top());
    this->bottom()->anchorTo(other->bottom());
  } else {
    if (_top)
      _top->anchorTo(nullptr);

    if (_left)
      _left->anchorTo(nullptr);

    if (_bottom)
      _bottom->anchorTo(nullptr);

    if (_right)
      _right->anchorTo(nullptr);
  }

  if (_centerH)
    _centerH->anchorTo(nullptr);

  if (_centerV)
    _centerV->anchorTo(nullptr);
  for (auto g : _customguides)
    g->anchorTo(nullptr);

  return;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::proportionalHorizontalGuide(float proportion) {
  auto guide         = std::make_shared<Guide>(this, Edge::CustomHorizontal);
  guide->_proportion = proportion;
  _customguides.insert(guide);
  return guide;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::proportionalVerticalGuide(float proportion) {
  auto guide         = std::make_shared<Guide>(this, Edge::CustomVertical);
  guide->_proportion = proportion;
  _customguides.insert(guide);
  return guide;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::fixedHorizontalGuide(int fixed) {
  auto guide    = std::make_shared<Guide>(this, Edge::CustomHorizontal);
  guide->_fixed = fixed;
  _customguides.insert(guide);
  return guide;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::fixedVerticalGuide(int fixed) {
  auto guide    = std::make_shared<Guide>(this, Edge::CustomVertical);
  guide->_fixed = fixed;
  _customguides.insert(guide);
  return guide;
}
/////////////////////////////////////////////////////////////////////////
bool Layout::isAnchorAllowed(guide_ptr_t guide) const {
  if (guide == nullptr)
    return false;

  return isAnchorAllowed(guide->_layout);
}
/////////////////////////////////////////////////////////////////////////
bool Layout::isAnchorAllowed(Layout* layout) const {
  if (layout == nullptr)
    return false;

  return layout->_widget == _widget->parent() or //
         layout->_widget->parent() == _widget->parent();
}
/////////////////////////////////////////////////////////////////////////
void Layout::dump() {
  printf("//////////////////////////\n");
  printf("// Layout<%d> margin<%d>\n", _name, _margin);
  if (_top)
    _top->dump();
  if (_left)
    _left->dump();
  if (_bottom)
    _bottom->dump();
  if (_right)
    _right->dump();
  if (_centerH)
    _centerH->dump();
  if (_centerV)
    _centerV->dump();
  for (auto g : _customguides)
    g->dump();
  for (auto l : _childlayouts)
    l->dump();
  printf("//////////////////////////\n");
}
/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui::anchor
