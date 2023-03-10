////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

namespace ork::lev2{
extern appinitdata_ptr_t _ginitdata;
} // namespace ork::lev2{

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
      , _camname("Camera")
      , _layername("None") { //
      
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ~ForwardPbrNodeImpl() {
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void init(lev2::Context* context, int iw, int ih) {

    if (nullptr == _rtgs_main) {

      _rtgs_main = std::make_shared<RtgSet>(context,intToMsaaEnum(_ginitdata->_msaa_samples));
      _rtgs_main->addBuffer("ForwardRt0", EBufferFormat::RGBA8);

      _layername = _node->_layers;

      printf( "PBRFWD_MSAA<%d>\n", int(_ginitdata->_msaa_samples) );
      //_rtg             = std::make_shared<RtGroup>(context, 8, 8, intToMsaaEnum(_ginitdata->_msaa_samples));
      //auto buf1        = _rtg->createRenderTarget(EBufferFormat::RGBA8);
      //buf1->_debugName = "ForwardRt0";
      _skybox_material = std::make_shared<PBRMaterial>(context);
      _skybox_material->_variant = "skybox.forward"_crcu;
      _skybox_fxcache = _skybox_material->pipelineCache();
      _enumeratedLights = std::make_shared<EnumeratedLights>();

      if(_ginitdata->_msaa_samples>1){
        _rtgs_resolve_msaa = std::make_shared<RtgSet>(context,MsaaSamples::MSAA_1X);
        _rtgs_resolve_msaa->addBuffer("MsaaDownsampleBuffer",EBufferFormat::RGBA8);
        //_rtg_resolve_msaa = std::make_shared<RtGroup>(context, 8, 8, MsaaSamples::MSAA_1X);
        //auto dsbuf        = _rtg_resolve_msaa->createRenderTarget(EBufferFormat::RGBA8);
        //dsbuf->_debugName = "MsaaDownsampleBuffer";
        _blit2screenmtl.gpuInit(context, "orkshader://solid");
        _fxtechnique1x1 = _blit2screenmtl.technique("texcolor");
        _fxpMVP         = _blit2screenmtl.param("MatMVP");
        _fxpColorMap    = _blit2screenmtl.param("ColorMap");
      }

    }
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void _render(ForwardNode* node, CompositorDrawData& drawdata) {
    EASY_BLOCK("pbr-_render");

    uint64_t rtg_key = node->_bufferKey;
    auto rtg_main = _rtgs_main->fetch(rtg_key);

    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();

    auto pbrcommon = _node->_pbrcommon;
    auto& ddprops  = drawdata._properties;
    auto VD        = drawdata.computeViewData();
    bool is_stereo = VD._isStereo;

    auto context         = RCFD.GetTarget();
    auto CIMPL        = drawdata._cimpl;
    auto FBI          = context->FBI();
    auto GBI          = context->GBI();
    auto this_buf     = FBI->GetThisBuffer();
    auto RSI          = context->RSI();
    auto DWI          = context->DWI();
    const auto TOPCPD = CIMPL->topCPD();
    auto tgt_rect     = context->mainSurfaceRectAtOrigin();


    auto irenderer = ddprops["irenderer"_crcu].get<lev2::IRenderer*>();
    int newwidth   = ddprops["OutputWidth"_crcu].get<int>();
    int newheight  = ddprops["OutputHeight"_crcu].get<int>();

    //////////////////////////////////////////////////////
    // Resize RenderTargets
    //////////////////////////////////////////////////////
    if (rtg_main->width() != newwidth or rtg_main->height() != newheight) {
      rtg_main->Resize(newwidth, newheight);
    }

    /////////////////////////////////////////////////
    // enumerate lights / PBR
    /////////////////////////////////////////////////

    if (auto lmgr = CIMPL->lightManager()) {
      EASY_BLOCK("lights-1");
      lmgr->enumerateInPass(TOPCPD, _enumeratedLights);
    }

    RCFD._pbrcommon = pbrcommon;

    //////////////////////////////////////////////////////
    //////////////////////////////////////////////////////
    //////////////////////////////////////////////////////
    rtg_main->_autoclear = false;
    context->debugPushGroup("ForwardPBR::render");
    RtGroupRenderTarget rt(rtg_main.get());
    {
      FBI->SetAutoClear(false); // explicit clear
      /////////////////////////////////////////////////////////////////////////////////////////
      auto DB             = RCFD.GetDB();
      auto CPD            = CIMPL->topCPD();
      CPD.assignLayers(_layername);
      CPD._clearColor     = pbrcommon->_clearColor;
      CPD._irendertarget  = &rt;
      CPD._cameraMatrices = ddprops["defcammtx"_crcu].get<const CameraMatrices*>();
      CPD.SetDstRect(tgt_rect);
      CPD._width = newwidth;
      CPD._height = newheight;
      ///////////////////////////////////////////////////////////////////////////
      if (DB) {

        ///////////////////////////////////////////////////////////////////////////
        // clear
        ///////////////////////////////////////////////////////////////////////////

        context->debugMarker(FormatString("ForwardPBR::preclear"));
        FBI->PushRtGroup(rtg_main.get());
        CIMPL->pushCPD(CPD);
        auto MTXI = context->MTXI();
        FBI->Clear(pbrcommon->_clearColor, 1.0f);

        /////////////////////////////////////////////////////
        // Render Skybox first so AA can blend with it
        /////////////////////////////////////////////////////

        context->debugPushGroup("ForwardPBR::skybox pass");
        rtg_main->_depthOnly = false;

        RCFD._renderingmodel = "CUSTOM"_crcu;
        RenderContextInstData RCID(&RCFD);
        RCID._pipeline_cache = _skybox_fxcache;
        auto pipeline = _skybox_fxcache->findPipeline(RCID);
        pipeline->wrappedDrawCall(RCID,[GBI](){
          GBI->render2dQuadEML(fvec4(-1, -1, 2, 2), //
                               fvec4(0, 0, 1, 1), //
                               fvec4(0, 0, 1, 1), //
                               0.9999f); // full screen quad
        });
        context->debugPopGroup();

        ///////////////////////////////////////////////////////////////////////////

        RCFD.setUserProperty("enumeratedlights"_crcu,_enumeratedLights);
        
        //printf( "BEG FWD PBR ENQ\n");
        for (const auto& layer_name : CPD.getLayerNames()) {
          context->debugMarker(FormatString("ForwardPBR::renderEnqueuedScene::layer<%s>", layer_name.c_str()));
          DB->enqueueLayerToRenderQueue(layer_name, irenderer);
        }
        //printf( "END FWD PBR ENQ\n");

        RCFD._renderingmodel = "FORWARD_PBR"_crcu;
        context->debugPushGroup("ForwardPBR::color pass");
        irenderer->drawEnqueuedRenderables();
        framerenderer.renderMisc();
        context->debugPopGroup();
        irenderer->resetQueue();

        FBI->PopRtGroup();

        /////////////////////////////////////////////////

        CIMPL->popCPD();

        if(_rtgs_resolve_msaa){
          FBI->msaaBlit(rtg_main,_rtgs_resolve_msaa->fetch(rtg_key));
        }
      }
    }
    context->debugPopGroup();
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ForwardNode* _node;
  std::string _camname, _layername;
  enumeratedlights_ptr_t _enumeratedLights;

  rtgset_ptr_t _rtgs_main;
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
  _impl.get<std::shared_ptr<ForwardPbrNodeImpl>>()->init(pTARG,iW,iH);
}
///////////////////////////////////////////////////////////////////////////////
void ForwardNode::DoRender(CompositorDrawData& drawdata) {
  auto impl = _impl.get<std::shared_ptr<ForwardPbrNodeImpl>>();
  impl->_render(this, drawdata);
}
///////////////////////////////////////////////////////////////////////////////
rtgroup_ptr_t ForwardNode::GetOutputGroup() const {
  auto fwd_impl = _impl.get<std::shared_ptr<ForwardPbrNodeImpl>>();
  auto rtg_output = fwd_impl->_rtgs_main->fetch(_bufferKey);
  if(fwd_impl->_rtgs_resolve_msaa){
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
