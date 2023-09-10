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

/////////////////////////////////////////////////////////////////////////
namespace ork::ui {
/////////////////////////////////////////////////////////////////////////
LayoutGroup::LayoutGroup(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h) {
  _layout = std::make_shared<anchor::Layout>(this);
}
/////////////////////////////////////////////////////////////////////////
LayoutGroup::~LayoutGroup() {
}
/////////////////////////////////////////////////////////////////////////
void LayoutGroup::_doOnResized() {
  _clear = true;
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
        _name.c_str(),
        (void*)_layout.get(),
        g._x,
        g._y,
        g._w,
        g._h);
  if (_layout)
    _layout->updateAll();

  ///////////////////////////////////////////
  // compute depth for each widget
  ///////////////////////////////////////////

  int depth = 0;
  std::stack<std::pair<Widget*, int>> stk; // Pair of widget and its depth
  stk.push({this, depth});

  while (!stk.empty()) {
    auto [w, currentDepth] = stk.top();
    stk.pop();

    w->_depth = currentDepth; // Assuming each widget has a depth member

    auto as_group = dynamic_cast<Group*>(w);
    if (as_group) {
      for (auto it : as_group->_children) {
        stk.push({it.get(), currentDepth + 1});
      }
    }
  }
}
/////////////////////////////////////////////////////////////////////////
void LayoutGroup::DoDraw(drawevent_constptr_t drwev) {
  if (_clear) {
    auto context = drwev->GetTarget();
    auto FBI     = context->FBI();
    lev2::ViewportRect vrect;
    vrect._x = x();
    vrect._y = y();
    vrect._w = width();
    vrect._h = height();
    FBI->pushScissor(vrect);
    FBI->pushViewport(vrect);
    // FBI->Clear(_clearColor,1);
    FBI->popViewport();
    FBI->popScissor();
    _clear = false;
  }
  drawChildren(drwev);
}
/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
