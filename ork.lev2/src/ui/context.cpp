#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/group.h>

namespace ork::ui {

HandlerResult Context::handleEvent(event_constptr_t ev) {
  OrkAssert(_top);
  HandlerResult rval;
  /////////////////////////////////
  // drag operations always target
  //  the widget they started on..
  /////////////////////////////////
  switch (ev->_eventcode) {
    case EventCode::DRAG: {
      if (_prevevent._eventcode != EventCode::DRAG) { // start drag
        auto target   = _top->routeUiEvent(ev);
        _evdragtarget = target;
      }
      if (_evdragtarget) {
        _evdragtarget->OnUiEvent(ev);
      } else {
        rval = _top->handleUiEvent(ev);
      }
      break;
    }
    default:
      //////////
      // and a release on drag always go to the same..
      //////////
      if (_evdragtarget and ev->_eventcode == EventCode::RELEASE)
        _evdragtarget->OnUiEvent(ev);
      else
        rval = _top->handleUiEvent(ev);
      //////////
      if (_prevevent._eventcode == EventCode::DRAG) { // end drag
        _evdragtarget = nullptr;
      }
      break;
  }
  /////////////////////////////////
  _prevevent = *ev;

  // UpdateMouseFocus(ret, Ev);

  return rval;
}

bool Context::hasMouseFocus(const Widget* w) const {
  return w == _mousefocuswidget;
}

/*
void Context::updateMouseFocus(const HandlerResult& r, event_constptr_t Ev) {
  Widget* ponenter = nullptr;
  Widget* ponexit  = nullptr;

  const auto& filtev = Ev->mFilteredEvent;

  if (r.mHandler != gMouseFocus) {
  }

  Widget* plfp = gFastPath;

  switch (Ev->_eventcode) {
    case ui::EventCode::PUSH:
      gMouseFocus = r.mHandler;
      gFastPath   = r.mHandler;
      break;
    case ui::EventCode::RELEASE:
      if (gFastPath)
        ponexit = gFastPath;
      gFastPath   = nullptr;
      gMouseFocus = nullptr;
      break;
    case ui::EventCode::MOVE:
    case ui::EventCode::DRAG:
    default:
      break;
  }
  if (plfp != gFastPath) {
    if (plfp)
      printf("widget<%p:%s> has lost the fastpath\n", plfp, plfp->msName.c_str());

    if (gFastPath)
      printf("widget<%p:%s> now has the fastpath\n", gFastPath, gFastPath->msName.c_str());
  }

  if (ponexit)
    ponexit->exit();

  if (ponenter)
    ponenter->enter();
}*/

} // namespace ork::ui
