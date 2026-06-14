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
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>

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
  c->floatProperty(
      "depth_reject_threshold",
      float_range{0.0f, 100.0f},
      &PostFxNodeSSSS::_depth_reject_threshold);
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
      // P3.D DEBUG: bumped from RGBA16F → RGBA32F to take precision off the
      // table while isolating banding cause. Revert when diagnosis is done.
      _rtg_blurx = std::make_shared<RtGroup>(context, w, h, lev2::MsaaSamples::MSAA_1X);
      _rtg_blury = std::make_shared<RtGroup>(context, w, h, lev2::MsaaSamples::MSAA_1X);
      _rtg_out   = std::make_shared<RtGroup>(context, w, h, lev2::MsaaSamples::MSAA_1X);
      auto b0 = _rtg_blurx->createRenderTarget(lev2::EBufferFormat::RGBA32F);
      b0->_debugName = "PostFxNodeSSSS::_rtg_blurx";
      auto b1 = _rtg_blury->createRenderTarget(lev2::EBufferFormat::RGBA32F);
      b1->_debugName = "PostFxNodeSSSS::_rtg_blury";
      auto b2 = _rtg_out->createRenderTarget(lev2::EBufferFormat::RGBA32F);
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
      _fxpAuxMap0    = _freestyle_mtl->param("AuxMap0");
      _fxpBlurFactor = _freestyle_mtl->param("BlurFactor");
      _fxpImageW     = _freestyle_mtl->param("image_width");
      _fxpImageH     = _freestyle_mtl->param("image_height");
      _fxpModColor   = _freestyle_mtl->param("ModColor");
      _fxpBlurFactorI = _freestyle_mtl->param("BlurFactorI");
      _fxpSsssZndcThresh = _freestyle_mtl->param("SsssZndcThresh");
      _fxpTintColor      = _freestyle_mtl->param("tint_color");
      _fxpAuxMap0_chk    = _freestyle_mtl->param("AuxMap0");
      // P3.D init diag — if any is null, the bind is a silent no-op.
      printf("[PostFxNodeSSSS::init] AuxMap0=%p SsssZndcThresh=%p tint_color=%p ModColor=%p BlurFactor=%p\n",
             (void*)_fxpAuxMap0, (void*)_fxpSsssZndcThresh, (void*)_fxpTintColor,
             (void*)_fxpModColor, (void*)_fxpBlurFactor);
      fflush(stdout);
    }
  }
  ///////////////////////////////////////
  void _render(CompositorDrawData& drawdata) {
    static int frame_count = 0;
    if ((frame_count++ % 60) == 0) {
      printf("[PostFxNodeSSSS::_render] frame=%d blurfactor=%g strength=%g debug=%d "
             "tint=(%g,%g,%g)\n",
             frame_count, _node->_blurfactor, _node->_strength, _node->_debug_mode,
             _node->_subsurface_tint.x, _node->_subsurface_tint.y,
             _node->_subsurface_tint.z);
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

    // PBR2 P3.D — runtime SSSS gate. When PbrCommon._enable_SSSS is false,
    // skip blur+composite entirely and pass target0 straight through to
    // _rtg_out. Lets the next post-fx node consume the lit composite
    // without the SSSS contribution — live A/B for "is SSSS doing anything?"
    auto rcfd_pbrcommon = framedata->_pbrcommon;
    bool sss_enabled = rcfd_pbrcommon ? rcfd_pbrcommon->_enable_SSSS : true;
    if (!sss_enabled) {
      target->debugPushGroup("PostFxNodeSSSS::passthrough");
      FBI->PushRtGroup(_rtg_out.get());
      _freestyle_mtl->begin(_tek_composite, framedata);
      _freestyle_mtl->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
      _freestyle_mtl->bindParamTexture(_fxpMrtMap0, final_rtg->texture(0).get());
      _freestyle_mtl->bindParamTexture(_fxpMrtMap1, final_rtg->texture(1).get());
      _freestyle_mtl->bindParamTexture(_fxpMrtMap2, final_rtg->texture(0).get());
      // ModColor.a = 0 makes tinted_blur = 0 → final = lit + mask*(0 - raw)?
      // Force debug mode = lit-only path via a sentinel: any value not in
      // {1,2,3,4} hits the else branch, and we want final = lit. Pass
      // ModColor.rgb = identity tint and strength = 0 so even the normal
      // branch reduces to lit + mask*(0 - raw)... — that's wrong, it
      // subtracts raw. Use a dedicated debug code that the composite
      // shader's else-fallback recognises: BlurFactorI = 5 → final = lit.
      _freestyle_mtl->bindParamVec4(_fxpModColor, fvec4(1, 1, 1, 0));
      _freestyle_mtl->bindParamInt(_fxpBlurFactorI, 5);  // 5 = passthrough lit
      _freestyle_mtl->bindParamMatrix(_fxpMVP, fmtx4::Identity());
      rquad(finalw, finalh);
      _freestyle_mtl->end(framedata);
      FBI->PopRtGroup();
      target->debugPopGroup();
      topcomp->topCPD()._single_pass_stereo = was_stereo;
      return;
    }

    // Depth-rejection inputs. P3.D — pull near/far from RCFD user-property
    // "NEAR_FAR" (set by the ForwardNode at fwdnode_impl_top.cpp:376).
    // Falls back to (0.1, 100) if the property isn't published. Earlier
    // attempt with drawdata.computeViewData() returned (0,1) defaults
    // because the CPD stack is in a different state by post-fx time.
    auto depth_tex = final_rtg->depthTexture();
    fvec2 near_far(0.1f, 100.0f);
    auto try_nf = framedata->tryUserProperty<fvec2>("NEAR_FAR"_crcu);
    if (try_nf) {
      near_far = try_nf.value();
    }
    fvec4 zndc_thresh(near_far.x, near_far.y,
                      _node->_depth_reject_threshold, 0.0f);

    // P3.D diagnostic — print every 60 frames.
    {
      static int diag_count = 0;
      if ((diag_count++ % 60) == 0) {
        printf("[PostFxNodeSSSS::diag] depth_tex=%p near=%g far=%g "
               "threshold=%g final_rtg=%dx%d (NEAR_FAR found=%d)\n",
               (void*)depth_tex.get(),
               near_far.x, near_far.y, _node->_depth_reject_threshold,
               finalw, finalh, (int)bool(try_nf));
        fflush(stdout);
      }
    }

    target->debugPushGroup("PostFxNodeSSSS::render");
    {
      ////////////////////////////////
      // Pass 1: horizontal blur of target1, depth-rejected per-tap.
      ////////////////////////////////
      FBI->PushRtGroup(_rtg_blurx.get());
      _freestyle_mtl->begin(_tek_blurx, framedata);
      _freestyle_mtl->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
      _freestyle_mtl->bindParamFloat(_fxpBlurFactor, _node->_blurfactor);
      _freestyle_mtl->bindParamInt(_fxpImageW, finalw);
      _freestyle_mtl->bindParamInt(_fxpImageH, finalh);
      _freestyle_mtl->bindParamTexture(_fxpMrtMap0, final_rtg->texture(1).get());
      _freestyle_mtl->bindParamTexture(_fxpAuxMap0, depth_tex.get());
      _freestyle_mtl->bindParamVec4(_fxpSsssZndcThresh, zndc_thresh);
      _freestyle_mtl->bindParamMatrix(_fxpMVP, fmtx4::Identity());
      rquad(finalw, finalh);
      _freestyle_mtl->end(framedata);
      FBI->PopRtGroup();
      ////////////////////////////////
      // Pass 2: vertical blur of blurx output, same depth gate.
      ////////////////////////////////
      FBI->PushRtGroup(_rtg_blury.get());
      _freestyle_mtl->begin(_tek_blury, framedata);
      _freestyle_mtl->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
      _freestyle_mtl->bindParamFloat(_fxpBlurFactor, _node->_blurfactor);
      _freestyle_mtl->bindParamInt(_fxpImageW, finalw);
      _freestyle_mtl->bindParamInt(_fxpImageH, finalh);
      _freestyle_mtl->bindParamTexture(_fxpMrtMap0, _rtg_blurx->texture(0).get());
      _freestyle_mtl->bindParamTexture(_fxpAuxMap0, depth_tex.get());
      _freestyle_mtl->bindParamVec4(_fxpSsssZndcThresh, zndc_thresh);
      _freestyle_mtl->bindParamMatrix(_fxpMVP, fmtx4::Identity());
      rquad(finalw, finalh);
      _freestyle_mtl->end(framedata);
      FBI->PopRtGroup();
      ////////////////////////////////
      // Pass 3: composite. lit + mask * (blurred - raw).
      // Also binds depth + Zndc params so debug_mode=6 can visualize
      // linear depth from the same source the blur uses.
      ////////////////////////////////
      FBI->PushRtGroup(_rtg_out.get());
      _freestyle_mtl->begin(_tek_composite, framedata);
      _freestyle_mtl->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
      _freestyle_mtl->bindParamTexture(_fxpMrtMap0, final_rtg->texture(0).get());
      _freestyle_mtl->bindParamTexture(_fxpMrtMap1, final_rtg->texture(1).get());
      _freestyle_mtl->bindParamTexture(_fxpMrtMap2, _rtg_blury->texture(0).get());
      _freestyle_mtl->bindParamTexture(_fxpAuxMap0, depth_tex.get());
      _freestyle_mtl->bindParamVec4(_fxpSsssZndcThresh, zndc_thresh);
      _freestyle_mtl->bindParamVec4(_fxpModColor, fvec4(1,1,1,1));//_node->_subsurface_tint, _node->_strength));
      // P3.D — bind tint_color from its own UBO (ublk_ssss_composite). The
      // composite shader now reads this instead of ModColor.
      _freestyle_mtl->bindParamVec4(_fxpTintColor,
          fvec4(_node->_subsurface_tint, _node->_strength));
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
  const FxShaderParam* _fxpAuxMap0    = nullptr;
  const FxShaderParam* _fxpBlurFactor = nullptr;
  const FxShaderParam* _fxpImageW     = nullptr;
  const FxShaderParam* _fxpImageH     = nullptr;
  const FxShaderParam* _fxpModColor    = nullptr;
  const FxShaderParam* _fxpBlurFactorI = nullptr;
  const FxShaderParam* _fxpSsssZndcThresh = nullptr;
  const FxShaderParam* _fxpTintColor      = nullptr;
  const FxShaderParam* _fxpAuxMap0_chk    = nullptr;  // diag only
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
