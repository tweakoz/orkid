////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "NodeCompositorScaleBias.h"

#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>

ImplementReflectionX(ork::lev2::ScaleBiasCompositingNode, "ScaleBiasCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void ScaleBiasCompositingNode::describeX(class_t* c) {
}
///////////////////////////////////////////////////////////////////////////
constexpr int NUMSAMPLES = 1;
///////////////////////////////////////////////////////////////////////////////
namespace scaleandbias {
struct IMPL {
  ///////////////////////////////////////
  IMPL(ScaleBiasCompositingNode*node)
      : _node(node) {}
  ///////////////////////////////////////
  ~IMPL() {
  }
  ///////////////////////////////////////
  void init(lev2::GfxTarget* pTARG) {
    if (nullptr == _rtg) {
      int w = pTARG->GetW();
      int h = pTARG->GetH();
      _rtg = new RtGroup(pTARG, w, h, NUMSAMPLES);
      auto buf = new RtBuffer(_rtg, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA32, w, h);
      buf->_debugName = FormatString("ScaleBiasCompositingNode::output");
      _rtg->SetMrt(0,buf);
      _material.Init(pTARG);
    }
  }
  ///////////////////////////////////////
  void _render(CompositorDrawData& drawdata) {
    GfxTarget* target                  = drawdata.target();
    //////////////////////////////////////////////////////
    target->FBI()->SetAutoClear(false);
    //////////////////////////////////////////////////////

    target->debugPushGroup("ScaleBiasCompositingNode::render");
    RtGroupRenderTarget rt(_rtg);
    {
      target->FBI()->PushRtGroup(_rtg);
      target->BeginFrame();
      //framedata.SetRenderingMode(RenderContextFrameData::ERENDMODE_STANDARD);
      //renderer.Render();
      target->EndFrame();
      target->FBI()->PopRtGroup();
    }
    target->debugPopGroup();
  }
  ///////////////////////////////////////
  CompositingMaterial _material;
  ScaleBiasCompositingNode* _node = nullptr;
  RtGroup* _rtg = nullptr;
};
} //namespace scaleandbias {
///////////////////////////////////////////////////////////////////////////////
ScaleBiasCompositingNode::ScaleBiasCompositingNode() { _impl = std::make_shared<scaleandbias::IMPL>(this); }
///////////////////////////////////////////////////////////////////////////////
ScaleBiasCompositingNode::~ScaleBiasCompositingNode() {}
///////////////////////////////////////////////////////////////////////////////
void ScaleBiasCompositingNode::DoInit(lev2::GfxTarget* pTARG, int iW, int iH) // virtual
{ _impl.Get<std::shared_ptr<scaleandbias::IMPL>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void ScaleBiasCompositingNode::DoRender(CompositorDrawData& drawdata) // virtual
{ _impl.Get<std::shared_ptr<scaleandbias::IMPL>>()->_render(drawdata);
}
///////////////////////////////////////////////////////////////////////////////
RtGroup* ScaleBiasCompositingNode::GetOutput() const {
  auto impl = _impl.Get<std::shared_ptr<scaleandbias::IMPL>>();
  return (impl->_rtg) ? impl->_rtg : nullptr;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
