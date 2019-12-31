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

Surface::Surface(const std::string& name, int x, int y, int w, int h, CColor3 color, F32 depth)
    : Group(name, x, y, w, h)
    , mbClear(true)
    , mcClearColor(color)
    , mfClearDepth(depth)
    , mRtGroup(nullptr)
    , mNeedsSurfaceRepaint(true)
    , mpPickBuffer(nullptr) {
}

///////////////////////////////////////////////////////////////////////////////

void Surface::GetPixel(int ix, int iy, lev2::PixelFetchContext& ctx) {
  int iW   = GetW();
  int iH   = GetH();
  float fx = float(ix) / float(iW);
  float fy = float(iy) / float(iH);
  /////////////////////////////////////////////////////////////
  if (mpPickBuffer) {
    auto tgt      = mpPickBuffer->context();
    auto fbi      = tgt->FBI();
    ctx.mRtGroup  = mpPickBuffer->mpPickRtGroup;
    ctx.mAsBuffer = mpPickBuffer;
    /////////////////////////////////////////////////////////////
    fbi->SetViewport(0, 0, iW, iH);
    fbi->SetScissor(0, 0, iW, iH);
    /////////////////////////////////////////////////////////////
    mpPickBuffer->Draw(ctx);
    /////////////////////////////////////////////////////////////
    fbi->GetPixel(fvec4(fx, fy, 0.0f), ctx);
    /////////////////////////////////////////////////////////////
  }
  /////////////////////////////////////////////////////////////
}

/////////////////////////////////////////////////////////////////////////

void Surface::OnResize(void) {
  /*printf( "Surface<%s>::OnResize x<%d> y<%d> w<%d> h<%d>\n", msName.c_str(), miX, miY, miW, miH );

  if( mRtGroup )
  {
      mRtGroup->Resize(GetW(),GetH());
  }*/
  DoSurfaceResize();
  SetDirty();
}

void Surface::RePaintSurface(ui::DrawEvent& ev) {
  DoRePaintSurface(ev);
}

void Surface::DoDraw(DrawEvent& drwev) {
  auto tgt    = drwev.GetTarget();
  auto mtxi   = tgt->MTXI();
  auto fbi    = tgt->FBI();
  auto fxi    = tgt->FXI();
  auto rsi    = tgt->RSI();
  auto& primi = lev2::GfxPrimitives::GetRef();
  auto defmtl = lev2::GfxEnv::GetDefaultUIMaterial();

  if (nullptr == mRtGroup) {
    mRtGroup  = new lev2::RtGroup(tgt, miW, miH, 1);
    auto mrt0 = new lev2::RtBuffer(mRtGroup, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA8, 1280, 720);
    mRtGroup->SetMrt(0, mrt0);
  }
  if (mRtGroup) {
    int irtgw  = mRtGroup->GetW();
    int irtgh  = mRtGroup->GetH();
    int isurfw = GetW();
    int isurfh = GetH();
    if (irtgw != isurfw || irtgh != isurfh) {
      // printf( "resize surface rtgroup<%d %d>\n", isurfw, isurfh);
      mRtGroup->Resize(isurfw, isurfh);
      mNeedsSurfaceRepaint = true;
    }
    if (mNeedsSurfaceRepaint || IsDirty()) {
      fbi->PushRtGroup(mRtGroup);
      RePaintSurface(drwev);
      fbi->PopRtGroup();
      mNeedsSurfaceRepaint = false;
      mDirty               = false;
    }
  } else {
    RePaintSurface(drwev);
  }
  // lev2::GfxMaterialUI UiMat(tgt);
  lev2::SRasterState defstate;
  rsi->BindRasterState(defstate);
  fxi->InvalidateStateBlock();

  if (mRtGroup)
    tgt->PushMaterial(mRtGroup->GetMrt(0)->GetMaterial());
  else
    tgt->PushMaterial(defmtl);

  bool has_foc = HasMouseFocus();
  tgt->PushModColor(has_foc ? fcolor4::Green() : fcolor4::Blue());
  mtxi->PushUIMatrix();
  {
    int ix_root = 0;
    int iy_root = 0;
    LocalToRoot(0, 0, ix_root, iy_root);

    // printf( "Surface<%s>::Draw wx<%d> wy<%d> w<%d> h<%d>\n", msName.c_str(), ix_root, iy_root, miW, miH );

    primi.RenderQuadAtZ(
        tgt,
        ix_root,
        ix_root + miW, // x0, x1
        iy_root,
        iy_root + miH, // y0, y1
        0.0f,          // z
        0.0f,
        1.0f, // u0, u1
        1.0f,
        0.0f // v0, v1
    );
  }
  mtxi->PopUIMatrix();
  tgt->PopModColor();
  tgt->PopMaterial();
}

/////////////////////////////////////////////////////////////////////////

void Surface::SurfaceRender(lev2::RenderContextFrameData& FrameData, const std::function<void()>& render_lambda) {
#if 0
	lev2::Context* pTARG = FrameData.GetTarget();
	lev2::IRenderTarget* pIT = FrameData.GetRenderTarget();

	int vpx = GetX();
	int vpy = GetY();
	int vpw = GetW();
	int vph = GetH();

	auto fbi = pTARG->FBI();

	//fbi->SetScissor( vpx, vpy, vpw, vph );
	//fbi->SetViewport( vpx, vpy, vpw, vph );

SRect VPRect( 0, 0, pIT->GetW(), pIT->GetH() );
	pTARG->FBI()->PushViewport( VPRect );
	pTARG->FBI()->PushScissor( VPRect );
	{
		render_lambda();
	}
	pTARG->FBI()->PopScissor();
	pTARG->FBI()->PopViewport();
#endif
}

/////////////////////////////////////////////////////////////////////////

void Surface::RenderCached() {
}

/////////////////////////////////////////////////////////////////////////

void Surface::Clear() {
  // const CColor3 &rCol = (surf!=nullptr) ? surf->GetClearColorRef() : CColor3::Black();
  auto fbi = mpTarget->FBI();

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
  lev2::Context* pTARG                  = FrameData.GetTarget();
}
void Surface::EndSurface(lev2::FrameRenderer& frenderer) {
  lev2::RenderContextFrameData& FrameData = frenderer.framedata();
  lev2::Context* pTARG                  = FrameData.GetTarget();
}

/////////////////////////////////////////////////////////////////////////

}} // namespace ork::ui
