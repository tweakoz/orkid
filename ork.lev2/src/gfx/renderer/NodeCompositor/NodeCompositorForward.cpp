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

#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorForward.h>

ImplementReflectionX(ork::lev2::ForwardCompositingNode, "ForwardCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void ForwardCompositingNode::describeX(class_t* c) {
  c->directProperty("ClearColor", &ForwardCompositingNode::_clearColor);
}
///////////////////////////////////////////////////////////////////////////
constexpr int NUMSAMPLES = 1;
///////////////////////////////////////////////////////////////////////////////
namespace forwardnode {
struct IMPL {
  ///////////////////////////////////////
  IMPL()
      : _camname("Camera") {
    _layername = "All";
  }
  ///////////////////////////////////////
  ~IMPL() {
  }
  ///////////////////////////////////////
  void init(lev2::Context* pTARG) {
    pTARG->debugPushGroup("Forward::rendeinitr");
    if (nullptr == _rtg) {
      _material.gpuInit(pTARG);
      _rtg             = new RtGroup(pTARG, 8, 8, NUMSAMPLES);
      auto buf1        = new RtBuffer(lev2::RtgSlot::Slot0, lev2::EBufferFormat::RGBA32F, 8, 8);
      auto buf2        = new RtBuffer(lev2::RtgSlot::Slot1, lev2::EBufferFormat::RGBA32F, 8, 8);
      buf1->_debugName = "ForwardRt0";
      buf2->_debugName = "ForwardRt1";
      _rtg->SetMrt(0, buf1);
      _rtg->SetMrt(1, buf2);
    }
    pTARG->debugPopGroup();
  }
  ///////////////////////////////////////
  void _render(ForwardCompositingNode* node, CompositorDrawData& drawdata) {
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
    int newwidth  = ddprops["OutputWidth"_crcu].Get<int>();
    int newheight = ddprops["OutputHeight"_crcu].Get<int>();
    if (_rtg->width() != newwidth or _rtg->height() != newheight) {
      _rtg->Resize(newwidth, newheight);
    }
    //////////////////////////////////////////////////////
    auto irenderer = ddprops["irenderer"_crcu].Get<lev2::IRenderer*>();
    //////////////////////////////////////////////////////
    targ->debugPushGroup("Forward::render");
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
      CPD._cameraMatrices = ddprops["defcammtx"_crcu].Get<const CameraMatrices*>();
      CPD.SetDstRect(tgt_rect);
      ///////////////////////////////////////////////////////////////////////////
      if (DB) {
        ///////////////////////////////////////////////////////////////////////////
        // DrawableBuffer -> RenderQueue enqueue
        ///////////////////////////////////////////////////////////////////////////
        for (const auto& layer_name : CPD.getLayerNames()) {
          targ->debugMarker(FormatString("Forward::renderEnqueuedScene::layer<%s>", layer_name.c_str()));
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
};
} // namespace forwardnode

///////////////////////////////////////////////////////////////////////////////
ForwardCompositingNode::ForwardCompositingNode() {
  _impl = std::make_shared<forwardnode::IMPL>();
}
///////////////////////////////////////////////////////////////////////////////
ForwardCompositingNode::~ForwardCompositingNode() {
}
///////////////////////////////////////////////////////////////////////////////
void ForwardCompositingNode::doGpuInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.Get<std::shared_ptr<forwardnode::IMPL>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void ForwardCompositingNode::DoRender(CompositorDrawData& drawdata) {
  auto impl = _impl.Get<std::shared_ptr<forwardnode::IMPL>>();
  impl->_render(this, drawdata);
}
///////////////////////////////////////////////////////////////////////////////
RtBuffer* ForwardCompositingNode::GetOutput() const {
  return _impl.Get<std::shared_ptr<forwardnode::IMPL>>()->_rtg->GetMrt(0);
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
