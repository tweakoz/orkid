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

///////////////////////////////////////////////////////////////////////////////

void Surface::GetPixel(int ix, int iy, lev2::PixelFetchContext& ctx) {
  int iW   = width();
  int iH   = height();
  float fx = float(ix) / float(iW);
  float fy = float(iy) / float(iH);
  /////////////////////////////////////////////////////////////
  if (_pickbuffer) {
    auto tgt     = _pickbuffer->context();
    auto fbi     = tgt->FBI();
    ctx._rtgroup = _pickbuffer->_rtgroup;
    /////////////////////////////////////////////////////////////
    fbi->pushViewport(0, 0, iW, iH); // ??
    fbi->pushScissor(0, 0, iW, iH);  // ??
    /////////////////////////////////////////////////////////////
    _pickbuffer->Draw(ctx);
    /////////////////////////////////////////////////////////////
    fbi->GetPixel(fvec4(fx, fy, 0.0f), ctx);
    /////////////////////////////////////////////////////////////
    fbi->popViewport();
    fbi->popScissor();
  }
  /////////////////////////////////////////////////////////////
}

/////////////////////////////////////////////////////////////////////////

void Surface::_doOnResized(void) {
  //printf( "Surface<%s>::OnResize x<%d> y<%d> w<%d> h<%d>\n", _name.c_str(), x(), y(), width(), height() );
  DoSurfaceResize();
  SetDirty();
}

void Surface::RePaintSurface(ui::drawevent_constptr_t drwev) {
  DoRePaintSurface(drwev);
}

void Surface::_doGpuInit(lev2::Context* context) {
  _rtgroup  = std::make_shared<lev2::RtGroup>(context, 8,8, lev2::MsaaSamples::MSAA_1X);
  _rtgroup->_name = FormatString("ui::Surface<%p>", (void*) this);
  auto mrt0 = _rtgroup->createRenderTarget(lev2::EBufferFormat::RGBA8);
}

void Surface::DoDraw(ui::drawevent_constptr_t drwev) {
  auto tgt    = drwev->GetTarget();
  auto mtxi   = tgt->MTXI();
  auto fbi    = tgt->FBI();
  auto fxi    = tgt->FXI();
  auto rsi    = tgt->RSI();
  auto& primi = lev2::GfxPrimitives::GetRef();
  ///////////////////////////////////////
  int irtgw  = _rtgroup->width();
  int irtgh  = _rtgroup->height();
  int isurfw = width();
  int isurfh = height();
  if (irtgw != isurfw || irtgh != isurfh) {
    _rtgroup->Resize(isurfw, isurfh);
    mNeedsSurfaceRepaint = true;
  }
  if (mNeedsSurfaceRepaint || IsDirty()) {
    fbi->PushRtGroup(_rtgroup.get());
    RePaintSurface(drwev);
    fbi->PopRtGroup();
    mNeedsSurfaceRepaint = false;
    _dirty               = false;
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
  lev2::SRasterState defstate;
  rsi->BindRasterState(defstate);

  lev2::material_ptr_t material = lev2::defaultUIMaterial();;
  if (_rtgroup) {
    static auto texmtl = std::make_shared<lev2::GfxMaterialUITextured>(tgt);
    auto ptex          = _rtgroup->GetMrt(0)->texture();
    texmtl->SetTexture(lev2::ETEXDEST_DIFFUSE, ptex);
    material = texmtl;
  }

  bool has_foc = hasMouseFocus();
  tgt->PushModColor(has_foc ? fcolor4::Green() : fcolor4::Blue());
  mtxi->PushUIMatrix();
  {
    int ix_root = 0;
    int iy_root = 0;
    LocalToRoot(0, 0, ix_root, iy_root);

    // printf( "Surface<%s>::Draw wx<%d> wy<%d> w<%d> h<%d>\n", _name.c_str(), ix_root, iy_root, _geometry._w, _geometry._h );

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
  mtxi->PopUIMatrix();
  tgt->PopModColor();
}

/////////////////////////////////////////////////////////////////////////

void Surface::SurfaceRender(lev2::RenderContextFrameData& FrameData, const std::function<void()>& render_lambda) {
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

  fbi->Clear(GetClearColorRef(), 1.0f);
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
