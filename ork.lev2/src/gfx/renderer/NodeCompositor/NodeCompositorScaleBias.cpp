////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScaleBias.h>

#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/pch.h>
#include <ork/reflect/properties/register.h>

ImplementReflectionX(ork::lev2::ScaleBiasCompositingNode, "ScaleBiasCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void ScaleBiasCompositingNode::describeX(class_t* c) {
}
///////////////////////////////////////////////////////////////////////////////
namespace scaleandbias {
struct IMPL {
  ///////////////////////////////////////
  IMPL(ScaleBiasCompositingNode* node)
      : _node(node) {
  }
  ///////////////////////////////////////
  ~IMPL() {
  }
  ///////////////////////////////////////
  void init(lev2::Context* pTARG) {
    if (nullptr == _rtg) {
      int w           = pTARG->mainSurfaceWidth();
      int h           = pTARG->mainSurfaceHeight();
      _rtg            = new RtGroup(pTARG, w, h, lev2::MsaaSamples::MSAA_1X);
      auto buf        = _rtg->createRenderTarget(lev2::EBufferFormat::RGBA8);
      buf->_debugName = FormatString("ScaleBiasCompositingNode::output");
      _material.gpuInit(pTARG);
    }
  }
  ///////////////////////////////////////
  void _render(CompositorDrawData& drawdata) {
    Context* target = drawdata.context();
    //////////////////////////////////////////////////////
    target->FBI()->SetAutoClear(false);
    //////////////////////////////////////////////////////

    target->debugPushGroup("ScaleBiasCompositingNode::render");
    RtGroupRenderTarget rt(_rtg);
    {
      target->FBI()->PushRtGroup(_rtg);
      target->beginFrame();
      // framedata.SetRenderingMode(RenderContextFrameData::ERENDMODE_STANDARD);
      // renderer.Render();
      target->endFrame();
      target->FBI()->PopRtGroup();
    }
    target->debugPopGroup();
  }
  ///////////////////////////////////////
  CompositingMaterial _material;
  ScaleBiasCompositingNode* _node = nullptr;
  RtGroup* _rtg                   = nullptr;
};
} // namespace scaleandbias
///////////////////////////////////////////////////////////////////////////////
ScaleBiasCompositingNode::ScaleBiasCompositingNode() {
  _impl = std::make_shared<scaleandbias::IMPL>(this);
}
///////////////////////////////////////////////////////////////////////////////
ScaleBiasCompositingNode::~ScaleBiasCompositingNode() {
}
///////////////////////////////////////////////////////////////////////////////
void ScaleBiasCompositingNode::doGpuInit(lev2::Context* pTARG, int iW, int iH) // virtual
{
  _impl.get<std::shared_ptr<scaleandbias::IMPL>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void ScaleBiasCompositingNode::DoRender(CompositorDrawData& drawdata) // virtual
{
  _impl.get<std::shared_ptr<scaleandbias::IMPL>>()->_render(drawdata);
}
///////////////////////////////////////////////////////////////////////////////
rtbuffer_ptr_t ScaleBiasCompositingNode::GetOutput() const {
  auto impl = _impl.get<std::shared_ptr<scaleandbias::IMPL>>();
  return (impl->_rtg) ? impl->_rtg->GetMrt(0) : nullptr;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
