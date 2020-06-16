#pragma once

#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/anchor.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// LayoutGroup :  collection of widgets which are layed out...
////////////////////////////////////////////////////////////////////

struct LayoutGroup : public Group {

  LayoutGroup(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);

  anchor::layout_ptr_t _layout;

  //////////////////////////////////////
  template <typename T, typename... A>
  std::pair<
      std::shared_ptr<T>, //
      anchor::layout_ptr_t>
  makeChild(A&&... args) {
    std::pair<
        std::shared_ptr<T>, //
        anchor::layout_ptr_t>
        rval;

    rval.first  = std::make_shared<T>(std::forward<A>(args)...);
    rval.second = _layout->childLayout(rval.first.get());

    addChild(rval.first);
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
