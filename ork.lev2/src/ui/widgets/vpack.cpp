#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/ui/pack.h>
#include <ork/lev2/ui/event.h>

namespace ork::ui {

/////////////////////////////////////////////////////////////////////////
VerticalPack::VerticalPack(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h) {
}

/////////////////////////////////////////////////////////////////////////
VerticalPack::~VerticalPack() {
}

/////////////////////////////////////////////////////////////////////////
void VerticalPack::_doOnResized() {
  DoLayout();
}

/////////////////////////////////////////////////////////////////////////
void VerticalPack::DoLayout() {

  size_t num_children = _children.size();
  if (num_children == 0) return;

  if (_uniform) {
    // Distribute children uniformly across width, respecting fixed widths
    int total_margin = _margin * (num_children - 1);
    int available_height = _geometry._h - total_margin;

    // First pass: count non-fixed children and sum fixed widths
    int num_non_fixed = 0;
    int total_fixed_height = 0;
    for (size_t i = 0; i < num_children; i++) {
      auto child = _children[i];
      if (child->_fixed_height) {
        total_fixed_height += child->_fixed_height;
      } else {
        num_non_fixed++;
      }
    }

    // Calculate height for uniform (non-fixed) children
    int remaining_height = available_height - total_fixed_height;
    int uniform_height = num_non_fixed > 0 ? remaining_height / num_non_fixed : 0;

    // Second pass: layout children
    int y = 0;
    for (size_t i = 0; i < num_children; i++) {
      auto child = _children[i];
      int h = child->_fixed_height ? child->_fixed_height : uniform_height;
      child->SetRect(0, y, _geometry._w, h);
      y += h + _margin;
    }
  } else {
    // Original behavior: use _item_height and _margin
    // First pass: calculate total fixed height to determine fill size
    int total_fixed = 0;
    int fill_index = -1;

    for (size_t i = 0; i < num_children; i++) {
      auto child = _children[i];
      bool is_fill_widget = (_fill_widget && child == _fill_widget) ||
                            (!_fill_widget && _fill && (i == num_children - 1));
      if (is_fill_widget) {
        fill_index = i;
      } else if (child->_fixed_height) {
        total_fixed += child->_fixed_height + _margin;
      } else {
        total_fixed += _item_height + _margin;
      }
    }

    // Calculate fill height
    int fill_height = (fill_index >= 0) ? _geometry._h - total_fixed : 0;

    // Second pass: layout children
    size_t Y = 0;
    for (size_t i = 0; i < num_children; i++) {
      auto child = _children[i];
      bool is_fill_widget = (int)i == fill_index;

      // Determine child height
      int child_height;
      if (is_fill_widget) {
        child_height = fill_height;
      } else if (child->_fixed_height) {
        child_height = child->_fixed_height;
      } else {
        child_height = _item_height;
      }

      child->SetRect(0, Y, _geometry._w, child_height);
      Y += child_height + _margin;
    }
  }
}

/////////////////////////////////////////////////////////////////////////
Widget* VerticalPack::doRouteUiEvent(event_constptr_t ev) {
  // Convert event coordinates to local space
  int localX = 0;
  int localY = 0;
  RootToLocal(ev->miX, ev->miY, localX, localY);

  // Find which child (if any) the event is inside
  // Must iterate through children to handle fixed-height widgets properly
  int y = 0;
  for (size_t i = 0; i < _children.size(); i++) {
    auto child = _children[i];
    int child_height = child->height();

    // Check if event is within this child's bounds
    if (localY >= y && localY < y + child_height) {
      if (child->IsEventInside(ev)) {
        return child->doRouteUiEvent(ev);
      }
      break;
    }
    y += child_height + _margin;
  }

  return nullptr;
}

/////////////////////////////////////////////////////////////////////////
HandlerResult VerticalPack::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;

  // Convert to local coordinates
  int localX = 0;
  int localY = 0;
  RootToLocal(ev->miX, ev->miY, localX, localY);

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
void VerticalPack::DoDraw(drawevent_constptr_t drwev) {
  // Draw content area background
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto primi = tgt->PRI();
  auto fbi = tgt->FBI();
  auto defmtl = lev2::defaultUIMaterial();

  ///////////////////////////////////
  // create scissor for content area
  ///////////////////////////////////

  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  int ix2 = ix1 + _geometry._w;
  int iy2 = iy1 + _geometry._h;

  size_t num_children = _children.size();
  int scissor_x = ix1;
  int scissor_y = iy1;
  int scissor_w = _geometry._w+_margin;
  int scissor_h = _geometry._h+_margin;

  ///////////////////////////////////
  fbi->pushScissor(scissor_x, scissor_y, scissor_w, scissor_h);

  if (_draw_background) {
    _drawColoredBox(drwev, _bgcolor);
  }

  for( auto c : _children ){
    c->draw(drwev);
  }
  fbi->popScissor();
  ///////////////////////////////////
}

/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui