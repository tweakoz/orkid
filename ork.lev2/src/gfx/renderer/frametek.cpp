////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>

#include <ork/application/application.h>
#include <ork/kernel/any.h>
#include <ork/kernel/orklut.hpp>
#include <ork/lev2/gfx/renderer/frametek.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/viewport.h>

template class ork::orklut<ork::PoolString, anyp>;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

FrameRenderer::FrameRenderer(RenderContextFrameData& RCFD, rendermisccb_t cb)
    : _framedata(RCFD)
    , _rendermisccb(cb) {
}
///////////////////////////////////////////
void FrameRenderer::renderMisc() {
  _rendermisccb();
}

///////////////////////////////////////////////////////////////////////////////

IRenderTarget::IRenderTarget() {
}

///////////////////////////////////////////////////////////////////////////////

RtGroupRenderTarget::RtGroupRenderTarget(RtGroup* prtgroup)
    : _rtgroup(prtgroup) {
}
int RtGroupRenderTarget::width() {
  // printf( "RtGroup W<%d> H<%d>\n", _rtgroup->width(), _rtgroup->height() );
  return _rtgroup->width();
}
int RtGroupRenderTarget::height() {
  return _rtgroup->height();
}
void RtGroupRenderTarget::BeginFrame(FrameRenderer& frenderer) {
}
void RtGroupRenderTarget::EndFrame(FrameRenderer& frenderer) {
}

///////////////////////////////////////////////////////////////////////////////

UiViewportRenderTarget::UiViewportRenderTarget(ui::Viewport* pVP)
    : mpViewport(pVP) {
}
int UiViewportRenderTarget::width() {
  return mpViewport->width();
}
int UiViewportRenderTarget::height() {
  return mpViewport->height();
}
void UiViewportRenderTarget::BeginFrame(FrameRenderer& frenderer) {
  RenderContextFrameData& FrameData = frenderer.framedata();
  Context* pTARG                    = FrameData.GetTarget();
  mpViewport->BeginFrame(pTARG);
}
void UiViewportRenderTarget::EndFrame(FrameRenderer& frenderer) {
  RenderContextFrameData& FrameData = frenderer.framedata();
  Context* pTARG                    = FrameData.GetTarget();
  mpViewport->EndFrame(pTARG);
}

///////////////////////////////////////////////////////////////////////////////

UiSurfaceRenderTarget::UiSurfaceRenderTarget(ui::Surface* pVP)
    : mSurface(pVP) {
}
int UiSurfaceRenderTarget::width() {
  return mSurface->width();
}
int UiSurfaceRenderTarget::height() {
  return mSurface->height();
}
void UiSurfaceRenderTarget::BeginFrame(FrameRenderer& frenderer) {
  mSurface->BeginSurface(frenderer);
}
void UiSurfaceRenderTarget::EndFrame(FrameRenderer& frenderer) {
  mSurface->EndSurface(frenderer);
}

///////////////////////////////////////////////////////////////////////////////

static const int kFINALW = 512;
static const int kFINALH = 512;

FrameTechniqueBase::FrameTechniqueBase(int iW, int iH)
    : miW(iW)
    , miH(iH)
    , mpMrtFinal(0) {
}

void FrameTechniqueBase::Init(Context* targ) {

  auto fbi                = targ->FBI();
  OffscreenBuffer* parent = fbi->GetThisBuffer();
  targ                    = parent ? parent->context() : targ;
  auto clear_color        = fbi->GetClearColor();

  mpMrtFinal = new RtGroup(targ, kFINALW, kFINALH, MsaaSamples::MSAA_1X);

  auto buf = mpMrtFinal->createRenderTarget(lev2::EBufferFormat::RGBA8);

  // mpMrtFinal->GetMrt(0)->RefClearColor() = clear_color;
  // mpMrtFinal->GetMrt(0)->SetContext( targ );

  DoInit(targ);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
