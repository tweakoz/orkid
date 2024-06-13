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

#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeHSVG.h>

ImplementReflectionX(ork::lev2::PostFxNodeHSVG, "PostFxNodeHSVG");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeHSVG::describeX(class_t* c) {
}
///////////////////////////////////////////////////////////////////////////////
namespace posteffect_hsvg {
struct IMPL {
  ///////////////////////////////////////
  IMPL(PostFxNodeHSVG* node)
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
      auto buf        = _rtg_out->createRenderTarget(lev2::EBufferFormat::RGBA32F);
      buf->_debugName = FormatString("PostFxNodeHSVG::_rtg_out");
      //buf        = _rtg_out->createRenderTarget(lev2::EBufferFormat::RGBA32F);
      _material.gpuInit(context);

      _freestyle_mtl = std::make_shared<FreestyleMaterial>();
      _freestyle_mtl->gpuInit(context, "orkshader://framefx");
      _tek_hsvg = _freestyle_mtl->technique("framefx_hsvg");
      _fxpMVP         = _freestyle_mtl->param("mvp");
      _fxpInputMap    = _freestyle_mtl->param("MrtMap0");
      _fxpHue = _freestyle_mtl->param("Hue");
      _fxpSaturation   = _freestyle_mtl->param("Saturation");
      _fxpValue  = _freestyle_mtl->param("Value");
      _fxpGamma  = _freestyle_mtl->param("Gamma");
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

          target->debugPushGroup("PostFxNodeHSVG::render"); { //

            auto final_rtg = try_final.value();
            int finalw = final_rtg->width();
            int finalh = final_rtg->height();
            target->beginFrame();
            /////////////////////
            // final blit
            /////////////////////
            //printf( "finalw<%d> finalh<%d>\n", finalw, finalh );
            _rtg_out->Resize(finalw,finalh);
            FBI->PushRtGroup(_rtg_out.get());
            _freestyle_mtl->begin(_tek_hsvg,framedata);
            _freestyle_mtl->_rasterstate.SetBlending(Blending::OFF);
            _freestyle_mtl->bindParamFloat(_fxpHue, _node->_hue);
            _freestyle_mtl->bindParamFloat(_fxpSaturation, _node->_saturation );
            _freestyle_mtl->bindParamFloat(_fxpValue, _node->_value );
            _freestyle_mtl->bindParamFloat(_fxpGamma, _node->_gamma );
            _freestyle_mtl->bindParamCTex(_fxpInputMap, final_rtg->GetMrt(0)->_texture.get());
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
  PostFxNodeHSVG* _node = nullptr;
  rtgroup_ptr_t _rtg_out;
  const FxShaderTechnique* _tek_hsvg;
  const FxShaderParam* _fxpMVP;
  const FxShaderParam* _fxpInputMap;
  const FxShaderParam* _fxpHue;
  const FxShaderParam* _fxpSaturation;
  const FxShaderParam* _fxpValue;
  const FxShaderParam* _fxpGamma;
  const FxShaderParam* _fxpImageW;
  const FxShaderParam* _fxpImageH;
};
} // namespace posteffect_hsvg
///////////////////////////////////////////////////////////////////////////////
PostFxNodeHSVG::PostFxNodeHSVG() {
  _impl = std::make_shared<posteffect_hsvg::IMPL>(this);
}
///////////////////////////////////////////////////////////////////////////////
PostFxNodeHSVG::~PostFxNodeHSVG() {
}
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeHSVG::doGpuInit(lev2::Context* pTARG, int iW, int iH) // virtual
{
  _impl.get<std::shared_ptr<posteffect_hsvg::IMPL>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeHSVG::DoRender(CompositorDrawData& drawdata) // virtual
{
  _impl.get<std::shared_ptr<posteffect_hsvg::IMPL>>()->_render(drawdata);
}
///////////////////////////////////////////////////////////////////////////////
rtbuffer_ptr_t PostFxNodeHSVG::GetOutput() const {
  auto impl = _impl.get<std::shared_ptr<posteffect_hsvg::IMPL>>();
  return (impl->_rtg_out) ? impl->_rtg_out->GetMrt(0) : nullptr;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
