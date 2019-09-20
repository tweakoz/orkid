////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "NodeCompositorForward.h"

#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>

ImplementReflectionX(ork::lev2::ForwardCompositingNode, "ForwardCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void ForwardCompositingNode::describeX(class_t* c) {
  c->memberProperty("Layer",&ForwardCompositingNode::_layername);
  c->memberProperty("ClearColor",&ForwardCompositingNode::_clearColor);
}
///////////////////////////////////////////////////////////////////////////
constexpr int NUMSAMPLES = 1;
struct ForwardTechnique final : public FrameTechniqueBase {
  //////////////////////////////////////////////////////////////////////////////
  ForwardTechnique(int w, int h)
      : FrameTechniqueBase(w, h)
      , _rtg(nullptr) {}
  //////////////////////////////////////////////////////////////////////////////
  void DoInit(GfxTarget* pTARG) final {
    if (nullptr == _rtg) {
      _rtg = new RtGroup(pTARG, miW, miH, NUMSAMPLES);
      auto buf = new RtBuffer(_rtg, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA32, miW, miH);
      buf->_debugName = FormatString("ForwardCompositingNode::output");
      _rtg->SetMrt(0,buf);
      _effect.PostInit(pTARG, "orkshader://framefx", "frameeffect_standard");
    }
  }
  //////////////////////////////////////////////////////////////////////////////
  void render(FrameRenderer& renderer,
              CompositorDrawData& drawdata,
              ForwardCompositingNode& node ) {
    RenderContextFrameData& framedata = renderer.framedata();
    GfxTarget* pTARG                  = framedata.GetTarget();

    SRect tgt_rect(0, 0, miW, miH);

    CompositingPassData _CPD;

    _CPD.mbDrawSource = true;
    _CPD.mpFrameTek   = this;
    _CPD.mpCameraName = nullptr;
    _CPD.mpLayerName  = &node._layername;
    _CPD._clearColor  = fvec4(0.61, 0.61, 0.75, 1);
    //_CPD._impl.Set<const CameraData*>(lcam);

    //////////////////////////////////////////////////////
    pTARG->FBI()->SetAutoClear(false);
    // clear will occur via _CPD
    //////////////////////////////////////////////////////

    RtGroupRenderTarget rt(_rtg);
    drawdata.mCompositingGroupStack.push(_CPD);
    pTARG->debugPushGroup("ForwardCompositingNode::render");
    {
      pTARG->SetRenderContextFrameData(&framedata);
      framedata.SetDstRect(tgt_rect);
      framedata.PushRenderTarget(&rt);
      pTARG->FBI()->PushRtGroup(_rtg);
      pTARG->BeginFrame();
      framedata.SetRenderingMode(RenderContextFrameData::ERENDMODE_STANDARD);
      renderer.Render();
      pTARG->EndFrame();
      pTARG->FBI()->PopRtGroup();
      framedata.PopRenderTarget();
      pTARG->SetRenderContextFrameData(nullptr);
      drawdata.mCompositingGroupStack.pop();
    }
    pTARG->debugPopGroup();
  }

  RtGroup* _rtg;
  BuiltinFrameEffectMaterial _effect;
  fmtx4 _viewOffsetMatrix;
};

///////////////////////////////////////////////////////////////////////////////
namespace forwardnode {
struct IMPL {
  ///////////////////////////////////////
  IMPL(ForwardCompositingNode*node)
      : _node(node)
      , _camname("Camera"_pool){}
  ///////////////////////////////////////
  ~IMPL() {
    if (_frametek){
      delete _frametek;
      _frametek = nullptr;
    }
  }
  ///////////////////////////////////////
  void init(lev2::GfxTarget* pTARG) {
    _material.Init(pTARG);
    _frametek = new ForwardTechnique(pTARG->GetW(),pTARG->GetH());
    _frametek->Init(pTARG);
  }
  ///////////////////////////////////////
  void _render(CompositorDrawData& drawdata) {
    FrameRenderer& the_renderer       = drawdata.mFrameRenderer;
    RenderContextFrameData& framedata = the_renderer.framedata();
    auto targ                         = framedata.GetTarget();
    auto onode = drawdata._properties["final"_crcu].Get<const OutputCompositingNode*>();
    if (_frametek) {
      framedata.setLayerName(_node->_layername.c_str());
      _frametek->render(the_renderer, drawdata,*_node);
    }
  }
  ///////////////////////////////////////
  PoolString _camname;
  CompositingMaterial _material;
  ForwardTechnique* _frametek = nullptr;
  ForwardCompositingNode* _node;
};
typedef std::shared_ptr<IMPL> implptr_t;
} //namespace forwardnode {

///////////////////////////////////////////////////////////////////////////////
ForwardCompositingNode::ForwardCompositingNode() : _layername("All"_pool) { _impl = std::make_shared<forwardnode::IMPL>(this); }
///////////////////////////////////////////////////////////////////////////////
ForwardCompositingNode::~ForwardCompositingNode() {
  _impl = nullptr;
}
///////////////////////////////////////////////////////////////////////////////
void ForwardCompositingNode::DoInit(lev2::GfxTarget* pTARG, int iW, int iH)
{ auto impl = _impl.Get<forwardnode::implptr_t>();
  if (nullptr == impl->_frametek) {
    impl->init(pTARG);
  }
}
///////////////////////////////////////////////////////////////////////////////
void ForwardCompositingNode::DoRender(CompositorDrawData& drawdata) {
  _impl.Get<forwardnode::implptr_t>()->_render(drawdata);
}
///////////////////////////////////////////////////////////////////////////////
RtGroup* ForwardCompositingNode::GetOutput() const {
  auto impl = _impl.Get<forwardnode::implptr_t>();
  if (impl->_frametek)
    return impl->_frametek->_rtg;
  else
    return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
