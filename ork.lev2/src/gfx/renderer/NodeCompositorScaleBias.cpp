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
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>

ImplementReflectionX(ork::lev2::ScaleBiasCompositingNode, "ScaleBiasCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void ScaleBiasCompositingNode::describeX(class_t* c) {
  c->memberProperty("Layer",&ScaleBiasCompositingNode::_layername);
}
///////////////////////////////////////////////////////////////////////////
constexpr int NUMSAMPLES = 1;
struct ScaleBiasTechnique final : public FrameTechniqueBase {
  //////////////////////////////////////////////////////////////////////////////
  ScaleBiasTechnique(int w, int h)
      : FrameTechniqueBase(w, h)
      , _rtg(nullptr) {}
  //////////////////////////////////////////////////////////////////////////////
  void DoInit(GfxTarget* pTARG) final {
    if (nullptr == _rtg) {
      _rtg = new RtGroup(pTARG, miW, miH, NUMSAMPLES);
      auto buf = new RtBuffer(_rtg, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA32, miW, miH);
      buf->_debugName = FormatString("ScaleBiasCompositingNode::output");
      _rtg->SetMrt(0,buf);
      _effect.PostInit(pTARG, "orkshader://framefx", "frameeffect_standard");
    }
  }
  //////////////////////////////////////////////////////////////////////////////
  void render(FrameRenderer& renderer,
              CompositorDrawData& drawdata,
              ScaleBiasCompositingNode& node ) {
    RenderContextFrameData& framedata = renderer.framedata();
    GfxTarget* pTARG                  = framedata.GetTarget();

    SRect tgt_rect(0, 0, miW, miH);

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

    pTARG->debugPushGroup("ScaleBiasCompositingNode::render");
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
    pTARG->debugPopGroup();
  }

  RtGroup* _rtg;
  BuiltinFrameEffectMaterial _effect;
  CompositingPassData _CPD;
  fmtx4 _viewOffsetMatrix;
};

///////////////////////////////////////////////////////////////////////////////
struct IMPL {
  ///////////////////////////////////////
  IMPL()
      : _frametek(nullptr)
      , _camname(AddPooledString("Camera"))
      , _layers(AddPooledString("All")) {}
  ///////////////////////////////////////
  ~IMPL() {
    if (_frametek)
      delete _frametek;
  }
  ///////////////////////////////////////
  void init(lev2::GfxTarget* pTARG) {
    _material.Init(pTARG);
    _frametek = new ScaleBiasTechnique(pTARG->GetW(),pTARG->GetH());
    _frametek->Init(pTARG);
  }
  ///////////////////////////////////////
  void _myrender(ScaleBiasCompositingNode* node, FrameRenderer& renderer, CompositorDrawData& drawdata) {
    _frametek->render(renderer, drawdata,*node);
  }
  ///////////////////////////////////////
  PoolString _camname, _layers;
  CompositingMaterial _material;
  ScaleBiasTechnique* _frametek;
};
///////////////////////////////////////////////////////////////////////////////
ScaleBiasCompositingNode::ScaleBiasCompositingNode() { _impl = std::make_shared<IMPL>(); }
///////////////////////////////////////////////////////////////////////////////
ScaleBiasCompositingNode::~ScaleBiasCompositingNode() {}
///////////////////////////////////////////////////////////////////////////////
void ScaleBiasCompositingNode::DoInit(lev2::GfxTarget* pTARG, int iW, int iH) // virtual
{
  auto impl = _impl.Get<std::shared_ptr<IMPL>>();
  if (nullptr == impl->_frametek) {
    impl->init(pTARG);
  }
}
///////////////////////////////////////////////////////////////////////////////
void ScaleBiasCompositingNode::DoRender(CompositorDrawData& drawdata, CompositingImpl* cimpl) // virtual
{
  FrameRenderer& the_renderer       = drawdata.mFrameRenderer;
  RenderContextFrameData& framedata = the_renderer.framedata();
  auto targ                         = framedata.GetTarget();
  auto impl                         = _impl.Get<std::shared_ptr<IMPL>>();
  impl->_layers = _layername;
  if (impl->_frametek) {
    framedata.setLayerName(_layername.c_str());
    impl->_myrender(this,the_renderer, drawdata);
  }
}
///////////////////////////////////////////////////////////////////////////////
RtGroup* ScaleBiasCompositingNode::GetOutput() const {
  auto impl = _impl.Get<std::shared_ptr<IMPL>>();
  if (impl->_frametek)
    return impl->_frametek->_rtg;
  else
    return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
