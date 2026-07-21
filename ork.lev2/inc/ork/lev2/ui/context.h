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
#include <ork/lev2/ui/overlay.h>
#include <ork/lev2/ui/style.h>
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
  // Event handler types for application-level event interception
  using event_handler_t = std::function<HandlerResult(event_constptr_t)>;
  using tick_lambda_t = std::function<void(updatedata_ptr_t)>;
  void tick(updatedata_ptr_t upd);
  //////////////////////////////////////
  HandlerResult handleEvent(event_constptr_t ev);
  HandlerResult _handleEventImpl(event_constptr_t ev);
  HandlerResult _dispatchToTarget(Widget* target, event_constptr_t ev);
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
  std::weak_ptr<const Widget> keyboardFocusWidget() const {
    return _keyboard_focus_widget;
  }
  bool isKeyDown(int code) const;
  //////////////////////////////////////
  void draw(drawevent_constptr_t drwev);
  //////////////////////////////////////
  void dumpWidgets(std::string label) const;
  //////////////////////////////////////
  // Clear any pointers to a widget (called when widget is destroyed)
  void clearWidgetPointers(Widget* w);
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
  themeengine_ptr_t _theme_engine;
  bool _hasKeyboardFocus             = false;
  Widget* _evpushtarget              = nullptr;
  Widget* _evdragtarget              = nullptr;
  const Widget* _mousefocuswidget    = nullptr;
  std::weak_ptr<const Widget> _keyboard_focus_widget;
  std::unordered_map<Widget*,tick_lambda_t> _tickSubscribers;
  Event _prevevent;
  event_ptr_t _tempevent;
  Timer _uitimer;
  // Click-clock virtualization for deterministic replay: when _use_virtual_time is
  // set, handleEvent reads _virtual_time instead of the wall-clock _uitimer, so
  // frame-paced injected PUSHes reproduce DOUBLECLICK from the session's frame
  // deltas. Default OFF => real wall clock, behavior unchanged when unused.
  bool _use_virtual_time = false;
  double _virtual_time = 0.0;
  double _prevtime = 0.0;
  double _prev_click_time = 0.0;
  double _prev_dbl_click_time = 0.0;
  std::unordered_map<int,bool> _downkeys;
  bool _debug_event_routing = false;
  bool _enable_event_bubbling = true;
  bool _dispatching = false;  // true while inside handleEvent dispatch (see deferred mutations)

  // Application-level event handlers
  // Preview: called BEFORE widget handling (for global shortcuts)
  // Fallback: called AFTER widget handling if unhandled (for app-level handling)
  event_handler_t _appPreviewHandler;
  event_handler_t _appFallbackHandler;

  //////////////////////////////////////
  // Overlay stack (drawn on top of widget tree, receives events first)
  //////////////////////////////////////
  void pushOverlay(widget_ptr_t widget, int x, int y, int w, int h,
                   bool dismiss_on_click_outside = true,
                   std::function<void()> on_dismissed = nullptr,
                   bool modal = false);
  void popOverlay();
  void dismissAllOverlays();
  bool hasOverlays() const;
  // Move an already-pushed overlay in place (no pop/push) — cheap per-frame
  // reposition for cursor-tracking hints. w/h < 0 preserve the current size.
  void repositionOverlay(const widget_ptr_t& widget, int x, int y, int w = -1, int h = -1);
  // Remove a specific overlay by widget (regardless of stack position).
  void removeOverlay(const widget_ptr_t& widget);
  void enqueueOnNextFrame(std::function<void()> op);
  void processNextFrameOps();
  //////////////////////////////////////
  // Deferred structural-mutation queue.
  //  Structural mutations (dock drops, unsplit, tab-close) enqueue here and are
  //  applied AFTER event dispatch completes, so widgets are never destroyed
  //  mid-dispatch.
  void enqueueDeferredMutation(std::function<void()> op);
  void _processDeferredMutations();
  //////////////////////////////////////
  std::vector<OverlayEntry> _overlay_stack;
  std::vector<std::function<void()>> _nextFrameOps;
  std::vector<std::function<void()>> _deferredMutations;
};

} // namespace ork::ui
