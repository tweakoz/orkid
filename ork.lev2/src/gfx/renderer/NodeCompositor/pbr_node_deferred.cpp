////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <algorithm>
#include <ork/pch.h>
#include <ork/rtti/Class.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/mutex.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/kernel/datacache.h>
#include <ork/gfx/brdf.inl>
#include <ork/gfx/dds.h>
// #include <ork/gfx/image.inl>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/texman.h>

#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_light_processor_simple.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_light_processor_cpu.h>
#include <ork/profiling.inl>
#include <ork/asset/Asset.inl>

ImplementReflectionX(ork::lev2::pbr::deferrednode::DeferredCompositingNodePbr, "DeferredCompositingNodePbr");

// fvec3 LightColor
// fvec4 LightPosR 16byte
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::pbr::deferrednode {
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNodePbr::describeX(class_t* c) {
  class_t::CreateClassAlias("DeferredCompositingNodeDebugNormal", c);
}

///////////////////////////////////////////////////////////////////////////////
struct PbrNodeImpl {
  static constexpr int KMAXLIGHTS = 32;
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  PbrNodeImpl(DeferredCompositingNodePbr* node)
      : _camname(AddPooledString("Camera"))
      , _context(std::make_shared<DeferredContext>(node, node->_shader_path, KMAXLIGHTS))
      , _lightProcessor(*_context, node) {
    _enumeratedLights = std::make_shared<EnumeratedLights>();
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ~PbrNodeImpl() {
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void init(lev2::Context* context) {
    _context->gpuInit(context);
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void _render(DeferredCompositingNodePbr* node, CompositorDrawData& drawdata) {

    auto rtg_gbuffer = _context->_rtgs_gbuffer->fetch(node->_bufferKey);
    auto rtg_laccum  = _context->_rtgs_laccum->fetch(node->_bufferKey);

    _timer.Start();
    EASY_BLOCK("PbrNodeImpl::_render", profiler::colors::Red);
    auto RCFD         = drawdata.RCFD();
    auto pbrcommon    = node->_pbrcommon;
    auto targ         = RCFD->GetTarget();
    auto CIMPL        = drawdata._cimpl;
    auto FBI          = targ->FBI();
    auto this_buf     = FBI->GetThisBuffer();
    auto RSI          = targ->RSI();
    auto DWI          = targ->DWI();
    const auto TOPCPD = CIMPL->topCPD();
    /////////////////////////////////////////////////
    RCFD->setUserProperty("rtg_gbuffer"_crc, rtg_gbuffer);
    RCFD->setUserProperty("rtb_gbuffer"_crc, rtg_gbuffer->GetMrt(0));
    RCFD->setUserProperty("rtb_accum"_crc, rtg_laccum->GetMrt(0));
    RCFD->_renderingmodel = node->_renderingmodel;
    RCFD->_pbrcommon      = pbrcommon;
    //////////////////////////////////////////////////////
    _context->renderUpdate(node, drawdata);
    auto VD = drawdata.computeViewData();
    _context->updateDebugLights(VD);
    _context->_clearColor = pbrcommon->_clearColor;
    /////////////////////////////////////////////////////////////////////////////////////////
    bool is_stereo = VD._isStereo;
    /////////////////////////////////////////////////////////////////////////////////////////
    targ->debugPushGroup("Deferred::render");
    _context->renderGbuffer(node, drawdata, VD);
    targ->debugPushGroup("Deferred::LightAccum");
    _context->_accumCPD = TOPCPD;
    /////////////////////////////////////////////////////////////////
    auto vprect   = ViewportRect(0, 0, _context->_width, _context->_height);
    auto quadrect = SRect(0, 0, _context->_width, _context->_height);
    _context->_accumCPD.SetDstRect(vprect);
    _context->_accumCPD._irendertarget        = rtg_laccum->_rendertarget.get();
    _context->_accumCPD._cameraMatrices       = nullptr;
    _context->_accumCPD._stereoCameraMatrices = nullptr;
    _context->_accumCPD._stereo1pass          = false;
    _context->_specularLevel                  = pbrcommon->specularLevel() * pbrcommon->environmentIntensity();
    _context->_diffuseLevel                   = pbrcommon->diffuseLevel() * pbrcommon->environmentIntensity();
    _context->_depthFogDistance               = pbrcommon->depthFogDistance();
    _context->_depthFogPower                  = pbrcommon->depthFogPower();
    float skybox_level                        = pbrcommon->skyboxLevel() * pbrcommon->environmentIntensity();
    CIMPL->pushCPD(_context->_accumCPD); // base lighting
    FBI->SetAutoClear(true);
    FBI->PushRtGroup(rtg_laccum.get());
    FBI->Clear(fvec4(0.1, 0.2, 0.3, 1), 1.0f);

    //////////////////////////////////////////////////////////////////
    if (auto lmgr = CIMPL->lightManager()) {
      EASY_BLOCK("lights-1");
      lmgr->enumerateInPass(_context->_accumCPD, _enumeratedLights);
      _lightProcessor.gpuUpdate(drawdata, VD, _enumeratedLights);
      auto& lights = _enumeratedLights->_alllights;
      if (lights.size())
        _lightProcessor.renderDecals(drawdata, VD, _enumeratedLights);
    }
    //////////////////////////////////////////////////////////////////
    // base lighting (environent IBL lighting)
    //////////////////////////////////////////////////////////////////
    targ->debugPushGroup("Deferred::BaseLighting");
    _context->_lightingmtl->_rasterstate.SetBlending(Blending::OFF);
    _context->_lightingmtl->_rasterstate.SetDepthTest(EDepthTest::OFF);
    _context->_lightingmtl->_rasterstate.SetCullTest(ECullTest::OFF);

    int pbr_model = RCFD->getUserProperty("pbr_model"_crc).get<int>();

    fxpipeline_ptr_t baselighting_pipeline = _context->_pipeline_envlighting_model0_mono;

    switch (pbr_model) {
      case 1:
        OrkAssert(false);
        break;
      case 0:
      default:
        OrkAssert(not is_stereo);
        break;
    }

    //////////////////////////////////////////////////////
    // defaults..
    //////////////////////////////////////////////////////

    /////////////////////////
    // pipeline wrapped (overrides here)
    /////////////////////////

    RenderContextInstData dummy_rcid(RCFD);
    baselighting_pipeline->wrappedDrawCall(dummy_rcid, [=]() {
      _context->_lightingmtl->bindParamFloat(_context->_parDepthFogDistance, 1.0f / pbrcommon->depthFogDistance());
      _context->_lightingmtl->bindParamFloat(_context->_parDepthFogPower, pbrcommon->depthFogPower());

      /////////////////////////

      _context->_lightingmtl->bindParamCTex(_context->_parMapGBuf, rtg_gbuffer->GetMrt(0)->texture());

      _context->_lightingmtl->bindParamCTex(_context->_parMapDepth, rtg_gbuffer->_depthBuffer->_texture.get());

      _context->_lightingmtl->bindParamCTex(_context->_parMapSpecularEnv, pbrcommon->envSpecularTexture().get());
      _context->_lightingmtl->bindParamCTex(_context->_parMapDiffuseEnv, pbrcommon->envDiffuseTexture().get());

      OrkAssert(_context->brdfIntegrationTexture() != nullptr);
      _context->_lightingmtl->bindParamCTex(_context->_parMapBrdfIntegration, _context->brdfIntegrationTexture().get());

      _context->_lightingmtl->bindParamCTex(_context->_parMapVolTexA, _context->_voltexA->_texture.get());

      /////////////////////////
      // SSAO
      /////////////////////////

      int node_frame = node->_frameIndex;

      _context->_lightingmtl->bindParamInt(_context->_parSSAONumSamples, pbrcommon->_ssaoNumSamples);
      if (_context->_parSSAONumSamples) {
        _context->_lightingmtl->bindParamInt(_context->_parSSAONumSteps, pbrcommon->_ssaoNumSteps);
        _context->_lightingmtl->bindParamFloat(_context->_parSSAOBias, pbrcommon->_ssaoBias);
        _context->_lightingmtl->bindParamFloat(_context->_parSSAORadius, pbrcommon->_ssaoRadius);
        _context->_lightingmtl->bindParamFloat(_context->_parSSAOWeight, pbrcommon->_ssaoWeight);
        _context->_lightingmtl->bindParamFloat(_context->_parSSAOPower, pbrcommon->_ssaoPower);
        if (pbrcommon->_ssaoNumSamples >= 8) {
          _context->_lightingmtl->bindParamCTex(_context->_parSSAOKernel, pbrcommon->ssaoKernel(targ, node_frame).get());
          _context->_lightingmtl->bindParamCTex(
              _context->_parSSAOScrNoise, pbrcommon->ssaoScrNoise(targ, node_frame, _context->_width, _context->_height).get());
        }
      }
      /////////////////////////
      _context->_lightingmtl->bindParamFloat(_context->_parSkyboxLevel, skybox_level);
      _context->_lightingmtl->bindParamVec3(_context->_parAmbientLevel, pbrcommon->ambientLevel());
      _context->_lightingmtl->bindParamFloat(_context->_parSpecularLevel, _context->_specularLevel);
      _context->_lightingmtl->bindParamFloat(_context->_parDiffuseLevel, _context->_diffuseLevel);

      /////////////////////////

      float num_mips = pbrcommon->envSpecularTexture()->_num_mips;

      _context->_lightingmtl->bindParamFloat(_context->_parEnvironmentMipBias, pbrcommon->environmentMipBias());
      _context->_lightingmtl->bindParamFloat(_context->_parEnvironmentMipScale, pbrcommon->environmentMipScale() * num_mips);
      _context->_lightingmtl->bindParamFloat(_context->_parSpecularMipBias, pbrcommon->_specularMipBias);
      /////////////////////////
      _context->_lightingmtl->_rasterstate.SetZWriteMask(false);
      _context->_lightingmtl->_rasterstate.SetDepthTest(EDepthTest::OFF);
      _context->_lightingmtl->_rasterstate.SetAlphaTest(EALPHATEST_OFF);

      _context->bindViewParams(VD);

      //////////////////////////////////////////////////////
      // aux bindings
      //////////////////////////////////////////////////////

      for (auto mapping : _context->_auxbindings) {
        auto name        = mapping.first;
        auto mappingdata = mapping.second;
        if (mappingdata->_param == nullptr) {
          mappingdata->_param = _context->_lightingmtl->param(name);
        }
        OrkAssert(mappingdata->_param != nullptr);
        auto param = mappingdata->_param;
        if (auto as_tex = mappingdata->_var.tryAsShared<Texture>()) {
          _context->_lightingmtl->bindParamCTex(param, as_tex.value().get());
        } else if (auto as_mtx4 = mappingdata->_var.tryAs<fmtx4>()) {
          _context->_lightingmtl->bindParamMatrix(param, as_mtx4.value());
        } else if (auto as_float = mappingdata->_var.tryAs<float>()) {
          _context->_lightingmtl->bindParamFloat(param, as_float.value());
        } else if (auto as_double = mappingdata->_var.tryAs<double>()) {
          _context->_lightingmtl->bindParamFloat(param, as_double.value());
        } else if (auto as_vec2 = mappingdata->_var.tryAs<fvec2>()) {
          _context->_lightingmtl->bindParamVec2(param, as_vec2.value());
        } else if (auto as_vec3 = mappingdata->_var.tryAs<fvec3>()) {
          _context->_lightingmtl->bindParamVec3(param, as_vec3.value());
        } else if (auto as_vec4 = mappingdata->_var.tryAs<fvec4>()) {
          _context->_lightingmtl->bindParamVec4(param, as_vec4.value());
        }
      }

      //////////////////////////////////////////////////////

      _context->_lightingmtl->commit();
      RSI->BindRasterState(_context->_lightingmtl->_rasterstate);

      DWI->quad2DEMLTiled(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 0, 0), 16);
    });

    targ->debugPopGroup(); // BaseLighting

    /////////////////////////////////
    // Dynamic Lighting
    /////////////////////////////////

    if (auto lmgr = CIMPL->lightManager()) {
      EASY_BLOCK("lights-2");
      if (_enumeratedLights->_alllights.size())
        _lightProcessor.renderLights(drawdata, VD, _enumeratedLights);
    }

    /////////////////////////////////
    // end frame
    /////////////////////////////////

    CIMPL->popCPD();   // base lighting
    FBI->PopRtGroup(); // deferredRtg

    targ->debugPopGroup(); // "Deferred::LightAccum"
    targ->debugPopGroup(); // "Deferred::render"
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  PoolString _camname;

  pbr_deferred_context_ptr_t _context;
  bool _needsinit = true;
  int _sequence   = 0;
  std::atomic<int> _lightjobcount;
  ork::Timer _timer;
  enumeratedlights_ptr_t _enumeratedLights;
  SimpleLightProcessor _lightProcessor;

}; // IMPL

///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNodePbr::DeferredCompositingNodePbr(pbr::commonstuff_ptr_t pbrc) {
  _shader_path    = "orkshader://deferred";
  _renderingmodel = RenderingModel("DEFERRED_PBR"_crcu);
  _impl           = std::make_shared<PbrNodeImpl>(this);
  _pbrcommon      = pbrc;
  if (_pbrcommon == nullptr) {
    _pbrcommon = std::make_shared<pbr::CommonStuff>();
  }
}

///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNodePbr::~DeferredCompositingNodePbr() {
}
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNodePbr::doGpuInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.get<std::shared_ptr<PbrNodeImpl>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNodePbr::DoRender(CompositorDrawData& drawdata) {
  auto impl = _impl.get<std::shared_ptr<PbrNodeImpl>>();
  impl->_render(this, drawdata);
}
///////////////////////////////////////////////////////////////////////////////
pbr_deferred_context_ptr_t DeferredCompositingNodePbr::deferredContext() {
  auto impl = _impl.get<std::shared_ptr<PbrNodeImpl>>();
  return impl->_context;
}
///////////////////////////////////////////////////////////////////////////////
rtbuffer_ptr_t DeferredCompositingNodePbr::GetOutput() const {
  return GetOutputGroup()->GetMrt(0);
}
///////////////////////////////////////////////////////////////////////////////
rtgroup_ptr_t DeferredCompositingNodePbr::GetOutputGroup() const {
  auto CTX = _impl.get<std::shared_ptr<PbrNodeImpl>>()->_context;
  return CTX->_rtgs_laccum->fetch(_bufferKey);
}
void DeferredCompositingNodePbr::overrideShader(std::string path) {
  auto CTX         = _impl.get<std::shared_ptr<PbrNodeImpl>>()->_context;
  CTX->_shadername = path;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::pbr::deferrednode
