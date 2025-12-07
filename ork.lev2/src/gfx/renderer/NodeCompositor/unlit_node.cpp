////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>

#include <ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h>

ImplementReflectionX(ork::lev2::compositor::UnlitNode, "UnlitNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::compositor {
///////////////////////////////////////////////////////////////////////////////
void UnlitNode::describeX(class_t* c) {
  c->directProperty("ClearColor", &UnlitNode::_clearColor);
}
///////////////////////////////////////////////////////////////////////////////
namespace _unlitnode {
struct IMPL {
  ///////////////////////////////////////
  IMPL()
      : _camname("Camera") {
    _layername = "std_forward";
  }
  ///////////////////////////////////////
  ~IMPL() {
  }
  ///////////////////////////////////////
  void init(lev2::Context* pTARG) {
    pTARG->debugPushGroup("Forward::rendeinitr");
    if (nullptr == _rtg) {
      _material.gpuInit(pTARG);
      _rtg             = new RtGroup(pTARG, 8, 8);
      auto buf1        = _rtg->createRenderTarget(lev2::EBufferFormat::RGBA32F);
      auto buf2        = _rtg->createRenderTarget(lev2::EBufferFormat::RGBA32F);
      buf1->_debugName = "ForwardRt0";
      buf2->_debugName = "ForwardRt1";
      _profile_timer.Start();
    }
    pTARG->debugPopGroup();
  }
  ///////////////////////////////////////
  void _render_top(UnlitNode* node, CompositorDrawData& drawdata) {
    // float t1 = _profile_timer.SecsSinceStart();

    auto context                 = drawdata.context();
    auto FBI                     = context->FBI();
    auto CIMPL                   = drawdata._cimpl;
    auto RCFD                    = drawdata.RCFD();
    auto this_buf                = FBI->GetThisBuffer();
    auto tgt_rect                = context->mainSurfaceRectAtOrigin();
    //auto& ddprops                = drawdata._properties;
    //////////////////////////////////////////////////////
    // Resize RenderTargets
    //////////////////////////////////////////////////////
    int newwidth  = drawdata.property("OutputWidth"_crcu).get<int>();
    int newheight = drawdata.property("OutputHeight"_crcu).get<int>();
    if (_rtg->width() != newwidth or _rtg->height() != newheight) {
      _rtg->Resize(newwidth, newheight);
    }
    // float t2 = _profile_timer.SecsSinceStart();
    //////////////////////////////////////////////////////
    auto irenderer = drawdata.property("irenderer"_crcu).get<lev2::IRenderer*>();
    irenderer->_debugLog = false;
    //////////////////////////////////////////////////////
    context->debugPushGroup("Forward::render");
    RtGroupRenderTarget rt(_rtg);
    {
      _rtg->buffer(0)->_clearColor = node->_clearColor;
      _rtg->_autoclear  = true;
      context->FBI()->PushRtGroup(_rtg);
      /////////////////////////////////////////////////////////////////////////////////////////
      auto DB  = RCFD->GetDB();
      if (DB) {
        auto CPD = CIMPL->topCPD();
        CPD._mono_cam_matrices = drawdata.property("defcammtx"_crcu).get<cameramatrices_ptr_t>();
        CPD.assignLayers("std_forward,xxx");
        CPD._clearColor     = node->_clearColor;
        CPD._irendertarget  = &rt;
        CPD.SetDstRect(tgt_rect);
        CIMPL->pushCPD(CPD);
        ///////////////////////////////////////////////////////////////////////////
        // float t3 = _profile_timer.SecsSinceStart();
        ///////////////////////////////////////////////////////////////////////////
        // DrawQueue -> RenderQueue enqueue
        ///////////////////////////////////////////////////////////////////////////
        DB->enqueueLayerToRenderQueue(_layername, irenderer);
        /////////////////////////////////////////////////
        RCFD->_renderingmodel = node->_renderingmodel;
        auto MTXI            = context->MTXI();
        context->debugPushGroup("toolvp::DrawEnqRenderables");
        size_t rencount = irenderer->_unsortedNodes.Size();
        if(0==rencount){
          printf("UnlitNode:: layer<%s> enqueued %zu renderables\n",
                 _layername.c_str(),
                 rencount);
        }
        irenderer->drawEnqueuedRenderables(true);
        context->debugPopGroup();
      }
      else{
        printf( "UnlitNode:: no DrawBuffer!\n" );
      }
      CIMPL->popCPD();
      // float t4 = _profile_timer.SecsSinceStart();
      // printf( "unlitnode t4-t3 %g(mSec)\n", (t2-t1)*1000.0f);
      /////////////////////////////////////////////////////////////////////////////////////////
      context->FBI()->PopRtGroup();
    }
    context->debugPopGroup();
    // float t5 = _profile_timer.SecsSinceStart();

    // printf( "unlitnode t5-t1 %g(mSec)\n", (t5-t1)*1000.0f);
  }
  ///////////////////////////////////////
  std::string _camname, _layername;
  CompositingMaterial _material;
  RtGroup* _rtg = nullptr;
  fmtx4 _viewOffsetMatrix;
  Timer _profile_timer;
};
} // namespace _unlitnode

///////////////////////////////////////////////////////////////////////////////
UnlitNode::UnlitNode() {
  _impl           = std::make_shared<_unlitnode::IMPL>();
  _renderingmodel = RenderingModel("FORWARD_UNLIT"_crcu);
  _clearColor     = fvec4(0, 0, 0, 1);
}
///////////////////////////////////////////////////////////////////////////////
UnlitNode::~UnlitNode() {
}
///////////////////////////////////////////////////////////////////////////////
void UnlitNode::doGpuInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.get<std::shared_ptr<_unlitnode::IMPL>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void UnlitNode::DoRender(CompositorDrawData& drawdata) {
  auto impl = _impl.get<std::shared_ptr<_unlitnode::IMPL>>();
  impl->_render_top(this, drawdata);
}
///////////////////////////////////////////////////////////////////////////////
rtbuffer_ptr_t UnlitNode::GetOutput() const {
  return _impl.get<std::shared_ptr<_unlitnode::IMPL>>()->_rtg->buffer(0);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::compositor
