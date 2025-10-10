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

  size_t num_children = _children.size();
  if (num_children == 0) return;

  if (_uniform) {
    // Distribute children uniformly across width, respecting fixed widths
    int total_margin = _margin * (num_children - 1);
    int available_width = _geometry._w - total_margin;

    // First pass: count non-fixed children and sum fixed widths
    int num_non_fixed = 0;
    int total_fixed_width = 0;
    for (size_t i = 0; i < num_children; i++) {
      auto child = _children[i];
      if (child->_fixed_width) {
        total_fixed_width += child->_fixed_width;
      } else {
        num_non_fixed++;
      }
    }

    // Calculate width for uniform (non-fixed) children
    int remaining_width = available_width - total_fixed_width;
    int uniform_width = num_non_fixed > 0 ? remaining_width / num_non_fixed : 0;

    // Second pass: layout children
    int x = 0;
    for (size_t i = 0; i < num_children; i++) {
      auto child = _children[i];
      int w = child->_fixed_width ? child->_fixed_width : uniform_width;
      child->SetRect(x, 0, w, _geometry._h);
      x += w + _margin;
    }
  } else {
    // Original behavior: use _item_width and _margin
    size_t X = 0;
    for (size_t i = 0; i < num_children; i++) {
      auto child = _children[i];

      // Determine child width
      int child_width;
      if (child->_fixed_width) {
        // Use existing width if fixed
        child_width = child->_fixed_width;
      } else if (_fill && (i == num_children - 1)) {
        // Last child fills remaining space
        child_width = _geometry._w - X;
      } else {
        // Use item_width
        child_width = _item_width;
      }

      child->SetRect(X, 0, child_width, _geometry._h);
      X += child_width + _margin;

      if (_fill && (i == num_children - 1)) {
        break;
      }
    }
  }
}

/////////////////////////////////////////////////////////////////////////
Widget* HorizontalPack::doRouteUiEvent(event_constptr_t ev) {
  // Convert event coordinates to local space
  int localX = 0;
  int localY = 0;
  RootToLocal(ev->miX, ev->miY, localX, localY);

  size_t num_children = _children.size();
  if (num_children == 0) return nullptr;

  // Find which child (if any) the event is inside
  // Must iterate through children to handle fixed-width widgets properly
  int x = 0;
  for (size_t i = 0; i < num_children; i++) {
    auto child = _children[i];
    int child_width = child->width();

    // Check if event is within this child's bounds
    if (localX >= x && localX < x + child_width) {
      if (child->IsEventInside(ev)) {
        return child->doRouteUiEvent(ev);
      }
      break;
    }
    x += child_width + _margin;
  }

  return nullptr;
}

/////////////////////////////////////////////////////////////////////////
HandlerResult HorizontalPack::DoOnUiEvent(event_constptr_t ev) {
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
  int scissor_w = _geometry._w+1;
  int scissor_h = _geometry._h+_margin;

  ///////////////////////////////////
  fbi->pushScissor(scissor_x, scissor_y, scissor_w, scissor_h);
  for( auto c : _children ){
    c->draw(drwev);
  }
  fbi->popScissor();
  ///////////////////////////////////
}

/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
