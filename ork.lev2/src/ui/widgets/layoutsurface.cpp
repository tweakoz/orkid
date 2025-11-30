#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/ui/layoutsurface.h>
#include <ork/lev2/ui/context.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pri.h>

namespace ork::ui {

/////////////////////////////////////////////////////////////////////////
LayoutSurface::LayoutSurface(const std::string& name, int x, int y, int w, int h, int margin)
    : Surface(name, x, y, w, h, fcolor3(0.2f, 0.2f, 0.2f), 1.0f) {
  // LayoutSurface owns its own UIContext since it's a self-contained surface
  _ownedContext = std::make_shared<Context>();
  _uicontext = _ownedContext.get();

  // Create internal LayoutGroup
  _layoutGroup = std::make_shared<LayoutGroup>("LOSURF.LG", 0, 0, 8, 8, margin);

  // Set up context's top widget
  _ownedContext->_top = _layoutGroup;

  // Set parent so _uicontext propagates through normal widget hierarchy
  _layoutGroup->setParent(this);

  // Start with virtual size same as widget size
  _virtualWidth = w;
  _virtualHeight = h;

  // Initial scroll position at origin
  _scrollX = 0;
  _scrollY = 0;

  // Calculate initial UV coordinates
  _u0 = 0.0f;
  _v0 = 0.0f;
  _u1 = 1.0f;
  _v1 = 1.0f;


}

/////////////////////////////////////////////////////////////////////////
LayoutSurface::~LayoutSurface() {
}

/////////////////////////////////////////////////////////////////////////
void LayoutSurface::setVirtualSize(int w, int h) {
  if (w != _virtualWidth || h != _virtualHeight) {
    _virtualWidth = w;
    _virtualHeight = h;

    // Resize the rtgroup to match virtual size
    _updateRenderTarget();

    // Layout the internal group to fill virtual space
    _layoutGroup->SetRect(0, 0, _virtualWidth, _virtualHeight);

    // Force repaint
    mNeedsSurfaceRepaint = true;
  }
}

/////////////////////////////////////////////////////////////////////////
void LayoutSurface::setScrollPosition(int x, int y) {
  // Clamp scroll position to valid range
  int vw = (_virtualWidth==0) ? _geometry._w : _virtualWidth;
  int vh = (_virtualHeight==0) ? _geometry._h : _virtualHeight;

  int maxScrollX = std::max(0, vw - _geometry._w);
  int maxScrollY = std::max(0, vh - _geometry._h);

  _scrollX = std::clamp(x, 0, maxScrollX);
  _scrollY = std::clamp(y, 0, maxScrollY);

  // Update UV coordinates for the viewport
  // UV coordinates represent which part of the texture to show
  if (vw > 0 && vh > 0) {
    _u0 = float(_scrollX) / float(vw);
    _v0 = float(_scrollY) / float(vh);
    _u1 = float(_scrollX + _geometry._w) / float(vw);
    _v1 = float(_scrollY + _geometry._h) / float(vh);

    // Clamp to [0,1]
    _u1 = std::min(_u1, 1.0f);
    _v1 = std::min(_v1, 1.0f);
  }
}

/////////////////////////////////////////////////////////////////////////
void LayoutSurface::_updateRenderTarget() {
  int vw = (_virtualWidth==0) ? _geometry._w : _virtualWidth;
  int vh = (_virtualHeight==0) ? _geometry._h : _virtualHeight;
  if (_rtgroup and (vw > 0) and (vh > 0)) {
    // Only resize if dimensions changed
    if (_rtgroup->width() != vw || _rtgroup->height() != vh) {
      printf("LayoutSurface<%s> resizing rtgroup to %d x %d\n", _name.c_str(), vw, vh);
      _rtgroup->Resize(vw, vh);
      mNeedsSurfaceRepaint = true;
      _layoutGroup->SetRect(0, 0, vw, vh);
    }
  }
}

/////////////////////////////////////////////////////////////////////////
void LayoutSurface::_doOnResized() {
  // When widget resizes, update scroll position (might need clamping)
  setScrollPosition(_scrollX, _scrollY);

  // Let Surface handle its resize
  Surface::_doOnResized();
}

/////////////////////////////////////////////////////////////////////////
void LayoutSurface::DoRePaintSurface(ui::drawevent_constptr_t drwev) {
  auto tgt    = drwev->GetTarget();
  auto fbi    = tgt->FBI();
  auto mtxi   = tgt->MTXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int vw = (_virtualWidth==0) ? _geometry._w : _virtualWidth;
  int vh = (_virtualHeight==0) ? _geometry._h : _virtualHeight;

  //
  fbi->pushViewport(0, 0, vw, vh);
  fbi->pushScissor(0, 0, vw, vh);
  auto uimtx = mtxi->uiMatrix(vw, vh);
  mtxi->PushMMatrix(fmtx4::Identity());
  mtxi->PushVMatrix(fmtx4::Identity());
  mtxi->PushPMatrix(uimtx);
  {
    int ix1, iy1, ix2, iy2;
    ix1 = 0;
    iy1 = 0;
    ix2 = vw;
    iy2 = vh;

    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
    defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
    defmtl->SetUIColorMode(lev2::UiColorMode::VTX);
    primi->RenderQuadAtZ(
        defmtl.get(),
        ix1,  // x0
        ix2,  // x1
        iy1,  // y0
        iy2,  // y1
        0.0f, // z
        0.0f,
        1.0f, // u0, u1
        0.0f,
        1.0f // v0, v1
    );

    // Draw the entire LayoutGroup hierarchy within the same matrix context
    _layoutGroup->draw(drwev);
  }
  mtxi->PopPMatrix();
  mtxi->PopVMatrix();
  mtxi->PopMMatrix();
  fbi->popScissor();
  fbi->popViewport();
}

/////////////////////////////////////////////////////////////////////////
void LayoutSurface::DoDraw(drawevent_constptr_t drwev) {
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto fbi = tgt->FBI();
  auto dwi = tgt->DWI();

  mNeedsSurfaceRepaint = true; // TEMP
  // First, ensure rtgroup is the right size
  int vw = (_virtualWidth==0) ? _geometry._w : _virtualWidth;
  int vh = (_virtualHeight==0) ? _geometry._h : _virtualHeight;
  if (_rtgroup) {
    _updateRenderTarget();
  } else {


    _rtgroup = std::make_shared<lev2::RtGroup>(tgt, vw, vh, lev2::MsaaSamples::MSAA_1X);
    _rtgroup->_name = FormatString("ui::LayoutSurface<%p>", (void*)this);
    auto mrt0 = _rtgroup->createRenderTarget(lev2::EBufferFormat::RGBA8);  
  }

  // Handle repainting if needed (renders LayoutGroup to texture)
  if (mNeedsSurfaceRepaint || IsDirty()) {
    _rtgroup->_autoclear = true;
    _rtgroup->buffer(0)->_clearColor = _clearColor;
    _rtgroup->buffer(0)->_clearDepth = mfClearDepth;
    fbi->PushRtGroup(_rtgroup.get());
    DoRePaintSurface(drwev);
    fbi->PopRtGroup();
    mNeedsSurfaceRepaint = false;
    _dirty = false;
  }

  // Now render the texture with our custom UV coordinates for scrolling
  if (_rtgroup) {
    static auto texmtl = std::make_shared<lev2::GfxMaterialUITextured>(tgt);
    auto ptex = _rtgroup->buffer(0)->texture();
    OrkAssert(ptex);
    texmtl->SetTexture(lev2::ETEXDEST_DIFFUSE, ptex);
    lev2::material_ptr_t material = texmtl;

    mtxi->PushUIMatrix();
    {
      int ix_root = 0;
      int iy_root = 0;
      LocalToRoot(0, 0, ix_root, iy_root);
      _v0 = _scrollY / float((vh > 0) ? vh : 1);
      _v1 = (_scrollY + _geometry._h) / float((vh > 0) ? vh : 1);
      //printf("LayoutSurface<%s> scroll
      // Calculate UV rect for scrolling
      // Note: V coordinates are flipped for Vulkan
      float u_start = _u0;
      float u_range = _u1 - _u0;
      float v_start = 1.0f - _v0;  // Flip for Vulkan
      float v_range = (_v0 - _v1);  // Negative because flipped

      material->BeginBlock(tgt);
      dwi->quad2D(
          fvec4(ix_root, iy_root, _geometry._w, _geometry._h),  // QuadRect: widget bounds
          fvec4(u_start, v_start, u_range, v_range),  // UvRect: scrolled viewport into texture
          fvec4(u_start, v_start, u_range, v_range),  // UvRect2
          0.0f  // depth
      );
      material->EndBlock(tgt);
    }
    mtxi->PopUIMatrix();
  }
}

///////////////////////////////////////////////////////////////////////////////
HandlerResult LayoutSurface::DoOnUiEvent(event_constptr_t ev) {
    switch (ev->_eventcode) {
      case EventCode::MOUSEWHEEL: {
        int offset = ev->miMWY;
        _scrollY += offset*3; // Scroll speed factor
        //printf("LayoutSurface<%s> scrollY<%d>\n", _name.c_str(), _scrollY);
        break;
      }
      default:
        break;
      }
  return HandlerResult();
}

/////////////////////////////////////////////////////////////////////////
Widget* LayoutSurface::doRouteUiEvent(event_constptr_t ev) {
  // Route events to the internal LayoutGroup
  if (_layoutGroup) {
    if(0)printf("LayoutSurface<%s>::doRouteUiEvent routing to _layoutGroup\n", _name.c_str());
    return _layoutGroup->routeUiEvent(ev);
  }
  return nullptr;
}

/////////////////////////////////////////////////////////////////////////
// 3D Embedding Support
/////////////////////////////////////////////////////////////////////////

void LayoutSurface::updateTextureIfNeeded(lev2::Context* ctx) {
  // Ensure rtgroup exists
  int vw = (_virtualWidth == 0) ? _geometry._w : _virtualWidth;
  int vh = (_virtualHeight == 0) ? _geometry._h : _virtualHeight;

  if (!_rtgroup) {
    _rtgroup = std::make_shared<lev2::RtGroup>(ctx, 8, 8, lev2::MsaaSamples::MSAA_1X);
    _rtgroup->_name = FormatString("ui::LayoutSurface<%p>", (void*)this);
    _rtgroup->createRenderTarget(lev2::EBufferFormat::RGBA8);
  }

  // Check if resize needed
  if (_rtgroup->width() != vw || _rtgroup->height() != vh) {
    _rtgroup->Resize(vw, vh);
    _layoutGroup->SetRect(0, 0, vw, vh);
    mNeedsSurfaceRepaint = true;
  }

  // Always repaint for now (debugging)
  {
    auto fbi = ctx->FBI();

    _rtgroup->_autoclear = true;
    _rtgroup->buffer(0)->_clearColor = _clearColor;
    _rtgroup->buffer(0)->_clearDepth = mfClearDepth;

    // Push isolated RCFD WITHOUT compositor so PushUIMatrix() uses viewport dimensions
    // instead of compositor's CPD (which has window dimensions)
    auto rcfd = std::make_shared<lev2::RenderContextFrameData>(ctx);
    ctx->pushRenderContextFrameData(rcfd);

    fbi->PushRtGroup(_rtgroup.get()); // pushes viewport/scissor
    {
      if(nullptr==_drwev){
        _drwev = std::make_shared<DrawEvent>(ctx);
        _acqdbuf = std::make_shared<lev2::AcquiredDrawQueueForRendering>();
        _drwev->_acqdbuf = _acqdbuf;
      }

      _acqdbuf->_RCFD = rcfd;
      DoRePaintSurface(_drwev);
    }
    fbi->PopRtGroup();

    ctx->popRenderContextFrameData();

    mNeedsSurfaceRepaint = false;
    _dirty = false;
  }
}

/////////////////////////////////////////////////////////////////////////

fvec2 LayoutSurface::localToPixel(const fvec2& local) const {
  // Input: local coords in [-0.5, 0.5] range
  // Output: pixel coords in [0, width] x [0, height]
  float u = local.x + 0.5f;  // → [0, 1]
  float v = local.y + 0.5f;  // → [0, 1]

  int w = (_virtualWidth > 0) ? _virtualWidth : _geometry._w;
  int h = (_virtualHeight > 0) ? _virtualHeight : _geometry._h;

  return fvec2(u * w, v * h);
}

/////////////////////////////////////////////////////////////////////////

fvec2 LayoutSurface::pixelToLocal(const fvec2& pixel) const {
  // Input: pixel coords in [0, width] x [0, height]
  // Output: local coords in [-0.5, 0.5] range

  int w = (_virtualWidth > 0) ? _virtualWidth : _geometry._w;
  int h = (_virtualHeight > 0) ? _virtualHeight : _geometry._h;

  float u = pixel.x / float(w);  // → [0, 1]
  float v = pixel.y / float(h);  // → [0, 1]

  return fvec2(u - 0.5f, v - 0.5f);
}

/////////////////////////////////////////////////////////////////////////

HandlerResult LayoutSurface::handleTransformedInput(
    const fvec2& surfacePixelCoords,
    EventCode eventCode,
    uint32_t buttonState,
    uint32_t modifierKeys) {

  // Create standard 2D UI event
  auto ev = std::make_shared<Event>();
  ev->_eventcode = eventCode;
  ev->miX = int(surfacePixelCoords.x);
  ev->miY = int(surfacePixelCoords.y);
  ev->mbLeftButton = (buttonState & 0x1) != 0;
  ev->mbMiddleButton = (buttonState & 0x2) != 0;
  ev->mbRightButton = (buttonState & 0x4) != 0;
  ev->mbSHIFT = (modifierKeys & 0x1) != 0;
  ev->mbCTRL = (modifierKeys & 0x2) != 0;
  ev->mbALT = (modifierKeys & 0x4) != 0;
  ev->mbSUPER = (modifierKeys & 0x8) != 0;

  // Route through normal UI event system
  return handleUiEvent(ev);
}

/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui