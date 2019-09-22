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
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto targ                         = RCFD.GetTarget();
    auto& ddprops                       = drawdata._properties;
    //auto onode                        = ddprops["final"_crcu].Get<const OutputCompositingNode*>();
    SRect tgt_rect(0, 0, targ->GetW(), targ->GetH());

    //////////////////////////////////////////////////////
    // Resize RenderTargets
    //////////////////////////////////////////////////////

    int newwidth = ddprops["OutputWidth"_crcu].Get<int>();
    int newheight = ddprops["OutputHeight"_crcu].Get<int>();
    if( _rtg->GetW()!=newwidth or _rtg->GetH()!=newheight ){
      _rtg->Resize(newwidth,newheight);
    }

    //////////////////////////////////////////////////////

    int primarycamindex = ddprops["primarycamindex"_crcu].Get<int>();
    int cullcamindex = ddprops["cullcamindex"_crcu].Get<int>();
    auto irenderer = ddprops["irenderer"_crcu].Get<lev2::IRenderer*>();
    //////////////////////////////////////////////////////


    //////////////////////////////////////////////////////
    targ->FBI()->SetAutoClear(false);
    // clear will occur via _CPD
    //////////////////////////////////////////////////////

    auto outerRT = RCFD.GetRenderTarget();

    targ->debugPushGroup("Deferred::render");

    RtGroupRenderTarget rt(_rtg);
    {
      targ->BeginFrame();
      targ->SetRenderContextFrameData(&RCFD);
      RCFD.SetDstRect(tgt_rect);
      RCFD.PushRenderTarget(&rt);
      targ->FBI()->PushRtGroup(_rtg);
      RCFD.SetRenderingMode(RenderContextFrameData::ERENDMODE_STANDARD);
      /////////////////////////////////////////////////////////////////////////////////////////
      auto DB = RCFD.GetDB();
      auto CPD = CompositingPassData::FromRCFD(RCFD);
      CPD._clearColor = node->_clearColor;
      CPD.mpLayerName  = &_layername;
      auto CAMDAT   = CPD.getCamera(RCFD, primarycamindex, cullcamindex);
      auto& CAMCCTX = RCFD.GetCameraCalcCtx();
      ///////////////////////////////////////////////////////////////////////////
      targ->debugMarker(FormatString("Deferred::CAMDAT<%p>", CAMDAT));
      if (CAMDAT and DB) {
        _tempcamdat = *CAMDAT;
        _tempcamdat.BindGfxTarget(targ);
        _tempcamdat.CalcCameraData(CAMCCTX);
        RCFD.SetCameraData(&_tempcamdat);
        ///////////////////////////////////////////////////////////////////////////
        _tempcamdat.GetVMatrix().dump("WTF");
        ddprops["selcamdat"_crcu].Set<const CameraData*>(&_tempcamdat);
        // DrawableBuffer -> RenderQueue enqueue
        for (const PoolString& layer_name : CPD.getLayerNames()){
          targ->debugMarker(FormatString("Deferred::renderEnqueuedScene::layer<%s>", layer_name.c_str()));
          DB->enqueueLayerToRenderQueue(layer_name, irenderer);
        }
        /////////////////////////////////////////////////
        auto MTXI = targ->MTXI();
        drawdata.mCompositingGroupStack.push(CPD);
        MTXI->PushPMatrix(CAMCCTX.mPMatrix);
        MTXI->PushVMatrix(CAMCCTX.mVMatrix);
        MTXI->PushMMatrix(fmtx4::Identity);
          targ->debugPushGroup("toolvp::DrawEnqRenderables");
              irenderer->drawEnqueuedRenderables();
              framerenderer.renderMisc();
          targ->debugPopGroup();
        MTXI->PopPMatrix();
        MTXI->PopVMatrix();
        MTXI->PopMMatrix();
        drawdata.mCompositingGroupStack.pop();
        /////////////////////////////////////////////////

      }
      /////////////////////////////////////////////////////////////////////////////////////////
      targ->FBI()->PopRtGroup();
      RCFD.PopRenderTarget();
      targ->EndFrame();
      targ->SetRenderContextFrameData(nullptr);
    }
    targ->debugPopGroup();
  }
  ///////////////////////////////////////
  PoolString _camname, _layername;
  CompositingMaterial _material;
  RtGroup* _rtg = nullptr;
  BuiltinFrameEffectMaterial _effect;
  CameraData _tempcamdat;
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
