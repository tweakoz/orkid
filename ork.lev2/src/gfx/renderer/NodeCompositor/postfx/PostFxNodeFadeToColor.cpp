////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
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

#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeFadeToColor.h>

ImplementReflectionX(ork::lev2::PostFxNodeFadeToColor, "PostFxNodeFadeToColor");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeFadeToColor::describeX(class_t* c) {
}
///////////////////////////////////////////////////////////////////////////////
namespace posteffect_fade_to_color {
struct IMPL {
  ///////////////////////////////////////
  IMPL(PostFxNodeFadeToColor* node)
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
      buf->_debugName = FormatString("PostFxNodeFadeToColor::_rtg_out");

      _material.gpuInit(context);
      _freestyle_mtl = std::make_shared<FreestyleMaterial>();
      _freestyle_mtl->gpuInit(context, "orkshader://framefx");
      _tek_fade       = _freestyle_mtl->technique("framefx_fade_to_color");
      _fxpMVP         = _freestyle_mtl->param("mvp");
      _fxpInputMap    = _freestyle_mtl->param("MrtMap0");
      _fxpFadeColor   = _freestyle_mtl->param("FadeColor");
      _fxpFadeAmount  = _freestyle_mtl->param("FadeAmount");
    }
  }
  ///////////////////////////////////////
  void _render(CompositorDrawData& drawdata) {
    Context* target = drawdata.context();
    auto FBI        = target->FBI();
    auto DWI        = target->DWI();
    auto framedata  = target->topRenderContextFrameData();
    auto topcomp    = framedata->topCompositor();
    bool was_stereo = framedata->isStereo();
    topcomp->topCPD()._single_pass_stereo = false;
    ///////////////////////////////////////
    FBI->SetAutoClear(false);

    if (auto try_final = drawdata._properties["postfx_in"_crcu].tryAs<rtgroup_ptr_t>()) {
      auto buf0 = try_final.value()->buffer(0);
      if (buf0) {
        auto tex = buf0->texture();
        if (tex) {
          auto rquad = [&](int w, int h) {
            ViewportRect extents(0, 0, w, h);
            FBI->pushViewport(extents);
            FBI->pushScissor(extents);
            // Standard UVs — see PostFxNodeHSVG for rationale.
            DWI->fullscreenQuad(fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
            FBI->popViewport();
            FBI->popScissor();
          };

          target->debugPushGroup("PostFxNodeFadeToColor::render");
          {
            auto final_rtg = try_final.value();
            int finalw     = final_rtg->width();
            int finalh     = final_rtg->height();
            _rtg_out->Resize(finalw, finalh);
            FBI->PushRtGroup(_rtg_out.get());
            _freestyle_mtl->begin(_tek_fade, framedata);
            _freestyle_mtl->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
            _freestyle_mtl->bindParamVec4(_fxpFadeColor, _node->_fadeColor);
            _freestyle_mtl->bindParamFloat(_fxpFadeAmount, _node->_fadeAmount);
            _freestyle_mtl->bindParamTexture(_fxpInputMap, final_rtg->texture(0).get());
            _freestyle_mtl->bindParamMatrix(_fxpMVP, fmtx4::Identity());
            rquad(finalw, finalh);
            _freestyle_mtl->end(framedata);
            FBI->PopRtGroup();
          }
          target->debugPopGroup();
        }
      }
    }
    topcomp->topCPD()._single_pass_stereo = was_stereo;
  }
  ///////////////////////////////////////
  CompositingMaterial _material;
  freestyle_mtl_ptr_t _freestyle_mtl;
  PostFxNodeFadeToColor* _node = nullptr;
  rtgroup_ptr_t _rtg_out;
  const FxShaderTechnique* _tek_fade;
  const FxShaderParam* _fxpMVP;
  const FxShaderParam* _fxpInputMap;
  const FxShaderParam* _fxpFadeColor;
  const FxShaderParam* _fxpFadeAmount;
};
} // namespace posteffect_fade_to_color
///////////////////////////////////////////////////////////////////////////////
PostFxNodeFadeToColor::PostFxNodeFadeToColor() {
  _impl = std::make_shared<posteffect_fade_to_color::IMPL>(this);
}
///////////////////////////////////////////////////////////////////////////////
PostFxNodeFadeToColor::~PostFxNodeFadeToColor() {
}
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeFadeToColor::doGpuInit(lev2::Context* pTARG, int iW, int iH) // virtual
{
  _impl.get<std::shared_ptr<posteffect_fade_to_color::IMPL>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeFadeToColor::DoRender(CompositorDrawData& drawdata) // virtual
{
  _impl.get<std::shared_ptr<posteffect_fade_to_color::IMPL>>()->_render(drawdata);
}
///////////////////////////////////////////////////////////////////////////////
rtbuffer_ptr_t PostFxNodeFadeToColor::GetOutput() const {
  auto impl = _impl.get<std::shared_ptr<posteffect_fade_to_color::IMPL>>();
  return (impl->_rtg_out) ? impl->_rtg_out->buffer(0) : nullptr;
}
///////////////////////////////////////////////////////////////////////////////
rtgroup_ptr_t PostFxNodeFadeToColor::GetOutputGroup() const {
  auto impl = _impl.get<std::shared_ptr<posteffect_fade_to_color::IMPL>>();
  return (impl->_rtg_out) ? impl->_rtg_out : nullptr;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
