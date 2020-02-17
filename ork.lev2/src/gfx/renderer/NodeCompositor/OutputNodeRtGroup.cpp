////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>

#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/material_freestyle.inl>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>

ImplementReflectionX(ork::lev2::RtGroupOutputCompositingNode, "RtGroupOutputCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
void RtGroupOutputCompositingNode::describeX(class_t* c) {
  c->memberProperty("Layer", &RtGroupOutputCompositingNode::_layername);
}
///////////////////////////////////////////////////////////////////////////////
struct RTGIMPL {
  ///////////////////////////////////////
  RTGIMPL(RtGroupOutputCompositingNode* node)
      : _node(node)
      , _camname(AddPooledString("Camera"))
      , _layers(AddPooledString("All")) {
  }
  ///////////////////////////////////////
  ~RTGIMPL() {
  }
  ///////////////////////////////////////
  void gpuInit(lev2::Context* ctx) {
    if (_needsinit) {
      _blit2screenmtl.gpuInit(ctx, "orkshader://solid");
      _fxtechnique1x1 = _blit2screenmtl.technique("texcolor4");
      _fxpMVP         = _blit2screenmtl.param("MatMVP");
      _fxpColorMap    = _blit2screenmtl.param("ColorMap");
      _needsinit      = false;
    }
  }
  ///////////////////////////////////////
  void beginAssemble(CompositorDrawData& drawdata) {
    auto& ddprops                = drawdata._properties;
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto CIMPL                   = drawdata._cimpl;
    auto DB                      = RCFD.GetDB();
    Context* targ                = drawdata.context();
    int w                        = targ->mainSurfaceWidth();
    int h                        = targ->mainSurfaceHeight();
    if (targ->hiDPI()) {
      w /= 2;
      h /= 2;
    }
    _width  = w;
    _height = h;
    //////////////////////////////////////////////////////
    ViewportRect tgt_rect(0, 0, _width, _height);

    _CPD.AddLayer("All"_pool);
    _CPD.mbDrawSource = true;
    _CPD.mpFrameTek   = nullptr;
    _CPD.mpCameraName = nullptr;
    _CPD.mpLayerName  = nullptr; // default == "All"
    _CPD._clearColor  = fvec4(0.61, 0, 0, 1);

    _CPD._cameraMatrices = ddprops["defcammtx"_crcu].Get<const CameraMatrices*>();

    _CPD.SetDstRect(tgt_rect);
    drawdata._properties["OutputWidth"_crcu].Set<int>(_width);
    drawdata._properties["OutputHeight"_crcu].Set<int>(_height);
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
  RtGroupOutputCompositingNode* _node = nullptr;
  CompositingPassData _CPD;
  FreestyleMaterial _blit2screenmtl;
  const FxShaderTechnique* _fxtechnique1x1;
  const FxShaderParam* _fxpMVP;
  const FxShaderParam* _fxpColorMap;
  bool _needsinit = true;
  int _width      = 0;
  int _height     = 0;
};
///////////////////////////////////////////////////////////////////////////////
RtGroupOutputCompositingNode::RtGroupOutputCompositingNode() {
  _impl = std::make_shared<RTGIMPL>(this);
}
RtGroupOutputCompositingNode::~RtGroupOutputCompositingNode() {
}
void RtGroupOutputCompositingNode::gpuInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.Get<std::shared_ptr<RTGIMPL>>()->gpuInit(pTARG);
}
void RtGroupOutputCompositingNode::beginAssemble(CompositorDrawData& drawdata) {
  _impl.Get<std::shared_ptr<RTGIMPL>>()->beginAssemble(drawdata);
}
void RtGroupOutputCompositingNode::endAssemble(CompositorDrawData& drawdata) {
  _impl.Get<std::shared_ptr<RTGIMPL>>()->endAssemble(drawdata);
}
void RtGroupOutputCompositingNode::composite(CompositorDrawData& drawdata) {
  drawdata.context()->debugPushGroup("RtGroupOutputCompositingNode::composite");
  auto impl = _impl.Get<std::shared_ptr<RTGIMPL>>();
  /////////////////////////////////////////////////////////////////////////////
  // VR compositor
  /////////////////////////////////////////////////////////////////////////////
  FrameRenderer& framerenderer      = drawdata.mFrameRenderer;
  RenderContextFrameData& framedata = framerenderer.framedata();
  Context* context                  = framedata.GetTarget();
  auto fbi                          = context->FBI();
  if (auto try_final = drawdata._properties["final_out"_crcu].TryAs<RtBuffer*>()) {
    auto buffer = try_final.value();
    if (buffer) {
      assert(buffer != nullptr);
      auto tex = buffer->texture();
      if (tex) {
        /////////////////////////////////////////////////////////////////////////////
        // be nice and composite to main screen as well...
        /////////////////////////////////////////////////////////////////////////////
        drawdata.context()->debugPushGroup("RtGroupOutputCompositingNode::output");
        auto this_buf = context->FBI()->GetThisBuffer();
        auto& mtl     = impl->_blit2screenmtl;
        mtl.bindTechnique(impl->_fxtechnique1x1);
        mtl.begin(framedata);
        mtl._rasterstate.SetBlending(EBLENDING_OFF);
        mtl.bindParamCTex(impl->_fxpColorMap, tex);
        mtl.bindParamMatrix(impl->_fxpMVP, fmtx4::Identity);
        this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
        mtl.end(framedata);
        drawdata.context()->debugPopGroup();
      }
    }
  }
  drawdata.context()->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
