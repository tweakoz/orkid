////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>

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

ImplementReflectionX(ork::lev2::ScreenOutputCompositingNode, "ScreenOutputCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
void ScreenOutputCompositingNode::describeX(class_t* c) {
  c->memberProperty("Layer", &ScreenOutputCompositingNode::_layername);
  c->memberProperty("SuperSample", &ScreenOutputCompositingNode::_supersample)
      ->annotate<ConstString>("editor.range.min", "0")
      ->annotate<ConstString>("editor.range.max", "5");
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
  void gpuInit(lev2::Context* ctx) {
    if (_needsinit) {
      _blit2screenmtl.gpuInit(ctx, "orkshader://solid");
      _fxtechnique1x1 = _blit2screenmtl.technique("texcolor");
      _fxtechnique2x2 = _blit2screenmtl.technique("downsample_2x2");
      _fxtechnique3x3 = _blit2screenmtl.technique("downsample_3x3");
      _fxtechnique4x4 = _blit2screenmtl.technique("downsample_4x4");
      _fxtechnique5x5 = _blit2screenmtl.technique("downsample_5x5");
      _fxtechnique6x6 = _blit2screenmtl.technique("downsample_6x6");
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
    _width  = w * (_node->supersample() + 1);
    _height = h * (_node->supersample() + 1);
    //////////////////////////////////////////////////////
    drawdata._properties["OutputWidth"_crcu].Set<int>(_width);
    drawdata._properties["OutputHeight"_crcu].Set<int>(_height);
    drawdata._properties["StereoEnable"_crcu].Set<bool>(false);
    _CPD.defaultSetup(drawdata);
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
  FreestyleMaterial _blit2screenmtl;
  const FxShaderTechnique* _fxtechnique1x1;
  const FxShaderTechnique* _fxtechnique2x2;
  const FxShaderTechnique* _fxtechnique3x3;
  const FxShaderTechnique* _fxtechnique4x4;
  const FxShaderTechnique* _fxtechnique5x5;
  const FxShaderTechnique* _fxtechnique6x6;
  const FxShaderParam* _fxpMVP;
  const FxShaderParam* _fxpColorMap;
  bool _needsinit = true;
  int _width      = 0;
  int _height     = 0;
};
///////////////////////////////////////////////////////////////////////////////
ScreenOutputCompositingNode::ScreenOutputCompositingNode()
    : _supersample(0) {
  _impl = std::make_shared<SCRIMPL>(this);
}
ScreenOutputCompositingNode::~ScreenOutputCompositingNode() {
}
void ScreenOutputCompositingNode::gpuInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.Get<std::shared_ptr<SCRIMPL>>()->gpuInit(pTARG);
}
void ScreenOutputCompositingNode::beginAssemble(CompositorDrawData& drawdata) {
  _impl.Get<std::shared_ptr<SCRIMPL>>()->beginAssemble(drawdata);
}
void ScreenOutputCompositingNode::endAssemble(CompositorDrawData& drawdata) {
  _impl.Get<std::shared_ptr<SCRIMPL>>()->endAssemble(drawdata);
}
void ScreenOutputCompositingNode::composite(CompositorDrawData& drawdata) {
  drawdata.context()->debugPushGroup("ScreenOutputCompositingNode::composite");
  auto impl = _impl.Get<std::shared_ptr<SCRIMPL>>();
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
        drawdata.context()->debugPushGroup("ScreenCompositingNode::to_screen");
        auto this_buf = context->FBI()->GetThisBuffer();
        auto& mtl     = impl->_blit2screenmtl;
        switch (this->supersample()) {
          case 0:
            mtl.bindTechnique(impl->_fxtechnique1x1);
            break;
          case 1:
            mtl.bindTechnique(impl->_fxtechnique2x2);
            break;
          case 2:
            mtl.bindTechnique(impl->_fxtechnique3x3);
            break;
          case 3:
            mtl.bindTechnique(impl->_fxtechnique4x4);
            break;
          case 4:
            mtl.bindTechnique(impl->_fxtechnique5x5);
            break;
          case 5:
            mtl.bindTechnique(impl->_fxtechnique6x6);
            break;
        }
        mtl.begin(framedata);
        mtl._rasterstate.SetBlending(EBLENDING_OFF);
        mtl.bindParamCTex(impl->_fxpColorMap, tex);
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
