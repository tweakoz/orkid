////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "NodeCompositorScreen.h"

#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>

ImplementReflectionX(ork::lev2::ScreenOutputCompositingNode, "ScreenOutputCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
void ScreenOutputCompositingNode::describeX(class_t* c) {
  c->memberProperty("Layer", &ScreenOutputCompositingNode::_layername);
}
///////////////////////////////////////////////////////////////////////////////
struct SCRIMPL {
  ///////////////////////////////////////
  SCRIMPL(ScreenOutputCompositingNode* node)
      : _node(node)
      , _camname(AddPooledString("Camera"))
      , _layers(AddPooledString("All")) {
  }
  ///////////////////////////////////////
  ~SCRIMPL() {
  }
  ///////////////////////////////////////
  void gpuInit(lev2::GfxTarget* pTARG) {
    if (_needsinit) {
      _blit2screenmtl.SetUserFx("orkshader://solid", "texcolor");
      _blit2screenmtl.Init(pTARG);
      _needsinit = false;
    }
  }
  ///////////////////////////////////////
  void beginAssemble(CompositorDrawData& drawdata) {
    auto& ddprops                = drawdata._properties;
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto CIMPL                   = drawdata._cimpl;
    auto DB                      = RCFD.GetDB();
    GfxTarget* targ              = drawdata.target();
    int w                        = targ->mainSurfaceWidth();
    int h                        = targ->mainSurfaceHeight();
    if (targ->hiDPI()) {
      w /= 2;
      h /= 2;
    }
    //////////////////////////////////////////////////////
    SRect tgt_rect(0, 0, w, h);

    _CPD.AddLayer("All"_pool);
    _CPD.mbDrawSource = true;
    _CPD.mpFrameTek   = nullptr;
    _CPD.mpCameraName = nullptr;
    _CPD.mpLayerName  = nullptr; // default == "All"
    _CPD._clearColor  = fvec4(0.61, 0, 0, 1);

    _CPD._cameraMatrices = ddprops["defcammtx"_crcu].Get<const CameraMatrices*>();

    _CPD.SetDstRect(tgt_rect);
    drawdata._properties["OutputWidth"_crcu].Set<int>(w);
    drawdata._properties["OutputHeight"_crcu].Set<int>(h);
    CIMPL->pushCPD(_CPD);
  }
  void endAssemble(CompositorDrawData& drawdata) {
    auto CIMPL                   = drawdata._cimpl;
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    CIMPL->popCPD();
  }
  ///////////////////////////////////////
  PoolString _camname, _layers;
  ScreenOutputCompositingNode* _node = nullptr;
  CompositingPassData _CPD;
  ork::lev2::GfxMaterial3DSolid _blit2screenmtl;
  bool _needsinit = true;
};
///////////////////////////////////////////////////////////////////////////////
ScreenOutputCompositingNode::ScreenOutputCompositingNode() {
  _impl = std::make_shared<SCRIMPL>(this);
}
ScreenOutputCompositingNode::~ScreenOutputCompositingNode() {
}
void ScreenOutputCompositingNode::gpuInit(lev2::GfxTarget* pTARG, int iW, int iH) {
  _impl.Get<std::shared_ptr<SCRIMPL>>()->gpuInit(pTARG);
}
void ScreenOutputCompositingNode::beginAssemble(CompositorDrawData& drawdata) {
  _impl.Get<std::shared_ptr<SCRIMPL>>()->beginAssemble(drawdata);
}
void ScreenOutputCompositingNode::endAssemble(CompositorDrawData& drawdata) {
  _impl.Get<std::shared_ptr<SCRIMPL>>()->endAssemble(drawdata);
}
void ScreenOutputCompositingNode::composite(CompositorDrawData& drawdata) {
  drawdata.target()->debugPushGroup("VrCompositingNode::composite");
  auto impl = _impl.Get<std::shared_ptr<SCRIMPL>>();
  /////////////////////////////////////////////////////////////////////////////
  // VR compositor
  /////////////////////////////////////////////////////////////////////////////
  FrameRenderer& framerenderer      = drawdata.mFrameRenderer;
  RenderContextFrameData& framedata = framerenderer.framedata();
  GfxTarget* targ                   = framedata.GetTarget();
  if (auto try_final = drawdata._properties["final_out"_crcu].TryAs<RtBuffer*>()) {
    auto buffer = try_final.value();
    if (buffer) {
      assert(buffer != nullptr);
      auto tex = buffer->GetTexture();
      if (tex) {
        /////////////////////////////////////////////////////////////////////////////
        // be nice and composite to main screen as well...
        /////////////////////////////////////////////////////////////////////////////
        drawdata.target()->debugPushGroup("ScreenCompositingNode::to_screen");
        auto this_buf = targ->FBI()->GetThisBuffer();
        auto& mtl     = impl->_blit2screenmtl;
        int iw        = targ->mainSurfaceWidth();
        int ih        = targ->mainSurfaceHeight();
        SRect vprect(0, 0, iw, ih);
        SRect quadrect(0, ih, iw, 0);
        fvec4 color(1.0f, 1.0f, 1.0f, 1.0f);
        mtl.SetAuxMatrix(fmtx4::Identity);
        mtl.SetTexture(tex);
        mtl.SetTexture2(nullptr);
        mtl.SetColorMode(GfxMaterial3DSolid::EMODE_USER);
        mtl._rasterstate.SetBlending(EBLENDING_OFF);
        this_buf->RenderMatOrthoQuad(
            vprect,
            quadrect,
            &mtl,
            0.0f,
            0.0f, // u0 v0
            1.0f,
            1.0f, // u1 v1
            nullptr,
            color);
        drawdata.target()->debugPopGroup();
      }
    }
  }
  drawdata.target()->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
