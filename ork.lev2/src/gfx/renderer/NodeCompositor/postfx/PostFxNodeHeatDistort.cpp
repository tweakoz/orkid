////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// PostFxNodeHeatDistort — see PostFxNodeHeatDistort.h. Refracts the
// composited frame by the screen-space gradient of a generic aux channel
// (the forward node's "aux_<channel>" pass output). Render mechanics mirror
// PostFxNodeUser (fullscreen quad, postfx_in input, own output RTG).
//
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
#include <ork/reflect/properties/registerX.inl>

#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeHeatDistort.h>

ImplementReflectionX(ork::lev2::PostFxNodeHeatDistort, "PostFxNodeHeatDistort");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeHeatDistort::describeX(class_t* c) {
  c->floatProperty("strength", float_range{0.0f, 256.0f}, &PostFxNodeHeatDistort::_strength);
  c->floatProperty("chroma", float_range{0.0f, 1.0f}, &PostFxNodeHeatDistort::_chroma);
  c->directProperty("channel", &PostFxNodeHeatDistort::_channel);
}
///////////////////////////////////////////////////////////////////////////////
namespace posteffect_heatdistort {
struct IMPL {
  ///////////////////////////////////////
  IMPL(PostFxNodeHeatDistort* node)
      : _node(node) {
  }
  ///////////////////////////////////////
  void init(lev2::Context* context) {
    if (nullptr == _rtg_out) {
      int w    = context->mainSurfaceWidth();
      int h    = context->mainSurfaceHeight();
      // NOTE: the 5th ctor arg is the USAGE selector ("user" = offscreen FBO
      // path in the vk FBI switch), NOT a debug name.
      _rtg_out = std::make_shared<RtGroup>(context, w, h, lev2::MsaaSamples::MSAA_1X, "user"_crcu);
      auto buf = _rtg_out->createRenderTarget(lev2::EBufferFormat::RGBA32F);
      buf->_debugName = "PostFxNodeHeatDistort::_rtg_out";

      _freestyle_mtl = std::make_shared<FreestyleMaterial>();
      _freestyle_mtl->gpuInit(context, "orkshader://heatdistort");
      _technique  = _freestyle_mtl->technique("heatdistort");
      OrkAssert(_technique != nullptr);
      _fxpMVP     = _freestyle_mtl->param("mvp");
      _fxpInput   = _freestyle_mtl->param("MrtMap0");
      _fxpHeat    = _freestyle_mtl->param("HeatMap");
      _fxpParams  = _freestyle_mtl->param("DistortParams");
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
    FBI->SetAutoClear(false);
    target->debugPushGroup("PostFxNodeHeatDistort::render");

    if (auto try_input = drawdata._properties["postfx_in"_crcu].tryAs<rtgroup_ptr_t>()) {
      auto input_rtg = try_input.value();
      auto buf0      = input_rtg->buffer(0);
      auto tex       = buf0 ? buf0->texture() : nullptr;
      if (tex) {
        // the aux channel published by the forward node's aux pass. Absent
        // (channel not declared / probe-only frame) -> clean passthrough:
        // bind the input as the heat field with strength 0.
        texture_ptr_t heat_tex;
        float strength    = _node->_strength;
        uint64_t chan_key = CrcString(("aux_" + _node->_channel).c_str()).hashed();
        auto it_aux       = drawdata._properties.find(chan_key);
        if (it_aux != drawdata._properties.end()) {
          if (auto try_aux = it_aux->second.tryAs<rtgroup_ptr_t>()) {
            auto aux_buf = try_aux.value()->buffer(0);
            if (aux_buf)
              heat_tex = aux_buf->_texture;
          }
        }
        if (nullptr == heat_tex) {
          heat_tex = buf0->_texture;
          strength = 0.0f;
        }

        int inputw = input_rtg->width();
        int inputh = input_rtg->height();
        _rtg_out->Resize(inputw, inputh);
        FBI->PushRtGroup(_rtg_out.get());
        _freestyle_mtl->begin(_technique, framedata);
        _freestyle_mtl->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
        _freestyle_mtl->bindParamMatrix(_fxpMVP, fmtx4::Identity());
        _freestyle_mtl->bindParamTexture(_fxpInput, tex);
        _freestyle_mtl->bindParamTexture(_fxpHeat, heat_tex.get());
        _freestyle_mtl->bindParamVec4(_fxpParams, fvec4(strength, _node->_chroma, 0.0f, 0.0f));
        ViewportRect extents(0, 0, inputw, inputh);
        FBI->pushViewport(extents);
        FBI->pushScissor(extents);
        DWI->fullscreenQuad(fvec4(0, 0, 1, 1));
        FBI->popViewport();
        FBI->popScissor();
        _freestyle_mtl->end(framedata);
        FBI->PopRtGroup();
      }
    }
    target->debugPopGroup();
    topcomp->topCPD()._single_pass_stereo = was_stereo;
  }
  ///////////////////////////////////////
  PostFxNodeHeatDistort* _node = nullptr;
  freestyle_mtl_ptr_t _freestyle_mtl;
  rtgroup_ptr_t _rtg_out;
  const FxShaderTechnique* _technique = nullptr;
  const FxShaderParam* _fxpMVP        = nullptr;
  const FxShaderParam* _fxpInput      = nullptr;
  const FxShaderParam* _fxpHeat       = nullptr;
  const FxShaderParam* _fxpParams     = nullptr;
};
} // namespace posteffect_heatdistort
///////////////////////////////////////////////////////////////////////////////
PostFxNodeHeatDistort::PostFxNodeHeatDistort() {
  _impl = std::make_shared<posteffect_heatdistort::IMPL>(this);
}
PostFxNodeHeatDistort::~PostFxNodeHeatDistort() {
}
void PostFxNodeHeatDistort::doGpuInit(lev2::Context* pTARG, int iW, int iH) {
  if (auto impl = _impl.tryAsShared<posteffect_heatdistort::IMPL>()) {
    impl.value()->init(pTARG);
  }
}
void PostFxNodeHeatDistort::DoRender(CompositorDrawData& drawdata) {
  if (auto impl = _impl.tryAsShared<posteffect_heatdistort::IMPL>()) {
    impl.value()->_render(drawdata);
  }
}
rtbuffer_ptr_t PostFxNodeHeatDistort::GetOutput() const {
  if (auto impl = _impl.tryAsShared<posteffect_heatdistort::IMPL>()) {
    auto& rtg = impl.value()->_rtg_out;
    return rtg ? rtg->buffer(0) : nullptr;
  }
  return nullptr;
}
rtgroup_ptr_t PostFxNodeHeatDistort::GetOutputGroup() const {
  if (auto impl = _impl.tryAsShared<posteffect_heatdistort::IMPL>()) {
    return impl.value()->_rtg_out;
  }
  return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
