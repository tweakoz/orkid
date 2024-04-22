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
struct ForwardPass {
  ForwardNode* _node            = nullptr;
  CompositorDrawData* _drawdata = nullptr;
  const DrawableBuffer* _DB     = nullptr;
  std::string _fwd_pass_layer = "std_forward";
  std::string _dpp_pass_layer = "depth_prepass";
  rtgroup_ptr_t _rtg_out;
  rtgroup_ptr_t _rtg_depth_copy;
  bool _renderingPROBE = false;
};
using forward_pass_ptr_t = std::shared_ptr<ForwardPass>;
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

      _rtg_main_depth_copy  = std::make_shared<RtGroup>(context, 8, 8);
      _rtg_cube1_depth_copy = std::make_shared<RtGroup>(context, 8, 8);

      auto pbrcommon = _node->_pbrcommon;

      EBufferFormat efmt = EBufferFormat::RGBA8;
      if (pbrcommon->_useFloatColorBuffer) {
        efmt = EBufferFormat::RGBA32F;
      }

      auto e_msaa = intToMsaaEnum(_ginitdata->_msaa_samples);
      _rtgs_main  = std::make_shared<RtgSet>(context, e_msaa, "rtgs-main");
      _rtgs_main->addBuffer("ForwardRt0", efmt);

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
        _rtgs_resolve_msaa->addBuffer("MsaaDownsampleBuffer", efmt);
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
  void _render_xxx(forward_pass_ptr_t fpass) {

    auto node     = fpass->_node;
    auto drawdata = fpass->_drawdata;
    auto DB       = fpass->_DB;
    auto rtg_out  = fpass->_rtg_out;

    /////////////////////////////////////////////////////////////////////////////////////////

    RtGroupRenderTarget rt(rtg_out.get());

    auto RCFD    = drawdata->RCFD();
    auto context = drawdata->context();
    auto FBI     = context->FBI();
    auto GBI     = context->GBI();
    auto irenderer = drawdata->property("irenderer"_crcu).get<lev2::IRenderer*>();

    auto CIMPL   = drawdata->_cimpl;
    auto CPD = CIMPL->topCPD();

    auto pbrcommon = _node->_pbrcommon;
    bool renderingPROBE = fpass->_renderingPROBE;

    ///////////////////////////////////////////////////////////////////////////
    // setup global RCFD state for PBR materials
    ///////////////////////////////////////////////////////////////////////////

    bool have_probes = (_enumeratedLights->_lightprobes.size() > 0);

    RCFD->setUserProperty("enumeratedlights"_crcu, _enumeratedLights);
    RCFD->setUserProperty("renderingPROBE"_crcu, renderingPROBE);
    RCFD->setUserProperty("havePROBES"_crcu, have_probes);

    ///////////////////////////////////////////////////////////////////////////
    // Render Skybox first so MSAA can blend with it
    ///////////////////////////////////////////////////////////////////////////

    context->debugPushGroup("ForwardPBR::skybox pass");

    rtg_out->_depthOnly = false;
    rtg_out->_autoclear = true;
    rtg_out->_clearMaskDepth = true;
    rtg_out->_clearMaskColor = true;

    RCFD->_renderingmodel = "CUSTOM"_crcu;
    RenderContextInstData RCID(RCFD);
    RCID._pipeline_cache = _skybox_fxcache;
    auto pipeline        = _skybox_fxcache->findPipeline(RCID);
    FBI->PushRtGroup(rtg_out.get());
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

    if (pbrcommon->_useDepthPrepass) {
      FBI->validateRtGroup(rtg_out);
      context->debugPushGroup("ForwardPBR::depth-pre pass");
      DB->enqueueLayerToRenderQueue(fpass->_dpp_pass_layer, irenderer);
      RCFD->_renderingmodel = "DEPTH_PREPASS"_crcu;

      rtg_out->_autoclear = true;
      rtg_out->_depthOnly = true;
      rtg_out->_clearMaskDepth = true;
      rtg_out->_clearMaskColor = false;
      FBI->PushRtGroup(rtg_out.get());

      irenderer->drawEnqueuedRenderables(true);
      FBI->PopRtGroup();
      context->debugPopGroup();

      FBI->cloneDepthBuffer(rtg_out, fpass->_rtg_depth_copy);
    }

    ///////////////////////////////////////////////////////////////////////////
    // main color pass
    ///////////////////////////////////////////////////////////////////////////

    CPD._cameraMatrices = drawdata->property("defcammtx"_crcu).get<const CameraMatrices*>();

    if (pbrcommon->_useDepthPrepass) {
      RCFD->setUserProperty("DEPTH_MAP"_crcu, fpass->_rtg_depth_copy->_depthBuffer->_texture);
    }

    context->debugMarker("ForwardPBR::renderEnqueuedScene::layer<std_forward>");
    DB->enqueueLayerToRenderQueue(fpass->_fwd_pass_layer, irenderer);

    RCFD->_renderingmodel = "FORWARD_PBR"_crcu;
    context->debugPushGroup("ForwardPBR::color pass");
    // irenderer->_debugLog = true;
    rtg_out->_autoclear = false;
    rtg_out->_depthOnly = false;
    rtg_out->_clearMaskDepth = false; // not clearing anyway ...
    rtg_out->_clearMaskColor = false; // not clearing anyway ...
    FBI->PushRtGroup(rtg_out.get());
    irenderer->drawEnqueuedRenderables(true);
    context->debugPopGroup();

    FBI->PopRtGroup();
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void _render(ForwardNode* node, CompositorDrawData& drawdata) {
    EASY_BLOCK("pbr-_render");

    auto context = drawdata.context();
    auto FBI     = context->FBI();
    auto TXI     = context->TXI();
    auto CIMPL   = drawdata._cimpl;
    auto RCFD    = drawdata.RCFD();
    auto topcomp = RCFD->topCompositor();

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

    int newwidth  = drawdata.property("OutputWidth"_crcu).get<int>();
    int newheight = drawdata.property("OutputHeight"_crcu).get<int>();

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

        auto CPD            = CIMPL->topCPD();
        CPD._cameraMatrices = drawdata.property("defcammtx"_crcu).get<const CameraMatrices*>();
        CPD.assignLayers("depth_prepass,std_forward,probe,depth_probe");
        CPD._clearColor = _node->_pbrcommon->_clearColor;
        RtGroupRenderTarget rt(rtg_main.get());
        CPD._irendertarget = &rt;
        CPD.SetDstRect(context->mainSurfaceRectAtOrigin());
        CPD._width  = newwidth;
        CPD._height = newheight;

        context->debugMarker(FormatString("ForwardPBR::preclear"));

        rtg_main->_autoclear = false;
        FBI->SetAutoClear(false); // explicit clear
        FBI->PushRtGroup(rtg_main.get());
        FBI->Clear(_node->_pbrcommon->_clearColor, 1.0f);
        FBI->PopRtGroup();

        CIMPL->pushCPD(CPD);

        ////////////////////////////
        // shadow passes
        //  these only need to be done once per final-frame
        ////////////////////////////

        if (1) {
          if (_enumeratedLights) {
            RCFD->_renderingmodel  = "DEPTH_PREPASS"_crcu;
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
                auto irenderer = drawdata.property("irenderer"_crcu).get<lev2::IRenderer*>();
                DB->enqueueLayerToRenderQueue("depth_prepass", irenderer);

                topcomp->pushCPD(shadowCPD);
                FBI->PushRtGroup(light->_depthRTG.get());

                irenderer->drawEnqueuedRenderables(true);

                FBI->PopRtGroup();
                topcomp->popCPD();
              }
              num_shadow_casters++;
            }
          }
        }

        /////////////////////////////////////////////////
        // update reflection probes
        /////////////////////////////////////////////////

        for (auto probe : _enumeratedLights->_lightprobes) {
          switch (probe->_type) {
            case LightProbeType::REFLECTION:
              if (nullptr == probe->_cubeRenderRTG) {
                probe->_cubeRenderRTG = std::make_shared<RtGroup>(context, 8, 8);
                probe->_cubeRenderRTG->_name = "ReflectionProbeRTG";
                auto colorbuf = probe->_cubeRenderRTG->createRenderTarget(EBufferFormat::RGBA8);
                colorbuf->_debugName = "ReflectionProbeColorCubeMap";
                probe->_cubeRenderRTG->_cubeMap = true;
                
              }
              if( probe->_dirty ){
                int prevW = probe->_cubeRenderRTG->width();
                int prevH = probe->_cubeRenderRTG->height();
                if (prevW != probe->_dim or prevH != probe->_dim) {
                  probe->_cubeRenderRTG->Resize(probe->_dim, probe->_dim);
                }

                auto CMATRIX = probe->_worldMatrix;

                fvec3 POSX = CMATRIX.xNormal()*-1;
                fvec3 POSY = CMATRIX.yNormal();
                fvec3 POSZ = CMATRIX.zNormal()*-1;

                fvec3 position = CMATRIX.translation();

                CompositingPassData cubemapCPD = CPD.clone();
                CameraMatrices CUBECAM;
                // compute projection matrix
                CUBECAM._pmatrix.perspective(90.0f*DTOR, 1.0f, 0.01f, 1000.0f);

                // flip y on projection matrix
                fmtx4 flipy;
                flipy.setScale(1,-1,1);
                CUBECAM._pmatrix = flipy * CUBECAM._pmatrix;



                for( int iface=0; iface<6; iface++ ){

                  context->debugPushGroup(FormatString("ForwardPBR::cubemap pass<%d>",iface));


                  // compute view matrices from cubeface and CMATRIX
                  //  face 0 = POSX
                  //  face 1 = NEGX
                  //  face 2 = POSY
                  //  face 3 = NEGY
                  //  face 4 = POSZ
                  //  face 5 = NEGZ

                  switch( iface ){
                    case 1: CUBECAM._vmatrix.lookAt( position, position + POSX, POSY ); break;
                    case 0: CUBECAM._vmatrix.lookAt( position, position - POSX, POSY ); break;
                    case 2: CUBECAM._vmatrix.lookAt( position, position + POSY, POSZ*-1 ); break;
                    case 3: CUBECAM._vmatrix.lookAt( position, position - POSY, POSZ ); break;
                    case 4: CUBECAM._vmatrix.lookAt( position, position + POSZ, POSY ); break;
                    case 5: CUBECAM._vmatrix.lookAt( position, position - POSZ, POSY ); break;
                  }


                  CUBECAM._vpmatrix                 = CUBECAM._vmatrix * CUBECAM._pmatrix;
                  CUBECAM._ivpmatrix                = CUBECAM._vpmatrix.inverse();
                  CUBECAM._ivmatrix                 = CUBECAM._vmatrix.inverse();
                  CUBECAM._ipmatrix                 = CUBECAM._pmatrix.inverse();
                  CUBECAM._frustum.set(CUBECAM._vmatrix,CUBECAM._pmatrix);
                  CUBECAM._explicitProjectionMatrix = true;
                  CUBECAM._explicitViewMatrix       = true;
                  CUBECAM._aspectRatio              = 1.0f;

                  auto probe_pass             = std::make_shared<ForwardPass>();
                  probe_pass->_node           = node;
                  probe_pass->_drawdata       = &drawdata;
                  probe_pass->_DB             = DB;
                  probe_pass->_rtg_out        = probe->_cubeRenderRTG;
                  probe_pass->_rtg_depth_copy = _rtg_cube1_depth_copy;
                  probe_pass->_renderingPROBE  = true;
                  probe_pass->_fwd_pass_layer = "probe";
                  probe->_cubeRenderRTG->_cubeRenderFace = iface;

                  cubemapCPD._cameraMatrices        = &CUBECAM;
                  topcomp->pushCPD(cubemapCPD);
                  _render_xxx(probe_pass);
                  topcomp->popCPD();

                  context->debugPopGroup();

                }

                probe->_cubeTexture = probe->_cubeRenderRTG->GetMrt(0)->_texture;
                TXI->generateMipMaps(probe->_cubeTexture.get());
                probe->_dirty = false;
              }
              break;
          }
        }

        ////////////////////////////
        // main pass
        ////////////////////////////

        context->debugPushGroup("ForwardPBR::MAIN RTG PASS");

        auto main_fwd_pass             = std::make_shared<ForwardPass>();
        main_fwd_pass->_node           = node;
        main_fwd_pass->_drawdata       = &drawdata;
        main_fwd_pass->_DB             = DB;
        main_fwd_pass->_rtg_out        = rtg_main;
        main_fwd_pass->_rtg_depth_copy = _rtg_main_depth_copy;
        main_fwd_pass->_renderingPROBE  = false;

        _render_xxx(main_fwd_pass);

        CIMPL->popCPD();

        context->debugPopGroup();

        ////////////////////////////
        // resolve msaa
        ////////////////////////////

        if (_rtgs_resolve_msaa) {
          context->debugPushGroup("ForwardPBR::MSAA RESOLVE");
          auto FBI = context->FBI();
          FBI->msaaBlit(rtg_main, _rtgs_resolve_msaa->fetch(rtg_key));
          context->debugPopGroup();
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
  rtgroup_ptr_t _rtg_main_depth_copy;
  rtgroup_ptr_t _rtg_cube1_depth_copy;
  rtgset_ptr_t _rtgs_resolve_msaa;
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
