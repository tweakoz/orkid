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

#include "NodeCompositorForward.h"

ImplementReflectionX(ork::lev2::ForwardCompositingNode, "ForwardCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void ForwardCompositingNode::describeX(class_t* c) {
  c->memberProperty("ClearColor", &ForwardCompositingNode::_clearColor);
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
    pTARG->debugPushGroup("Forward::rendeinitr");
    if (nullptr == _rtg) {
      _material.Init(pTARG);
      _rtg            = new RtGroup(pTARG, 8, 8, NUMSAMPLES);
      auto buf        = new RtBuffer(lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA8, 8, 8);
      buf->_debugName = "ForwardRt";
      _rtg->SetMrt(0, buf);
      _effect.PostInit(pTARG, "orkshader://framefx", "frameeffect_standard");
    }
    pTARG->debugPopGroup();
  }
  ///////////////////////////////////////
  void _render(ForwardCompositingNode* node, CompositorDrawData& drawdata) {
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto CIMPL                   = drawdata._cimpl;
    auto targ                    = RCFD.GetTarget();
    auto& ddprops                = drawdata._properties;
    SRect tgt_rect               = targ->mainSurfaceRectAtOrigin();
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
    targ->debugPushGroup("Forward::render");
    RtGroupRenderTarget rt(_rtg);
    {
      targ->FBI()->PushRtGroup(_rtg);
      targ->FBI()->SetAutoClear(false); // explicit clear
      targ->beginFrame();
      /////////////////////////////////////////////////////////////////////////////////////////
      auto DB            = RCFD.GetDB();
      auto CPD           = CIMPL->topCPD();
      CPD._clearColor    = node->_clearColor;
      CPD.mpLayerName    = &_layername;
      CPD._irendertarget = &rt;
      CPD.SetDstRect(tgt_rect);
      ///////////////////////////////////////////////////////////////////////////
      if (DB) {
        ///////////////////////////////////////////////////////////////////////////
        // DrawableBuffer -> RenderQueue enqueue
        ///////////////////////////////////////////////////////////////////////////
        for (const PoolString& layer_name : CPD.getLayerNames()) {
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
  PoolString _camname, _layername;
  CompositingMaterial _material;
  RtGroup* _rtg = nullptr;
  BuiltinFrameEffectMaterial _effect;
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
void ForwardCompositingNode::DoInit(lev2::Context* pTARG, int iW, int iH) {
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
