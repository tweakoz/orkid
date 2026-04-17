////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>

#include <ork/application/application.h>
#include <ork/lev2/gfx/pri.h>
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
    _CPD._debugName = "RtgOutNode:"+defaultrtg->_name;
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
      _blit2screenmtl.gpuInit(ctx, "orkshader://blit");
      _blit2screenmtl._rasterstate->setCullTest(ECullTest::OFF);
      _fxtechnique1x1 = _blit2screenmtl.technique("blit");
      _fxtechnique2x2 = _blit2screenmtl.technique("downsample_2x2");
      _fxtechnique3x3 = _blit2screenmtl.technique("downsample_3x3");
      _fxtechnique4x4 = _blit2screenmtl.technique("downsample_4x4");
      _fxtechnique5x5 = _blit2screenmtl.technique("downsample_5x5");
      _fxtechnique6x6 = _blit2screenmtl.technique("downsample_6x6");
      _fxtechnique7x7 = _blit2screenmtl.technique("downsample_7x7");
      _fxpMVP         = _blit2screenmtl.param("MatMVP");
      _fxpColorMap    = _blit2screenmtl.param("ColorMap");
      _fxpFlipY       = _blit2screenmtl.param("FlipY");
      _fxpVpDim       = _blit2screenmtl.param("ViewportDim");
      _fxtechnique_temporal = _blit2screenmtl.technique("tek_temporal_blend");
      _fxpAccumMap    = _blit2screenmtl.param("AccumMap");
      _fxpBlendWeight = _blit2screenmtl.param("BlendWeight");
      _needsinit      = false;
      int w           = ctx->mainSurfaceWidth();
      int h           = ctx->mainSurfaceHeight();
      if (ctx->hiDPI()) {
        //w /= 2;
        //h /= 2;
      }
      _width  = w;
      _height = h;

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
    drawdata._properties["SinglePassStereo"_crcu].set<bool>(false);
    drawdata._properties["eyeindex"_crcu].set<int>(0);
    auto RCFD     = drawdata.RCFD();
    RCFD->setUserProperty("eyeindex"_crcu, 0);
    _CPD.defaultSetup(drawdata);
    CIMPL->pushCPD(_CPD);
  }
  ///////////////////////////////////////
  void endAssemble(CompositorDrawData& drawdata) {
    auto CIMPL                   = drawdata._cimpl;
    Context* targ                = drawdata.context();
    //targ->endSubPass(_subpass_assemble);
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
  fxparam_constptr_t     _fxpFlipY;
  fxparam_constptr_t     _fxpVpDim;
  // Temporal accumulation
  fxtechnique_constptr_t _fxtechnique_temporal = nullptr;
  fxparam_constptr_t     _fxpAccumMap = nullptr;
  fxparam_constptr_t     _fxpBlendWeight = nullptr;
  rtgroup_ptr_t _tempRTG;       // downsample target (temp)
  rtgroup_ptr_t _accumRTG[2];   // ping-pong blend buffers
  int _accumWriteIdx = 0;
  int _accumFrameCount = 0;
  bool _needsinit = true;
  int _width      = 0;
  int _height     = 0;
};
///////////////////////////////////////////////////////////////////////////////
RtGroupOutputCompositingNode::RtGroupOutputCompositingNode(rtgroup_ptr_t defaultrtg) 
  : _supersample(0) {
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
  auto dwi = context->DWI();
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

        auto output_buffer = output_rtg->buffer(0);

        int srcw = src_buffer->_width;
        int srch = src_buffer->_height;
        int dstw = output_buffer->_width;
        int dsth = output_buffer->_height;

        static int _dbg_ss = 0;
        if ((_dbg_ss++ % 300) == 0 && this->supersample() > 0) {
          printf("SSAA resolve: ss=%d src=%dx%d dst=%dx%d ratio=%.1fx%.1f\n",
                 this->supersample(), srcw, srch, dstw, dsth,
                 float(srcw)/float(dstw), float(srch)/float(dsth));
        }

        assert(src_buffer != nullptr);
        auto tex = src_buffer->texture();
        auto& mtl     = impl->_blit2screenmtl;
        mtl._rasterstate->_force = true;
        mtl._rasterstate->setBlendingMacro(BlendingMacro::OFF);
        mtl._rasterstate->setDepthTest(EDepthTest::LEQUALS);
        // TODO: set cull test to pass front and change winding order of primitives
        mtl._rasterstate->setCullTest(ECullTest::OFF);
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
        mtl.bindParamTexture(impl->_fxpColorMap, tex);
        mtl.bindParamMatrix(impl->_fxpMVP, fmtx4::Identity());
        mtl.bindParamInt(impl->_fxpFlipY, _flipY ? 1 : 0);
        //printf("RtGroupOutputCompositingNode _flipY<%d>\n", _flipY ? 1 : 0);
        mtl.bindParamVec2(impl->_fxpVpDim, fvec2(float(dstw), float(dsth)));
        ViewportRect extents(0, 0, dstw, dsth);
        int temporalFrames = _temporalFrames;
        bool doTemporal = (temporalFrames > 0 && impl->_fxtechnique_temporal);

        if (!doTemporal) {
          // No temporal — downsample directly into output_rtg (existing path)
          fbi->pushViewport(extents);
          fbi->pushScissor(extents);
          dwi->fullscreenQuad();
          fbi->popViewport();
          fbi->popScissor();
          mtl.end(framedata);
          fbi->PopRtGroup();
        } else {
          // Temporal enabled — use 3-buffer approach to avoid read-after-write
          // _tempRTG: downsample target
          // _accumRTG[read]: previous blended result
          // _accumRTG[write]: new blended result
          mtl.end(framedata);
          fbi->PopRtGroup();

          // Ensure temp + accumulation buffers exist and are sized correctly
          auto ensureBuf = [&](rtgroup_ptr_t& rtg) {
            if (!rtg || rtg->width() != dstw || rtg->height() != dsth) {
              rtg = std::make_shared<RtGroup>(context, dstw, dsth, MsaaSamples::MSAA_1X);
              rtg->createRenderTarget(EBufferFormat::RGBA32F);
              rtg->_autoclear = false;
              impl->_accumFrameCount = 0;
            }
          };
          ensureBuf(impl->_tempRTG);
          ensureBuf(impl->_accumRTG[0]);
          ensureBuf(impl->_accumRTG[1]);

          int writeIdx = impl->_accumWriteIdx;
          int readIdx = writeIdx ^ 1;
          float weight = 1.0f / float(std::min(impl->_accumFrameCount + 1, temporalFrames));

          // Pass 1: Downsample final_out → _tempRTG
          fbi->PushRtGroup(impl->_tempRTG.get());
          mtl._rasterstate->_force = true;
          mtl._rasterstate->setBlendingMacro(BlendingMacro::OFF);
          mtl._rasterstate->setDepthTest(EDepthTest::OFF);
          mtl._rasterstate->setCullTest(ECullTest::OFF);
          switch (this->supersample()) {
            case 0: mtl.begin(impl->_fxtechnique1x1, framedata); break;
            case 1: mtl.begin(impl->_fxtechnique2x2, framedata); break;
            case 2: mtl.begin(impl->_fxtechnique3x3, framedata); break;
            case 3: mtl.begin(impl->_fxtechnique4x4, framedata); break;
            case 4: mtl.begin(impl->_fxtechnique5x5, framedata); break;
            case 5: mtl.begin(impl->_fxtechnique6x6, framedata); break;
            case 6: mtl.begin(impl->_fxtechnique7x7, framedata); break;
          }
          mtl.bindParamTexture(impl->_fxpColorMap, tex);
          mtl.bindParamMatrix(impl->_fxpMVP, fmtx4::Identity());
          mtl.bindParamInt(impl->_fxpFlipY, _flipY ? 1 : 0);
          mtl.bindParamVec2(impl->_fxpVpDim, fvec2(float(dstw), float(dsth)));
          fbi->pushViewport(extents);
          fbi->pushScissor(extents);
          dwi->fullscreenQuad();
          fbi->popViewport();
          fbi->popScissor();
          mtl.end(framedata);
          fbi->PopRtGroup();

          // Pass 2: Temporal blend _tempRTG + accum[read] → accum[write]
          auto temp_tex = impl->_tempRTG->buffer(0)->texture();
          auto accum_read_tex = impl->_accumRTG[readIdx]->buffer(0)->texture();
          fbi->PushRtGroup(impl->_accumRTG[writeIdx].get());
          mtl._rasterstate->_force = true;
          mtl._rasterstate->setBlendingMacro(BlendingMacro::OFF);
          mtl._rasterstate->setDepthTest(EDepthTest::OFF);
          mtl._rasterstate->setCullTest(ECullTest::OFF);
          mtl.begin(impl->_fxtechnique_temporal, framedata);
          mtl.bindParamTexture(impl->_fxpColorMap, temp_tex);
          mtl.bindParamTexture(impl->_fxpAccumMap, accum_read_tex);
          mtl.bindParamMatrix(impl->_fxpMVP, fmtx4::Identity());
          mtl.bindParamFloat(impl->_fxpBlendWeight, weight);
          fbi->pushViewport(extents);
          fbi->pushScissor(extents);
          dwi->fullscreenQuad();
          fbi->popViewport();
          fbi->popScissor();
          mtl.end(framedata);
          fbi->PopRtGroup();

          // Pass 3: Blit accum[write] → output_rtg (final display)
          auto accum_result_tex = impl->_accumRTG[writeIdx]->buffer(0)->texture();
          fbi->PushRtGroup(output_rtg);
          mtl._rasterstate->_force = true;
          mtl._rasterstate->setBlendingMacro(BlendingMacro::OFF);
          mtl._rasterstate->setDepthTest(EDepthTest::OFF);
          mtl._rasterstate->setCullTest(ECullTest::OFF);
          mtl.begin(impl->_fxtechnique1x1, framedata);
          mtl.bindParamTexture(impl->_fxpColorMap, accum_result_tex);
          mtl.bindParamMatrix(impl->_fxpMVP, fmtx4::Identity());
          mtl.bindParamInt(impl->_fxpFlipY, 0);
          mtl.bindParamVec2(impl->_fxpVpDim, fvec2(float(dstw), float(dsth)));
          fbi->pushViewport(extents);
          fbi->pushScissor(extents);
          dwi->fullscreenQuad();
          fbi->popViewport();
          fbi->popScissor();
          mtl.end(framedata);
          fbi->PopRtGroup();

          // Swap ping-pong and advance frame count
          impl->_accumWriteIdx ^= 1;
          if (impl->_accumFrameCount < temporalFrames)
            impl->_accumFrameCount++;
        }
      }
    }
  }

  drawdata.context()->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
