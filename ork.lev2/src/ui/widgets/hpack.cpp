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

  // Count enabled children for margin calculation
  size_t num_enabled = 0;
  for (size_t i = 0; i < num_children; i++) {
    if (_children[i]->_enable) num_enabled++;
  }
  if (num_enabled == 0) return;

  if (_uniform) {
    // Distribute children uniformly across width, respecting fixed widths
    int total_margin = _margin * (num_enabled - 1);
    int available_width = _geometry._w - total_margin;

    // First pass: count non-fixed children and sum fixed widths
    int num_non_fixed = 0;
    int total_fixed_width = 0;
    for (size_t i = 0; i < num_children; i++) {
      auto child = _children[i];
      if (!child->_enable) continue;  // skip disabled
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
      if (!child->_enable) continue;  // skip disabled
      int w = child->_fixed_width ? child->_fixed_width : uniform_width;
      child->SetRect(x, 0, w, _geometry._h);
      x += w + _margin;
    }
  } else {
    // Original behavior: use _item_width and _margin
    // First pass: calculate total fixed width to determine fill size
    int total_fixed = 0;
    int fill_index = -1;

    for (size_t i = 0; i < num_children; i++) {
      auto child = _children[i];
      if (!child->_enable) continue;  // skip disabled
      bool is_fill_widget = (_fill_widget && child == _fill_widget) ||
                            (!_fill_widget && _fill && (i == num_children - 1));
      if (is_fill_widget) {
        fill_index = i;
      } else if (child->_fixed_width) {
        total_fixed += child->_fixed_width + _margin;
      } else {
        total_fixed += _item_width + _margin;
      }
    }

    // Calculate fill width
    int fill_width = (fill_index >= 0) ? _geometry._w - total_fixed : 0;

    // Second pass: layout children
    size_t X = 0;
    for (size_t i = 0; i < num_children; i++) {
      auto child = _children[i];
      if (!child->_enable) continue;  // skip disabled
      bool is_fill_widget = (int)i == fill_index;

      // Determine child width
      int child_width;
      if (is_fill_widget) {
        child_width = fill_width;
      } else if (child->_fixed_width) {
        child_width = child->_fixed_width;
      } else {
        child_width = _item_width;
      }

      child->SetRect(X, 0, child_width, _geometry._h);
      X += child_width + _margin;
    }
  }
}

/////////////////////////////////////////////////////////////////////////
Widget* HorizontalPack::doRouteUiEvent(event_constptr_t ev) {
  // Don't route events if drawing is disabled
  if (!_enable) return nullptr;

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
    if (!child->_enable) continue;  // skip disabled
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
  fbi->pushScissorIntersected(scissor_x, scissor_y, scissor_w, scissor_h);

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
int HorizontalPack::desiredWidth() const {
  size_t num_children = _children.size();
  if (num_children == 0) return 0;

  int total_width = 0;

  if (_uniform) {
    // In uniform mode, all children share available width
    // Just return current width (no intrinsic size)
    return 0;
  } else {
    // Sum up widths based on _item_width or _fixed_width
    for (size_t i = 0; i < num_children; i++) {
      auto child = _children[i];
      if (child->_fixed_width) {
        total_width += child->_fixed_width;
      } else {
        total_width += _item_width;
      }
      // Add margin between items
      if (i < num_children - 1) {
        total_width += _margin;
      }
    }
  }

  return total_width;
}

/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
