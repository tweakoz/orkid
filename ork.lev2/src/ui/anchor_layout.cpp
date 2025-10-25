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
  //OrkAssert(_name!=1);
}
/////////////////////////////////////////////////////////////////////////
Layout::~Layout() {
}
////////////////////////////////////////////////////////////////////////
void Layout::lockAllGuides() {
  auto gfn = [this](Guide* g){
    g->_locked = true;
  };
  visitGuides(gfn);
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
  l->_parent = this;
  l->setMargin(_margin);  // Inherit parent's margin
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
    _top->_locked = _locked;
  }
  return _top;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::left() {
  if (_left == nullptr) {
    _left = std::make_shared<Guide>(this, Edge::Left);
    _left->setMargin(_margin);
    _left->_locked = _locked;
  }
  return _left;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::bottom() {
  if (_bottom == nullptr) {
    _bottom = std::make_shared<Guide>(this, Edge::Bottom);
    _bottom->setMargin(_margin);
    _bottom->_locked = _locked;
  }
  return _bottom;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::right() {
  if (_right == nullptr) {
    _right = std::make_shared<Guide>(this, Edge::Right);
    _right->setMargin(_margin);
    _right->_locked = _locked;
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
void Layout::setProportionalRect(Layout* parent, float x, float y, float w, float h, bool locked) {
  if (!isAnchorAllowed(parent))
    return;

  // Find or create proportional guides at the specified positions
  auto left_guide   = parent->proportionalVerticalGuide(x);
  auto right_guide  = parent->proportionalVerticalGuide(x + w);
  auto top_guide    = parent->proportionalHorizontalGuide(y);
  auto bottom_guide = parent->proportionalHorizontalGuide(y + h);

  // Lock guides if requested
  if (locked) {
    left_guide->lock();
    right_guide->lock();
    top_guide->lock();
    bottom_guide->lock();
  }

  // Anchor this layout's edges to the guides
  this->left()->anchorTo(left_guide);
  this->right()->anchorTo(right_guide);
  this->top()->anchorTo(top_guide);
  this->bottom()->anchorTo(bottom_guide);
}
/////////////////////////////////////////////////////////////////////////
void Layout::setFixedRect(Layout* parent, int x, int y, int w, int h, bool locked) {
  if (!isAnchorAllowed(parent))
    return;

  // Find or create fixed guides at the specified positions
  auto left_guide   = parent->fixedVerticalGuide(x);
  auto right_guide  = parent->fixedVerticalGuide(x + w);
  auto top_guide    = parent->fixedHorizontalGuide(y);
  auto bottom_guide = parent->fixedHorizontalGuide(y + h);

  // Lock guides if requested
  if (locked) {
    left_guide->lock();
    right_guide->lock();
    top_guide->lock();
    bottom_guide->lock();
  }

  // Anchor this layout's edges to the guides
  this->left()->anchorTo(left_guide);
  this->right()->anchorTo(right_guide);
  this->top()->anchorTo(top_guide);
  this->bottom()->anchorTo(bottom_guide);
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::proportionalHorizontalGuide(float proportion) {
  // Check if a guide with this proportion already exists
  const float epsilon = 0.0001f;
  for (auto& existing : _customguides) {
    if (existing->_type == GuideType::PROPORTIONAL &&
        existing->_edge == Edge::CustomHorizontal &&
        std::abs(existing->_proportion - proportion) < epsilon) {
      return existing;
    }
  }

  // Create new guide
  auto guide         = std::make_shared<Guide>(this, Edge::CustomHorizontal);
  guide->_proportion = proportion;
  guide->_type = GuideType::PROPORTIONAL;
  guide->_locked = _locked;
  guide->_margin = _margin;  // Inherit layout's margin
  _customguides.insert(guide);
  return guide;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::proportionalVerticalGuide(float proportion) {
  // Check if a guide with this proportion already exists
  const float epsilon = 0.0001f;
  for (auto& existing : _customguides) {
    if (existing->_type == GuideType::PROPORTIONAL &&
        existing->_edge == Edge::CustomVertical &&
        std::abs(existing->_proportion - proportion) < epsilon) {
      return existing;
    }
  }

  // Create new guide
  auto guide         = std::make_shared<Guide>(this, Edge::CustomVertical);
  guide->_proportion = proportion;
  guide->_type = GuideType::PROPORTIONAL;
  guide->_locked = _locked;
  guide->_margin = _margin;  // Inherit layout's margin
  _customguides.insert(guide);
  return guide;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::fixedHorizontalGuide(int fixed) {
  // Check if a guide with this fixed position already exists
  for (auto& existing : _customguides) {
    if (existing->_type == GuideType::FIXED &&
        existing->_edge == Edge::CustomHorizontal &&
        existing->_fixed == fixed) {
      return existing;
    }
  }

  // Create new guide
  auto guide    = std::make_shared<Guide>(this, Edge::CustomHorizontal);
  guide->_fixed = fixed;
  guide->_type = GuideType::FIXED;
  guide->_locked = _locked;
  guide->_margin = _margin;  // Inherit layout's margin
  _customguides.insert(guide);
  return guide;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::fixedVerticalGuide(int fixed) {
  // Check if a guide with this fixed position already exists
  for (auto& existing : _customguides) {
    if (existing->_type == GuideType::FIXED &&
        existing->_edge == Edge::CustomVertical &&
        existing->_fixed == fixed) {
      return existing;
    }
  }

  // Create new guide
  auto guide    = std::make_shared<Guide>(this, Edge::CustomVertical);
  guide->_fixed = fixed;
  guide->_type = GuideType::FIXED;
  guide->_locked = _locked;
  guide->_margin = _margin;  // Inherit layout's margin
  _customguides.insert(guide);
  return guide;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::offsetHorizontalGuide(guide_ptr_t base, int offset) {
  OrkAssert(base != nullptr);
  OrkAssert(base->isHorizontal());

  // Check if an offset guide with this base and offset already exists
  for (auto& existing : _customguides) {
    if (existing->_type == GuideType::OFFSET &&
        existing->_edge == Edge::CustomHorizontal &&
        existing->_offset_base == base.get() &&
        existing->_offset == offset) {
      return existing;
    }
  }

  // Create new offset guide
  auto guide = std::make_shared<Guide>(this, Edge::CustomHorizontal);
  guide->_offset_base = base.get();
  guide->_offset = offset;
  guide->_type = GuideType::OFFSET;
  guide->_locked = _locked;
  guide->_margin = _margin;
  _customguides.insert(guide);
  return guide;
}
/////////////////////////////////////////////////////////////////////////
guide_ptr_t Layout::offsetVerticalGuide(guide_ptr_t base, int offset) {
  OrkAssert(base != nullptr);
  OrkAssert(base->isVertical());

  // Check if an offset guide with this base and offset already exists
  for (auto& existing : _customguides) {
    if (existing->_type == GuideType::OFFSET &&
        existing->_edge == Edge::CustomVertical &&
        existing->_offset_base == base.get() &&
        existing->_offset == offset) {
      return existing;
    }
  }

  // Create new offset guide
  auto guide = std::make_shared<Guide>(this, Edge::CustomVertical);
  guide->_offset_base = base.get();
  guide->_offset = offset;
  guide->_type = GuideType::OFFSET;
  guide->_locked = _locked;
  guide->_margin = _margin;
  _customguides.insert(guide);
  return guide;
}
/////////////////////////////////////////////////////////////////////////
void Layout::setRect(guide_ptr_t top, guide_ptr_t left, guide_ptr_t right, guide_ptr_t bottom) {
  // Sanity checks
  OrkAssert(top == nullptr || top->isHorizontal());
  OrkAssert(left == nullptr || left->isVertical());
  OrkAssert(right == nullptr || right->isVertical());
  OrkAssert(bottom == nullptr || bottom->isHorizontal());

  // Anchor to the provided guides
  if (top)
    this->top()->anchorTo(top);
  if (left)
    this->left()->anchorTo(left);
  if (right)
    this->right()->anchorTo(right);
  if (bottom)
    this->bottom()->anchorTo(bottom);
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
void Layout::dump(int level) {
  if(level==0){
    printf("//////////////////////////\n");
  }
  auto indent = std::string(level*2,' ');
  printf("%sLayout<%d:%p> margin<%d> widget<%p:%s>\n",indent.c_str(),  _name, this, _margin, _widget, _widget->_name.c_str());
  if (_top)
    _top->dump(level+1);
  if (_left)
    _left->dump(level+1);
  if (_bottom)
    _bottom->dump(level+1);
  if (_right)
    _right->dump(level+1);
  if (_centerH)
    _centerH->dump(level+1);
  if (_centerV)
    _centerV->dump(level+1);
  for (auto g : _customguides)
    g->dump(level+1);
  for (auto l : _childlayouts)
    l->dump(level+1);
  if(level==0){
    printf("//////////////////////////\n");
  }
}
/////////////////////////////////////////////////////////////////////////
std::vector<guide_ptr_t> Layout::getDraggableGuides() const {
  std::vector<guide_ptr_t> guides;

  // Recursively get from child layouts only (traverse down the hierarchy)
  for (const auto& child : _childlayouts) {
    auto child_guides = child->getDraggableGuides();
    guides.insert(guides.end(), child_guides.begin(), child_guides.end());
  }

  // Add unlocked custom guides from this layout
  for (auto& custom_guide : _customguides) {
    if (!custom_guide->_locked) {
      guides.push_back(custom_guide);
    }
  }


  return guides;
}
/////////////////////////////////////////////////////////////////////////

guide_ptr_t Layout::findGuideBetween(layout_ptr_t layout_a, layout_ptr_t layout_b) {
  guide_ptr_t found_guide = nullptr;

  // First check if layouts share an edge guide directly
  if (layout_a->_right && layout_b->_left && layout_a->_right.get() == layout_b->_left.get()) {
    return layout_a->_right;  // Side by side (a on left, b on right)
  }
  if (layout_a->_left && layout_b->_right && layout_a->_left.get() == layout_b->_right.get()) {
    return layout_a->_left;  // Side by side (a on right, b on left)
  }
  if (layout_a->_bottom && layout_b->_top && layout_a->_bottom.get() == layout_b->_top.get()) {
    return layout_a->_bottom;  // Stacked (a on top, b on bottom)
  }
  if (layout_a->_top && layout_b->_bottom && layout_a->_top.get() == layout_b->_bottom.get()) {
    return layout_a->_top;  // Stacked (a on bottom, b on top)
  }

  // Search all custom guides in this layout (looking for dividers only, not shared edges)
  for (auto& guide : _customguides) {
    Guide* layout_a_guide = nullptr;
    Guide* layout_b_guide = nullptr;

    // Check if guide has associates from both layouts
    for (auto* assoc : guide->_associates) {
      if (assoc->_layout == layout_a.get()) layout_a_guide = assoc;
      if (assoc->_layout == layout_b.get()) layout_b_guide = assoc;
    }

    // Guide is between them if both anchor to it from opposite sides
    if (layout_a_guide && layout_b_guide) {
      bool is_divider = false;

      // Check for vertical divider (Left vs Right)
      if ((layout_a_guide->_edge == Edge::Left && layout_b_guide->_edge == Edge::Right) ||
          (layout_a_guide->_edge == Edge::Right && layout_b_guide->_edge == Edge::Left)) {
        is_divider = true;
      }

      // Check for horizontal divider (Top vs Bottom)
      if ((layout_a_guide->_edge == Edge::Top && layout_b_guide->_edge == Edge::Bottom) ||
          (layout_a_guide->_edge == Edge::Bottom && layout_b_guide->_edge == Edge::Top)) {
        is_divider = true;
      }

      if (is_divider) {
        if (found_guide != nullptr) {
          // Already found one - ambiguous case
          return nullptr;
        }
        found_guide = guide;
      }
    }
  }

  // Recursively search child layouts
  for (auto& child : _childlayouts) {
    auto result = child->findGuideBetween(layout_a, layout_b);
    if (result) {
      if (found_guide != nullptr) {
        // Found in multiple places - ambiguous
        return nullptr;
      }
      found_guide = result;
    }
  }

  return found_guide;  // nullptr if not found, guide_ptr if found exactly once
}
/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui::anchor
