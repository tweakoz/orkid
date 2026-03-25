#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/group.h>
#include <ork/profiling.inl>
/////////////////////////////////////////////////////////////////////////
namespace ork::ui {
/////////////////////////////////////////////////////////////////////////
Context::Context() {
  _tempevent = std::make_shared<Event>();
  _uitimer.Start();
  _prevtime = 0.0;

  // Initialize default theme engine
  auto default_styledb = createDefaultStyleDatabase();
  _theme_engine = std::make_shared<ThemeEngine>(default_styledb);
}
/////////////////////////////////////////////////////////////////////////
bool Context::isKeyDown(int code) const {
  auto it = _downkeys.find(code);
  if(it==_downkeys.end()){
    return false;
  }
  return it->second;
}
/////////////////////////////////////////////////////////////////////////
void Context::tick(updatedata_ptr_t updata){
  for( auto sitem : _tickSubscribers ){
    auto w = sitem.first;
    auto cb = sitem.second;
    cb(updata);
  }
}
/////////////////////////////////////////////////////////////////////////
// Helper: dispatch event to target widget with optional bubbling
/////////////////////////////////////////////////////////////////////////
HandlerResult Context::_dispatchToTarget(Widget* target, event_constptr_t ev) {
  if (!target) return HandlerResult();

  HandlerResult rval = target->OnUiEvent(ev);

  // Bubble up to parent widgets if not handled and bubbling is enabled
  if (_enable_event_bubbling && !rval.wasHandled()) {
    Widget* parent = target->_parent;
    while (parent && !rval.wasHandled()) {
      rval = parent->OnUiEvent(ev);
      parent = parent->_parent;
    }
  }

  return rval;
}
/////////////////////////////////////////////////////////////////////////
HandlerResult Context::handleEvent(event_constptr_t ev) {
  EASY_BLOCK("uictx::handleEvent", profiler::colors::Red);
  OrkAssert(_top);
  HandlerResult rval;
  double curtime = _uitimer.SecsSinceStart();

  /////////////////////////////////
  // PHASE 1: Application Preview Handler
  // (for global shortcuts, mode keys, etc.)
  /////////////////////////////////
  if (_appPreviewHandler) {
    rval = _appPreviewHandler(ev);
    if (rval.wasHandled()) {
      _prevevent = *ev;
      _prevtime = curtime;
      return rval;
    }
  }

  /////////////////////////////////
  // PHASE 1.5: Overlay event handling
  // Overlays receive events before the widget tree
  /////////////////////////////////
  if (not _overlay_stack.empty()) {
    int mx = ev->miX;
    int my = ev->miY;
    bool event_in_overlay = false;

    // Check overlays from top to bottom for hit (mouse/click events only —
    // KEY events are handled separately below to avoid double-dispatch)
    bool is_key_event = (ev->_eventcode == EventCode::KEY_DOWN
                      || ev->_eventcode == EventCode::KEY_UP
                      || ev->_eventcode == EventCode::KEY_REPEAT);
    if (!is_key_event) {
      for (int i = int(_overlay_stack.size()) - 1; i >= 0; i--) {
        auto& entry = _overlay_stack[i];
        // Hold a local shared_ptr copy so the widget survives
        // even if the event handler pops/dismisses the overlay
        auto w_shared = entry._widget;
        auto w = w_shared.get();
        if (w) {
          int lx = mx - w->x();
          int ly = my - w->y();
          bool inside = (lx >= 0 && lx < w->width() && ly >= 0 && ly < w->height());
          if (inside) {
            event_in_overlay = true;
            // Route to this overlay widget
            rval = w->OnUiEvent(ev);
            // Set push target so DRAG events in Phase 2 route to this overlay widget
            if (ev->_eventcode == EventCode::PUSH || ev->_eventcode == EventCode::DOUBLECLICK) {
              _evpushtarget = w;
            }
            break;
          }
        }
      }
    }

    // Handle KEY events: route to topmost overlay
    if (ev->_eventcode == EventCode::KEY_DOWN || ev->_eventcode == EventCode::KEY_UP
        || ev->_eventcode == EventCode::KEY_REPEAT) {
      _downkeys[ev->miKeyCode] = (ev->_eventcode != EventCode::KEY_UP);
      // The hit-test loop above may have already handled this event
      // and popped overlays (e.g. OverlayLineEdit ENTER). Guard against empty stack.
      if (!_overlay_stack.empty()) {
        // Hold a local copy of the widget shared_ptr so it survives
        // even if the event handler pops/dismisses the overlay
        auto top_widget = _overlay_stack.back()._widget;
        if (top_widget) {
          rval = top_widget->OnUiEvent(ev);
          if (rval.wasHandled()) {
            _prevevent = *ev;
            _prevtime = curtime;
            return rval;
          }
        }
      }
    }

    // MOVE events: route to overlays first, then fall through to widget tree
    if (ev->_eventcode == EventCode::MOVE) {
      if (event_in_overlay) {
        _prevevent = *ev;
        _prevtime = curtime;
        return rval;
      }
      // fall through to normal MOVE handling below
    }

    // PUSH/DOUBLECLICK: if click was inside an overlay, consume it
    if (ev->_eventcode == EventCode::PUSH || ev->_eventcode == EventCode::DOUBLECLICK) {
      if (event_in_overlay) {
        _prev_click_time = curtime;
        _prevevent = *ev;
        _prevtime = curtime;
        return rval;
      }
      // Click outside all overlays: check dismiss policy
      if (!_overlay_stack.empty() && _overlay_stack.back()._dismiss_on_click_outside) {
        dismissAllOverlays();
      }
      rval.setHandled(nullptr);
      _prev_click_time = curtime;
      _prevevent = *ev;
      _prevtime = curtime;
      return rval;
    }

    // MOUSEWHEEL inside overlay
    if (ev->_eventcode == EventCode::MOUSEWHEEL && event_in_overlay) {
      _prevevent = *ev;
      _prevtime = curtime;
      return rval;
    }
  }

  /////////////////////////////////
  // PHASE 2: Widget-specific handling
  // drag operations always target
  //  the widget they started on..
  /////////////////////////////////
  switch (ev->_eventcode) {
    case EventCode::KEY_DOWN: {
      _downkeys[ev->miKeyCode] = true;
      _evdragtarget = nullptr;
      auto dest     = _top->routeUiEvent(ev);
      rval = _dispatchToTarget(dest, ev);
      break;
    }
    case EventCode::KEY_UP: {
      _downkeys[ev->miKeyCode] = false;
      _evdragtarget = nullptr;
      auto dest     = _top->routeUiEvent(ev);
      rval = _dispatchToTarget(dest, ev);
      break;
    }
    /////////////////////////////////
    case EventCode::DRAG: {
      if (_prevevent._eventcode != EventCode::DRAG) { // start drag
        // Use push target instead of routing again (mouse may have moved)
        _evdragtarget = _evpushtarget;
        //////////////////////////
        // synthesize BEGIN_DRAG event
        //////////////////////////
        *_tempevent            = *ev;
        _tempevent->_eventcode = EventCode::BEGIN_DRAG;
        if(_evdragtarget)
          _evdragtarget->OnUiEvent(_tempevent);
        //////////////////////////
      }
      rval = _evdragtarget //
                 ? _dispatchToTarget(_evdragtarget, ev)
                 : _top->handleUiEvent(ev);
      break;
    }
    /////////////////////////////////
    case EventCode::MOVE: {
      EASY_BLOCK("uictx::evc::MOVE", profiler::colors::Red);
      _evdragtarget = nullptr;
      auto target   = _top->routeUiEvent(ev);
      if (target != _mousefocuswidget) {
        if (_mousefocuswidget) {
          if (target) {
            //////////////////////////
            EASY_BLOCK("uictx::evc::MOVEH1", profiler::colors::Red);
            rval = target->OnUiEvent(ev);
            EASY_END_BLOCK;
            //////////////////////////
            // synthesize MOUSE_LEAVE event for OLD widget
            //////////////////////////
            EASY_BLOCK("uictx::evc::MOVEH2", profiler::colors::Red);
            *_tempevent            = *ev;
            _tempevent->_eventcode = EventCode::MOUSE_LEAVE;
            const_cast<Widget*>(_mousefocuswidget)->OnUiEvent(_tempevent);
            EASY_END_BLOCK;
            //////////////////////////
            // synthesize MOUSE_ENTER event
            //////////////////////////
            EASY_BLOCK("uictx::evc::MOVEH3", profiler::colors::Red);
            *_tempevent            = *ev;
            _tempevent->_eventcode = EventCode::MOUSE_ENTER;
            target->OnUiEvent(_tempevent);
            EASY_END_BLOCK;
          }
        }
        _mousefocuswidget = target;
      }
      EASY_BLOCK("uictx::evc::MOVEH4", profiler::colors::Red);
      rval = _dispatchToTarget(target, ev);
      break;
    }
    /////////////////////////////////
    case EventCode::RELEASE: {
      //////////
      // and a release on drag always go to the same..
      //////////
      if (_evdragtarget) {
        //////////////////////////
        // synthesize END_DRAG event
        //////////////////////////
        *_tempevent            = *ev;
        _tempevent->_eventcode = EventCode::END_DRAG;
        _evdragtarget->OnUiEvent(_tempevent);
        //////////////////////////
        rval          = _dispatchToTarget(_evdragtarget, ev);
        _evdragtarget = nullptr;
      } else
        rval = _top->handleUiEvent(ev);
      _evpushtarget = nullptr;  // Clear push target on release
      break;
    }
    /////////////////////////////////
    case EventCode::GOT_KEYFOCUS: {
      _evdragtarget     = nullptr;
      rval              = _top->handleUiEvent(ev);
      _hasKeyboardFocus = true;
      break;
    }
    /////////////////////////////////
    case EventCode::LOST_KEYFOCUS: {
      _evdragtarget     = nullptr;
      rval              = _top->handleUiEvent(ev);
      _hasKeyboardFocus = false;
      break;
    }
    /////////////////////////////////
    case EventCode::PUSH: {

      double clickdelta = curtime - _prev_click_time;
      double dblclickdelta = curtime - _prev_dbl_click_time;

      _evdragtarget = nullptr;
      auto dest     = _top->routeUiEvent(ev);
      _evpushtarget = dest;  // Store push target for drag promotion
      if (dest){

        // SYNTHESIZE DOUBLECLICK EVENT
        if ((dblclickdelta>0.75) and (clickdelta < 0.5)) {
          auto mut_ev = std::const_pointer_cast<Event>(ev);
          mut_ev->_eventcode = EventCode::DOUBLECLICK;
          _prev_dbl_click_time = curtime; // add delay for double-double click
        }

        rval = _dispatchToTarget(dest, ev);
      }
      _prev_click_time = curtime;

      break;
    }
    /////////////////////////////////
    default: {
      _evdragtarget = nullptr;
      auto dest     = _top->routeUiEvent(ev);
      rval = _dispatchToTarget(dest, ev);
      break;
    }
      /////////////////////////////////
  }

  /////////////////////////////////
  // PHASE 3: Application Fallback Handler
  // (if event was not handled by widgets)
  /////////////////////////////////
  if (!rval.wasHandled() && _appFallbackHandler) {
    rval = _appFallbackHandler(ev);
  }

  /////////////////////////////////
  _prevevent = *ev;
  _prevtime      = curtime;
  /////////////////////////////////
  return rval;
}
/////////////////////////////////////////////////////////////////////////
bool Context::hasMouseFocus(const Widget* w) const {
  return w == _mousefocuswidget;
}
//////////////////////////////////////
void Context::draw(drawevent_constptr_t drwev) {
  // Process deferred operations from previous frame
  processNextFrameOps();

  // Lazy init theme engine on first draw
  if (_theme_engine && _theme_engine->_impl.isSet() == false) {
    auto tgt = drwev->GetTarget();
    _theme_engine->gpuInit(tgt);
  }

  _top->draw(drwev);

  // Draw overlays on top of widget tree (bottom to top)
  for (auto& entry : _overlay_stack) {
    if (entry._widget) {
      entry._widget->draw(drwev);
    }
  }
}
/////////////////////////////////////////////////////////////////////////
void Context::clearWidgetPointers(Widget* w) {
  if (_evpushtarget == w) _evpushtarget = nullptr;
  if (_evdragtarget == w) _evdragtarget = nullptr;
  if (_mousefocuswidget == w) _mousefocuswidget = nullptr;
  if (auto sp = _keyboard_focus_widget.lock(); sp && sp.get() == w) _keyboard_focus_widget.reset();
}
/////////////////////////////////////////////////////////////////////////
void Context::dumpWidgets(std::string label) const{

  struct Item{
    Widget* w = nullptr;
    int level = 0;
  };

  std::stack<Item> wstack;
  wstack.push({_top.get(),0});

  printf( "///////////////////////////////////////////////////////\n");
  printf( "// UICONTEXT<%p> widgetdump<%s>\n", this, label.c_str() );
  while(not wstack.empty()){
    auto top = wstack.top();
    auto w = top.w;
    int l = top.level;
    wstack.pop();
    auto indent = std::string(l*2,'.');
    printf( "// %s widget<%p> name<%s>\n", indent.c_str(), top.w, top.w->_name.c_str() );
    if( auto as_group = dynamic_cast<Group*>(w) ){
      for( auto child : as_group->_children ){
        wstack.push({child.get(),l+1});
      }
    }
  }
  printf( "///////////////////////////////////////////////////////\n");
}
/////////////////////////////////////////////////////////////////////////
void Context::pushOverlay(widget_ptr_t widget, int x, int y, int w, int h,
                          bool dismiss_on_click_outside,
                          std::function<void()> on_dismissed) {
  // Flip upward if the overlay would overflow below the window
  if (_top && (y + h) > _top->height()) {
    y = y - h;
    if (y < 0) y = 0;
  }
  OverlayEntry entry;
  entry._widget = widget;
  entry._dismiss_on_click_outside = dismiss_on_click_outside;
  entry._onDismissed = on_dismissed;
  widget->SetRect(x, y, w, h);
  widget->_uicontext = this;
  _overlay_stack.push_back(entry);
}
/////////////////////////////////////////////////////////////////////////
void Context::popOverlay() {
  if (_overlay_stack.empty()) return;
  auto entry = _overlay_stack.back();
  _overlay_stack.pop_back();
  if (entry._widget) {
    clearWidgetPointers(entry._widget.get());
    entry._widget->onPreDestroy();
  }
  if (entry._onDismissed) {
    entry._onDismissed();
  }
}
/////////////////////////////////////////////////////////////////////////
void Context::dismissAllOverlays() {
  while (not _overlay_stack.empty()) {
    popOverlay();
  }
}
/////////////////////////////////////////////////////////////////////////
bool Context::hasOverlays() const {
  return not _overlay_stack.empty();
}
/////////////////////////////////////////////////////////////////////////
void Context::enqueueOnNextFrame(std::function<void()> op) {
  _nextFrameOps.push_back(std::move(op));
}
/////////////////////////////////////////////////////////////////////////
void Context::processNextFrameOps() {
  auto ops = std::move(_nextFrameOps);
  _nextFrameOps.clear();
  for (auto& op : ops) {
    op();
  }
}
/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
