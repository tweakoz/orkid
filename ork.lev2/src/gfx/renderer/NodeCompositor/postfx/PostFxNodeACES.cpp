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
#include <ork/reflect/properties/registerX.inl> // template DEFINITIONS — needed to instantiate floatProperty<>()

#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeACES.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/sky_atmosphere.h>

ImplementReflectionX(ork::lev2::PostFxNodeACES, "PostFxNodeACES");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeACES::describeX(class_t* c) {
  // Reflect the exposure so an AUTHORED value survives tojson -> player; without
  // it the deserialized node silently falls back to the header default (1.0 =
  // tonemap only, no grade).
  c->floatProperty("exposure", float_range{0.0f, 64.0f}, &PostFxNodeACES::_exposure);
  // The adaptation curve's knobs, reflected for the same reason: they are what
  // an author (and the owner, live) tunes, and the shipped playback path
  // re-deserializes this node from the .ecs with no python anywhere.
  c->floatProperty("adaptDayLuminance", float_range{1.0e-9f, 1.0e6f}, &PostFxNodeACES::_adaptDayLuminance);
  c->floatProperty("adaptTwilightLuminance", float_range{1.0e-9f, 1.0e6f}, &PostFxNodeACES::_adaptTwilightLuminance);
  c->floatProperty("adaptFloorLuminance", float_range{1.0e-9f, 1.0e6f}, &PostFxNodeACES::_adaptFloorLuminance);
  c->floatProperty("adaptDay", float_range{0.0f, 64.0f}, &PostFxNodeACES::_adaptDay);
  c->floatProperty("adaptTwilight", float_range{0.0f, 64.0f}, &PostFxNodeACES::_adaptTwilight);
  // The floor's range is the ODD one on purpose: it is the dark-adaptation
  // opening, and a night that a monitor can show is three decades above the
  // grading gains its two siblings live in.
  c->floatProperty("adaptFloor", float_range{0.0f, 4096.0f}, &PostFxNodeACES::_adaptFloor);
}
///////////////////////////////////////////////////////////////////////////////
namespace {
// 0 at x0, 1 at x1, smoothstep between — C1 at both ends, so the adaptation
// never kinks where one anchor segment hands over to the next.
inline float _smoothramp(float x, float x0, float x1) {
  if (x1 == x0)
    return (x >= x1) ? 1.0f : 0.0f;
  float t = (x - x0) / (x1 - x0);
  if (t <= 0.0f)
    return 0.0f;
  if (t >= 1.0f)
    return 1.0f;
  return t * t * (3.0f - 2.0f * t);
}
} // namespace
///////////////////////////////////////////////////////////////////////////////
// The work is done in log10(luminance) because available light spans four to
// five decades between noon and a moonless night: a linear interpolation across
// that range spends its whole resolution on the day end and steps the entire
// night in one texel of the curve.
float PostFxNodeACES::sceneAdaptation(float luminance, float seed_sun_elevation_sin) const {
  // WARM-UP SEED: no publish has happened, so there is no measurement. Sun up =
  // the day anchor, sun down = the floor anchor. Two branches, not a ramp —
  // growing an elevation curve here would rebuild the very thing the measured
  // drive replaced.
  if (luminance < 0.0f)
    return (seed_sun_elevation_sin > 0.0f) ? _adaptDay : _adaptFloor;

  const float lo   = std::max(_adaptFloorLuminance, 1.0e-12f);
  const float logL = log10f(std::max(luminance, lo));
  const float logF = log10f(lo);
  const float logT = log10f(std::max(_adaptTwilightLuminance, lo));
  const float logD = log10f(std::max(_adaptDayLuminance, lo));

  if (logL <= logT) {
    // FLOOR -> TWILIGHT: brighter means LESS gain. This is the limb a moonrise
    // travels, and the one the sign gate pins down.
    return _adaptFloor + (_adaptTwilight - _adaptFloor) * _smoothramp(logL, logF, logT);
  }
  // TWILIGHT -> DAY: back up to the identity as real daylight arrives.
  return _adaptTwilight + (_adaptDay - _adaptTwilight) * _smoothramp(logL, logT, logD);
}
///////////////////////////////////////////////////////////////////////////////
namespace posteffect_aces {
struct IMPL {
  ///////////////////////////////////////
  IMPL(PostFxNodeACES* node)
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
      buf->_debugName = FormatString("PostFxNodeACES::_rtg_out");
      _material.gpuInit(context);

