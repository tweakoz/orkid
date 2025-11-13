#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/ui/alignmentgroup.h>
#include <ork/lev2/ui/event.h>

namespace ork::ui {

/////////////////////////////////////////////////////////////////////////
AlignmentGroup::AlignmentGroup(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h) {
}

/////////////////////////////////////////////////////////////////////////
AlignmentGroup::~AlignmentGroup() {
}

/////////////////////////////////////////////////////////////////////////
void AlignmentGroup::_doOnResized() {
  DoLayout();
}

/////////////////////////////////////////////////////////////////////////
void AlignmentGroup::DoLayout() {
  if (_children.empty()) {
    return;
  }

  // AlignmentGroup only layouts the first child
  auto child = _children[0];

  // Available space (minus margins)
  int available_width = _geometry._w - (_margin * 2);
  int available_height = _geometry._h - (_margin * 2);

  ///////////////////////////////////////////////
  // Calculate child width
  ///////////////////////////////////////////////
  int child_width = child->width();

  // Apply proportional width if set
  if (_width_proportional >= 0.0f) {
    child_width = int(available_width * _width_proportional);
  }

  // Apply pixel constraints
  if (_min_width_pixels >= 0 && child_width < _min_width_pixels) {
    child_width = _min_width_pixels;
  }
  if (_max_width_pixels >= 0 && child_width > _max_width_pixels) {
    child_width = _max_width_pixels;
  }

  // Clamp to available space
  if (child_width > available_width) {
    child_width = available_width;
  }

  ///////////////////////////////////////////////
  // Calculate child height
  ///////////////////////////////////////////////
  int child_height = child->height();

  // Apply proportional height if set
  if (_height_proportional >= 0.0f) {
    child_height = int(available_height * _height_proportional);
  }

  // Apply pixel constraints
  if (_min_height_pixels >= 0 && child_height < _min_height_pixels) {
    child_height = _min_height_pixels;
  }
  if (_max_height_pixels >= 0 && child_height > _max_height_pixels) {
    child_height = _max_height_pixels;
  }

  // Clamp to available space
  if (child_height > available_height) {
    child_height = available_height;
  }

  ///////////////////////////////////////////////
  // Maintain aspect ratio if requested
  ///////////////////////////////////////////////
  if (_maintain_aspect_ratio > 0.0f) {
    float desired_ratio = _maintain_aspect_ratio;

    // Start with current calculated dimensions
    int ideal_width = child_width;
    int ideal_height = (int)(ideal_width / desired_ratio);

    // Apply HARD max constraints
    if (_max_width_pixels >= 0 && ideal_width > _max_width_pixels) {
      ideal_width = _max_width_pixels;
      ideal_height = (int)(ideal_width / desired_ratio);
    }
    if (_max_height_pixels >= 0 && ideal_height > _max_height_pixels) {
      ideal_height = _max_height_pixels;
      ideal_width = (int)(ideal_height * desired_ratio);
    }

    // Apply HARD available space constraint
    if (ideal_width > available_width || ideal_height > available_height) {
      // Scale down to fit, maintaining ratio
      float width_scale = (float)available_width / ideal_width;
      float height_scale = (float)available_height / ideal_height;
      float scale = std::min(width_scale, height_scale);

      ideal_width = (int)(ideal_width * scale);
      ideal_height = (int)(ideal_height * scale);
    }

    child_width = ideal_width;
    child_height = ideal_height;

    // Note: min constraints are NOT re-applied - they can be violated
  }

  ///////////////////////////////////////////////
  // Calculate child position based on alignment
  ///////////////////////////////////////////////
  int child_x = _margin;
  int child_y = _margin;

  switch (_alignment) {
    case Alignment::TOP_LEFT:
      child_x = _margin;
      child_y = _margin;
      break;
    case Alignment::TOP_CENTER:
      child_x = _margin + (available_width - child_width) / 2;
      child_y = _margin;
      break;
    case Alignment::TOP_RIGHT:
      child_x = _margin + (available_width - child_width);
      child_y = _margin;
      break;
    case Alignment::CENTER_LEFT:
      child_x = _margin;
      child_y = _margin + (available_height - child_height) / 2;
      break;
    case Alignment::CENTER:
      child_x = _margin + (available_width - child_width) / 2;
      child_y = _margin + (available_height - child_height) / 2;
      break;
    case Alignment::CENTER_RIGHT:
      child_x = _margin + (available_width - child_width);
      child_y = _margin + (available_height - child_height) / 2;
      break;
    case Alignment::BOTTOM_LEFT:
      child_x = _margin;
      child_y = _margin + (available_height - child_height);
      break;
    case Alignment::BOTTOM_CENTER:
      child_x = _margin + (available_width - child_width) / 2;
      child_y = _margin + (available_height - child_height);
      break;
    case Alignment::BOTTOM_RIGHT:
      child_x = _margin + (available_width - child_width);
      child_y = _margin + (available_height - child_height);
      break;
  }

  child->SetRect(child_x, child_y, child_width, child_height);
}

/////////////////////////////////////////////////////////////////////////
Widget* AlignmentGroup::doRouteUiEvent(event_constptr_t ev) {
  if (_children.empty()) {
    return nullptr;
  }

  auto child = _children[0];
  if (child->IsEventInside(ev)) {
    return child->doRouteUiEvent(ev);
  }

  return nullptr;
}

/////////////////////////////////////////////////////////////////////////
HandlerResult AlignmentGroup::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;

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
void AlignmentGroup::DoDraw(drawevent_constptr_t drwev) {
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto primi = tgt->PRI();
  auto fbi = tgt->FBI();
  auto defmtl = lev2::defaultUIMaterial();

  ///////////////////////////////////
  // Create scissor for content area
  ///////////////////////////////////

  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  int ix2 = ix1 + _geometry._w;
  int iy2 = iy1 + _geometry._h;

  int scissor_x = ix1;
  int scissor_y = iy1;
  int scissor_w = _geometry._w;
  int scissor_h = _geometry._h;

  ///////////////////////////////////
  fbi->pushScissor(scissor_x, scissor_y, scissor_w, scissor_h);

  if (_draw_background) {
    _drawColoredBox(drwev, _bgcolor);
  }

  // Draw child
  if (!_children.empty()) {
    _children[0]->draw(drwev);
  }

  fbi->popScissor();
  ///////////////////////////////////
}

/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
