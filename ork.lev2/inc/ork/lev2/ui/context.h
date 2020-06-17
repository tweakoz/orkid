#pragma once

#include <ork/util/fsm.h>
#include <ork/lev2/ui/group.h>
#include <functional>

namespace ork::ui {

struct Context {
  Context();
  //////////////////////////////////////
  template <typename T, typename... A> std::shared_ptr<T> makeTop(A&&... args) {
    OrkAssert(not _top);
    std::shared_ptr<T> rval = std::make_shared<T>(std::forward<A>(args)...);
    _top                    = rval;
    _top->_uicontext        = this;
    return rval;
  }
  //////////////////////////////////////

  HandlerResult handleEvent(event_constptr_t ev);
  // void updateMouseFocus(const HandlerResult& r, event_constptr_t Ev);
  bool hasMouseFocus(const Widget* w) const;
  //////////////////////////////////////

  group_ptr_t _top;
  Widget* _evdragtarget           = nullptr;
  const Widget* _mousefocuswidget = nullptr;
  Event _prevevent;
  event_ptr_t _tempevent;
};

} // namespace ork::ui
