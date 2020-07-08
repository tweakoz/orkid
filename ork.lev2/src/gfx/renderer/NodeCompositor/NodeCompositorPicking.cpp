////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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
///////////////////////////////////////////////////////////////////////////
constexpr int NUMSAMPLES = 1;
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
      _rtg             = new RtGroup(pTARG, _width, _height, NUMSAMPLES);
      auto buf1        = new RtBuffer(lev2::RtgSlot::Slot0, lev2::EBufferFormat::RGBA16UI, _width, _height);
      auto buf2        = new RtBuffer(lev2::RtgSlot::Slot1, lev2::EBufferFormat::RGBA32F, _width, _height);
      buf1->_debugName = "PickingRt0";
      buf2->_debugName = "PickingRt1";
      _rtg->SetMrt(0, buf1);
      _rtg->SetMrt(1, buf2);
    }
    pTARG->debugPopGroup();
    _initted = true;
  }
  ///////////////////////////////////////
  void _render(PickingCompositingNode* node, CompositorDrawData& drawdata) {
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto targ                    = RCFD.GetTarget();
    auto CIMPL                   = drawdata._cimpl;
    auto FBI                     = targ->FBI();
    auto this_buf                = FBI->GetThisBuffer();
    auto RSI                     = targ->RSI();
    const auto TOPCPD            = CIMPL->topCPD();
    auto tgt_rect                = targ->mainSurfaceRectAtOrigin();
    auto& ddprops                = drawdata._properties;
    //////////////////////////////////////////////////////
    // Resize RenderTargets
    //////////////////////////////////////////////////////
    if (_rtg->width() != _width or _rtg->height() != _height) {
      _rtg->Resize(_width, _height);
    }
    //////////////////////////////////////////////////////
    auto irenderer = ddprops["irenderer"_crcu].Get<lev2::IRenderer*>();
    //////////////////////////////////////////////////////
    targ->debugPushGroup("Picking::render");
    RtGroupRenderTarget rt(_rtg);
    {
      targ->FBI()->PushRtGroup(_rtg);
      targ->FBI()->SetAutoClear(false); // explicit clear
      targ->beginFrame();
      /////////////////////////////////////////////////////////////////////////////////////////
      auto DB             = RCFD.GetDB();
      auto CPD            = CIMPL->topCPD();
      CPD._clearColor     = node->_clearColor;
      CPD._layerName      = _layername;
      CPD._irendertarget  = &rt;
      CPD._ispicking      = true;
      CPD._cameraMatrices = ddprops["defcammtx"_crcu].Get<const CameraMatrices*>();
      CPD.SetDstRect(tgt_rect);
      ///////////////////////////////////////////////////////////////////////////
      if (DB) {
        ///////////////////////////////////////////////////////////////////////////
        // DrawableBuffer -> RenderQueue enqueue
        ///////////////////////////////////////////////////////////////////////////
        for (const auto& layer_name : CPD.getLayerNames()) {
          targ->debugMarker(FormatString("Picking::renderEnqueuedScene::layer<%s>", layer_name.c_str()));
          DB->enqueueLayerToRenderQueue(layer_name, irenderer);
        }
        /////////////////////////////////////////////////
        auto MTXI = targ->MTXI();
        CIMPL->pushCPD(CPD);
        targ->debugPushGroup("toolvp::DrawEnqRenderables");
        targ->FBI()->Clear(node->_clearColor, 1.0f);
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
  ///////////////////////////////////////
  std::string _camname, _layername;
  CompositingMaterial _material;
  RtGroup* _rtg = nullptr;
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
  auto impl     = _impl.Get<std::shared_ptr<picking::IMPL>>();
  impl->_width  = w;
  impl->_height = h;
}
///////////////////////////////////////////////////////////////////////////////
void PickingCompositingNode::doGpuInit(lev2::Context* context, int iW, int iH) {
  auto impl = _impl.Get<std::shared_ptr<picking::IMPL>>();
  if (not impl->_initted) {
    resize(iW, iH);
    impl->gpuInit(context);
  }
}
///////////////////////////////////////////////////////////////////////////////
void PickingCompositingNode::DoRender(CompositorDrawData& drawdata) {
  auto impl = _impl.Get<std::shared_ptr<picking::IMPL>>();
  impl->_render(this, drawdata);
}
///////////////////////////////////////////////////////////////////////////////
RtBuffer* PickingCompositingNode::GetOutput() const {
  return _impl.Get<std::shared_ptr<picking::IMPL>>()->_rtg->GetMrt(0);
}
///////////////////////////////////////////////////////////////////////////////
RtGroup* PickingCompositingNode::GetOutputGroup() const {
  return _impl.Get<std::shared_ptr<picking::IMPL>>()->_rtg;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
