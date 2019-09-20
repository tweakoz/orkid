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
  void init(lev2::GfxTarget* pTARG) {
  }
  ///////////////////////////////////////
  void _produce(CompositorDrawData& drawdata,OutputCompositingNode::innerl_t lambda) {
    FrameRenderer& fr_renderer       = drawdata.mFrameRenderer;
    RenderContextFrameData& framedata = fr_renderer.framedata();
    auto targ                         = framedata.GetTarget();
    framedata.setLayerName(_node->_layername.c_str());
    lambda();
  }
  ///////////////////////////////////////
  PoolString _camname, _layers;
  ScreenOutputCompositingNode* _node = nullptr;
};
///////////////////////////////////////////////////////////////////////////////
ScreenOutputCompositingNode::ScreenOutputCompositingNode() { _impl = std::make_shared<IMPL>(this); }
ScreenOutputCompositingNode::~ScreenOutputCompositingNode() {}
void ScreenOutputCompositingNode::DoInit(lev2::GfxTarget* pTARG, int iW, int iH)
{ _impl.Get<std::shared_ptr<IMPL>>()->init(pTARG);
}
void ScreenOutputCompositingNode::_produce(CompositorDrawData& drawdata, CompositingImpl* cimpl,innerl_t lambda)
{ _impl.Get<std::shared_ptr<IMPL>>()->_produce(drawdata,lambda);
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
