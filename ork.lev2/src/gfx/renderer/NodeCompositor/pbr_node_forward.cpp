////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>

#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/asset/Asset.inl>
#include <ork/profiling.inl>

ImplementReflectionX(ork::lev2::pbr::ForwardNode, "PbrForwardNode");

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
extern appinitdata_ptr_t _ginitdata;
} // namespace ork::lev2

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::pbr {
///////////////////////////////////////////////////////////////////////////////
void ForwardNode::describeX(class_t* c) {
}
///////////////////////////////////////////////////////////////////////////////
struct ForwardPbrNodeImpl {
  static const int KMAXLIGHTS = 32;
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ForwardPbrNodeImpl(ForwardNode* node)
      : _node(node)
      , _camname("Camera") { //
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ~ForwardPbrNodeImpl() {
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void init(lev2::Context* context, int iw, int ih) {

    if (nullptr == _rtgs_main) {

      _rtg_depth_copy = std::make_shared<RtGroup>(context, 8, 8);

      auto e_msaa = intToMsaaEnum(_ginitdata->_msaa_samples);
      _rtgs_main  = std::make_shared<RtgSet>(context, e_msaa, "rtgs-main");
      _rtgs_main->addBuffer("ForwardRt0", EBufferFormat::RGBA8);

      printf("PBRFWD_MSAA<%d>\n", int(_ginitdata->_msaa_samples));
      //_rtg             = std::make_shared<RtGroup>(context, 8, 8, intToMsaaEnum(_ginitdata->_msaa_samples));
      // auto buf1        = _rtg->createRenderTarget(EBufferFormat::RGBA8);
      // buf1->_debugName = "ForwardRt0";
      _skybox_material           = std::make_shared<PBRMaterial>(context);
      _skybox_material->_variant = "skybox.forward"_crcu;
      _skybox_fxcache            = _skybox_material->pipelineCache();
      _enumeratedLights          = std::make_shared<EnumeratedLights>();

      if (_ginitdata->_msaa_samples > 1) {
        _rtgs_resolve_msaa = std::make_shared<RtgSet>(context, MsaaSamples::MSAA_1X, "rtgs-,main-resolve");
        _rtgs_resolve_msaa->addBuffer("MsaaDownsampleBuffer", EBufferFormat::RGBA8);
        //_rtg_resolve_msaa = std::make_shared<RtGroup>(context, 8, 8, MsaaSamples::MSAA_1X);
        // auto dsbuf        = _rtg_resolve_msaa->createRenderTarget(EBufferFormat::RGBA8);
        // dsbuf->_debugName = "MsaaDownsampleBuffer";
        _blit2screenmtl.gpuInit(context, "orkshader://solid");
        _fxtechnique1x1 = _blit2screenmtl.technique("texcolor");
        _fxpMVP         = _blit2screenmtl.param("MatMVP");
        _fxpColorMap    = _blit2screenmtl.param("ColorMap");
      }
    }
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void _render_xxx(ForwardNode* node, CompositorDrawData& drawdata, const DrawableBuffer* DB, rtgroup_ptr_t rtg_main) {

    /////////////////////////////////////////////////////////////////////////////////////////
    RtGroupRenderTarget rt(rtg_main.get());

    auto RCFD = drawdata.RCFD();

    auto pbrcommon = _node->_pbrcommon;
    auto& ddprops  = drawdata._properties;

    auto context = RCFD->GetTarget();
    auto CIMPL = drawdata._cimpl;

    ///////////////////////////////////////////////////////////////////////////

    auto irenderer      = ddprops["irenderer"_crcu].get<lev2::IRenderer*>();
    int newwidth        = ddprops["OutputWidth"_crcu].get<int>();
    int newheight       = ddprops["OutputHeight"_crcu].get<int>();
    auto SCENE_MONOCAMS = ddprops["defcammtx"_crcu].get<const CameraMatrices*>();

    auto CPD   = CIMPL->topCPD();
    CPD._cameraMatrices = SCENE_MONOCAMS;
    CPD.assignLayers("depth_prepass,std_forward");
    CPD._clearColor    = pbrcommon->_clearColor;
    CPD._irendertarget = &rt;
    CPD.SetDstRect(context->mainSurfaceRectAtOrigin());
    CPD._width  = newwidth;
    CPD._height = newheight;

    ///////////////////////////////////////////////////////////////////////////
    // clear
    ///////////////////////////////////////////////////////////////////////////

    context->debugMarker(FormatString("ForwardPBR::preclear"));

    rtg_main->_autoclear = false;
    auto FBI     = context->FBI();
    FBI->SetAutoClear(false); // explicit clear
    FBI->PushRtGroup(rtg_main.get());
    FBI->Clear(pbrcommon->_clearColor, 1.0f);

    CIMPL->pushCPD(CPD);

    /////////////////////////////////////////////////////
    // Render Skybox first so AA can blend with it
    /////////////////////////////////////////////////////

    auto GBI     = context->GBI();

    context->debugPushGroup("ForwardPBR::skybox pass");
    rtg_main->_depthOnly = false;

    RCFD->_renderingmodel = "CUSTOM"_crcu;
    RenderContextInstData RCID(RCFD);
    RCID._pipeline_cache = _skybox_fxcache;
    auto pipeline        = _skybox_fxcache->findPipeline(RCID);
    pipeline->wrappedDrawCall(RCID, [GBI]() {
      GBI->render2dQuadEML(
          fvec4(-1, -1, 2, 2), //
          fvec4(0, 0, 1, 1),   //
          fvec4(0, 0, 1, 1),   //
          0.9999f);            // full screen quad
    });
    context->debugPopGroup();
    FBI->PopRtGroup();

    ///////////////////////////////////////////////////////////////////////////
    // depth prepass
    ///////////////////////////////////////////////////////////////////////////

    FBI->validateRtGroup(rtg_main);

    context->debugPushGroup("ForwardPBR::depth-pre pass");
    DB->enqueueLayerToRenderQueue("depth_prepass", irenderer);
    RCFD->_renderingmodel = "DEPTH_PREPASS"_crcu;

    FBI->PushRtGroup(rtg_main.get());

    irenderer->drawEnqueuedRenderables();
    FBI->PopRtGroup();
    context->debugPopGroup();

    FBI->cloneDepthBuffer(rtg_main, _rtg_depth_copy);

    ///////////////////////////////////////////////////////////////////////////
    // shadow passes
    ///////////////////////////////////////////////////////////////////////////

    if (1) {
      if (_enumeratedLights) {
        auto topcomp                  = RCFD->topCompositor();
        RCFD->_renderingmodel = "DEPTH_PREPASS"_crcu;
        int num_shadow_casters = 0;
        for (auto light : _enumeratedLights->_alllights) {
          if (not light->_castsShadows)
            continue;

          if (light->_depthRTG == nullptr) {
            int dim                      = light->_data->_shadowMapSize;
            light->_depthRTG             = std::make_shared<RtGroup>(context, dim, dim);
            light->_depthRTG->_depthOnly = true;
          }
          if (auto as_spotlight = dynamic_cast<SpotLight*>(light)) {

            CompositingPassData shadowCPD = CPD.clone();
            CameraMatrices SHADOWCAM;
            shadowCPD._cameraMatrices           = &SHADOWCAM;
            SHADOWCAM._pmatrix                  = as_spotlight->mProjectionMatrix;
            SHADOWCAM._vmatrix                  = as_spotlight->mViewMatrix;
            SHADOWCAM._vpmatrix                 = SHADOWCAM._vmatrix * SHADOWCAM._pmatrix;
            SHADOWCAM._ivpmatrix                = SHADOWCAM._vpmatrix.inverse();
            SHADOWCAM._ivmatrix                 = SHADOWCAM._vmatrix.inverse();
            SHADOWCAM._ipmatrix                 = SHADOWCAM._pmatrix.inverse();
            SHADOWCAM._frustum                  = as_spotlight->mWorldSpaceLightFrustum;
            SHADOWCAM._explicitProjectionMatrix = true;
            SHADOWCAM._explicitViewMatrix       = true;
            SHADOWCAM._aspectRatio              = 1.0f;

            FBI->validateRtGroup(light->_depthRTG);
            DB->enqueueLayerToRenderQueue("depth_prepass", irenderer);

            topcomp->pushCPD(shadowCPD);
            FBI->PushRtGroup(light->_depthRTG.get());

            irenderer->drawEnqueuedRenderables();

            FBI->PopRtGroup();
            topcomp->popCPD();
          }
          num_shadow_casters++;
        }
      }
    }

    ///////////////////////////////////////////////////////////////////////////
    // main color pass
    ///////////////////////////////////////////////////////////////////////////

    CPD._cameraMatrices = SCENE_MONOCAMS;

    rtg_main->_depthOnly = false;

    irenderer->resetQueue();

    RCFD->setUserProperty("enumeratedlights"_crcu, _enumeratedLights);
    RCFD->setUserProperty("DEPTH_MAP"_crcu, _rtg_depth_copy->_depthBuffer->_texture);

    FBI->PushRtGroup(rtg_main.get());
    context->debugMarker("ForwardPBR::renderEnqueuedScene::layer<std_forward>");
    DB->enqueueLayerToRenderQueue("std_forward", irenderer);

    RCFD->_renderingmodel = "FORWARD_PBR"_crcu;
    context->debugPushGroup("ForwardPBR::color pass");
    // irenderer->_debugLog = true;
    irenderer->drawEnqueuedRenderables();
    context->debugPopGroup();
    irenderer->resetQueue();
    FBI->PopRtGroup();


    /////////////////////////////////////////////////

    CIMPL->popCPD();
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void _render(ForwardNode* node, CompositorDrawData& drawdata) {
    EASY_BLOCK("pbr-_render");

    auto context      = drawdata.context();
    auto CIMPL        = drawdata._cimpl;
    auto  RCFD        = drawdata.RCFD();

    /////////////////////////////////////////////////
    // enumerate lights / PBR
    /////////////////////////////////////////////////

    if (auto lmgr = CIMPL->lightManager()) {
      EASY_BLOCK("lights-1");
      const auto TOPCPD = CIMPL->topCPD();
      lmgr->enumerateInPass(TOPCPD, _enumeratedLights);
    }

    //////////////////////////////////////////////////////
    // Resize RenderTargets
    //////////////////////////////////////////////////////

    auto& ddprops = drawdata._properties;
    int newwidth  = ddprops["OutputWidth"_crcu].get<int>();
    int newheight = ddprops["OutputHeight"_crcu].get<int>();

    uint64_t rtg_key = node->_bufferKey;
    auto rtg_main    = _rtgs_main->fetch(rtg_key);

    if (rtg_main->width() != newwidth or rtg_main->height() != newheight) {
      rtg_main->Resize(newwidth, newheight);
    }
    rtg_main->_autoclear = false;

    //////////////////////////////////////////////////////
    //////////////////////////////////////////////////////
    //////////////////////////////////////////////////////
    context->debugPushGroup("ForwardPBR::render");
    {
      if (auto DB = RCFD->GetDB()) {

        RCFD->_pbrcommon = _node->_pbrcommon;

        _render_xxx(node, drawdata, DB, rtg_main);

        if (_rtgs_resolve_msaa) {
          auto FBI = context->FBI();
          FBI->msaaBlit(rtg_main, _rtgs_resolve_msaa->fetch(rtg_key));
        }
      }
    }
    context->debugPopGroup();
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ForwardNode* _node;
  std::string _camname;
  enumeratedlights_ptr_t _enumeratedLights;

  rtgset_ptr_t _rtgs_main;
  rtgset_ptr_t _rtgs_resolve_msaa;
  rtgroup_ptr_t _rtg_depth_copy;
  fmtx4 _viewOffsetMatrix;
  pbrmaterial_ptr_t _skybox_material;
  fxpipelinecache_constptr_t _skybox_fxcache;

  FreestyleMaterial _blit2screenmtl;
  const FxShaderTechnique* _fxtechnique1x1;
  const FxShaderParam* _fxpMVP;
  const FxShaderParam* _fxpColorMap;

}; // IMPL

///////////////////////////////////////////////////////////////////////////////
ForwardNode::ForwardNode() {
  _impl           = std::make_shared<ForwardPbrNodeImpl>(this);
  _renderingmodel = RenderingModel("FORWARD_PBR"_crcu);
  _pbrcommon      = std::make_shared<pbr::CommonStuff>();
}
///////////////////////////////////////////////////////////////////////////////
ForwardNode::~ForwardNode() {
}
///////////////////////////////////////////////////////////////////////////////
void ForwardNode::doGpuInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.get<std::shared_ptr<ForwardPbrNodeImpl>>()->init(pTARG, iW, iH);
}
///////////////////////////////////////////////////////////////////////////////
void ForwardNode::DoRender(CompositorDrawData& drawdata) {
  auto impl = _impl.get<std::shared_ptr<ForwardPbrNodeImpl>>();
  impl->_render(this, drawdata);
}
///////////////////////////////////////////////////////////////////////////////
rtgroup_ptr_t ForwardNode::GetOutputGroup() const {
  auto fwd_impl   = _impl.get<std::shared_ptr<ForwardPbrNodeImpl>>();
  auto rtg_output = fwd_impl->_rtgs_main->fetch(_bufferKey);
  if (fwd_impl->_rtgs_resolve_msaa) {
    rtg_output = fwd_impl->_rtgs_resolve_msaa->fetch(_bufferKey);
  }
  return rtg_output;
}
///////////////////////////////////////////////////////////////////////////////
rtbuffer_ptr_t ForwardNode::GetOutput() const {
  return GetOutputGroup()->GetMrt(0);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::pbr
