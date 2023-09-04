#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/group.h>
/////////////////////////////////////////////////////////////////////////
namespace ork::ui {
/////////////////////////////////////////////////////////////////////////
Context::Context() {
  _tempevent = std::make_shared<Event>();
  _uitimer.Start();
  _prevtime = 0.0;
  _renderpass = std::make_shared<lev2::RenderPass>();
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
  OrkAssert(_top);
  HandlerResult rval;
  double curtime = _uitimer.SecsSinceStart();
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
  auto gfx_ctx = drwev->GetTarget();
  gfx_ctx->beginRenderPass(_renderpass);
  _top->draw(drwev);
  if (_overlayWidget) {
    _overlayWidget->draw(drwev);
  }
  gfx_ctx->endRenderPass(_renderpass);
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
