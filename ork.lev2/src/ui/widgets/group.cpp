#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/layoutgroup.inl>

namespace ork { namespace ui {
/////////////////////////////////////////////////////////////////////////
Group::Group(const std::string& name, int x, int y, int w, int h)
    : Widget(name, x, y, w, h) {
}
/////////////////////////////////////////////////////////////////////////
void Group::addChild(widget_ptr_t w) {
  assert(w->parent() == nullptr);
  _children.push_back(w);
  w->mParent    = this;
  w->_uicontext = this->_uicontext;
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
    child->Draw(drwev);
  }
}
/////////////////////////////////////////////////////////////////////////
void Group::OnResize() {
  // printf( "Group<%s>::OnResize x<%d> y<%d> w<%d> h<%d>\n", msName.c_str(), miX, miY, miW, miH );
  for (auto& it : _children) {
    if (it->mSizeDirty)
      it->OnResize();
  }
}
/////////////////////////////////////////////////////////////////////////
void Group::DoLayout() {
  const auto& g = _geometry;
  if (0)
    printf("Group<%s>::DoLayout x<%d> y<%d> w<%d> h<%d>\n", msName.c_str(), g._x, g._y, g._w, g._h);
  for (auto& it : _children) {
    it->ReLayout();
  }
}
/////////////////////////////////////////////////////////////////////////
Widget* Group::doRouteUiEvent(event_constptr_t ev) {
  if (0)
    printf("Group<%s>::doRouteUiEvent\n", msName.c_str());
  for (auto& child : _children) {
    bool inside = child->IsEventInside(ev);
    if (0)
      printf("Group<%s>::doRouteUiEvent ch<%p> inside<%d>\n", msName.c_str(), child.get(), int(inside));
    if (inside) {
      auto child_target = child->routeUiEvent(ev);
      if (child_target)
        return child_target;
    }
  }
  if (IsEventInside(ev))
    return this;
  return nullptr;
}
/////////////////////////////////////////////////////////////////////////
LayoutGroup::LayoutGroup(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h) {
  _layout = std::make_shared<anchor::Layout>(this);
}
/////////////////////////////////////////////////////////////////////////
void LayoutGroup::OnResize() {
}
/////////////////////////////////////////////////////////////////////////
void LayoutGroup::DoLayout() {
  // in this case, the layout is responsible
  // for laying out all children, recursively..
  // note that the layout will use the geometry of this group
  //  to compute the layout of all children
  // So it is expected that you set the size of this group
  //  either manually or driven indirectly through the resize
  //  of a parent..
  const auto& g = _geometry;
  if (0)
    printf(
        "LayoutGroup<%s>::DoLayout l<%p> x<%d> y<%d> w<%d> h<%d>\n", //
        msName.c_str(),
        _layout.get(),
        g._x,
        g._y,
        g._w,
        g._h);
  if (_layout)
    _layout->updateAll();
  //
}
/////////////////////////////////////////////////////////////////////////
void LayoutGroup::DoDraw(drawevent_constptr_t drwev) {
  drawChildren(drwev);
}
/////////////////////////////////////////////////////////////////////////
}} // namespace ork::ui
