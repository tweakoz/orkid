#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>

namespace ork { namespace ui {

/////////////////////////////////////////////////////////////////////////

Group::Group(const std::string& name, int x, int y, int w, int h)
    : Widget(name, x, y, w, h) {
}

/////////////////////////////////////////////////////////////////////////

void Group::addChild(widget_ptr_t w) {
  assert(w->GetParent() == nullptr);
  _children.push_back(w);
  w->mParent = this;
  DoLayout();
}

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
  // printf( "Group<%s>::DoLayout x<%d> y<%d> w<%d> h<%d>\n", msName.c_str(), miX, miY, miW, miH );
  for (auto& it : _children) {
    it->ReLayout();
  }
}

/////////////////////////////////////////////////////////////////////////

HandlerResult Group::DoRouteUiEvent(event_constptr_t Ev) {
  HandlerResult res;
  for (auto& child : _children) {
    if (res.mHandler == nullptr) {
      bool binside = child->IsEventInside(Ev);
      /*printf(
          "Group::RouteUiEvent ev<%d,%d> child<%p> inside<%d>\n", //
          Ev->miX,
          Ev->miY,
          child.get(),
          int(binside));*/
      if (binside) {
        auto child_res = child->RouteUiEvent(Ev);
        if (child_res.wasHandled()) {
          res = child_res;
        }
      }
    }
  }
  if (res.mHandler == nullptr) {
    res = OnUiEvent(Ev);
  }
  return res;
}

/////////////////////////////////////////////////////////////////////////

}} // namespace ork::ui
