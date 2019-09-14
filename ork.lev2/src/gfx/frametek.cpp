////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
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
#include <ork/lev2/gfx/frametek.h>
#include <ork/lev2/gfx/rendercontext.h>
#include <ork/lev2/gfx/renderable.h>
#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/viewport.h>

template class ork::orklut<ork::PoolString, anyp>;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

IRenderTarget::IRenderTarget() {}

///////////////////////////////////////////////////////////////////////////////

RtGroupRenderTarget::RtGroupRenderTarget(RtGroup* prtgroup)
    : mpRtGroup(prtgroup) {}
int RtGroupRenderTarget::GetW() {
  // printf( "RtGroup W<%d> H<%d>\n", mpRtGroup->GetW(), mpRtGroup->GetH() );
  return mpRtGroup->GetW();
}
int RtGroupRenderTarget::GetH() { return mpRtGroup->GetH(); }
void RtGroupRenderTarget::BeginFrame(FrameRenderer& frenderer) {}
void RtGroupRenderTarget::EndFrame(FrameRenderer& frenderer) {}

///////////////////////////////////////////////////////////////////////////////

UiViewportRenderTarget::UiViewportRenderTarget(ui::Viewport* pVP)
    : mpViewport(pVP) {}
int UiViewportRenderTarget::GetW() { return mpViewport->GetW(); }
int UiViewportRenderTarget::GetH() { return mpViewport->GetH(); }
void UiViewportRenderTarget::BeginFrame(FrameRenderer& frenderer) {
  RenderContextFrameData& FrameData = frenderer.GetFrameData();
  GfxTarget* pTARG                  = FrameData.GetTarget();
  mpViewport->BeginFrame(pTARG);
}
void UiViewportRenderTarget::EndFrame(FrameRenderer& frenderer) {
  RenderContextFrameData& FrameData = frenderer.GetFrameData();
  GfxTarget* pTARG                  = FrameData.GetTarget();
  mpViewport->EndFrame(pTARG);
}

///////////////////////////////////////////////////////////////////////////////

UiSurfaceRenderTarget::UiSurfaceRenderTarget(ui::Surface* pVP)
    : mSurface(pVP) {}
int UiSurfaceRenderTarget::GetW() { return mSurface->GetW(); }
int UiSurfaceRenderTarget::GetH() { return mSurface->GetH(); }
void UiSurfaceRenderTarget::BeginFrame(FrameRenderer& frenderer) { mSurface->BeginSurface(frenderer); }
void UiSurfaceRenderTarget::EndFrame(FrameRenderer& frenderer) { mSurface->EndSurface(frenderer); }

///////////////////////////////////////////////////////////////////////////////

static const int kFINALW = 512;
static const int kFINALH = 512;

FrameTechniqueBase::FrameTechniqueBase(int iW, int iH)
    : miW(iW)
    , miH(iH)
    , mpMrtFinal(0) {}

void FrameTechniqueBase::Init(GfxTarget* targ) {
  static const int kmultisamples = 1;

  auto fbi          = targ->FBI();
  GfxBuffer* parent = fbi->GetThisBuffer();
  targ              = parent ? parent->GetContext() : targ;
  auto clear_color  = fbi->GetClearColor();

  mpMrtFinal = new RtGroup(targ, kFINALW, kFINALH, kmultisamples);

  mpMrtFinal->SetMrt(0, new RtBuffer(mpMrtFinal, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA32, kFINALW, kFINALH));

  // mpMrtFinal->GetMrt(0)->RefClearColor() = clear_color;
  // mpMrtFinal->GetMrt(0)->SetContext( targ );

  DoInit(targ);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2 {
