////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "NodeCompositorScreen.h"

#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>

ImplementReflectionX(ork::lev2::ScreenOutputCompositingNode, "ScreenOutputCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void ScreenOutputCompositingNode::describeX(class_t* c) {
  c->memberProperty("Layer",&ScreenOutputCompositingNode::_layername);
}
///////////////////////////////////////////////////////////////////////////////
struct IMPL {
  ///////////////////////////////////////
  IMPL(ScreenOutputCompositingNode* node)
      : _node(node)
      , _camname(AddPooledString("Camera"))
      , _layers(AddPooledString("All")) {}
  ///////////////////////////////////////
  ~IMPL() {
  }
  ///////////////////////////////////////
  void gpuInit(lev2::GfxTarget* pTARG) {
  }
  ///////////////////////////////////////
  void beginFrame(CompositorDrawData& drawdata) {
    FrameRenderer& fr_renderer       = drawdata.mFrameRenderer;
    RenderContextFrameData& framedata = fr_renderer.framedata();
    auto targ                         = framedata.GetTarget();
    framedata.setLayerName(_node->_layername.c_str());
  }
  void endFrame(CompositorDrawData& drawdata,RtGroup* final) {
  }
  ///////////////////////////////////////
  PoolString _camname, _layers;
  ScreenOutputCompositingNode* _node = nullptr;
};
///////////////////////////////////////////////////////////////////////////////
ScreenOutputCompositingNode::ScreenOutputCompositingNode() { _impl = std::make_shared<IMPL>(this); }
ScreenOutputCompositingNode::~ScreenOutputCompositingNode() {}
void ScreenOutputCompositingNode::gpuInit(lev2::GfxTarget* pTARG, int iW, int iH)
{ _impl.Get<std::shared_ptr<IMPL>>()->gpuInit(pTARG);
}
void ScreenOutputCompositingNode::beginFrame(CompositorDrawData& drawdata, CompositingImpl* cimpl)
{ _impl.Get<std::shared_ptr<IMPL>>()->beginFrame(drawdata);
}
void ScreenOutputCompositingNode::endFrame(CompositorDrawData& drawdata, CompositingImpl* cimpl, RtGroup* final)
{ _impl.Get<std::shared_ptr<IMPL>>()->endFrame(drawdata,final);
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
