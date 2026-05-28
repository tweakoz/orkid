////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/reflect/properties/register.h>
#include <ork/reflect/properties/registerX.inl>

#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeSSSS.h>

ImplementReflectionX(ork::lev2::PostFxNodeSSSS, "PostFxNodeSSSS");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeSSSS::describeX(class_t* c) {
  c->floatProperty(
      "blurfactor",
      float_range{0.0f, 128.0f},
      &PostFxNodeSSSS::_blurfactor);
  c->floatProperty(
      "strength",
      float_range{0.0f, 16.0f},
      &PostFxNodeSSSS::_strength);
  c->directProperty("subsurface_tint", &PostFxNodeSSSS::_subsurface_tint);
  c->directProperty("debug_mode",      &PostFxNodeSSSS::_debug_mode);
}
///////////////////////////////////////////////////////////////////////////////
namespace ssss_post {
struct IMPL {
  IMPL(PostFxNodeSSSS* node) : _node(node) {}
  ~IMPL() {}
  ///////////////////////////////////////
  void init(lev2::Context* context) {
    if (nullptr == _rtg_out) {
      int w = context->mainSurfaceWidth();
      int h = context->mainSurfaceHeight();
      // Temp RTGs for h-blur and v-blur. RGBA16F preserves the SSS mask
      // in alpha across both passes + handles HDR diffuse magnitudes.
      _rtg_blurx = std::make_shared<RtGroup>(context, w, h, lev2::MsaaSamples::MSAA_1X);
      _rtg_blury = std::make_shared<RtGroup>(context, w, h, lev2::MsaaSamples::MSAA_1X);
      _rtg_out   = std::make_shared<RtGroup>(context, w, h, lev2::MsaaSamples::MSAA_1X);
      auto b0 = _rtg_blurx->createRenderTarget(lev2::EBufferFormat::RGBA16F);
      b0->_debugName = "PostFxNodeSSSS::_rtg_blurx";
      auto b1 = _rtg_blury->createRenderTarget(lev2::EBufferFormat::RGBA16F);
      b1->_debugName = "PostFxNodeSSSS::_rtg_blury";
      auto b2 = _rtg_out->createRenderTarget(lev2::EBufferFormat::RGBA16F);
      b2->_debugName = "PostFxNodeSSSS::_rtg_out";

      _freestyle_mtl = std::make_shared<FreestyleMaterial>();
      _freestyle_mtl->gpuInit(context, "orkshader://framefx");
      _tek_blurx     = _freestyle_mtl->technique("framefx_ssss_blurx");
      _tek_blury     = _freestyle_mtl->technique("framefx_ssss_blury");
      _tek_composite = _freestyle_mtl->technique("framefx_ssss_composite");
      _fxpMVP        = _freestyle_mtl->param("mvp");
      _fxpMrtMap0    = _freestyle_mtl->param("MrtMap0");
      _fxpMrtMap1    = _freestyle_mtl->param("MrtMap1");
      _fxpMrtMap2    = _freestyle_mtl->param("MrtMap2");
      _fxpBlurFactor = _freestyle_mtl->param("BlurFactor");
      _fxpImageW     = _freestyle_mtl->param("image_width");
      _fxpImageH     = _freestyle_mtl->param("image_height");
      _fxpModColor   = _freestyle_mtl->param("ModColor");
      _fxpBlurFactorI = _freestyle_mtl->param("BlurFactorI");
    }
  }
  ///////////////////////////////////////
  void _render(CompositorDrawData& drawdata) {
    static int frame_count = 0;
    if ((frame_count++ % 60) == 0) {
      printf("[PostFxNodeSSSS::_render] frame=%d blurfactor=%g strength=%g debug=%d\n",
             frame_count, _node->_blurfactor, _node->_strength, _node->_debug_mode);
      fflush(stdout);
    }
    Context* target = drawdata.context();
    auto FBI = target->FBI();
    auto DWI = target->DWI();
    auto framedata = target->topRenderContextFrameData();
    auto topcomp   = framedata->topCompositor();
    bool was_stereo = framedata->isStereo();
    topcomp->topCPD()._single_pass_stereo = false;
    FBI->SetAutoClear(false);

    auto try_final = drawdata._properties["postfx_in"_crcu].tryAs<rtgroup_ptr_t>();
    if (!try_final) return;
    auto final_rtg = try_final.value();
    if (!final_rtg) return;
    // Forward node must have written 2 color attachments (P3.B). target1
    // (buffer index 1) carries the diffuse + sss_mask. If absent, bail.
    if (final_rtg->numImageBuffers() < 2) return;
    if (!final_rtg->buffer(0) || !final_rtg->buffer(1)) return;

    int finalw = final_rtg->width();
    int finalh = final_rtg->height();
    _rtg_blurx->Resize(finalw, finalh);
    _rtg_blury->Resize(finalw, finalh);
    _rtg_out->Resize(finalw, finalh);

    auto rquad = [&](int w, int h) {
      ViewportRect extents(0, 0, w, h);
      FBI->pushViewport(extents);
      FBI->pushScissor(extents);
      DWI->fullscreenQuad(fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
      FBI->popViewport();
      FBI->popScissor();
    };

    target->debugPushGroup("PostFxNodeSSSS::render");
    {
      ////////////////////////////////
      // Pass 1: horizontal blur of target1.
      ////////////////////////////////
      FBI->PushRtGroup(_rtg_blurx.get());
      _freestyle_mtl->begin(_tek_blurx, framedata);
      _freestyle_mtl->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
      _freestyle_mtl->bindParamFloat(_fxpBlurFactor, _node->_blurfactor);
      _freestyle_mtl->bindParamInt(_fxpImageW, finalw);
      _freestyle_mtl->bindParamInt(_fxpImageH, finalh);
      _freestyle_mtl->bindParamTexture(_fxpMrtMap0, final_rtg->texture(1).get());
      _freestyle_mtl->bindParamMatrix(_fxpMVP, fmtx4::Identity());
      rquad(finalw, finalh);
      _freestyle_mtl->end(framedata);
      FBI->PopRtGroup();
      ////////////////////////////////
      // Pass 2: vertical blur of blurx output.
      ////////////////////////////////
      FBI->PushRtGroup(_rtg_blury.get());
      _freestyle_mtl->begin(_tek_blury, framedata);
      _freestyle_mtl->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
      _freestyle_mtl->bindParamFloat(_fxpBlurFactor, _node->_blurfactor);
      _freestyle_mtl->bindParamInt(_fxpImageW, finalw);
      _freestyle_mtl->bindParamInt(_fxpImageH, finalh);
      _freestyle_mtl->bindParamTexture(_fxpMrtMap0, _rtg_blurx->texture(0).get());
      _freestyle_mtl->bindParamMatrix(_fxpMVP, fmtx4::Identity());
      rquad(finalw, finalh);
      _freestyle_mtl->end(framedata);
      FBI->PopRtGroup();
      ////////////////////////////////
      // Pass 3: composite. lit + mask * (blurred - raw).
      ////////////////////////////////
      FBI->PushRtGroup(_rtg_out.get());
      _freestyle_mtl->begin(_tek_composite, framedata);
      _freestyle_mtl->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
      _freestyle_mtl->bindParamTexture(_fxpMrtMap0, final_rtg->texture(0).get());
      _freestyle_mtl->bindParamTexture(_fxpMrtMap1, final_rtg->texture(1).get());
      _freestyle_mtl->bindParamTexture(_fxpMrtMap2, _rtg_blury->texture(0).get());
      _freestyle_mtl->bindParamVec4(_fxpModColor, fvec4(_node->_subsurface_tint, _node->_strength));
      _freestyle_mtl->bindParamInt(_fxpBlurFactorI, _node->_debug_mode);
      _freestyle_mtl->bindParamMatrix(_fxpMVP, fmtx4::Identity());
      rquad(finalw, finalh);
      _freestyle_mtl->end(framedata);
      FBI->PopRtGroup();
    }
    target->debugPopGroup();
    topcomp->topCPD()._single_pass_stereo = was_stereo;
  }
  ///////////////////////////////////////
  freestyle_mtl_ptr_t _freestyle_mtl;
  PostFxNodeSSSS* _node = nullptr;
  rtgroup_ptr_t _rtg_blurx;
  rtgroup_ptr_t _rtg_blury;
  rtgroup_ptr_t _rtg_out;
  const FxShaderTechnique* _tek_blurx     = nullptr;
  const FxShaderTechnique* _tek_blury     = nullptr;
  const FxShaderTechnique* _tek_composite = nullptr;
  const FxShaderParam* _fxpMVP        = nullptr;
  const FxShaderParam* _fxpMrtMap0    = nullptr;
  const FxShaderParam* _fxpMrtMap1    = nullptr;
  const FxShaderParam* _fxpMrtMap2    = nullptr;
  const FxShaderParam* _fxpBlurFactor = nullptr;
  const FxShaderParam* _fxpImageW     = nullptr;
  const FxShaderParam* _fxpImageH     = nullptr;
  const FxShaderParam* _fxpModColor    = nullptr;
  const FxShaderParam* _fxpBlurFactorI = nullptr;
};
} // namespace ssss_post
///////////////////////////////////////////////////////////////////////////////
PostFxNodeSSSS::PostFxNodeSSSS() {
  _impl = std::make_shared<ssss_post::IMPL>(this);
}
///////////////////////////////////////////////////////////////////////////////
PostFxNodeSSSS::~PostFxNodeSSSS() {
}
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeSSSS::doGpuInit(lev2::Context* pTARG, int /*iW*/, int /*iH*/) {
  _impl.get<std::shared_ptr<ssss_post::IMPL>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeSSSS::DoRender(CompositorDrawData& drawdata) {
  _impl.get<std::shared_ptr<ssss_post::IMPL>>()->_render(drawdata);
}
///////////////////////////////////////////////////////////////////////////////
rtbuffer_ptr_t PostFxNodeSSSS::GetOutput() const {
  auto impl = _impl.get<std::shared_ptr<ssss_post::IMPL>>();
  return (impl->_rtg_out) ? impl->_rtg_out->buffer(0) : nullptr;
}
///////////////////////////////////////////////////////////////////////////////
rtgroup_ptr_t PostFxNodeSSSS::GetOutputGroup() const {
  auto impl = _impl.get<std::shared_ptr<ssss_post::IMPL>>();
  return (impl->_rtg_out) ? impl->_rtg_out : nullptr;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