      _freestyle_mtl = std::make_shared<FreestyleMaterial>();
      _freestyle_mtl->gpuInit(context, "orkshader://framefx");
      _tek_aces = _freestyle_mtl->technique("framefx_aces");
      _fxpMVP         = _freestyle_mtl->param("mvp");
      _fxpInputMap    = _freestyle_mtl->param("MrtMap0");
      _fxpExposure    = _freestyle_mtl->param("Exposure");
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
    topcomp->topCPD()._single_pass_stereo = false;
    //////////////////////////////////////////////////////
    FBI->SetAutoClear(false);
    //////////////////////////////////////////////////////

    if (auto try_final = drawdata._properties["postfx_in"_crcu].tryAs<rtgroup_ptr_t>()) {
      auto buf0 = try_final.value()->buffer(0);
      if (buf0) {
        assert(buf0 != nullptr);
        auto tex = buf0->texture();
        if (tex) {

          auto rquad = [&](int w, int h){
            ViewportRect extents(0, 0, w, h);
            FBI->pushViewport(extents);
            FBI->pushScissor(extents);
            // Standard UVs — Y-flip now lives at the rasterizer (negative-
            // height viewport), so sampled texels are in the expected
            // orientation.
            DWI->fullscreenQuad(fvec4(0, 0, 1, 1),
                                fvec4(0, 0, 1, 1));
            FBI->popViewport();
            FBI->popScissor();
          };

          target->debugPushGroup("PostFxNodeACES::render"); {

            auto final_rtg = try_final.value();
            int finalw = final_rtg->width();
            int finalh = final_rtg->height();
            /////////////////////
            // ACES tonemap blit
            /////////////////////
            _rtg_out->Resize(finalw,finalh);
            FBI->PushRtGroup(_rtg_out.get());
            _freestyle_mtl->begin(_tek_aces,framedata);
            _freestyle_mtl->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
            // AUTHORED exposure COMPOSED with this frame's scene adaptation —
            // never replaced by it. pbrcommon is set on the RCFD by the forward
            // node's render; a chain with no forward node in front of it (unlit,
            // 2D) has none, and then there is no environment to have measured, so
            // the node grades on the authored value alone.
            float adaptation = 1.0f;
            if (auto pbrcommon = framedata->_pbrcommon) {
              adaptation = _node->sceneAdaptation( //
                  pbrcommon->availableLightLuminance(),
                  pbrcommon->skySunElevationSin());
            }
            _freestyle_mtl->bindParamFloat(_fxpExposure, _node->_exposure * adaptation);
            _freestyle_mtl->bindParamTexture(_fxpInputMap, final_rtg->texture(0).get());
            _freestyle_mtl->bindParamMatrix(_fxpMVP, fmtx4::Identity());
            rquad(finalw,finalh);
            _freestyle_mtl->end(framedata);
            FBI->PopRtGroup();
            /////////////////////
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
  PostFxNodeACES* _node = nullptr;
  rtgroup_ptr_t _rtg_out;
  const FxShaderTechnique* _tek_aces;
  const FxShaderParam* _fxpMVP;
  const FxShaderParam* _fxpInputMap;
  const FxShaderParam* _fxpExposure;
};
} // namespace posteffect_aces
///////////////////////////////////////////////////////////////////////////////
PostFxNodeACES::PostFxNodeACES() {
  _impl = std::make_shared<posteffect_aces::IMPL>(this);
}
///////////////////////////////////////////////////////////////////////////////
PostFxNodeACES::~PostFxNodeACES() {
}
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeACES::doGpuInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.get<std::shared_ptr<posteffect_aces::IMPL>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeACES::DoRender(CompositorDrawData& drawdata) {
  _impl.get<std::shared_ptr<posteffect_aces::IMPL>>()->_render(drawdata);
}
///////////////////////////////////////////////////////////////////////////////
rtbuffer_ptr_t PostFxNodeACES::GetOutput() const {
  auto impl = _impl.get<std::shared_ptr<posteffect_aces::IMPL>>();
  return (impl->_rtg_out) ? impl->_rtg_out->buffer(0) : nullptr;
}
///////////////////////////////////////////////////////////////////////////////
rtgroup_ptr_t PostFxNodeACES::GetOutputGroup() const {
  auto impl = _impl.get<std::shared_ptr<posteffect_aces::IMPL>>();
  return (impl->_rtg_out) ? impl->_rtg_out : nullptr;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
