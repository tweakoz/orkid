#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/ui/layoutsurface.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pri.h>

namespace ork::ui {

/////////////////////////////////////////////////////////////////////////
LayoutSurface::LayoutSurface(const std::string& name, int x, int y, int w, int h, int margin)
    : Surface(name, x, y, w, h, fcolor3(0.2f, 0.2f, 0.2f), 1.0f) {
  // Create internal LayoutGroup
  _layoutGroup = std::make_shared<LayoutGroup>("content", 0, 0, w, h, margin);

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
  int maxScrollX = std::max(0, _virtualWidth - _geometry._w);
  int maxScrollY = std::max(0, _virtualHeight - _geometry._h);

  _scrollX = std::clamp(x, 0, maxScrollX);
  _scrollY = std::clamp(y, 0, maxScrollY);

  // Update UV coordinates for the viewport
  // UV coordinates represent which part of the texture to show
  if (_virtualWidth > 0 && _virtualHeight > 0) {
    _u0 = float(_scrollX) / float(_virtualWidth);
    _v0 = float(_scrollY) / float(_virtualHeight);
    _u1 = float(_scrollX + _geometry._w) / float(_virtualWidth);
    _v1 = float(_scrollY + _geometry._h) / float(_virtualHeight);

    // Clamp to [0,1]
    _u1 = std::min(_u1, 1.0f);
    _v1 = std::min(_v1, 1.0f);
  }
}

/////////////////////////////////////////////////////////////////////////
void LayoutSurface::_updateRenderTarget() {
  int vw = (_virtualWidth==0) ? _geometry._w : _virtualWidth;
  int vh = (_virtualHeight==0) ? _geometry._h : _virtualHeight;
    printf("LayoutSurface<%s> resizing rtgroup to %d x %d\n", _name.c_str(), vw, vh);
  if (_rtgroup and (vw > 0) and (vh > 0)) {
    // Only resize if dimensions changed
   // if (_rtgroup->width() != _virtualWidth || _rtgroup->height() != _virtualHeight) {
   _rtgroup->Resize(vw, vh);
    mNeedsSurfaceRepaint = true;
    //}
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

  //_layoutGroup->SetSize(_virtualWidth, _virtualHeight);
  int lgw = _layoutGroup->_geometry._w;
  int lgh = _layoutGroup->_geometry._h;
  //

  auto uimtx = mtxi->uiMatrix(vw, vh);
  mtxi->PushMMatrix(fmtx4::Identity());
  mtxi->PushVMatrix(fmtx4::Identity());
  mtxi->PushPMatrix(uimtx);
  {
    int ix1, iy1, ix2, iy2;
    //LocalToRoot(0, 0, ix1, iy1);
    ix1 = 0;
    iy1 = 0;
    ix2 = vw;
    iy2 = vh;

    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
    defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
    //tgt->PushModColor(color);
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
    //tgt->PopModColor();
  }
  mtxi->PopPMatrix();
  mtxi->PopVMatrix();
  mtxi->PopMMatrix();

  if(0)printf("LayoutSurface::DoRePaintSurface wx<%d> wy<%d> w<%d> h<%d> vw<%d> vh<%d> lgw<%d> lgh<%d>\n",
          _geometry._x, 
          _geometry._y, 
          _geometry._w, 
          _geometry._h, 
          vw, 
          vh,
          lgw, lgh);


          

  // The rtgroup is already set as the current render target by Surface::DoDraw
  // We just need to render our LayoutGroup hierarchy into it

  // Clear is handled by the rtgroup autoclear settings

  // Draw the entire LayoutGroup hierarchy
  // It renders to the full virtual size (the rtgroup size)
  //_layoutGroup->draw(drwev);
}

/////////////////////////////////////////////////////////////////////////
void LayoutSurface::DoDraw(drawevent_constptr_t drwev) {
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto fbi = tgt->FBI();
  auto dwi = tgt->DWI();

  mNeedsSurfaceRepaint = true; // TEMP
  // First, ensure rtgroup is the right size
  if (_rtgroup) {
    _updateRenderTarget();
  } else {

    int vw = (_virtualWidth==0) ? _geometry._w : _virtualWidth;
    int vh = (_virtualHeight==0) ? _geometry._h : _virtualHeight;

    _rtgroup = std::make_shared<lev2::RtGroup>(tgt, vw, vh, lev2::MsaaSamples::MSAA_1X);
    _rtgroup->_name = FormatString("ui::LayoutSurface<%p>", (void*)this);
    auto mrt0 = _rtgroup->createRenderTarget(lev2::EBufferFormat::RGBA8);  
  }

  // Handle repainting if needed (renders LayoutGroup to texture)
  if (mNeedsSurfaceRepaint || IsDirty()) {
    _rtgroup->_autoclear = true;
    _rtgroup->_clearColor = _clearColor;
    _rtgroup->_clearDepth = mfClearDepth;
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
          fvec4(0, 0, 1, 1),  // UvRect2
          0.0f  // depth
      );
      material->EndBlock(tgt);
    }
    mtxi->PopUIMatrix();
  }
}

/////////////////////////////////////////////////////////////////////////
Widget* LayoutSurface::doRouteUiEvent(event_constptr_t ev) {
  // Transform event coordinates from Surface space to LayoutGroup space
  // Account for scrolling
  if (_layoutGroup && IsEventInside(ev)) {
    // Create a modified event with adjusted coordinates
    auto localEv = std::make_shared<ui::Event>(*ev);
    localEv->miX = ev->miX - _geometry._x + _scrollX;
    localEv->miY = ev->miY - _geometry._y + _scrollY;

    // Route to the LayoutGroup
    return _layoutGroup->routeUiEvent(localEv);
  }

  return nullptr;
}

/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui