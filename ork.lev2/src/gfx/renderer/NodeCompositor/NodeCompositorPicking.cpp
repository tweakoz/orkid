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
#include <ork/reflect/RegisterProperty.h>

#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorPicking.h>

ImplementReflectionX(ork::lev2::PickingCompositingNode, "PickingCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void PickingCompositingNode::describeX(class_t* c) {
  c->memberProperty("ClearColor", &PickingCompositingNode::_clearColor);
}
///////////////////////////////////////////////////////////////////////////
constexpr int NUMSAMPLES = 1;
///////////////////////////////////////////////////////////////////////////////
namespace forwardnode {
struct IMPL {
  ///////////////////////////////////////
  IMPL()
      : _camname(AddPooledString("Camera")) {
    _layername = "All"_pool;
  }
  ///////////////////////////////////////
  ~IMPL() {
  }
  ///////////////////////////////////////
  void init(lev2::Context* pTARG) {
    pTARG->debugPushGroup("Picking::rendeinitr");
    if (nullptr == _rtg) {
      _material.Init(pTARG);
      _rtg             = new RtGroup(pTARG, 8, 8, NUMSAMPLES);
      auto buf1        = new RtBuffer(lev2::ERTGSLOT0, lev2::EBUFFMT_RGBA32F, 8, 8);
      auto buf2        = new RtBuffer(lev2::ERTGSLOT1, lev2::EBUFFMT_RGBA32F, 8, 8);
      buf1->_debugName = "PickingRt0";
      buf2->_debugName = "PickingRt1";
      _rtg->SetMrt(0, buf1);
      _rtg->SetMrt(1, buf2);
    }
    pTARG->debugPopGroup();
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
    int newwidth  = ddprops["OutputWidth"_crcu].Get<int>();
    int newheight = ddprops["OutputHeight"_crcu].Get<int>();
    if (_rtg->GetW() != newwidth or _rtg->GetH() != newheight) {
      _rtg->Resize(newwidth, newheight);
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
      CPD.mpLayerName     = &_layername;
      CPD._irendertarget  = &rt;
      CPD._ispicking      = true;
      CPD._cameraMatrices = ddprops["defcammtx"_crcu].Get<const CameraMatrices*>();
      CPD.SetDstRect(tgt_rect);
      ///////////////////////////////////////////////////////////////////////////
      if (DB) {
        ///////////////////////////////////////////////////////////////////////////
        // DrawableBuffer -> RenderQueue enqueue
        ///////////////////////////////////////////////////////////////////////////
        for (const PoolString& layer_name : CPD.getLayerNames()) {
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
  PoolString _camname, _layername;
  CompositingMaterial _material;
  RtGroup* _rtg = nullptr;
  fmtx4 _viewOffsetMatrix;
};
} // namespace forwardnode

///////////////////////////////////////////////////////////////////////////////
PickingCompositingNode::PickingCompositingNode() {
  _impl = std::make_shared<forwardnode::IMPL>();
}
///////////////////////////////////////////////////////////////////////////////
PickingCompositingNode::~PickingCompositingNode() {
}
///////////////////////////////////////////////////////////////////////////////
void PickingCompositingNode::DoInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.Get<std::shared_ptr<forwardnode::IMPL>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void PickingCompositingNode::DoRender(CompositorDrawData& drawdata) {
  auto impl = _impl.Get<std::shared_ptr<forwardnode::IMPL>>();
  impl->_render(this, drawdata);
}
///////////////////////////////////////////////////////////////////////////////
RtBuffer* PickingCompositingNode::GetOutput() const {
  return _impl.Get<std::shared_ptr<forwardnode::IMPL>>()->_rtg->GetMrt(0);
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
