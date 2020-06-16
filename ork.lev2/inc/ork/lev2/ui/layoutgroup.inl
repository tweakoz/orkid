#pragma once

#include <ork/lev2/ui/group.h>
#include <ork/lev2/ui/anchor.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// LayoutGroup :  collection of widgets which are layed out...
////////////////////////////////////////////////////////////////////

template <typename T> struct LayoutItem {
  std::shared_ptr<T> _widget;
  anchor::layout_ptr_t _layout;
};

struct LayoutGroup : public Group {

  LayoutGroup(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);

  anchor::layout_ptr_t _layout;

  //////////////////////////////////////
  template <typename T, typename... A> LayoutItem<T> makeChild(A&&... args) {
    LayoutItem<T> rval;
    rval._widget = std::make_shared<T>(std::forward<A>(args)...);
    rval._layout = _layout->childLayout(rval._widget.get());
    addChild(rval._widget);
    return rval;
  }
  //////////////////////////////////////

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  void OnResize() override;
  void DoLayout() override;
  HandlerResult DoRouteUiEvent(event_constptr_t Ev) override;
};

} // namespace ork::ui
