////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
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
      , _layername("All") { //
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ~ForwardPbrNodeImpl() {
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void init(lev2::Context* context) {
    if (nullptr == _rtg) {
      _rtg             = std::make_shared<RtGroup>(context, 8, 8, _ginitdata->_msaa_samples);
      auto buf1        = _rtg->createRenderTarget(lev2::EBufferFormat::RGBA8);
      buf1->_debugName = "ForwardRt0";
      //_rtg_donly = _rtg->clone();
      //_rtg_donly->_depthOnly = true;
      _skybox_material = std::make_shared<PBRMaterial>(context);
      _skybox_fxinstlut = _skybox_material->createSkyboxFxInstLut();
      _enumeratedLights = std::make_shared<EnumeratedLights>();
    }
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void _render(CompositorDrawData& drawdata) {
    EASY_BLOCK("pbr-_render");

    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();

    auto pbrcommon = _node->_pbrcommon;
    auto& ddprops  = drawdata._properties;
    auto VD        = drawdata.computeViewData();
    bool is_stereo = VD._isStereo;

    auto targ         = RCFD.GetTarget();
    auto CIMPL        = drawdata._cimpl;
    auto FBI          = targ->FBI();
    auto GBI          = targ->GBI();
    auto this_buf     = FBI->GetThisBuffer();
    auto RSI          = targ->RSI();
    auto DWI          = targ->DWI();
    const auto TOPCPD = CIMPL->topCPD();
    auto tgt_rect     = targ->mainSurfaceRectAtOrigin();


    auto irenderer = ddprops["irenderer"_crcu].get<lev2::IRenderer*>();
    int newwidth   = ddprops["OutputWidth"_crcu].get<int>();
    int newheight  = ddprops["OutputHeight"_crcu].get<int>();

    //////////////////////////////////////////////////////
    // Resize RenderTargets
    //////////////////////////////////////////////////////
    if (_rtg->width() != newwidth or _rtg->height() != newheight) {
      _rtg->Resize(newwidth, newheight);
      //_rtg_donly->Resize(newwidth, newheight);
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
    _rtg->_autoclear = false;
    targ->debugPushGroup("ForwardPBR::render");
    RtGroupRenderTarget rt(_rtg.get());
    {
      FBI->SetAutoClear(false); // explicit clear
      targ->beginFrame();
      /////////////////////////////////////////////////////////////////////////////////////////
      auto DB             = RCFD.GetDB();
      auto CPD            = CIMPL->topCPD();
      CPD._clearColor     = pbrcommon->_clearColor;
      CPD._layerName      = _layername;
      CPD._irendertarget  = &rt;
      CPD._cameraMatrices = ddprops["defcammtx"_crcu].get<const CameraMatrices*>();
      CPD.SetDstRect(tgt_rect);
      ///////////////////////////////////////////////////////////////////////////
      if (DB) {

        ///////////////////////////////////////////////////////////////////////////
        // clear
        ///////////////////////////////////////////////////////////////////////////

        //_rtg->_depthOnly = true;
        targ->debugMarker(FormatString("ForwardPBR::preclear"));
        FBI->PushRtGroup(_rtg.get());
        CIMPL->pushCPD(CPD);
        auto MTXI = targ->MTXI();
        FBI->Clear(pbrcommon->_clearColor, 1.0f);


        /////////////////////////////////////////////////////
        // Depth Prepass
        /////////////////////////////////////////////////////

        //targ->debugPushGroup("ForwardPBR::depth-pre pass");
        //RCFD._renderingmodel = RenderingModel("DEPTH_PREPASS"_crcu);
        //irenderer->drawEnqueuedRenderables();
        //targ->debugPopGroup();
        //FBI->PopRtGroup();

        /////////////////////////////////////////////////////
        // Render Skybox first so AA can blend with it
        /////////////////////////////////////////////////////

        //FBI->PushRtGroup(_rtg.get());

        targ->debugPushGroup("ForwardPBR::skybox pass");
        _rtg->_depthOnly = false;

        RCFD._renderingmodel = "CUSTOM"_crcu;
        RenderContextInstData RCID(&RCFD);
        RCID._fx_instance_lut = _skybox_fxinstlut;
        auto fxinst = _skybox_fxinstlut->findfxinst(RCID);
        fxinst->wrappedDrawCall(RCID,[GBI](){
          GBI->render2dQuadEML(fvec4(-1, -1, 2, 2), //
                               fvec4(0, 0, 1, 1), //
                               fvec4(0, 0, 1, 1), //
                               0.9999f); // full screen quad
        });
        targ->debugPopGroup();

        ///////////////////////////////////////////////////////////////////////////

        //auto& pointlights_untextured = _enumeratedLights._untexturedpointlights;

        RCFD.setUserProperty("enumeratedlights"_crcu,_enumeratedLights);
        
        for (const auto& layer_name : CPD.getLayerNames()) {
          targ->debugMarker(FormatString("ForwardPBR::renderEnqueuedScene::layer<%s>", layer_name.c_str()));
          DB->enqueueLayerToRenderQueue(layer_name, irenderer);
        }

        RCFD._renderingmodel = "FORWARD_PBR"_crcu;
        targ->debugPushGroup("ForwardPBR::color pass");
        irenderer->drawEnqueuedRenderables();
        framerenderer.renderMisc();
        targ->debugPopGroup();
        irenderer->resetQueue();

        FBI->PopRtGroup();

        /////////////////////////////////////////////////

        CIMPL->popCPD();


      }
      /////////////////////////////////////////////////////////////////////////////////////////
      targ->endFrame();
    }
    targ->debugPopGroup();
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ForwardNode* _node;
  std::string _camname, _layername;
  enumeratedlights_ptr_t _enumeratedLights;
  rtgroup_ptr_t _rtg;
  rtgroup_ptr_t _rtg_donly;
  fmtx4 _viewOffsetMatrix;
  pbrmaterial_ptr_t _skybox_material;
  fxinstancelut_ptr_t _skybox_fxinstlut;

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
  _impl.get<std::shared_ptr<ForwardPbrNodeImpl>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void ForwardNode::DoRender(CompositorDrawData& drawdata) {
  auto impl = _impl.get<std::shared_ptr<ForwardPbrNodeImpl>>();
  impl->_render(drawdata);
}
///////////////////////////////////////////////////////////////////////////////
rtbuffer_ptr_t ForwardNode::GetOutput() const {
  return _impl.get<std::shared_ptr<ForwardPbrNodeImpl>>()->_rtg->GetMrt(0);
}
///////////////////////////////////////////////////////////////////////////////
rtgroup_ptr_t ForwardNode::GetOutputGroup() const {
  return _impl.get<std::shared_ptr<ForwardPbrNodeImpl>>()->_rtg;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::pbr
