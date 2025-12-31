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
        auto target   = _top->routeUiEvent(ev);
        _evdragtarget = target;
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
            // synthesize MOUSE_LEAVE event
            //////////////////////////
            EASY_BLOCK("uictx::evc::MOVEH2", profiler::colors::Red);
            *_tempevent            = *ev;
            _tempevent->_eventcode = EventCode::MOUSE_LEAVE;
            target->OnUiEvent(_tempevent);
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
  // Lazy init theme engine on first draw
  if (_theme_engine && _theme_engine->_impl.isSet() == false) {
    auto tgt = drwev->GetTarget();
    _theme_engine->gpuInit(tgt);
  }

  _top->draw(drwev);
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
} // namespace ork::ui
