////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "NodeCompositorDeferred.h"

#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>

ImplementReflectionX(ork::lev2::DeferredCompositingNode, "DeferredCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNode::describeX(class_t* c) { c->memberProperty("ClearColor", &DeferredCompositingNode::_clearColor); }
///////////////////////////////////////////////////////////////////////////
constexpr int NUMSAMPLES = 1;
///////////////////////////////////////////////////////////////////////////////
namespace deferrednode {
struct IMPL {
  ///////////////////////////////////////
  IMPL()
      : _camname(AddPooledString("Camera")) {
    _layername = "All"_pool;
  }
  ///////////////////////////////////////
  ~IMPL() {}
  ///////////////////////////////////////
  void init(lev2::GfxTarget* pTARG) {
    pTARG->debugPushGroup("Deferred::rendeinitr");
    if (nullptr == _rtg) {
      _material.Init(pTARG);
      _rtg            = new RtGroup(pTARG, 8, 8, NUMSAMPLES);
      auto buf        = new RtBuffer(_rtg, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA32, 8, 8 );
      buf->_debugName = "DeferredRt";
      _rtg->SetMrt(0, buf);
      _effect.PostInit(pTARG, "orkshader://framefx", "frameeffect_standard");
    }
    pTARG->debugPopGroup();
  }
  ///////////////////////////////////////
  void _render(DeferredCompositingNode* node, CompositorDrawData& drawdata) {
    FrameRenderer& framerenderer       = drawdata.mFrameRenderer;
    RenderContextFrameData& framedata = framerenderer.framedata();
    auto targ                         = framedata.GetTarget();
    auto onode                        = drawdata._properties["final"_crcu].Get<const OutputCompositingNode*>();
    SRect tgt_rect(0, 0, targ->GetW(), targ->GetH());

    //////////////////////////////////////////////////////
    // Resize RenderTargets
    //////////////////////////////////////////////////////

    int newwidth = drawdata._properties["OutputWidth"_crcu].Get<int>();
    int newheight = drawdata._properties["OutputHeight"_crcu].Get<int>();

    if( _rtg->GetW()!=newwidth or _rtg->GetH()!=newheight ){
      _rtg->Resize(newwidth,newheight);
    }

    //////////////////////////////////////////////////////

    _CPD.mbDrawSource = true;
    _CPD.mpFrameTek   = nullptr;
    _CPD.mpCameraName = nullptr;
    _CPD.mpLayerName  = &_layername;
    _CPD._clearColor  = node->_clearColor;
    //_CPD._impl.Set<const CameraData*>(lcam);

    //////////////////////////////////////////////////////
    targ->FBI()->SetAutoClear(false);
    // clear will occur via _CPD
    //////////////////////////////////////////////////////

    auto outerRT = framedata.GetRenderTarget();

    targ->debugPushGroup("Deferred::render");

    RtGroupRenderTarget rt(_rtg);
    drawdata.mCompositingGroupStack.push(_CPD);
    {
      targ->SetRenderContextFrameData(&framedata);
      framedata.SetDstRect(tgt_rect);
      framedata.PushRenderTarget(&rt);
      targ->FBI()->PushRtGroup(_rtg);
      targ->BeginFrame();
      framedata.SetRenderingMode(RenderContextFrameData::ERENDMODE_STANDARD);
      framerenderer.Render();
      targ->EndFrame();
      targ->FBI()->PopRtGroup();
      framedata.PopRenderTarget();
      targ->SetRenderContextFrameData(nullptr);
      drawdata.mCompositingGroupStack.pop();
    }
    targ->debugPopGroup();
  }
  ///////////////////////////////////////
  PoolString _camname, _layername;
  CompositingMaterial _material;
  RtGroup* _rtg = nullptr;
  BuiltinFrameEffectMaterial _effect;
  CompositingPassData _CPD;
  fmtx4 _viewOffsetMatrix;
};
} // namespace deferrednode

///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNode::DeferredCompositingNode() { _impl = std::make_shared<deferrednode::IMPL>(); }
///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNode::~DeferredCompositingNode() {}
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNode::DoInit(lev2::GfxTarget* pTARG, int iW, int iH) {
  _impl.Get<std::shared_ptr<deferrednode::IMPL>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNode::DoRender(CompositorDrawData& drawdata) {
  auto impl = _impl.Get<std::shared_ptr<deferrednode::IMPL>>();
  impl->_render(this, drawdata);
}
///////////////////////////////////////////////////////////////////////////////
RtGroup* DeferredCompositingNode::GetOutput() const { return _impl.Get<std::shared_ptr<deferrednode::IMPL>>()->_rtg; }
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
