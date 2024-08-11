////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::lev2::RtGroupOutputCompositingNode, "RtGroupOutputCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
void RtGroupOutputCompositingNode::describeX(class_t* c) {
  c->directProperty("Layer", &RtGroupOutputCompositingNode::_layername);
}
///////////////////////////////////////////////////////////////////////////////
struct RTGIMPL {
  ///////////////////////////////////////
  RTGIMPL(const RtGroupOutputCompositingNode* node,rtgroup_ptr_t defaultrtg)
      : _node(node)
      , _camname(AddPooledString("Camera"))
      , _layers(AddPooledString("All")) {
    _outputRTG = defaultrtg;
    _CPD._debugName = "RtGroupOutputCompositingNode";
  }
  ///////////////////////////////////////
  ~RTGIMPL() {
  }
  ///////////////////////////////////////
  void gpuInit(lev2::Context* ctx) {
    if(nullptr==_outputRTG){
      _outputRTG = std::make_shared<RtGroup>(ctx,8, 8, MsaaSamples::MSAA_1X);
      auto rtb = _outputRTG->createRenderTarget(EBufferFormat::RGBA32F);
    }
    if (_needsinit) {
      _blit2screenmtl.gpuInit(ctx, "orkshader://solid");
      _blit2screenmtl.gpuInit(ctx, "orkshader://solid");
      _fxtechnique1x1 = _blit2screenmtl.technique("texcolor");
      _fxtechnique2x2 = _blit2screenmtl.technique("downsample_2x2");
      _fxtechnique3x3 = _blit2screenmtl.technique("downsample_3x3");
      _fxtechnique4x4 = _blit2screenmtl.technique("downsample_4x4");
      _fxtechnique5x5 = _blit2screenmtl.technique("downsample_5x5");
      _fxtechnique6x6 = _blit2screenmtl.technique("downsample_6x6");
      _fxtechnique7x7 = _blit2screenmtl.technique("downsample_7x7");
      _fxpMVP         = _blit2screenmtl.param("MatMVP");
      _fxpColorMap   = _blit2screenmtl.param("ColorMap");
      _needsinit      = false;
      int w           = ctx->mainSurfaceWidth();
      int h           = ctx->mainSurfaceHeight();
      if (ctx->hiDPI()) {
        //w /= 2;
        //h /= 2;
      }
      _width  = w;
      _height = h;
      int ssaa = _node->_supersample;
      printf("RTGIMPL<%p> node<%p>::gpuInit ssaa<%d>\n", this, _node, ssaa);
    }
  }
  ///////////////////////////////////////
  void beginAssemble(CompositorDrawData& drawdata) {
    auto& ddprops                = drawdata._properties;
    auto CIMPL                   = drawdata._cimpl;
    const auto& CCTX = CIMPL->compositingContext();
    Context* targ                = drawdata.context();
    int w                        = CCTX.miWidth;
    int h                        = CCTX.miHeight;
    _width  = w * (_node->supersample() + 1);
    _height = h * (_node->supersample() + 1);
    //////////////////////////////////////////////////////
    drawdata._properties["OutputWidth"_crcu].set<int>(_width);
    drawdata._properties["OutputHeight"_crcu].set<int>(_height);
    drawdata._properties["StereoEnable"_crcu].set<bool>(false);
    _CPD.defaultSetup(drawdata);
    CIMPL->pushCPD(_CPD);
  }
  ///////////////////////////////////////
  void endAssemble(CompositorDrawData& drawdata) {
    auto CIMPL                   = drawdata._cimpl;
    CIMPL->popCPD();
  }
  ///////////////////////////////////////
  const RtGroupOutputCompositingNode* _node = nullptr;
  rtgroup_ptr_t _outputRTG;
  PoolString _camname, _layers;
  CompositingPassData _CPD;
  FreestyleMaterial _blit2screenmtl;
  fxtechnique_constptr_t _fxtechnique1x1;
  fxtechnique_constptr_t _fxtechnique2x2;
  fxtechnique_constptr_t _fxtechnique3x3;
  fxtechnique_constptr_t _fxtechnique4x4;
  fxtechnique_constptr_t _fxtechnique5x5;
  fxtechnique_constptr_t _fxtechnique6x6;
  fxtechnique_constptr_t _fxtechnique7x7;
  fxparam_constptr_t     _fxpMVP;
  fxparam_constptr_t     _fxpColorMap;
  bool _needsinit = true;
  int _width      = 0;
  int _height     = 0;
};
///////////////////////////////////////////////////////////////////////////////
RtGroupOutputCompositingNode::RtGroupOutputCompositingNode(rtgroup_ptr_t defaultrtg) {
  _impl       = std::make_shared<RTGIMPL>(this,defaultrtg);
}
RtGroupOutputCompositingNode::~RtGroupOutputCompositingNode() {
}
void RtGroupOutputCompositingNode::resize(int w, int h) {
  auto impl     = _impl.get<std::shared_ptr<RTGIMPL>>();
  impl->_width  = w;
  impl->_height = h;
}
void RtGroupOutputCompositingNode::gpuInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.get<std::shared_ptr<RTGIMPL>>()->gpuInit(pTARG);
}
void RtGroupOutputCompositingNode::beginAssemble(CompositorDrawData& drawdata) {
  _impl.get<std::shared_ptr<RTGIMPL>>()->beginAssemble(drawdata);
}
void RtGroupOutputCompositingNode::endAssemble(CompositorDrawData& drawdata) {
  _impl.get<std::shared_ptr<RTGIMPL>>()->endAssemble(drawdata);
}
void RtGroupOutputCompositingNode::composite(CompositorDrawData& drawdata) {
  drawdata.context()->debugPushGroup("RtGroupOutputCompositingNode::composite");
  auto impl = _impl.get<std::shared_ptr<RTGIMPL>>();
  /////////////////////////////////////////////////////////////////////////////
  // VR compositor
  /////////////////////////////////////////////////////////////////////////////
  Context* context                  = drawdata.context();
  auto framedata = drawdata.RCFD();
  auto fbi                          = context->FBI();
  auto gbi = context->GBI();
  auto output_rtg = impl->_outputRTG.get();

  if(0)
  printf( "composite into rtg<%s> w<%d> h<%d>\n", //
           output_rtg->_name.c_str(), //
           output_rtg->width(), //
           output_rtg->height() );

  if (output_rtg) {
    if (auto try_final = drawdata._properties["final_out"_crcu].tryAs<RtBuffer*>()) {
      auto src_buffer = try_final.value();
      if (src_buffer) {

        fbi->PushRtGroup(output_rtg);

        auto output_buffer = output_rtg->GetMrt(0);

        int srcw = src_buffer->_width;
        int srch = src_buffer->_height;
        int dstw = output_buffer->_width;
        int dsth = output_buffer->_height;

        //printf( "src<%d %d> dst<%d %d>\n", srcw, srch, dstw, dsth );

        assert(src_buffer != nullptr);
        auto tex = src_buffer->texture();
        auto& mtl     = impl->_blit2screenmtl;
        mtl._rasterstate.SetBlending(Blending::OFF);
        // TODO: set cull test to pass front and change winding order of primitives
        mtl._rasterstate.SetCullTest(ECullTest::PASS_BACK);
        switch (this->supersample()) {
          case 0:
            mtl.begin(impl->_fxtechnique1x1, framedata);
            break;
          case 1:
            mtl.begin(impl->_fxtechnique2x2, framedata);
            break;
          case 2:
            mtl.begin(impl->_fxtechnique3x3, framedata);
            break;
          case 3:
            mtl.begin(impl->_fxtechnique4x4, framedata);
            break;
          case 4:
            mtl.begin(impl->_fxtechnique5x5, framedata);
            break;
          case 5:
            mtl.begin(impl->_fxtechnique6x6, framedata);
            break;
          case 6:
            mtl.begin(impl->_fxtechnique7x7, framedata);
            break;
        }
        mtl.bindParamCTex(impl->_fxpColorMap, tex);
        mtl.bindParamMatrix(impl->_fxpMVP, fmtx4::Identity());
        ViewportRect extents(0, 0, dstw, dsth);
        fbi->pushViewport(extents);
        fbi->pushScissor(extents);
        gbi->render2dQuadEML(); // full screen quad
        fbi->popViewport();
        fbi->popScissor();
        mtl.end(framedata);

        fbi->PopRtGroup();
      }
    }
  }

  drawdata.context()->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
