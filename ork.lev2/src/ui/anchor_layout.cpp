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
/////////////////////////////////////////////////////////////////////////
Layout::~Layout() {
}
////////////////////////////////////////////////////////////////////////
void Layout::visitHierarchy(visit_fn_t vfn){
  vfn(this);
  for( auto it : _childlayouts ){
    it->visitHierarchy(vfn);
  }
}
////////////////////////////////////////////////////////////////////////
void Layout::visitGuides(guide_visit_fn gfn){
  if( _top ) gfn( _top.get() );
  if( _left ) gfn( _left.get() );
  if( _bottom ) gfn( _bottom.get() );
  if( _right ) gfn( _right.get() );
  if( _centerH ) gfn( _centerH.get() );
  if( _centerV ) gfn( _centerV.get() );
  for( auto g : _customguides ){
    gfn( g.get() );
  }
}
////////////////////////////////////////////////////////////////////////
layout_ptr_t Layout::childLayout(Widget* w) {
  auto l = std::make_shared<Layout>(w);
  _childlayouts.push_back(l);
  return l;
}
/////////////////////////////////////////////////////////////////////////
void Layout::removeChild(layout_ptr_t l){
  auto it = std::find(_childlayouts.begin(), _childlayouts.end(), l);
  if(it!=_childlayouts.end()){
    _childlayouts.erase(it);
    prune();
  }
}
/////////////////////////////////////////////////////////////////////////
void Layout::prune(){
  /////////////////////////////////////
  std::unordered_set<Guide*> referenced_guides;
  auto vfn = [this,&referenced_guides](Layout* l){
    auto gfn = [this,&referenced_guides](Guide* g){
      referenced_guides.insert(g);
    };
    //printf( "visit L<%d>\n", l->_name);
    l->visitGuides(gfn);
  };
  visitHierarchy(vfn);
  /////////////////////////////////////
  //for( auto g : referenced_guides ){
    //printf( "REFERENCED g<%d>\n", g->_name );
  //}
  /////////////////////////////////////
  std::unordered_set<Guide*> guides_removed;
  auto vfn2 = [this,&referenced_guides,&guides_removed](Layout* l){
    auto gfn = [this,&referenced_guides,&guides_removed](Guide* g){
      std::unordered_set<Guide*> guides2rem;
      for( auto g2 : g->_associates ){
        if( referenced_guides.find(g2) == referenced_guides.end() ){
          guides2rem.insert(g2);
        }
      }
      for( auto g2 : guides2rem ){
        g->_disassociate(g2);
        //printf( "DISASSOCIATE g<%d> g2<%d>\n", g->_name, g2->_name);
        guides_removed.insert(g2);
      }
    };
    l->visitGuides(gfn);
  };
  visitHierarchy(vfn2);
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
  bool log = false;
  if(log)printf( "  layout<%p>::_doUpdateAll  ", this );
  if (_top){
    if(log)printf( "  _top<%p>  ", _top.get() );
    _top->updateAssociates(vset);
  }
  if (_left){
    if(log)printf( "  _left<%p>  ", _left.get() );
    _left->updateAssociates(vset);
  }
  if (_bottom){
    if(log)printf( "  _bottom<%p>  ", _bottom.get() );
    _bottom->updateAssociates(vset);
  }
  if (_right){
    if(log)printf( "  _right<%p>  ", _right.get() );
    _right->updateAssociates(vset);
  }
  if (_centerH){
    if(log)printf( "  _centerH<%p>  ", _centerH.get() );
    _centerH->updateAssociates(vset);
  }
  if (_centerV){
    if(log)printf( "  _centerV<%p>  ", _centerV.get() );
    _centerV->updateAssociates(vset);
  }
  if(_widget){
    if(log)printf( "  _widget<%p:%s>  ", _widget, _widget->_name.c_str() );
  }
  for (auto g : _customguides)
    g->updateAssociates(vset);

  if(log)printf( "\n");
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
  guide->_type = GuideType::PROPORTIONAL;
  _customguides.insert(guide);
  return guide;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::proportionalVerticalGuide(float proportion) {
  auto guide         = std::make_shared<Guide>(this, Edge::CustomVertical);
  guide->_proportion = proportion;
  guide->_type = GuideType::PROPORTIONAL;
  _customguides.insert(guide);
  return guide;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::fixedHorizontalGuide(int fixed) {
  auto guide    = std::make_shared<Guide>(this, Edge::CustomHorizontal);
  guide->_fixed = fixed;
  guide->_type = GuideType::FIXED;
  _customguides.insert(guide);
  return guide;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::fixedVerticalGuide(int fixed) {
  auto guide    = std::make_shared<Guide>(this, Edge::CustomVertical);
  guide->_fixed = fixed;
  guide->_type = GuideType::FIXED;
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
  printf("// Layout<%d:%p> margin<%d> widget<%p:%s>\n", _name, this, _margin, _widget, _widget->_name.c_str());
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
