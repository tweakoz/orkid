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
      auto lbuf = new RtBuffer(_rtg, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA32, miW, miH);
      _rtg->SetMrt(0, lbuf);
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

    framedata.setStereoOnePass(false);
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
      , _camname("Camera"_pool)
      , _layers("All"_pool) {}
  ///////////////////////////////////////
  ~IMPL() {
    assert(false);
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
  void _render(FrameRenderer& renderer, CompositorDrawData& drawdata) {
    _frametek->render(renderer, drawdata,*_node);
  }
  ///////////////////////////////////////
  PoolString _camname, _layers;
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
  assert(false);
  _impl = nullptr;
}
///////////////////////////////////////////////////////////////////////////////
void ForwardCompositingNode::DoInit(lev2::GfxTarget* pTARG, int iW, int iH) // virtual
{
  auto impl = _impl.Get<forwardnode::implptr_t>();
  if (nullptr == impl->_frametek) {
    impl->init(pTARG);
  }
}
///////////////////////////////////////////////////////////////////////////////
void ForwardCompositingNode::DoRender(CompositorDrawData& drawdata, CompositingImpl* cimpl) // virtual
{
  FrameRenderer& the_renderer       = drawdata.mFrameRenderer;
  RenderContextFrameData& framedata = the_renderer.framedata();
  auto targ                         = framedata.GetTarget();
  auto impl                         = _impl.Get<forwardnode::implptr_t>();
  impl->_layers = _layername;
  if (impl->_frametek) {
    framedata.setLayerName(_layername.c_str());
    impl->_render(the_renderer, drawdata);
  }
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
