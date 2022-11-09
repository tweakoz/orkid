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
namespace ork::lev2::pbr {
///////////////////////////////////////////////////////////////////////////////
void ForwardNode::describeX(class_t* c) {
}
///////////////////////////////////////////////////////////////////////////
constexpr int NUMSAMPLES = 1;
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
      _material.gpuInit(context);
      _rtg             = std::make_shared<RtGroup>(context, 8, 8, NUMSAMPLES);
      auto buf1        = _rtg->createRenderTarget(lev2::EBufferFormat::RGBA32F);
      auto buf2        = _rtg->createRenderTarget(lev2::EBufferFormat::RGBA32F);
      buf1->_debugName = "ForwardRt0";
      buf2->_debugName = "ForwardRt1";
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
    auto this_buf     = FBI->GetThisBuffer();
    auto RSI          = targ->RSI();
    auto DWI          = targ->DWI();
    const auto TOPCPD = CIMPL->topCPD();
    auto tgt_rect     = targ->mainSurfaceRectAtOrigin();


    auto irenderer = ddprops["irenderer"_crcu].get<lev2::IRenderer*>();
    int newwidth   = ddprops["OutputWidth"_crcu].get<int>();
    int newheight  = ddprops["OutputHeight"_crcu].get<int>();

    RCFD._pbrcommon = pbrcommon;

    /////////////////////////////////////////////////
    // enumerate lights
    /////////////////////////////////////////////////

    if (auto lmgr = CIMPL->lightManager()) {
      EASY_BLOCK("lights-1");
      lmgr->enumerateInPass(TOPCPD, _enumeratedLights);
      auto& lights = _enumeratedLights._enumeratedLights;
      //printf("got lights<%zu>\n", lights.size());
    }

    RCFD._renderingmodel = _node->_renderingmodel;

    //////////////////////////////////////////////////////
    // Resize RenderTargets
    //////////////////////////////////////////////////////
    if (_rtg->width() != newwidth or _rtg->height() != newheight) {
      _rtg->Resize(newwidth, newheight);
    }
    //////////////////////////////////////////////////////
    //////////////////////////////////////////////////////
    //////////////////////////////////////////////////////
    targ->debugPushGroup("ForwardPBR::render");
    RtGroupRenderTarget rt(_rtg.get());
    {
      targ->FBI()->PushRtGroup(_rtg.get());
      targ->FBI()->SetAutoClear(false); // explicit clear
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
        // DrawableBuffer -> RenderQueue enqueue
        ///////////////////////////////////////////////////////////////////////////
        for (const auto& layer_name : CPD.getLayerNames()) {
          targ->debugMarker(FormatString("ForwardPBR::renderEnqueuedScene::layer<%s>", layer_name.c_str()));
          DB->enqueueLayerToRenderQueue(layer_name, irenderer);
        }
        /////////////////////////////////////////////////
        auto MTXI = targ->MTXI();
        CIMPL->pushCPD(CPD);
        targ->debugPushGroup("toolvp::DrawEnqRenderables");
        targ->FBI()->Clear(pbrcommon->_clearColor, 1.0f);
        irenderer->drawEnqueuedRenderables();
        framerenderer.renderMisc();
        targ->debugPopGroup();
        CIMPL->popCPD();
      }
      /////////////////////////////////////////////////////////////////////////////////////////
      targ->endFrame();
      targ->FBI()->PopRtGroup();
    }
    targ->debugPopGroup();
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ForwardNode* _node;
  std::string _camname, _layername;
  EnumeratedLights _enumeratedLights;
  CompositingMaterial _material;
  rtgroup_ptr_t _rtg;
  fmtx4 _viewOffsetMatrix;

}; // IMPL

///////////////////////////////////////////////////////////////////////////////
ForwardNode::ForwardNode() {
  _impl           = std::make_shared<ForwardPbrNodeImpl>(this);
  _renderingmodel = RenderingModel(ERenderModelID::FORWARD_PBR);
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
