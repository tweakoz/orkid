////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/ui/collapsable.h>
#include <ork/lev2/ui/group.h>
#include <ork/lev2/ui/scroll_container.h>
#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/style.h>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////

Collapsable::Collapsable(
    const std::string& name,
    int x,
    int y,
    int w,
    int h)
    : Widget(name, x, y, w, h) {
  _draw_label = false;  // We draw our own label in the header
}

///////////////////////////////////////////////////////////////////////////////

Collapsable::~Collapsable() {
  if (_child) {
    _child->setParent(nullptr);
  }
}

///////////////////////////////////////////////////////////////////////////////

void Collapsable::setChild(widget_ptr_t child) {
  // Remove old child
  if (_child) {
    _child->setParent(nullptr);
  }

  _child = child;

  if (_child) {
    _child->setParent(reinterpret_cast<Group*>(this));
    _child->_uicontext = _uicontext;
    _child->_target = _target;
    _layoutChild();
  }
}

///////////////////////////////////////////////////////////////////////////////

void Collapsable::setExpanded(bool expanded) {
  if (_expanded != expanded) {
    // Calculate height change for scroll adjustment
    int old_height = desiredHeight();
    _expanded = expanded;
    int new_height = desiredHeight();
    int height_delta = new_height - old_height;

    _layoutChild();

    // Trigger parent relayout so it can adjust to our new desired height
    if (_parent) {
      _parent->DoLayout();
    }

    // Find parent ScrollContainer and adjust scroll offset to keep header in place
    // When expanding, we need to scroll down by the height increase
    // so that the header stays under the mouse
    if (height_delta != 0) {
      Group* p = _parent;
      while (p) {
        // Check if this is a ScrollContainer by trying dynamic_cast
        // We include scroll_container.h to get access to the type
        if (auto* sc = dynamic_cast<ScrollContainer*>(p)) {
          // Only adjust if this widget is above the bottom of the visible area
          int current_scroll = sc->scrollOffsetY();
          sc->setScrollOffsetY(current_scroll + height_delta);
          break;
        }
        p = p->parent();
      }
    }

    if (_onToggle) {
      _onToggle(_expanded);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

int Collapsable::desiredWidth() const {
  if (_child && _expanded) {
    int child_w = _child->desiredWidth();
    return (child_w > 0) ? child_w : _geometry._w;
  }
  return _geometry._w;
}

///////////////////////////////////////////////////////////////////////////////

int Collapsable::desiredHeight() const {
  if (_expanded && _child) {
    int child_h = _child->desiredHeight();
    if (child_h > 0) {
      // header + top margin + child + bottom margin
      return _header_height + _content_margin + child_h + _content_margin;
    }
    // If child has no desired height, use its current height
    return _header_height + _content_margin + _child->height() + _content_margin;
  }
  return _header_height;
}

///////////////////////////////////////////////////////////////////////////////

void Collapsable::_doGpuInit(lev2::Context* ctx) {
  if (_child) {
    _child->gpuInit(ctx);
  }
}

///////////////////////////////////////////////////////////////////////////////

void Collapsable::DoLayout() {
  _layoutChild();
}

///////////////////////////////////////////////////////////////////////////////

void Collapsable::_doOnResized() {
  _layoutChild();
}

///////////////////////////////////////////////////////////////////////////////

void Collapsable::_layoutChild() {
  if (!_child) return;

  if (_expanded) {
    // Position child below header with margin on all sides
    int child_x = _content_margin;
    int child_y = _header_height + _content_margin;
    int child_w = _geometry._w - (_content_margin * 2);
    int child_h = _geometry._h - _header_height - (_content_margin * 2);

    // Respect child's desired height if set
    int desired_h = _child->desiredHeight();
    if (desired_h > 0) {
      child_h = desired_h;
    }

    _child->SetRect(child_x, child_y, child_w, child_h);
  }
}

///////////////////////////////////////////////////////////////////////////////

bool Collapsable::_isEventInHeader(event_constptr_t ev) const {
  int localX, localY;
  RootToLocal(ev->miX, ev->miY, localX, localY, true);

  return (localX >= 0 && localX < _geometry._w &&
          localY >= 0 && localY < _header_height);
}

///////////////////////////////////////////////////////////////////////////////

Widget* Collapsable::doRouteUiEvent(event_constptr_t ev) {
  if (!IsEventInside(ev)) {
    return nullptr;
  }

  // If event is in header area, this widget handles it
  if (_isEventInHeader(ev)) {
    return this;
  }

  // If expanded and have child, route to child
  if (_expanded && _child) {
    auto routed = _child->doRouteUiEvent(ev);
    if (routed) {
      return routed;
    }
  }

  return this;
}

///////////////////////////////////////////////////////////////////////////////

HandlerResult Collapsable::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult rval;

  switch (ev->_eventcode) {
    case EventCode::PUSH: {
      // Click in header toggles expansion
      if (_isEventInHeader(ev)) {
        toggle();
        rval.setHandled(this);
      }
      break;
    }

    case EventCode::RELEASE: {
      if (_isEventInHeader(ev)) {
        rval.setHandled(this);
      }
      break;
    }

    default:
      break;
  }

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void Collapsable::DoDraw(drawevent_constptr_t drwev) {
  auto tgt = drwev->GetTarget();
  auto fbi = tgt->FBI();
  auto mtxi = tgt->MTXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  int ix2 = ix1 + _geometry._w;
  int header_y2 = iy1 + _header_height;

  mtxi->PushUIMatrix();
  {
    ///////////////////////////////
    // Draw header background
    ///////////////////////////////

    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);

    tgt->PushModColor(_header_bg_color);
    primi->RenderQuadAtZ(
        defmtl.get(),
        ix1, ix2,
        iy1, header_y2,
        0.0f,
        0.0f, 1.0f,
        0.0f, 1.0f);
    tgt->PopModColor();

    ///////////////////////////////
    // Draw disclosure triangle (matching property sheet style)
    ///////////////////////////////

    if (_uicontext && _uicontext->_theme_engine) {
      auto theme = _uicontext->_theme_engine;

      Style tri_style;
      tri_style._bg_color = _triangle_color;
      tri_style._border_color = fvec4(_triangle_color.xyz(), 0.0f);
      tri_style._corner_radius = 0;
      tri_style._border_width = 0;
      tri_style._blend_mode = lev2::BlendingMacro::ALPHA;

      const int tri_size = 10;
      int tri_x = ix1 + (_indent_width - tri_size) / 2;
      int tri_y = iy1 + (_header_height - tri_size) / 2;

      // Rotation: 0 = point down (expanded), PI/2 = point right (collapsed)
      float rotation = _expanded ? 0.0f : PI / 2.0f;
      theme->drawTriangle(tri_x, tri_y, tri_size, tri_size, drwev, &tri_style, rotation);
    }

    ///////////////////////////////
    // Draw label text
    ///////////////////////////////

    auto font = lev2::FontMan::fontForId("i14");
    if (font && !_name.empty()) {
      lev2::FontMan::PushFont(font);
      tgt->PushModColor(_header_fg_color);
      lev2::FontMan::beginTextBlock(tgt, _name.length());
      int label_x = ix1 + _indent_width;
      int text_y = iy1 + (_header_height - font->description().miAdvanceHeight) / 2;
      lev2::FontMan::DrawText(tgt, label_x, text_y, _name.c_str());
      lev2::FontMan::endTextBlock(tgt);
      tgt->PopModColor();
      lev2::FontMan::PopFont();
    }

    ///////////////////////////////
    // Draw child if expanded
    ///////////////////////////////

    if (_expanded && _child) {
      // Draw content background (wraps around child with margin)
      if (_draw_content_background) {
        int content_y1 = header_y2;
        int content_y2 = iy1 + _geometry._h;

        tgt->PushModColor(_content_bg_color);
        primi->RenderQuadAtZ(
            defmtl.get(),
            ix1, ix2,
            content_y1, content_y2,
            0.0f,
            0.0f, 1.0f,
            0.0f, 1.0f);
        tgt->PopModColor();
      }

      // Draw the child widget
      _child->draw(drwev);
    }
  }
  mtxi->PopUIMatrix();
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ui
