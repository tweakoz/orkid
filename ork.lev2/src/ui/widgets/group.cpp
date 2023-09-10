////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/layoutgroup.inl>

namespace ork::ui {
/////////////////////////////////////////////////////////////////////////
Group::Group(const std::string& name, int x, int y, int w, int h)
    : Widget(name, x, y, w, h) {
}
/////////////////////////////////////////////////////////////////////////
Group::~Group(){
}
/////////////////////////////////////////////////////////////////////////
void Group::visitHeirarchy(visit_fn_t vfn){
  std::stack<Widget*> stk;
  stk.push(this);
  while(not stk.empty()){
    auto w = stk.top();
    vfn(w);
    stk.pop();
    auto as_group = dynamic_cast<Group*>(this);
    if(as_group){
      for( auto it : as_group->_children ){
        stk.push(it.get());
      }
    }
  }
}
/////////////////////////////////////////////////////////////////////////
void Group::addChild(widget_ptr_t w) {
  if (w->parent()) {
    w->parent()->removeChild(w);
  }
  _children.push_back(w);
  w->setParent(this);
  DoLayout();
}
/////////////////////////////////////////////////////////////////////////
void Group::removeChild(widget_ptr_t w) {
  _children.erase(
      std::remove_if(
          _children.begin(),
          _children.end(),
          [=](widget_ptr_t test) -> bool {
            // Do "some stuff", then return true if element should be removed.
            return test == w;
          }),
      _children.end());

  DoLayout();
}
/////////////////////////////////////////////////////////////////////////
void Group::removeChild(Widget* w) {
  _children.erase(
      std::remove_if(
          _children.begin(),
          _children.end(),
          [=](widget_ptr_t test) -> bool {
            // Do "some stuff", then return true if element should be removed.
            return test.get() == w;
          }),
      _children.end());

  DoLayout();
}
/////////////////////////////////////////////////////////////////////////
void Group::drawChildren(ui::drawevent_constptr_t drwev) {
  for (auto child : _children) {
    child->draw(drwev);
  }
}
/////////////////////////////////////////////////////////////////////////
void Group::_doOnResized() {
  // printf( "Group<%s>::OnResize x<%d> y<%d> w<%d> h<%d>\n", _name.c_str(), miX, miY, miW, miH );
  for (auto& it : _children) {
    if (it->mSizeDirty)
      it->_doOnResized();
  }
}
/////////////////////////////////////////////////////////////////////////
void Group::DoLayout() {
  const auto& g = _geometry;
  if (0)
    printf("Group<%s>::DoLayout x<%d> y<%d> w<%d> h<%d>\n", _name.c_str(), g._x, g._y, g._w, g._h);
  for (auto& it : _children) {
    it->ReLayout();
  }
}
/////////////////////////////////////////////////////////////////////////
Widget* Group::doRouteUiEvent(event_constptr_t ev) {
  if (0){
    printf("Group<%s>::doRouteUiEvent\n", _name.c_str());
  }
  //
  for (auto& child : _children) {
    bool inside = child->IsEventInside(ev);
    if (0)
      printf("Group<%s>::doRouteUiEvent ch<%s> inside<%d>\n", _name.c_str(), child->_name.c_str(), int(inside));
    if (inside) {
      auto child_target = child->routeUiEvent(ev);
      if (child_target){
        if (0)
          printf("  child_target<%s>\n", child_target->_name.c_str() );
        return child_target;
      }
    }
  }
  //
  if (IsEventInside(ev))
    return this;
  //
  return nullptr;
}
/////////////////////////////////////////////////////////////////////////
void Group::_doOnPreDestroy() {
  for( auto c : _children ){
    c->onPreDestroy();
  }
}
/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
