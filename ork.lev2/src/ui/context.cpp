#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/group.h>
/////////////////////////////////////////////////////////////////////////
namespace ork::ui {
/////////////////////////////////////////////////////////////////////////
Context::Context() {
  _tempevent = std::make_shared<Event>();
}
/////////////////////////////////////////////////////////////////////////
HandlerResult Context::handleEvent(event_constptr_t ev) {
  OrkAssert(_top);
  HandlerResult rval;
  /////////////////////////////////
  // drag operations always target
  //  the widget they started on..
  /////////////////////////////////
  switch (ev->_eventcode) {
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
      _evdragtarget = nullptr;
      auto target   = _top->routeUiEvent(ev);
      if (target != _mousefocuswidget) {
        if (_mousefocuswidget) {
          if (target) {
            //////////////////////////
            rval = target->OnUiEvent(ev);
            //////////////////////////
            // synthesize MOUSE_LEAVE event
            //////////////////////////
            *_tempevent            = *ev;
            _tempevent->_eventcode = EventCode::MOUSE_LEAVE;
            target->OnUiEvent(_tempevent);
            //////////////////////////
            // synthesize MOUSE_ENTER event
            //////////////////////////
            *_tempevent            = *ev;
            _tempevent->_eventcode = EventCode::MOUSE_ENTER;
            target->OnUiEvent(_tempevent);
          }
        }
        _mousefocuswidget = target;
      }
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
    default: {
      _evdragtarget = nullptr;
      rval          = _top->handleUiEvent(ev);
      break;
    }
      /////////////////////////////////
  }
  /////////////////////////////////
  _prevevent = *ev;
  return rval;
}
/////////////////////////////////////////////////////////////////////////
bool Context::hasMouseFocus(const Widget* w) const {
  return w == _mousefocuswidget;
}
/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
