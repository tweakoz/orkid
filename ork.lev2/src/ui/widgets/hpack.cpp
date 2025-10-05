#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/ui/pack.h>
#include <ork/lev2/ui/event.h>

namespace ork::ui {

/////////////////////////////////////////////////////////////////////////
HorizontalPack::HorizontalPack(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h) {
}

/////////////////////////////////////////////////////////////////////////
HorizontalPack::~HorizontalPack() {
}

/////////////////////////////////////////////////////////////////////////
void HorizontalPack::_doOnResized() {
  DoLayout();
}

/////////////////////////////////////////////////////////////////////////
void HorizontalPack::DoLayout() {

  size_t X = 0;
  // Layout all children horizontally
  size_t num_children = _children.size();
  for( size_t i=0; i<num_children; i++ ){
    auto child = _children[i];
    if( _fill && (i==num_children-1) ){
      int remaining_w = _geometry._w - X;
      child->SetRect(X, 0, remaining_w, _geometry._h);
      break;
    }
    child->SetRect(X, 0, _item_width, _geometry._h);
    X += _item_width + _margin;
  }
}

/////////////////////////////////////////////////////////////////////////
Widget* HorizontalPack::doRouteUiEvent(event_constptr_t ev) {
  // Convert event coordinates to local space
  int localX = ev->miX - _geometry._x;
  int localY = ev->miY - _geometry._y;
  // find which child (if any) the event is inside
  int child_index = localX / (_item_width + _margin);
  if (child_index >= 0 && child_index < int(_children.size())) {
    auto child = _children[child_index];
    if (child->IsEventInside(ev)) {
      return child->doRouteUiEvent(ev);
    }
  }
  return nullptr;
}

/////////////////////////////////////////////////////////////////////////
HandlerResult HorizontalPack::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;

  // Convert to local coordinates
  int localX = ev->miX - _geometry._x;
  int localY = ev->miY - _geometry._y;

  switch (ev->_eventcode) {
    case EventCode::PUSH: 
    case EventCode::MOVE: 
    case EventCode::MOUSE_ENTER:
    case EventCode::MOUSE_LEAVE: 
    default:
      break;
  }

  return result;
}

/////////////////////////////////////////////////////////////////////////
void HorizontalPack::DoDraw(drawevent_constptr_t drwev) {
  // Draw content area background
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto primi = tgt->PRI();
  auto fbi = tgt->FBI();
  auto defmtl = lev2::defaultUIMaterial();

  mtxi->PushUIMatrix();
  mtxi->PopUIMatrix();

  ///////////////////////////////////
  // create scissor for content area
  ///////////////////////////////////

  size_t num_children = _children.size();
  int scissor_x = _geometry._x;
  int scissor_y = _geometry._y;
  int scissor_w = num_children*(_item_width+_margin);
  scissor_w = std::min(scissor_w, _geometry._w);
  int scissor_h = _geometry._h;

  ///////////////////////////////////
  //fbi->pushScissor(scissor_x, scissor_y, scissor_w, scissor_h);
  for( auto c : _children ){
    c->draw(drwev);
  }
  //fbi->popScissor();
  ///////////////////////////////////
}

/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
