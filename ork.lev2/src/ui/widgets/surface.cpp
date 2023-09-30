#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/ui/surface.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/renderer/frametek.h>

namespace ork { namespace ui {

/////////////////////////////////////////////////////////////////////////

Surface::Surface(const std::string& name, int x, int y, int w, int h, fcolor3 color, F32 depth)
    : Group(name, x, y, w, h)
    , mbClear(true)
    , _clearColor(color)
    , mfClearDepth(depth)
    , mNeedsSurfaceRepaint(true)
    , _pickbuffer(nullptr) {
}

int Surface::_safeWidth() const{
  int w = width();
  if(w<8){
    w = 8;
  }
  return w;
}
int Surface::_safeHeight() const{
  int h = height();
  if(h<8){
    h = 8;
  }
  return h;
}

///////////////////////////////////////////////////////////////////////////////

Surface::Surface(const std::string& name, fcolor3 color, F32 depth)
    : Surface(name, 0, 0, 0, 0, color, depth) {
}

///////////////////////////////////////////////////////////////////////////////

void Surface::GetPixel(int ix, int iy, lev2::PixelFetchContext& pfc) {
  int iW   = _safeWidth();
  int iH   = _safeHeight();
  float fx = float(ix) / float(iW);
  float fy = float(iy) / float(iH);
  /////////////////////////////////////////////////////////////
  if (_pickbuffer) {
    auto tgt     = _pickbuffer->context();
    auto fbi     = tgt->FBI();
    pfc._rtgroup = _pickbuffer->_rtgroup;
    /////////////////////////////////////////////////////////////
    fbi->pushViewport(0, 0, iW, iH); // ??
    fbi->pushScissor(0, 0, iW, iH);  // ??
    /////////////////////////////////////////////////////////////
    // repaint this surface into the pickbuffer's rtgroup
    /////////////////////////////////////////////////////////////
    _pickbuffer->Draw(pfc);
    /////////////////////////////////////////////////////////////
    fbi->GetPixel(fvec4(fx, fy, 0.0f), pfc);
    /////////////////////////////////////////////////////////////
    fbi->popViewport();
    fbi->popScissor();
  }
  /////////////////////////////////////////////////////////////
}

/////////////////////////////////////////////////////////////////////////

void Surface::_doOnResized(void) {

  int w = _safeWidth();
  int h = _safeHeight();

  printf( "Surface<%s>::OnResize x<%d> y<%d> w<%d> h<%d>\n", _name.c_str(), x(), y(), w, h );
  DoSurfaceResize();
  SetDirty();
}

void Surface::_doUpdateSurfaces(ui::drawevent_constptr_t drwev){
  auto tgt    = drwev->GetTarget();
  auto fbi    = tgt->FBI();
  ///////////////////////////////////////
  // predraw init
  ///////////////////////////////////////
  if (_needsinit) {
    gpuInit(tgt);
    _needsinit = false;
  }
  if (mSizeDirty) {
    _doOnResized();
    mPosDirty     = false;
    mSizeDirty    = false;
    _prevGeometry = _geometry;
  }
  ///////////////////////////////////////
  // final size computation
  ///////////////////////////////////////
  if (_decouple_from_ui_size) {
    int irtgw  = _rtgroup->width();
    int irtgh  = _rtgroup->height();
    int isurfw = _decoupled_width;
    int isurfh = _decoupled_height;

    if (irtgw != isurfw or irtgh != isurfh) {
      _rtgroup->Resize(isurfw, isurfh);
      mNeedsSurfaceRepaint = true;
    }
  } else {
    int irtgw  = _rtgroup->width();
    int irtgh  = _rtgroup->height();
    int isurfw = _safeWidth();
    int isurfh = _safeHeight();

    if (irtgw != isurfw or irtgh != isurfh) {
      _rtgroup->Resize(isurfw, isurfh);
      mNeedsSurfaceRepaint = true;
    }
  }
  ///////////////////////////////////////
  // actual repaint
  ///////////////////////////////////////
  fbi->PushRtGroup(_rtgroup.get());
  tgt->debugMarker("repaint-surface");
  DoRePaintSurface(drwev);
  tgt->debugMarker("post-repaint-surface");
  fbi->PopRtGroup(true);
  mNeedsSurfaceRepaint = false;
  _dirty               = false;
  ///////////////////////////////////////
}

void Surface::_doGpuInit(lev2::Context* context) {
  _rtgroup        = std::make_shared<lev2::RtGroup>(context, 8, 8, lev2::MsaaSamples::MSAA_1X,true);
  _rtgroup->_name = FormatString("ui::Surface<%p>", (void*)this);
  auto mrt0       = _rtgroup->createRenderTarget(lev2::EBufferFormat::RGBA8,"color"_crcu);
  _rtgroup->_clearColor = _clearColor;
}

///////////////////////////////////////////////////////////////////////////////

void Surface::decoupleFromUiSize(int w, int h) {
  _decoupled_width       = w;
  _decoupled_height      = h;
  _decouple_from_ui_size = true;
}

void Surface::DoDraw(ui::drawevent_constptr_t drwev) {
  auto tgt    = drwev->GetTarget();
  auto mtxi   = tgt->MTXI();
  auto fbi    = tgt->FBI();
  auto fxi    = tgt->FXI();
  //auto rsi    = tgt->RSI();
  auto& primi = lev2::GfxPrimitives::GetRef();
  
  if (_postRenderCallback) {
    _postRenderCallback();
  }

  ///////////////////////////////////
  // pickbuffer debug ?
  ///////////////////////////////////
  if (false) {
    if (_pickbuffer) {
      ork::lev2::PixelFetchContext pfc;
      pfc.miMrtMask = (1 << 0); // | (1 << 1); // ObjectID and ObjectUVD
      pfc.mUsage[0] = lev2::PixelFetchContext::EPU_PTR64;
      pfc.mUsage[1] = lev2::PixelFetchContext::EPU_FLOAT;
      GetPixel(100, 100, pfc);
    }
  }
  ///////////////////////////////////////

  lev2::material_ptr_t ui_material = lev2::defaultUIMaterial();
  lev2::material_ptr_t material = ui_material;

  if (_rtgroup) {
    static auto texmtl = std::make_shared<lev2::GfxMaterialUITextured>(tgt);
    auto ptex          = _rtgroup->GetMrt(0)->texture();
    OrkAssert(ptex);
    texmtl->SetTexture(lev2::ETEXDEST_DIFFUSE, ptex);
    material = texmtl;
  }

  if(1){
    bool has_foc = hasMouseFocus();
    tgt->PushModColor(has_foc ? fcolor4::Green() : fcolor4::Blue());
    mtxi->PushUIMatrix();
    {
      int ix_root = 0;
      int iy_root = 0;
      LocalToRoot(0, 0, ix_root, iy_root);

       //printf( "Surface<%s>::Draw wx<%d> wy<%d> w<%d> h<%d>\n", _name.c_str(), ix_root, iy_root, _geometry._w, _geometry._h );

      if (_decouple_from_ui_size and _aspect_from_rtgroup) {

        float u0 = 0.0f;
        float u1 = 1.0f;
        float v0 = 1.0f;
        float v1 = 0.0f;

      tgt->PushModColor(fcolor4::Black());

        primi.RenderQuadAtZ(
            ui_material.get(),
            tgt,
            ix_root,
            ix_root + _geometry._w, // x0, x1
            iy_root,
            iy_root + _geometry._h, // y0, y1
            0.0f,                   // z
            0.0f,
            1.0f, // u0, u1
            1.0f,
            0.0f // v0, v1
        );

      tgt->PopModColor();

        float inp_aspect = float(_decoupled_width) / float(_decoupled_height);
        float out_aspect = float(_geometry._w) / float(_geometry._h);
        float aspectt    = inp_aspect / out_aspect;

        //printf("inp_aspect<%g> out_aspect<%g> aspectt<%g>\n", inp_aspect, out_aspect, aspectt);
    
        if (aspectt > 1.0) { // wider than UI (vertical letterbox)

          int hdiff = _geometry._h - int(float(_geometry._h)/aspectt);
          int oy0 = hdiff/2;
          int oy1 = -hdiff/2;

          primi.RenderQuadAtZ(
              material.get(),
              tgt,
              ix_root,
              ix_root + _geometry._w, // x0, x1
              iy_root + oy0,
              iy_root + _geometry._h + oy1, // y0, y1
              0.0f,                   // z
              u0,
              u1,
              v0,
              v1);

        } else {
          int wdiff = _geometry._w - int(float(_geometry._w)*aspectt);
          int ox0 = wdiff/2;
          int ox1 = -wdiff/2;

          primi.RenderQuadAtZ(
              material.get(),
              tgt,
              ix_root + ox0,
              ix_root + _geometry._w + ox1, // x0, x1
              iy_root,
              iy_root + _geometry._h, // y0, y1
              0.0f,                   // z
              u0,
              u1,
              v0,
              v1);
        }
      } else {
        primi.RenderQuadAtZ(
            material.get(),
            tgt,
            ix_root,
            ix_root + _geometry._w, // x0, x1
            iy_root,
            iy_root + _geometry._h, // y0, y1
            0.0f,                   // z
            0.0f,
            1.0f, // u0, u1
            1.0f,
            0.0f // v0, v1
        );
      }
    }
    mtxi->PopUIMatrix();
    tgt->PopModColor();
  }
}

/////////////////////////////////////////////////////////////////////////

void Surface::SurfaceRender(lev2::RenderContextFrameData& FrameData, const std::function<void()>& render_lambda) {
  OrkAssert(false);
#if 0
	lev2::Context* pTARG = FrameData.GetTarget();
	lev2::IRenderTarget* pIT = FrameData.GetRenderTarget();

	int vpx = x;
	int vpy = y;
	int vpw = width();
	int vph = height();

	auto fbi = pTARG->FBI();

	//fbi->setScissor( vpx, vpy, vpw, vph );
	//fbi->setViewport( vpx, vpy, vpw, vph );

SRect VPRect( 0, 0, pIT->width(), pIT->height() );
	pTARG->FBI()->pushViewport( VPRect );
	pTARG->FBI()->pushScissor( VPRect );
	{
		render_lambda();
	}
	pTARG->FBI()->popScissor();
	pTARG->FBI()->popViewport();
#endif
}

/////////////////////////////////////////////////////////////////////////

void Surface::RenderCached() {
}

/////////////////////////////////////////////////////////////////////////

void Surface::Clear() {
  // const fcolor3 &rCol = (surf!=nullptr) ? surf->GetClearColorRef() : fcolor3::Black();
  auto fbi = _target->FBI();

  //fbi->Clear(GetClearColorRef(), 1.0f);
}

/////////////////////////////////////////////////////////////////////////

void Surface::PushFrameTechnique(lev2::FrameTechniqueBase* ptek) {
  mpActiveFrameTek.push(ptek);
}

/////////////////////////////////////////////////////////////////////////

void Surface::PopFrameTechnique() {
  mpActiveFrameTek.pop();
}

/////////////////////////////////////////////////////////////////////////

lev2::FrameTechniqueBase* Surface::GetFrameTechnique() const {
  return mpActiveFrameTek.size() ? mpActiveFrameTek.top() : 0;
}

void Surface::BeginSurface(lev2::FrameRenderer& frenderer) {
  lev2::RenderContextFrameData& FrameData = frenderer.framedata();
  lev2::Context* pTARG                    = FrameData.GetTarget();
}
void Surface::EndSurface(lev2::FrameRenderer& frenderer) {
  lev2::RenderContextFrameData& FrameData = frenderer.framedata();
  lev2::Context* pTARG                    = FrameData.GetTarget();
}

/////////////////////////////////////////////////////////////////////////

}} // namespace ork::ui
