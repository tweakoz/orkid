////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/timer.h>
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
  using tick_lambda_t = std::function<void(updatedata_ptr_t)>;
  void tick(updatedata_ptr_t upd);
  //////////////////////////////////////
  HandlerResult handleEvent(event_constptr_t ev);
  // void updateMouseFocus(const HandlerResult& r, event_constptr_t Ev);
  bool hasMouseFocus(const Widget* w) const;
  //////////////////////////////////////
  bool hasKeyboardFocus() const {
    return _hasKeyboardFocus;
  }
  //////////////////////////////////////
  const Widget* mouseFocusWidget() const {
    return _mousefocuswidget;
  }
  //////////////////////////////////////
  const Widget* keyboardFocusWidget() const {
    return _keyboardFocusWidget;
  }
  //////////////////////////////////////
  void draw(drawevent_constptr_t drwev);
  //////////////////////////////////////
  void dumpWidgets(std::string label) const;
  //////////////////////////////////////
  inline void subscribeToTicks(Widget* w, tick_lambda_t lambda){
    _tickSubscribers[w] = lambda;
  }
  //////////////////////////////////////
  inline void unsubscribeFromTicks(Widget* w){
    auto it = _tickSubscribers.find(w);
    if(it!=_tickSubscribers.end()){
      _tickSubscribers.erase(it);
    }
  }
  //////////////////////////////////////

  std::string _id;
  group_ptr_t _top;
  bool _hasKeyboardFocus             = false;
  Widget* _evdragtarget              = nullptr;
  const Widget* _mousefocuswidget    = nullptr;
  const Widget* _keyboardFocusWidget = nullptr;
  Widget* _overlayWidget             = nullptr;
  std::unordered_map<Widget*,tick_lambda_t> _tickSubscribers;
  Event _prevevent;
  event_ptr_t _tempevent;
  Timer _uitimer;
  double _prevtime = 0.0;
  double _prev_click_time = 0.0;
  double _prev_dbl_click_time = 0.0;
};

} // namespace ork::ui
