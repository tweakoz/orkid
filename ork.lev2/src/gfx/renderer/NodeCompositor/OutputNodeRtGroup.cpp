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
#include <ork/lev2/gfx/material_freestyle.h>
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
      _blit2screenmtl.gpuInit(ctx, "orkshader://compositor");
      _fxtechnique1x1 = _blit2screenmtl.technique("OutputNodeRtGroupDual");
      _fxpMVP         = _blit2screenmtl.param("MatMVP");
      _fxpColorMapA   = _blit2screenmtl.param("MapA");
      _fxpColorMapB   = _blit2screenmtl.param("MapB");
      _needsinit      = false;
      int w           = ctx->mainSurfaceWidth();
      int h           = ctx->mainSurfaceHeight();
      if (ctx->hiDPI()) {
        w /= 2;
        h /= 2;
      }
      _width  = w;
      _height = h;
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
    //////////////////////////////////////////////////////
    drawdata._properties["OutputWidth"_crcu].Set<int>(_width);
    drawdata._properties["OutputHeight"_crcu].Set<int>(_height);
    drawdata._properties["StereoEnable"_crcu].Set<bool>(false);
    _CPD.defaultSetup(drawdata);
    CIMPL->pushCPD(_CPD);
  }
  ///////////////////////////////////////
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
  const FxShaderParam* _fxpColorMapA;
  const FxShaderParam* _fxpColorMapB;
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
void RtGroupOutputCompositingNode::resize(int w, int h) {
  auto impl     = _impl.Get<std::shared_ptr<RTGIMPL>>();
  impl->_width  = w;
  impl->_height = h;
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
  if (auto try_outgroup = drawdata._properties["render_outgroup"_crcu].TryAs<RtGroup*>()) {
    auto rtgroup = try_outgroup.value();
    if (rtgroup) {
      auto bufA = rtgroup->GetMrt(0);
      auto bufB = rtgroup->GetMrt(1);
      assert(bufA != nullptr);
      assert(bufB != nullptr);
      auto texA = bufA->texture();
      auto texB = bufB->texture();
      if (texA and texB) {
        /////////////////////////////////////////////////////////////////////////////
        // be nice and composite to main screen as well...
        /////////////////////////////////////////////////////////////////////////////
        drawdata.context()->debugPushGroup("RtGroupOutputCompositingNode::output");
        auto this_buf = context->FBI()->GetThisBuffer();
        auto& mtl     = impl->_blit2screenmtl;
        mtl.begin(impl->_fxtechnique1x1, framedata);
        mtl._rasterstate.SetBlending(EBLENDING_OFF);
        mtl.bindParamCTex(impl->_fxpColorMapA, texA);
        mtl.bindParamCTex(impl->_fxpColorMapB, texB);
        mtl.bindParamMatrix(impl->_fxpMVP, fmtx4::Identity());
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
