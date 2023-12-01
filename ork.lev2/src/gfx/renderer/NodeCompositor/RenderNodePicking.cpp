////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>

#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorPicking.h>

ImplementReflectionX(ork::lev2::PickingCompositingNode, "PickingCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void PickingCompositingNode::describeX(class_t* c) {
  c->directProperty("ClearColor", &PickingCompositingNode::_clearColor);
}
///////////////////////////////////////////////////////////////////////////////
namespace picking {
struct IMPL {
  ///////////////////////////////////////
  IMPL()
      : _camname("Camera") {
    _layername = "All";
    _width     = 8;
    _height    = 8;
  }
  ///////////////////////////////////////
  ~IMPL() {
  }
  ///////////////////////////////////////
  void gpuInit(lev2::Context* pTARG) {
    pTARG->debugPushGroup("Picking::rendeinitr");
    if (nullptr == _rtg) {
      _material.gpuInit(pTARG);
      _rtg                 = std::make_shared<RtGroup>(pTARG, _width, _height, MsaaSamples::MSAA_1X);
      auto buf_id          = _rtg->createRenderTarget(EBufferFormat::RGBA32F);
      auto buf_wpos        = _rtg->createRenderTarget(EBufferFormat::RGBA32F);
      auto buf_wnrm        = _rtg->createRenderTarget(EBufferFormat::RGBA32F);
      buf_id->_debugName   = "rt0-pickid";
      buf_wpos->_debugName = "rt0-wpos";
      buf_wnrm->_debugName = "rt0-wnrm";
      _rtg->_name = "PickingRtGroup";
    }
    pTARG->debugPopGroup();
    _initted = true;
  }
  ///////////////////////////////////////
  void _render(PickingCompositingNode* node, CompositorDrawData& drawdata) {
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto& ddprops                = drawdata._properties;

    auto context                    = RCFD.GetTarget();
    auto CIMPL                   = drawdata._cimpl;
    auto FBI                     = context->FBI();
    auto this_buf                = FBI->GetThisBuffer();
    auto RSI                     = context->RSI();
    const auto TOPCPD            = CIMPL->topCPD();
    auto tgt_rect                = context->mainSurfaceRectAtOrigin();
    //////////////////////////////////////////////////////
    // Resize RenderTargets
    //////////////////////////////////////////////////////
    if (_rtg->width() != _width or _rtg->height() != _height) {
      _rtg->Resize(_width, _height);
    }
    //////////////////////////////////////////////////////
    auto irenderer = ddprops["irenderer"_crcu].get<lev2::IRenderer*>();
    //////////////////////////////////////////////////////
    context->debugPushGroup("Picking::render");
    RtGroupRenderTarget rt(_rtg.get());
    {
      //targ->triggerFrameDebugCapture();
      FBI->SetAutoClear(false); // explicit clear
      /////////////////////////////////////////////////////////////////////////////////////////
      auto DB             = RCFD.GetDB();
      auto CPD            = CIMPL->topCPD();
      CPD._clearColor     = node->_clearColor;
      CPD._irendertarget  = &rt;
      CPD._ispicking      = true;
      CPD._cameraMatrices = ddprops["defcammtx"_crcu].get<const CameraMatrices*>();
      CPD.SetDstRect(tgt_rect);
      CPD.assignLayers(_layername);
      RCFD._renderingmodel = "PICKING"_crcu;
      auto& userprops = RCFD.userProperties();
      auto it_pfc = userprops.find("pixel_fetch_context"_crc);
      if(it_pfc == userprops.end()){
        auto pfc = std::make_shared<PixelFetchContext>(3);
        RCFD.setUserProperty("pixel_fetch_context"_crc, pfc);
        RCFD.setUserProperty("pickbufferMvpMatrix"_crc, std::make_shared<fmtx4>());
      }
      ///////////////////////////////////////////////////////////////////////////
      if (DB) {

        ///////////////////////////////////////////////////////////////////////////
        // enqueue renderables to DB
        ///////////////////////////////////////////////////////////////////////////

        for (const auto& layer_name : CPD.getLayerNames()) {
          context->debugMarker(FormatString("Picking::renderEnqueuedScene::layer<%s>", layer_name.c_str()));
          DB->enqueueLayerToRenderQueue(layer_name, irenderer);
        }

        ///////////////////////////////////////////////////////////////////////////
        // clear
        ///////////////////////////////////////////////////////////////////////////

        FBI->PushRtGroup(_rtg.get());
        FBI->Clear(fvec3(0,0,0), 1.0f);

        /////////////////////////////////////////////////
        // render enqueued
        /////////////////////////////////////////////////
        auto MTXI = context->MTXI();
        CIMPL->pushCPD(CPD);
        context->debugPushGroup("rnodePicking::drawEnqueuedRenderables");
        irenderer->drawEnqueuedRenderables();
        framerenderer.renderMisc();
        context->debugPopGroup();
        CIMPL->popCPD();
        FBI->PopRtGroup();
        irenderer->resetQueue();
      }
      /////////////////////////////////////////////////////////////////////////////////////////
    }
    context->debugPopGroup();
  }
  ///////////////////////////////////////
  std::string _camname, _layername;
  CompositingMaterial _material;
  rtgroup_ptr_t _rtg;
  fmtx4 _viewOffsetMatrix;
  int _width, _height;
  bool _initted = false;
};
} // namespace picking

///////////////////////////////////////////////////////////////////////////////
PickingCompositingNode::PickingCompositingNode() {
  _impl = std::make_shared<picking::IMPL>();
}
///////////////////////////////////////////////////////////////////////////////
PickingCompositingNode::~PickingCompositingNode() {
}
///////////////////////////////////////////////////////////////////////////////
void PickingCompositingNode::resize(int w, int h) {
  auto impl     = _impl.get<std::shared_ptr<picking::IMPL>>();
  impl->_width  = w;
  impl->_height = h;
}
///////////////////////////////////////////////////////////////////////////////
void PickingCompositingNode::doGpuInit(lev2::Context* context, int iW, int iH) {
  auto impl = _impl.get<std::shared_ptr<picking::IMPL>>();
  if (not impl->_initted) {
    resize(iW, iH);
    impl->gpuInit(context);
  }
}
///////////////////////////////////////////////////////////////////////////////
void PickingCompositingNode::DoRender(CompositorDrawData& drawdata) {
  auto impl = _impl.get<std::shared_ptr<picking::IMPL>>();
  impl->_render(this, drawdata);
}
///////////////////////////////////////////////////////////////////////////////
rtbuffer_ptr_t PickingCompositingNode::GetOutput() const {
  return _impl.get<std::shared_ptr<picking::IMPL>>()->_rtg->GetMrt(0);
}
///////////////////////////////////////////////////////////////////////////////
rtgroup_ptr_t PickingCompositingNode::GetOutputGroup() const {
  return _impl.get<std::shared_ptr<picking::IMPL>>()->_rtg;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
