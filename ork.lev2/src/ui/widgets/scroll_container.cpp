#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/ui/scroll_container.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/context.h>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////
ScrollContainer::ScrollContainer(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h) {
  // Match the old indicator color (0.5 alpha vs default 0.4)
  _vscroller._indicator_color = fvec4(1.0f, 1.0f, 1.0f, 0.5f);
  _hscroller._indicator_color = fvec4(1.0f, 1.0f, 1.0f, 0.5f);
}

///////////////////////////////////////////////////////////////////////////////
ScrollContainer::~ScrollContainer() {
}

///////////////////////////////////////////////////////////////////////////////
void ScrollContainer::_doGpuInit(lev2::Context* ctx) {
  // Create initial RTGroup - will be resized as needed
  _rtgroup = std::make_shared<lev2::RtGroup>(ctx, 8, 8, lev2::MsaaSamples::MSAA_1X);
  _rtgroup->_name = FormatString("ScrollContainer<%s>", _name.c_str());
  auto mrt0 = _rtgroup->createRenderTarget(lev2::EBufferFormat::RGBA8);
}

///////////////////////////////////////////////////////////////////////////////
void ScrollContainer::setChild(widget_ptr_t child) {
  // Remove old child if exists
  if (_child) {
    removeChild(_child);
  }

  _child = child;

  if (_child) {
    addChild(_child);
    _layoutChild();
  }

  _hscroller._scroll_offset = 0;
  scrollToTop();
  _content_dirty = true;
}

///////////////////////////////////////////////////////////////////////////////
void ScrollContainer::setScrollMode(ScrollMode mode) {
  _mode = mode;
  _layoutChild();
  _clampScrollOffset();
  _content_dirty = true;
}

///////////////////////////////////////////////////////////////////////////////
void ScrollContainer::setScrollOffsetX(int offset) {
  _hscroller._scroll_offset = offset;
  _clampScrollOffset();
}

///////////////////////////////////////////////////////////////////////////////
void ScrollContainer::setScrollOffsetY(int offset) {
  // Public API uses old convention (0=bottom, maxScrollY=top)
  // Controller uses 0=top, maxScroll=bottom
  _vscroller._scroll_offset = maxScrollY() - offset;
  _clampScrollOffset();
}

///////////////////////////////////////////////////////////////////////////////
void ScrollContainer::setScrollOffset(int x, int y) {
  _hscroller._scroll_offset = x;
  _vscroller._scroll_offset = maxScrollY() - y;
  _clampScrollOffset();
}

///////////////////////////////////////////////////////////////////////////////
void ScrollContainer::scrollToTop() {
  _vscroller._scroll_offset = 0;  // 0 = top in controller convention
}

///////////////////////////////////////////////////////////////////////////////
void ScrollContainer::scrollToBottom() {
  _vscroller._scroll_offset = maxScrollY();  // maxScroll = bottom
}

///////////////////////////////////////////////////////////////////////////////
void ScrollContainer::scrollToLeft() {
  _hscroller._scroll_offset = 0;
}

///////////////////////////////////////////////////////////////////////////////
void ScrollContainer::scrollToRight() {
  _hscroller._scroll_offset = maxScrollX();
}

///////////////////////////////////////////////////////////////////////////////
int ScrollContainer::contentWidth() const {
  if (!_child) return _geometry._w;

  int desired = _child->desiredWidth();
  if (desired > 0) {
    return desired;
  }
  return _child->width();
}

///////////////////////////////////////////////////////////////////////////////
int ScrollContainer::contentHeight() const {
  if (!_child) return _geometry._h;

  int desired = _child->desiredHeight();
  if (desired > 0) {
    return desired;
  }
  return _child->height();
}

///////////////////////////////////////////////////////////////////////////////
int ScrollContainer::maxScrollX() const {
  return std::max(0, contentWidth() - _geometry._w);
}

///////////////////////////////////////////////////////////////////////////////
int ScrollContainer::maxScrollY() const {
  return std::max(0, contentHeight() - _geometry._h);
}

///////////////////////////////////////////////////////////////////////////////
void ScrollContainer::_clampScrollOffset() {
  // Update controller sizes from current content/viewport
  _vscroller._content_size = contentHeight();
  _vscroller._viewport_size = _geometry._h;
  _hscroller._content_size = contentWidth();
  _hscroller._viewport_size = _geometry._w;

  // Clamp based on scroll mode
  switch (_mode) {
    case ScrollMode::Y:
      _hscroller._scroll_offset = 0;
      _vscroller.clamp();
      break;
    case ScrollMode::X:
      _hscroller.clamp();
      _vscroller._scroll_offset = 0;
      break;
    case ScrollMode::XY:
      _hscroller.clamp();
      _vscroller.clamp();
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
void ScrollContainer::_layoutChild() {
  if (!_child) return;

  int child_w = _geometry._w;
  int child_h = _geometry._h;

  int desired_w = _child->desiredWidth();
  int desired_h = _child->desiredHeight();

  switch (_mode) {
    case ScrollMode::Y: {
      // Width constrained to container, height is child's desired height
      child_w = _geometry._w;
      child_h = (desired_h > 0) ? desired_h : _geometry._h;
      break;
    }
    case ScrollMode::X: {
      // Height constrained to container, width is child's desired width
      child_w = (desired_w > 0) ? desired_w : _geometry._w;
      child_h = _geometry._h;
      break;
    }
    case ScrollMode::XY: {
      // Both dimensions use child's desired size
      child_w = (desired_w > 0) ? desired_w : _geometry._w;
      child_h = (desired_h > 0) ? desired_h : _geometry._h;
      break;
    }
  }

  // Cache the desired sizes
  _cached_child_desired_w = desired_w;
  _cached_child_desired_h = desired_h;

  // Position child at origin within the RTG coordinate space
  _child->SetRect(0, 0, child_w, child_h);
  _content_dirty = true;
}

///////////////////////////////////////////////////////////////////////////////
void ScrollContainer::_checkChildSizeChanged() {
  if (!_child) return;

  int desired_w = _child->desiredWidth();
  int desired_h = _child->desiredHeight();

  // Only relayout if desired size changed
  if (desired_w != _cached_child_desired_w || desired_h != _cached_child_desired_h) {
    _layoutChild();
    scrollToTop();
  }
}

///////////////////////////////////////////////////////////////////////////////
void ScrollContainer::DoLayout() {
  _layoutChild();
  _clampScrollOffset();
}

///////////////////////////////////////////////////////////////////////////////
void ScrollContainer::_doOnResized() {
  _layoutChild();
  _clampScrollOffset();
}

///////////////////////////////////////////////////////////////////////////////
void ScrollContainer::_renderContentToRTG(drawevent_constptr_t drwev) {
  if (!_child || !_rtgroup) return;

  auto tgt = drwev->GetTarget();
  auto fbi = tgt->FBI();

  int content_w = contentWidth();
  int content_h = contentHeight();

  // Ensure minimum size
  content_w = std::max(content_w, 8);
  content_h = std::max(content_h, 8);

  // Get ScrollContainer's root position - child renders at these window coordinates
  int root_x, root_y;
  LocalToRoot(0, 0, root_x, root_y);

  // RTG must be large enough to hold content at its window position
  // Child renders at (root_x, root_y) to (root_x + content_w, root_y + content_h)
  int rtg_w = root_x + content_w;
  int rtg_h = root_y + content_h;

  // Resize RTGroup if needed
  if (_rtgroup->width() != rtg_w || _rtgroup->height() != rtg_h) {
    _rtgroup->Resize(rtg_w, rtg_h);
  }

  // Cache the root offset for UV calculations in DoDraw
  _rtg_root_x = root_x;
  _rtg_root_y = root_y;
  _rtg_content_w = content_w;
  _rtg_content_h = content_h;

  // Setup RTGroup for rendering
  _rtgroup->_autoclear = true;
  _rtgroup->buffer(0)->_clearColor = fcolor4(0, 0, 0, 0);  // Transparent background

  // Push a clean RCFD without compositor so PushUIMatrix uses viewport dimensions
  // instead of compositor's CPD (which has window dimensions)
  auto rcfd = std::make_shared<lev2::RenderContextFrameData>(tgt);
  tgt->pushRenderContextFrameData(rcfd);

  // Push RTGroup as render target
  fbi->PushRtGroup(_rtgroup.get());
  fbi->pushViewport(0, 0, rtg_w, rtg_h);
  fbi->pushScissor(0, 0, rtg_w, rtg_h);

  _child->draw(drwev);

  fbi->popScissor();
  fbi->popViewport();
  fbi->PopRtGroup();

  tgt->popRenderContextFrameData();

  _content_dirty = false;
}

///////////////////////////////////////////////////////////////////////////////
Widget* ScrollContainer::doRouteUiEvent(event_constptr_t ev) {
  if (_ignoreEvents) {
    return nullptr;
  }

  // Check if event is within our bounds
  int lx, ly;
  RootToLocal(ev->miX, ev->miY, lx, ly);

  if (lx < 0 || lx >= _geometry._w || ly < 0 || ly >= _geometry._h) {
    return nullptr;
  }

  // MOUSEWHEEL events: ScrollContainer handles them directly if we can scroll
  if (ev->_eventcode == EventCode::MOUSEWHEEL) {
    bool can_scroll_y = (_mode == ScrollMode::Y || _mode == ScrollMode::XY) && maxScrollY() > 0;
    bool can_scroll_x = (_mode == ScrollMode::X || _mode == ScrollMode::XY) && maxScrollX() > 0;
    if ((ev->miMWY != 0 && can_scroll_y) || (ev->miMWX != 0 && can_scroll_x)) {
      return this;
    }
  }

  if (!_child) {
    return this;
  }

  // Adjust local coords by scroll offset for child hit testing
  // Controller uses 0=top convention, so offset is already in display coords
  int child_local_x = lx + _hscroller._scroll_offset;
  int child_local_y = ly + _vscroller._scroll_offset;

  // Check if within child bounds (accounting for scroll offset)
  if (child_local_x >= 0 && child_local_x < _child->width() &&
      child_local_y >= 0 && child_local_y < _child->height()) {
    // Create a modified event with adjusted coordinates for the child
    // This compensates for scroll offset so RootToLocal works correctly
    auto modified_ev = std::make_shared<Event>(*ev);
    modified_ev->miX = ev->miX + _hscroller._scroll_offset;
    modified_ev->miY = ev->miY + _vscroller._scroll_offset;

    Widget* routed = _child->routeUiEvent(modified_ev);

    if (routed) {
      return routed;
    }
  }

  return this;
}

///////////////////////////////////////////////////////////////////////////////
HandlerResult ScrollContainer::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;

  switch (ev->_eventcode) {
    case EventCode::MOUSEWHEEL: {
      float current_time = _uicontext->_uitimer.SecsSinceStart();
      // Vertical wheel
      if (ev->miMWY != 0) {
        if (_mode == ScrollMode::Y || _mode == ScrollMode::XY) {
          _vscroller.applyMouseWheel(ev->miMWY, current_time);
          result.setHandled(this);
        }
      }
      // Horizontal wheel (or shift+wheel on some systems)
      if (ev->miMWX != 0) {
        if (_mode == ScrollMode::X || _mode == ScrollMode::XY) {
          _hscroller.applyMouseWheel(ev->miMWX, current_time);
          result.setHandled(this);
        }
      }
      break;
    }
    default:
      break;
  }

  return result;
}

///////////////////////////////////////////////////////////////////////////////
void ScrollContainer::DoDraw(drawevent_constptr_t drwev) {
  _checkChildSizeChanged();

  auto tgt = drwev->GetTarget();
  auto fbi = tgt->FBI();
  auto mtxi = tgt->MTXI();
  auto dwi = tgt->DWI();

  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);

  // Draw background if enabled
  if (_draw_background) {
    auto defmtl = lev2::defaultUIMaterial();
    mtxi->PushUIMatrix();
    {
      defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
      defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
      tgt->PushModColor(_bg_color);
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
      tgt->PRI()->RenderQuadAtZ(
          defmtl.get(),
          ix1, ix1 + _geometry._w,
          iy1, iy1 + _geometry._h,
          0.0f,
          0.0f, 1.0f,
          0.0f, 1.0f);
      tgt->PopModColor();
    }
    mtxi->PopUIMatrix();
  }

  if (!_child || !_child->_enable || !_rtgroup) {
    return;
  }

  // Render content to RTGroup if dirty
  if (_content_dirty || _child->IsDirty()) {
    _renderContentToRTG(drwev);
  }

  // Get the texture from RTGroup
  auto ptex = _rtgroup->buffer(0)->texture();
  if (!ptex) return;

  int rtg_w = _rtgroup->width();
  int rtg_h = _rtgroup->height();

  if (rtg_w <= 0 || rtg_h <= 0) return;

  // Content is rendered at (_rtg_root_x, _rtg_root_y) in the RTG
  // For 1:1 texel-to-pixel mapping, we must display exactly as many texels as pixels
  // If content is smaller than visible area, display at actual size (don't stretch)
  int display_w = std::min(_geometry._w, _rtg_content_w);
  int display_h = std::min(_geometry._h, _rtg_content_h);

  // UV coordinates select the visible portion of content
  // Controller uses 0=top convention, offset is already in display coords
  float u0 = float(_rtg_root_x + _hscroller._scroll_offset) / float(rtg_w);
  float u1 = float(_rtg_root_x + _hscroller._scroll_offset + display_w) / float(rtg_w);

  float v0 = float(_rtg_root_y + _vscroller._scroll_offset) / float(rtg_h);
  float v1 = float(_rtg_root_y + _vscroller._scroll_offset + display_h) / float(rtg_h);

  // Swap V to flip vertically (RTG renders bottom-up, display top-down)
  float v0_flipped = v1;
  float v1_flipped = v0;

  // Draw the texture quad with alpha blending so transparent RTG areas show background
  static auto texmtl = std::make_shared<lev2::GfxMaterialUITextured>(tgt);
  texmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
  texmtl->SetTexture(lev2::ETEXDEST_DIFFUSE, ptex);

  tgt->PushModColor(fcolor4::White());
  mtxi->PushUIMatrix();

  texmtl->BeginBlock(tgt, lev2::RenderContextInstData::Default);

  // QuadRect: x, y, width, height
  // UvRect: u0, v0, du, dv
  // Use display_w/display_h for 1:1 texel-to-pixel mapping
  dwi->quad2D(
      fvec4(ix1, iy1, display_w, display_h),
      fvec4(u0, v0_flipped, u1 - u0, v1_flipped - v0_flipped),
      fvec4(0, 0, 1, 1),
      0.0f
  );

  texmtl->EndBlock(tgt);

  mtxi->PopUIMatrix();
  tgt->PopModColor();

  // Draw scroll indicators on top
  if (_draw_scroll_indicator) {
    bool need_vscroll = (_mode == ScrollMode::Y || _mode == ScrollMode::XY);
    bool need_hscroll = (_mode == ScrollMode::X || _mode == ScrollMode::XY);

    if (need_vscroll) {
      _vscroller.drawIndicator(drwev, _uicontext, ix1, iy1, _geometry._w, _geometry._h, false);
    }
    if (need_hscroll) {
      _hscroller.drawIndicator(drwev, _uicontext, ix1, iy1, _geometry._w, _geometry._h, true);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
