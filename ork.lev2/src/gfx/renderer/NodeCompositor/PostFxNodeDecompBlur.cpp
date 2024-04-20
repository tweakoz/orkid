////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/reflect/properties/register.h>

#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeDecompBlur.h>

ImplementReflectionX(ork::lev2::PostFxNodeDecompBlur, "PostFxNodeDecompBlur");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeDecompBlur::describeX(class_t* c) {
}
///////////////////////////////////////////////////////////////////////////////
namespace decomp_blur {
struct IMPL {
  ///////////////////////////////////////
  IMPL(PostFxNodeDecompBlur* node)
      : _node(node) {
  }
  ///////////////////////////////////////
  ~IMPL() {
  }
  ///////////////////////////////////////
  void init(lev2::Context* context) {
    if (nullptr == _rtg_out) {
      int w           = context->mainSurfaceWidth();
      int h           = context->mainSurfaceHeight();
      _rtg_out        = std::make_shared<RtGroup>(context, w, h, lev2::MsaaSamples::MSAA_1X);
      _rtg_a          = std::make_shared<RtGroup>(context, w, h, lev2::MsaaSamples::MSAA_1X);
      _rtg_b          = std::make_shared<RtGroup>(context, w, h, lev2::MsaaSamples::MSAA_1X);
      _rtg_c          = std::make_shared<RtGroup>(context, w, h, lev2::MsaaSamples::MSAA_1X);
      _rtg_d          = std::make_shared<RtGroup>(context, w, h, lev2::MsaaSamples::MSAA_1X);
      _rtg_e          = std::make_shared<RtGroup>(context, w, h, lev2::MsaaSamples::MSAA_1X);
      auto buf        = _rtg_out->createRenderTarget(lev2::EBufferFormat::RGBA32F);
      buf->_debugName = FormatString("PostFxNodeDecompBlur::_rtg_out");
      buf        = _rtg_a->createRenderTarget(lev2::EBufferFormat::RGBA32F);
      buf->_debugName = FormatString("PostFxNodeDecompBlur::_rtg_a");
      buf        = _rtg_b->createRenderTarget(lev2::EBufferFormat::RGBA32F);
      buf->_debugName = FormatString("PostFxNodeDecompBlur::_rtg_b");
      buf        = _rtg_c->createRenderTarget(lev2::EBufferFormat::RGBA32F);
      buf->_debugName = FormatString("PostFxNodeDecompBlur::_rtg_c");
      buf        = _rtg_d->createRenderTarget(lev2::EBufferFormat::RGBA32F);
      buf->_debugName = FormatString("PostFxNodeDecompBlur::_rtg_d");
      buf        = _rtg_e->createRenderTarget(lev2::EBufferFormat::RGBA32F);
      buf->_debugName = FormatString("PostFxNodeDecompBlur::_rtg_e");
      _material.gpuInit(context);

      _freestyle_mtl = std::make_shared<FreestyleMaterial>();
      _freestyle_mtl->gpuInit(context, "orkshader://framefx");
      _tek_blurx = _freestyle_mtl->technique("framefx_glow_blurx");
      _tek_blury = _freestyle_mtl->technique("framefx_glow_blury");
      _tek_maskbright = _freestyle_mtl->technique("framefx_glow_maskbright");
      _tek_join = _freestyle_mtl->technique("framefx_glow_join");
      _fxpMVP         = _freestyle_mtl->param("mvp");
      _fxpMrtMap0    = _freestyle_mtl->param("MrtMap0");
      _fxpMrtMap1    = _freestyle_mtl->param("MrtMap1");
      _fxpMaskThreshold = _freestyle_mtl->param("MaskThreshold");
      _fxpEffectAmount   = _freestyle_mtl->param("EffectAmount");
      _fxpBlurFactor   = _freestyle_mtl->param("BlurFactor");
      _fxpBlurFactorI   = _freestyle_mtl->param("BlurFactorI");
      _fxpImageW   = _freestyle_mtl->param("image_width");
      _fxpImageH   = _freestyle_mtl->param("image_height");

    }
  }
  ///////////////////////////////////////
  void _render(CompositorDrawData& drawdata) {
    Context* target = drawdata.context();
    auto FBI = target->FBI();
    auto DWI = target->DWI();
    auto framedata = target->topRenderContextFrameData();
    auto topcomp = framedata->topCompositor();
    bool was_stereo = framedata->isStereo();
    topcomp->topCPD()._stereo1pass = false;
    //////////////////////////////////////////////////////
    FBI->SetAutoClear(false);
    //////////////////////////////////////////////////////

    if (auto try_final = drawdata._properties["render_outgroup"_crcu].tryAs<rtgroup_ptr_t>()) {
      auto buf0 = try_final.value()->GetMrt(0);
      if (buf0) {
        assert(buf0 != nullptr);
        auto tex = buf0->texture();
        if (tex) {

          auto rquad = [&](int w, int h){
            ViewportRect extents(0, 0, w, h);
            FBI->pushViewport(extents);
            FBI->pushScissor(extents);
            DWI->quad2DEMLCCL(fvec4(-1, -1, 2, 2), // pos
                              fvec4(0, 0, 1, 1), // uv0
                              fvec4(0, 0, 1, 1));
            FBI->popViewport();
            FBI->popScissor();
          };

          target->debugPushGroup("PostFxNodeDecompBlur::render"); { //

            auto final_rtg = try_final.value();
            int finalw = final_rtg->width();
            int finalh = final_rtg->height();
            target->beginFrame();
            /////////////////////
            // downsample 2x2
            /////////////////////
            FBI->downsample2x2(final_rtg,_rtg_b);
            //FBI->downsample2x2(_rtg_a,_rtg_b);
            /////////////////////
            /////////////////////
            // brightness mask
            /////////////////////
            int smallw = _rtg_b->width();
            int smallh = _rtg_b->height();
            //printf( "smallw<%d> smallh<%d>\n", smallw, smallh );
            _rtg_c->Resize(smallw,smallh);
            FBI->PushRtGroup(_rtg_c.get());
            _freestyle_mtl->begin(_tek_maskbright,framedata);
            _freestyle_mtl->_rasterstate.SetBlending(Blending::OFF);
            _freestyle_mtl->bindParamFloat(_fxpMaskThreshold, _node->_threshold);
            _freestyle_mtl->bindParamCTex(_fxpMrtMap0, _rtg_b->GetMrt(0)->_texture.get());
            _freestyle_mtl->bindParamMatrix(_fxpMVP, fmtx4::Identity());
            rquad(smallw,smallh);
            _freestyle_mtl->end(framedata);
            FBI->PopRtGroup();
            /////////////////////
            // blurx
            /////////////////////
            _rtg_d->Resize(smallw,smallh);
            FBI->PushRtGroup(_rtg_d.get());
            _freestyle_mtl->begin(_tek_blurx,framedata);
            _freestyle_mtl->_rasterstate.SetBlending(Blending::OFF);
            _freestyle_mtl->bindParamFloat(_fxpBlurFactor, _node->_blurfactor);
            _freestyle_mtl->bindParamInt(_fxpBlurFactorI, _node->_blurwidth );
            _freestyle_mtl->bindParamInt(_fxpImageW,smallw );
            _freestyle_mtl->bindParamInt(_fxpImageH,smallh );
            _freestyle_mtl->bindParamCTex(_fxpMrtMap0, _rtg_c->GetMrt(0)->_texture.get());
            _freestyle_mtl->bindParamMatrix(_fxpMVP, fmtx4::Identity());
            rquad(smallw,smallh);
            _freestyle_mtl->end(framedata);
            FBI->PopRtGroup();
            /////////////////////
            // blury
            /////////////////////
            _rtg_e->Resize(smallw,smallh);
            FBI->PushRtGroup(_rtg_e.get());
            _freestyle_mtl->begin(_tek_blury,framedata);
            _freestyle_mtl->_rasterstate.SetBlending(Blending::OFF);
            _freestyle_mtl->bindParamFloat(_fxpBlurFactor, _node->_blurfactor);
            _freestyle_mtl->bindParamInt(_fxpBlurFactorI, _node->_blurwidth);
            _freestyle_mtl->bindParamInt(_fxpImageW,smallw );
            _freestyle_mtl->bindParamInt(_fxpImageH,smallh );
            _freestyle_mtl->bindParamCTex(_fxpMrtMap0, _rtg_d->GetMrt(0)->_texture.get());
            _freestyle_mtl->bindParamMatrix(_fxpMVP, fmtx4::Identity());
            rquad(smallw,smallh);
            _freestyle_mtl->end(framedata);
            FBI->PopRtGroup();
            /////////////////////
            // final blit
            /////////////////////
            //printf( "finalw<%d> finalh<%d>\n", finalw, finalh );
            _rtg_out->Resize(finalw,finalh);
            FBI->PushRtGroup(_rtg_out.get());
            _freestyle_mtl->begin(_tek_join,framedata);
            _freestyle_mtl->_rasterstate.SetBlending(Blending::OFF);
            _freestyle_mtl->bindParamFloat(_fxpBlurFactor, _node->_blurfactor);
            _freestyle_mtl->bindParamFloat(_fxpEffectAmount, _node->_amount );
            _freestyle_mtl->bindParamCTex(_fxpMrtMap0, _rtg_e->GetMrt(0)->_texture.get());
            _freestyle_mtl->bindParamCTex(_fxpMrtMap1, final_rtg->GetMrt(0)->_texture.get());
            _freestyle_mtl->bindParamMatrix(_fxpMVP, fmtx4::Identity());
            rquad(finalw,finalh);
            _freestyle_mtl->end(framedata);
            FBI->PopRtGroup();
            /////////////////////
            target->endFrame();
          }
          target->debugPopGroup();
        }
      }
    }
    topcomp->topCPD()._stereo1pass = was_stereo;
  }
  ///////////////////////////////////////
  CompositingMaterial _material;
  freestyle_mtl_ptr_t _freestyle_mtl;
  PostFxNodeDecompBlur* _node = nullptr;
  rtgroup_ptr_t _rtg_out;
  rtgroup_ptr_t _rtg_a;
  rtgroup_ptr_t _rtg_b;
  rtgroup_ptr_t _rtg_c;
  rtgroup_ptr_t _rtg_d;
  rtgroup_ptr_t _rtg_e;
  const FxShaderTechnique* _tek_blurx;
  const FxShaderTechnique* _tek_blury;
  const FxShaderTechnique* _tek_maskbright;
  const FxShaderTechnique* _tek_join;
  const FxShaderParam* _fxpMVP;
  const FxShaderParam* _fxpMrtMap0;
  const FxShaderParam* _fxpMrtMap1;
  const FxShaderParam* _fxpMaskThreshold;
  const FxShaderParam* _fxpEffectAmount;
  const FxShaderParam* _fxpBlurFactor;
  const FxShaderParam* _fxpBlurFactorI;
  const FxShaderParam* _fxpImageW;
  const FxShaderParam* _fxpImageH;
};
} // namespace decomp_blur
///////////////////////////////////////////////////////////////////////////////
PostFxNodeDecompBlur::PostFxNodeDecompBlur() {
  _impl = std::make_shared<decomp_blur::IMPL>(this);
}
///////////////////////////////////////////////////////////////////////////////
PostFxNodeDecompBlur::~PostFxNodeDecompBlur() {
}
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeDecompBlur::doGpuInit(lev2::Context* pTARG, int iW, int iH) // virtual
{
  _impl.get<std::shared_ptr<decomp_blur::IMPL>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeDecompBlur::DoRender(CompositorDrawData& drawdata) // virtual
{
  _impl.get<std::shared_ptr<decomp_blur::IMPL>>()->_render(drawdata);
}
///////////////////////////////////////////////////////////////////////////////
rtbuffer_ptr_t PostFxNodeDecompBlur::GetOutput() const {
  auto impl = _impl.get<std::shared_ptr<decomp_blur::IMPL>>();
  return (impl->_rtg_out) ? impl->_rtg_out->GetMrt(0) : nullptr;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
