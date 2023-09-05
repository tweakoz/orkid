////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/rtgroup.h>

#include <ork/lev2/glfw/ctx_glfw.h>
#include <ork/kernel/prop.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/rtti/downcast.h>

ImplementReflectionX(ork::lev2::DisplayBuffer, "ork::lev2::DisplayBuffer");

namespace ork { namespace lev2 {

/////////////////////////////////////////////////////////////////////////

void DisplayBuffer::describeX(object::ObjectClass* clazz) {
}

DisplayBuffer::DisplayBuffer(
    DisplayBuffer* Parent,
    int iX,
    int iY,
    int iW,
    int iH,
    EBufferFormat efmt,
    const std::string& name)
    : _parent(Parent)
    , miWidth(iW)
    , miHeight(iH)
    , meFormat(efmt)
    , mbDirty(true)
    , mbSizeIsDirty(true)
    , _name(name) {
}

DisplayBuffer::~DisplayBuffer() {
}

/////////////////////////////////////////////////////////////////////////

void DisplayBuffer::Resize(int ix, int iy, int iw, int ih) {

  // TODO: DisplayBuffer is probably completely superseded
  //  by RtBuffer (RenderTargetBuffer)
  // DisplayBuffer was originally intended for pbuffers..
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
    : DisplayBuffer(0, iX, iY, iW, iH, EBufferFormat::RGBA8, name)
    , mpCTXBASE(0) {
  gGfxEnv.SetMainWindow(this);
}

Window::~Window() {
  if (mpCTXBASE) {
    mpCTXBASE = 0;
  }
}

/////////////////////////////////////////////////////////////////////////

void DisplayBuffer::BeginFrame(void) {
  _sharedcontext->beginFrame();
}

/////////////////////////////////////////////////////////////////////////

void DisplayBuffer::EndFrame(void) {
  _sharedcontext->endFrame();
}

/////////////////////////////////////////////////////////////////////////

Context* DisplayBuffer::context(void) const {
  return _sharedcontext.get();
}

/////////////////////////////////////////////////////////////////////////

void DisplayBuffer::initContext() {
  auto ctxclazz  = GfxEnv::GetRef().contextClass();
  _sharedcontext = std::dynamic_pointer_cast<Context>(ctxclazz->createShared());
  _sharedcontext->initializeOffscreenContext(this);
}

/////////////////////////////////////////////////////////////////////////

void Window::initContext() {
  auto ctxclazz  = GfxEnv::GetRef().contextClass();
  OrkAssert(ctxclazz);
  auto class_name = ctxclazz->Name();
  printf( "CLASSNAME<%s>\n", class_name.c_str() );
  auto factory = ctxclazz->annotationTyped<context_factory_t>("context_factory").value();
  OrkAssert(factory!=nullptr);
  _sharedcontext = factory();
  if (mpCTXBASE) {
    mpCTXBASE->setContext(_sharedcontext.get());
  }
  _sharedcontext->initializeWindowContext(this, mpCTXBASE);
}

/////////////////////////////////////////////////////////////////////////

void DisplayBuffer::Render2dQuadEML(const fvec4& QuadRect, const fvec4& UvRect, const fvec4& UvRect2) {
  auto ctx = context();
  ctx->GBI()->render2dQuadEML(QuadRect, UvRect, UvRect2);
}

/////////////////////////////////////////////////////////////////////////

void DisplayBuffer::Render2dQuadsEML(size_t count, const fvec4* QuadRects, const fvec4* UvRects, const fvec4* UvRect2s) {

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

  ctx->GBI()->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES, 6 * count);
}

/////////////////////////////////////////////////////////////////////////

void DisplayBuffer::RenderMatOrthoQuad(
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
  //ctx->RSI()->BindRasterState(DefaultRasterState, true);
  fbi->pushViewport(vprectNew);
  fbi->pushScissor(vprectNew);
  { // Draw Full Screen Quad with specified material
    ctx->PushModColor(clr);
    {
      DynamicVertexBuffer<SVtxV12C4T16>& vb = GfxEnv::GetSharedDynamicVB();

      // U32 uc = clr.GetBGRAU32();
      U32 uc = clr.vertexColorU32();
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
        ctx->GBI()->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
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

/////////////////////////////////////////////////////////////////////////

void DisplayBuffer::RenderMatOrthoQuad(
    const SRect& vprect,
    const SRect& QuadRect,
    GfxMaterial* pmat,
    fvec2 uv0,
    fvec2 uv1,
    fvec2 uv2,
    fvec2 uv3,
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

  mtxi->PushPMatrix(mtxi->Ortho(fvx0, fvx1, fvy0, fvy1, 0.0f, 1.0f));
  mtxi->PushVMatrix(fmtx4::Identity());
  mtxi->PushMMatrix(fmtx4::Identity());
  //ctx->RSI()->BindRasterState(DefaultRasterState, true);
  fbi->pushViewport(vprectNew);
  fbi->pushScissor(vprectNew);
  { // Draw Full Screen Quad with specified material
    ctx->PushModColor(clr);
    {
      DynamicVertexBuffer<SVtxV12C4T16>& vb = GfxEnv::GetSharedDynamicVB();

      // U32 uc = clr.GetBGRAU32();
      U32 uc = clr.vertexColorU32();
      ork::lev2::VtxWriter<SVtxV12C4T16> vw;
      vw.Lock(context(), &vb, 6);
      vw.AddVertex(SVtxV12C4T16(fx0, fy0, 0.0f, uv0.x, uv0.y, 0.0f, 0.0f, uc));
      vw.AddVertex(SVtxV12C4T16(fx1, fy0, 0.0f, uv1.x, uv1.y, 0.0f, 0.0f, uc));
      vw.AddVertex(SVtxV12C4T16(fx1, fy1, 0.0f, uv2.x, uv2.y, 0.0f, 0.0f, uc));

      vw.AddVertex(SVtxV12C4T16(fx0, fy0, 0.0f, uv0.x, uv0.y, 0.0f, 0.0f, uc));
      vw.AddVertex(SVtxV12C4T16(fx1, fy1, 0.0f, uv2.x, uv2.y, 0.0f, 0.0f, uc));
      vw.AddVertex(SVtxV12C4T16(fx0, fy1, 0.0f, uv3.x, uv3.y, 0.0f, 0.0f, uc));
      vw.UnLock(context());

      int inumpasses = pmat->BeginBlock(ctx);
      for (int ipass = 0; ipass < inumpasses; ipass++) {
        bool bDRAW = pmat->BeginPass(ctx, ipass);
        ctx->GBI()->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
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
    : mfu0a(0.0f)
    , mfv0a(0.0f)
    , mfu0b(0.0f)
    , mfv0b(0.0f)
    , mfu1a(0.0f)
    , mfv1a(0.0f)
    , mfu1b(0.0f)
    , mfv1b(0.0f)
    , mfrot(0.0f) {
}

/////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
