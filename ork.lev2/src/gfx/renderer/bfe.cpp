////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/reflect/enum_serializer.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

BasicFrameTechnique::BasicFrameTechnique()
    : FrameTechniqueBase(0, 0)
    , _shouldBeginAndEndFrame(true) {
}

///////////////////////////////////////////////////////////////////////////////

void BasicFrameTechnique::Render(FrameRenderer& frenderer) {
  RenderContextFrameData& FrameData = frenderer.framedata();
  Context* pTARG                    = FrameData.GetTarget();
  auto tgt_rect                     = pTARG->mainSurfaceRectAtOrigin();
  // FrameData.SetDstRect( tgt_rect );
  /*
  IRenderTarget* pTopRenderTarget = FrameData.GetRenderTarget();
  if( _shouldBeginAndEndFrame )
      pTopRenderTarget->BeginFrame( frenderer );
  {
      frenderer.renderMisc();
  }
  if( _shouldBeginAndEndFrame )
      pTopRenderTarget->EndFrame( frenderer );
      */
}

///////////////////////////////////////////////////////////////////////////////

PickFrameTechnique::PickFrameTechnique()
    : FrameTechniqueBase(0, 0) {
}

///////////////////////////////////////////////////////////////////////////////

void PickFrameTechnique::Render(FrameRenderer& frenderer) {
  RenderContextFrameData& FrameData = frenderer.framedata();
  Context* pTARG                    = FrameData.GetTarget();
  auto tgt_rect                     = pTARG->mainSurfaceRectAtOrigin();
  // FrameData.SetDstRect( tgt_rect );
  {
    // frenderer.renderMisc();
  }
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
