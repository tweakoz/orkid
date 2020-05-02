////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/rtgroup.h>

#if defined(ORK_CONFIG_QT)
#include <ork/lev2/qtui/qtui.h>
#endif
#include <ork/kernel/prop.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/rtti/downcast.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::OffscreenBuffer, "ork::lev2::OffscreenBuffer");

namespace ork { namespace lev2 {

/////////////////////////////////////////////////////////////////////////

void OffscreenBuffer::Describe() {
}

OffscreenBuffer::OffscreenBuffer(
    OffscreenBuffer* Parent,
    int iX,
    int iY,
    int iW,
    int iH,
    EBufferFormat efmt,
    const std::string& name)
    : mpContext(nullptr)
    , mParent(Parent)
    , miWidth(iW)
    , miHeight(iH)
    , mbDirty(true)
    , msName(name)
    , meFormat(efmt)
    , mParentRtGroup(nullptr)
    , mpTexture(nullptr)
    , mRootWidget(nullptr)
    , mpMaterial(nullptr)
    , mbSizeIsDirty(true) {
}

OffscreenBuffer::~OffscreenBuffer() {
  //	if( mpContext )
  //	{
  //		delete mpContext;
  //	}
}

/////////////////////////////////////////////////////////////////////////

void OffscreenBuffer::Resize(int ix, int iy, int iw, int ih) {

  // TODO: OffscreenBuffer is probably completely superseded
  //  by RtBuffer (RenderTargetBuffer)
  // OffscreenBuffer was originally intended for pbuffers..
  //  though maybe no if we need offscreen hardware backed devices
  //  for commandline tools

  context()->resizeMainSurface(iw, ih);

  if (GetRootWidget()) {
    GetRootWidget()->SetRect(ix, iy, iw, ih);
    // GetRootWidget()->OnResize();
  }
}

/////////////////////////////////////////////////////////////////////////

Window::Window(int iX, int iY, int iW, int iH, const std::string& name, void* pdata)
    : OffscreenBuffer(0, iX, iY, iW, iH, EBufferFormat::RGBA8, name)
    , mpCTXBASE(0) {
  gGfxEnv.SetMainWindow(this);
}

Window::~Window() {
  if (mpCTXBASE) {
    mpCTXBASE = 0;
  }
}

/////////////////////////////////////////////////////////////////////////

void OffscreenBuffer::BeginFrame(void) {
  if (0 == mpContext) {
    context();
  }
  mpContext->beginFrame();
}

/////////////////////////////////////////////////////////////////////////

void OffscreenBuffer::EndFrame(void) {
  if (mpContext) {
    mpContext->endFrame();
  }
}

/////////////////////////////////////////////////////////////////////////

Context* OffscreenBuffer::context(void) const {
  //	if( 0 == mpContext )
  //{
  // initContext();
  //}

  return mpContext;
}

/////////////////////////////////////////////////////////////////////////

void OffscreenBuffer::initContext() {
  mpContext = ork::rtti::safe_downcast<Context*>(GfxEnv::GetRef().contextClass()->CreateObject());
  mpContext->initializeOffscreenContext(this);
}

/////////////////////////////////////////////////////////////////////////

void Window::initContext() {
  mpContext = ork::rtti::safe_downcast<Context*>(GfxEnv::GetRef().contextClass()->CreateObject());
  if (mpCTXBASE) {
    mpCTXBASE->setContext(mpContext);
  }
  mpContext->initializeWindowContext(this, mpCTXBASE);
}

/////////////////////////////////////////////////////////////////////////

void OffscreenBuffer::Render2dQuadEML(const fvec4& QuadRect, const fvec4& UvRect, const fvec4& UvRect2) {
  auto ctx = context();
  ctx->GBI()->render2dQuadEML(QuadRect, UvRect, UvRect2);
}

/////////////////////////////////////////////////////////////////////////

void OffscreenBuffer::Render2dQuadsEML(size_t count, const fvec4* QuadRects, const fvec4* UvRects, const fvec4* UvRect2s) {

  auto ctx = context();

  DynamicVertexBuffer<SVtxV12C4T16>& vb = GfxEnv::GetSharedDynamicVB();
  U32 uc                                = 0xffffffff;
  ork::lev2::VtxWriter<SVtxV12C4T16> vw;
  vw.Lock(context(), &vb, 6 * count);

  for (size_t i = 0; i < count; i++) {

    const fvec4& QuadRect = QuadRects[i];
    const fvec4& UvRect   = UvRects[i];
    const fvec4& UvRect2  = UvRect2s[i];

    float fx0 = QuadRect.x;
    float fy0 = QuadRect.y;
    float fx1 = QuadRect.x + QuadRect.z;
    float fy1 = QuadRect.y + QuadRect.w;

    float fua0 = UvRect.x;
    float fva0 = UvRect.y;
    float fua1 = UvRect.x + UvRect.z;
    float fva1 = UvRect.y + UvRect.w;

    float fub0 = UvRect2.x;
    float fvb0 = UvRect2.y;
    float fub1 = UvRect2.x + UvRect2.z;
    float fvb1 = UvRect2.y + UvRect2.w;

    vw.AddVertex(SVtxV12C4T16(fx0, fy0, 0.0f, fua0, fva0, fub0, fvb0, uc));
    vw.AddVertex(SVtxV12C4T16(fx1, fy1, 0.0f, fua1, fva1, fub1, fvb1, uc));
    vw.AddVertex(SVtxV12C4T16(fx1, fy0, 0.0f, fua1, fva0, fub1, fvb0, uc));

    vw.AddVertex(SVtxV12C4T16(fx0, fy0, 0.0f, fua0, fva0, fub0, fvb0, uc));
    vw.AddVertex(SVtxV12C4T16(fx0, fy1, 0.0f, fua0, fva1, fub0, fvb1, uc));
    vw.AddVertex(SVtxV12C4T16(fx1, fy1, 0.0f, fua1, fva1, fub1, fvb1, uc));
  }
  vw.UnLock(context());

  ctx->GBI()->DrawPrimitiveEML(vw, EPrimitiveType::TRIANGLES, 6 * count);
}

/////////////////////////////////////////////////////////////////////////

void OffscreenBuffer::RenderMatOrthoQuad(
    const SRect& vprect,
    const SRect& QuadRect,
    GfxMaterial* pmat,
    float fu0,
    float fv0,
    float fu1,
    float fv1,
    float* uv2,
    const fcolor4& clr) {
  static SRasterState DefaultRasterState;
  auto ctx  = context();
  auto mtxi = ctx->MTXI();
  auto fbi  = ctx->FBI();

  ViewportRect vprectNew(vprect.miX, vprect.miY, vprect.miX2 - vprect.miX, vprect.miY2 - vprect.miY);

  // align source pixels to target pixels if sizes match
  float fx0  = float(QuadRect.miX);
  float fy0  = float(QuadRect.miY);
  float fx1  = float(QuadRect.miX2);
  float fy1  = float(QuadRect.miY2);
  float fvx0 = float(vprect.miX);
  float fvy0 = float(vprect.miY);
  float fvx1 = float(vprect.miX2);
  float fvy1 = float(vprect.miY2);

  float zeros[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  if (NULL == uv2)
    uv2 = zeros;

  mtxi->PushPMatrix(mtxi->Ortho(fvx0, fvx1, fvy0, fvy1, 0.0f, 1.0f));
  mtxi->PushVMatrix(fmtx4::Identity());
  mtxi->PushMMatrix(fmtx4::Identity());
  ctx->RSI()->BindRasterState(DefaultRasterState, true);
  fbi->pushViewport(vprectNew);
  fbi->pushScissor(vprectNew);
  { // Draw Full Screen Quad with specified material
    ctx->FXI()->InvalidateStateBlock();
    ctx->PushModColor(clr);
    {
      DynamicVertexBuffer<SVtxV12C4T16>& vb = GfxEnv::GetSharedDynamicVB();

      // U32 uc = clr.GetBGRAU32();
      U32 uc = clr.GetVtxColorAsU32();
      ork::lev2::VtxWriter<SVtxV12C4T16> vw;
      vw.Lock(context(), &vb, 6);
      vw.AddVertex(SVtxV12C4T16(fx0, fy0, 0.0f, fu0, fv0, uv2[0], uv2[1], uc));
      vw.AddVertex(SVtxV12C4T16(fx1, fy1, 0.0f, fu1, fv1, uv2[4], uv2[5], uc));
      vw.AddVertex(SVtxV12C4T16(fx1, fy0, 0.0f, fu1, fv0, uv2[2], uv2[3], uc));

      vw.AddVertex(SVtxV12C4T16(fx0, fy0, 0.0f, fu0, fv0, uv2[0], uv2[1], uc));
      vw.AddVertex(SVtxV12C4T16(fx0, fy1, 0.0f, fu0, fv1, uv2[6], uv2[7], uc));
      vw.AddVertex(SVtxV12C4T16(fx1, fy1, 0.0f, fu1, fv1, uv2[4], uv2[5], uc));
      vw.UnLock(context());

      int inumpasses = pmat->BeginBlock(ctx);
      for (int ipass = 0; ipass < inumpasses; ipass++) {
        bool bDRAW = pmat->BeginPass(ctx, ipass);
        ctx->GBI()->DrawPrimitiveEML(vw, EPrimitiveType::TRIANGLES);
        pmat->EndPass(ctx);
      }
      pmat->EndBlock(ctx);
    }
    ctx->PopModColor();
  }
  fbi->popScissor();
  fbi->popViewport();
  mtxi->PopPMatrix();
  mtxi->PopVMatrix();
  mtxi->PopMMatrix();
}

OrthoQuad::OrthoQuad()
    : mfrot(0.0f)
    , mfu0a(0.0f)
    , mfu1a(0.0f)
    , mfu0b(0.0f)
    , mfu1b(0.0f)
    , mfv0a(0.0f)
    , mfv1a(0.0f)
    , mfv0b(0.0f)
    , mfv1b(0.0f) {
}

void OffscreenBuffer::RenderMatOrthoQuads(const OrthoQuads& oquads) {
  int inumquads           = oquads.miNumQuads;
  const SRect& vprect     = oquads.mViewportRect;
  const SRect& OrthoRect  = oquads.mOrthoRect;
  GfxMaterial* pmtl       = oquads.mpMaterial;
  const OrthoQuad* pquads = oquads.mpQUADS;

  if (0 == inumquads)
    return;

  static SRasterState DefaultRasterState;

  ViewportRect vprectNew(vprect.miX, vprect.miY, vprect.miX2 - vprect.miX, vprect.miY2 - vprect.miY);

  // align source pixels to target pixels if sizes match
  float fvx0 = float(OrthoRect.miX);
  float fvy0 = float(OrthoRect.miY);
  float fvx1 = float(OrthoRect.miX2);
  float fvy1 = float(OrthoRect.miY2);

  context()->MTXI()->PushPMatrix(context()->MTXI()->Ortho(fvx0, fvx1, fvy0, fvy1, 0.0f, 1.0f));
  context()->MTXI()->PushVMatrix(fmtx4::Identity());
  context()->MTXI()->PushMMatrix(fmtx4::Identity());
  context()->RSI()->BindRasterState(DefaultRasterState, true);
  context()->FBI()->pushViewport(vprectNew);
  context()->FBI()->pushScissor(vprectNew);
  { // Draw Full Screen Quad with specified material
    context()->FXI()->InvalidateStateBlock();
    context()->PushModColor(ork::fcolor4::White());
    {
      ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16>& vb = GfxEnv::GetSharedDynamicVB();

      ork::lev2::VtxWriter<ork::lev2::SVtxV12C4T16> vw;
      vw.Lock(context(), &vb, 6 * inumquads);
      {
        for (int iq = 0; iq < inumquads; iq++) {
          const OrthoQuad& Q    = pquads[iq];
          const SRect& QuadRect = Q.mQrect;
          const fcolor4& C      = Q.mColor;
          U32 uc                = C.GetBGRAU32();

          bool brot = Q.mfrot != 0.0f;

          if (brot) {
            float fcx = float(QuadRect.miX + QuadRect.miX2) * 0.5f;
            float fcy = float(QuadRect.miY + QuadRect.miY2) * 0.5f;
            float fw  = float(QuadRect.miX2) - fcx;
            float fh  = float(QuadRect.miY2) - fcy;

            float fsr  = sinf(Q.mfrot);
            float fcr  = cosf(Q.mfrot);
            float fsr2 = sinf(Q.mfrot + PI * 0.5f);
            float fcr2 = cosf(Q.mfrot + PI * 0.5f);

            float fx0 = fcx + fcr * fw;
            float fy0 = fcy + fsr * fh;
            float fx1 = fcx + fcr2 * fw;
            float fy1 = fcy + fsr2 * fh;
            float fx2 = fcx - fcr * fw;
            float fy2 = fcy - fsr * fh;
            float fx3 = fcx - fcr2 * fw;
            float fy3 = fcy - fsr2 * fh;

            vw.AddVertex(ork::lev2::SVtxV12C4T16(fx0, fy0, 0.0f, Q.mfu0a, Q.mfv0a, Q.mfu0b, Q.mfv0b, uc));
            vw.AddVertex(ork::lev2::SVtxV12C4T16(fx1, fy1, 0.0f, Q.mfu1a, Q.mfv0a, Q.mfu1b, Q.mfv0b, uc));
            vw.AddVertex(ork::lev2::SVtxV12C4T16(fx2, fy2, 0.0f, Q.mfu1a, Q.mfv1a, Q.mfu1b, Q.mfv1b, uc));

            vw.AddVertex(ork::lev2::SVtxV12C4T16(fx0, fy0, 0.0f, Q.mfu0a, Q.mfv0a, Q.mfu0b, Q.mfv0b, uc));
            vw.AddVertex(ork::lev2::SVtxV12C4T16(fx2, fy2, 0.0f, Q.mfu1a, Q.mfv1a, Q.mfu1b, Q.mfv1b, uc));
            vw.AddVertex(ork::lev2::SVtxV12C4T16(fx3, fy3, 0.0f, Q.mfu0a, Q.mfv1a, Q.mfu0b, Q.mfv1b, uc));

          } else {
            float fx0 = float(QuadRect.miX);
            float fy0 = float(QuadRect.miY);
            float fx1 = float(QuadRect.miX2);
            float fy1 = float(QuadRect.miY2);

            vw.AddVertex(ork::lev2::SVtxV12C4T16(fx0, fy0, 0.0f, Q.mfu0a, Q.mfv0a, Q.mfu0b, Q.mfv0b, uc));
            vw.AddVertex(ork::lev2::SVtxV12C4T16(fx1, fy1, 0.0f, Q.mfu1a, Q.mfv1a, Q.mfu1b, Q.mfv1b, uc));
            vw.AddVertex(ork::lev2::SVtxV12C4T16(fx1, fy0, 0.0f, Q.mfu1a, Q.mfv0a, Q.mfu1b, Q.mfv0b, uc));

            vw.AddVertex(ork::lev2::SVtxV12C4T16(fx0, fy0, 0.0f, Q.mfu0a, Q.mfv0a, Q.mfu0b, Q.mfv0b, uc));
            vw.AddVertex(ork::lev2::SVtxV12C4T16(fx0, fy1, 0.0f, Q.mfu0a, Q.mfv1a, Q.mfu0b, Q.mfv1b, uc));
            vw.AddVertex(ork::lev2::SVtxV12C4T16(fx1, fy1, 0.0f, Q.mfu1a, Q.mfv1a, Q.mfu1b, Q.mfv1b, uc));
          }
        }
      }
      vw.UnLock(context());

      int inumpasses = pmtl->BeginBlock(context());
      for (int ipass = 0; ipass < inumpasses; ipass++) {
        bool bDRAW = pmtl->BeginPass(context(), ipass);
        context()->GBI()->DrawPrimitiveEML(vw, EPrimitiveType::TRIANGLES);
        pmtl->EndPass(context());
      }
      pmtl->EndBlock(context());
    }
    context()->PopModColor();
  }
  context()->FBI()->popScissor();
  context()->FBI()->popViewport();
  context()->MTXI()->PopPMatrix();
  context()->MTXI()->PopVMatrix();
  context()->MTXI()->PopMMatrix();
}

/////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
