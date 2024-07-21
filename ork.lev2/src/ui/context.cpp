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
HandlerResult Context::handleEvent(event_constptr_t ev) {
  EASY_BLOCK("uictx::handleEvent", profiler::colors::Red);
  OrkAssert(_top);
  HandlerResult rval;
  double curtime = _uitimer.SecsSinceStart();
  if(_overlayWidget){
    auto route  = _overlayWidget->routeUiEvent(ev);
    if(route == _overlayWidget.get() ){
      rval = _overlayWidget->OnUiEvent(ev);
      bool was_handled_by_overlay = rval.wasHandled();

      if( not _overlayHandledPrevious and was_handled_by_overlay){
        _mousefocuswidget = nullptr;
        // emit a mouse leave event for native system
        auto evcopy = std::make_shared<Event>(*ev);
        evcopy->_eventcode = EventCode::MOUSE_LEAVE;
        _top->handleUiEvent(evcopy);
      } else if( _overlayHandledPrevious and not was_handled_by_overlay){
        // emit a mouse enter event for native system
        auto evcopy = std::make_shared<Event>(*ev);
        evcopy->_eventcode = EventCode::MOUSE_ENTER;
        _top->handleUiEvent(evcopy);
      }
      _overlayHandledPrevious = was_handled_by_overlay;
      if(was_handled_by_overlay)
        return rval;
    }
  }
  /////////////////////////////////
  // drag operations always target
  //  the widget they started on..
  /////////////////////////////////
  switch (ev->_eventcode) {
    case EventCode::KEY_DOWN: {
      _downkeys[ev->miKeyCode] = true;
      _evdragtarget = nullptr;
      auto dest     = _top->routeUiEvent(ev);
      if (dest)
        rval = dest->OnUiEvent(ev);
      break;
    }
    case EventCode::KEY_UP: {
      _downkeys[ev->miKeyCode] = false;
      _evdragtarget = nullptr;
      auto dest     = _top->routeUiEvent(ev);
      if (dest)
        rval = dest->OnUiEvent(ev);
      break;
    }
    /////////////////////////////////
    case EventCode::DRAG: {
      if (_prevevent._eventcode != EventCode::DRAG) { // start drag
        auto target   = _top->routeUiEvent(ev);
        _evdragtarget = target;
        //////////////////////////
        // synthesize BEGIN_DRAGg event
        //////////////////////////
        *_tempevent            = *ev;
        _tempevent->_eventcode = EventCode::BEGIN_DRAG;
        if(_evdragtarget)
          _evdragtarget->OnUiEvent(_tempevent);
        //////////////////////////
      }
      rval = _evdragtarget //
                 ? _evdragtarget->OnUiEvent(ev)
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
      if (target)
        rval = target->OnUiEvent(ev);
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
        rval          = _evdragtarget->OnUiEvent(ev);
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

         rval = dest->OnUiEvent(ev);
      }
      _prev_click_time = curtime;

      break;
    }
    /////////////////////////////////
    default: {
      _evdragtarget = nullptr;
      auto dest     = _top->routeUiEvent(ev);
      if (dest)
        rval = dest->OnUiEvent(ev);
      break;
    }
      /////////////////////////////////
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
  _top->draw(drwev);
  if (_overlayWidget) {
    _overlayWidget->draw(drwev);
  }
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
