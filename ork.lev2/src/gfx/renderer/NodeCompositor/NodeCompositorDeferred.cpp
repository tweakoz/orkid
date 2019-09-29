////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>

#include "NodeCompositorDeferred.h"

ImplementReflectionX(ork::lev2::DeferredCompositingNode, "DeferredCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNode::describeX(class_t* c) {
c->memberProperty("ClearColor", &DeferredCompositingNode::_clearColor);
c->memberProperty("FogColor", &DeferredCompositingNode::_fogColor);
}
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
  // deferred layout
  // rt0/GL_RGBA8    (32,32)  - albedo,ao (primary color)
  // rt1/GL_RGB10_A2 (32,64)  - normal,model
  // rt2/GL_RGBA8    (32,96)  - mtl,ruf,aux1,aux2
  // rt3/GL_R32F     (32,128) - depth
  ///////////////////////////////////////
  void init(lev2::GfxTarget* pTARG) {
    pTARG->debugPushGroup("Deferred::rendeinitr");
    if (nullptr == _rtgGbuffer) {
      _blit2screenmtl.SetUserFx("orkshader://deferred", "deferred_test");
      _blit2screenmtl.Init(pTARG);
      _rtgGbuffer            = new RtGroup(pTARG, 8, 8, NUMSAMPLES);
      auto buf0        = new RtBuffer(_rtgGbuffer, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA8, 8, 8);
      auto buf1        = new RtBuffer(_rtgGbuffer, lev2::ETGTTYPE_MRT1, lev2::EBUFFMT_RGBA8, 8, 8);
      auto buf2        = new RtBuffer(_rtgGbuffer, lev2::ETGTTYPE_MRT2, lev2::EBUFFMT_RGB10A2, 8, 8);
      buf0->_debugName = "DeferredRtAlbAo";
      buf1->_debugName = "DeferredRRufMtl";
      buf2->_debugName = "DeferredRtNormalDist";
      _rtgGbuffer->SetMrt(0, buf0);
      _rtgGbuffer->SetMrt(1, buf1);
      _rtgGbuffer->SetMrt(2, buf2);

      _rtgLaccum            = new RtGroup(pTARG, 8, 8, NUMSAMPLES);
      auto bufLA        = new RtBuffer(_rtgLaccum, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA16F, 8, 8);
      bufLA->_debugName = "DeferredLightAccum";
      _rtgLaccum->SetMrt(0, bufLA);
    }
    pTARG->debugPopGroup();
  }
  ///////////////////////////////////////
  void _render(DeferredCompositingNode* node, CompositorDrawData& drawdata) {
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto CIMPL = drawdata._cimpl;
    auto targ                    = RCFD.GetTarget();
    auto& ddprops                = drawdata._properties;
    SRect tgt_rect(0, 0, targ->GetW(), targ->GetH());
    //////////////////////////////////////////////////////
    // Resize RenderTargets
    //////////////////////////////////////////////////////
    int newwidth  = ddprops["OutputWidth"_crcu].Get<int>();
    int newheight = ddprops["OutputHeight"_crcu].Get<int>();
    if (_rtgGbuffer->GetW() != newwidth or _rtgGbuffer->GetH() != newheight) {
      _rtgGbuffer->Resize(newwidth, newheight);
      _rtgLaccum->Resize(newwidth,newheight);
      _width = newwidth;
      _height = newheight;
    }
    //////////////////////////////////////////////////////
    auto irenderer = ddprops["irenderer"_crcu].Get<lev2::IRenderer*>();
    //////////////////////////////////////////////////////
    targ->debugPushGroup("Deferred::render");
    RtGroupRenderTarget rtgbuf(_rtgGbuffer);
    {
      targ->FBI()->PushRtGroup(_rtgGbuffer);
      targ->FBI()->SetAutoClear(false); // explicit clear
      targ->BeginFrame();
      /////////////////////////////////////////////////////////////////////////////////////////
      auto DB         = RCFD.GetDB();
      auto CPD = CIMPL->topCPD();
      CPD._clearColor = node->_clearColor;
      CPD.mpLayerName = &_layername;
      CPD._irendertarget = & rtgbuf;
      CPD.SetDstRect(tgt_rect);
      CPD._passID = "defgbuffer1"_crcu;
      ///////////////////////////////////////////////////////////////////////////
      if (DB) {
        ///////////////////////////////////////////////////////////////////////////
        // DrawableBuffer -> RenderQueue enqueue
        ///////////////////////////////////////////////////////////////////////////
        for (const PoolString& layer_name : CPD.getLayerNames()) {
          targ->debugMarker(FormatString("Deferred::renderEnqueuedScene::layer<%s>", layer_name.c_str()));
          DB->enqueueLayerToRenderQueue(layer_name, irenderer);
        }
        /////////////////////////////////////////////////
        auto MTXI = targ->MTXI();
        CIMPL->pushCPD(CPD);
        targ->debugPushGroup("toolvp::DrawEnqRenderables");
        targ->FBI()->Clear(node->_clearColor, 1.0f);
        irenderer->drawEnqueuedRenderables();
        framerenderer.renderMisc();
        targ->debugPopGroup();
        CIMPL->popCPD();
      }
      /////////////////////////////////////////////////////////////////////////////////////////
      targ->EndFrame();
      targ->FBI()->PopRtGroup();
      /////////////////////////////////////////////////////////////////////////////////////////
      // Light Accumulation
      /////////////////////////////////////////////////////////////////////////////////////////
      RtGroupRenderTarget rtlaccum(_rtgLaccum);
      SRect vprect(0, 0, _width, _height);
      SRect quadrect(0, 0, _width, _height);
      CPD.SetDstRect(vprect);
      CPD._irendertarget = & rtlaccum;
      CPD._cameraMatrices = nullptr;
      CPD._stereoCameraMatrices = nullptr;
      CPD._stereo1pass = false;
      CIMPL->pushCPD(CPD);
      targ->debugPushGroup("PtxCompositingNode::to_output");
      targ->FBI()->SetAutoClear(false);
      targ->FBI()->PushRtGroup(_rtgLaccum);
      targ->BeginFrame();
      targ->FBI()->Clear(fvec4(0.1,0.2,0.3,1), 1.0f);
      auto this_buf = targ->FBI()->GetThisBuffer();
      fvec4 vtxcolor(1.0f, 1.0f, 1.0f, 1.0f);
      _blit2screenmtl.SetAuxMatrix(fmtx4::Identity);
      _blit2screenmtl.SetTexture(_rtgGbuffer->GetMrt(0)->GetTexture());
      _blit2screenmtl.SetTexture2(_rtgGbuffer->GetMrt(1)->GetTexture());
      _blit2screenmtl.SetTexture3(_rtgGbuffer->GetMrt(2)->GetTexture());
      _blit2screenmtl.SetUser0(node->_fogColor);
      _blit2screenmtl.SetColorMode(GfxMaterial3DSolid::EMODE_USER);
      _blit2screenmtl.mRasterState.SetBlending(EBLENDING_OFF);
      _blit2screenmtl.mRasterState.SetDepthTest(EDEPTHTEST_OFF);
      this_buf->RenderMatOrthoQuad(vprect,
                                   quadrect,
                                   &_blit2screenmtl,
                                   0.0f,
                                   1.0f, // u0 v0
                                   1.0f,
                                   0.0f, // u1 v1
                                   nullptr,
                                   vtxcolor);
      targ->EndFrame();
      targ->FBI()->PopRtGroup();
      targ->debugPopGroup();
      CIMPL->popCPD();
      /////////////////////////////////////////////////////////////////////////////////////////
    }
    targ->debugPopGroup();
  }
  ///////////////////////////////////////
  PoolString _camname, _layername;
  ork::lev2::GfxMaterial3DSolid _blit2screenmtl;
  RtGroup* _rtgGbuffer = nullptr;
  RtGroup* _rtgLaccum = nullptr;
  fmtx4 _viewOffsetMatrix;
  int _width = 0;
  int _height = 0;
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
RtBuffer* DeferredCompositingNode::GetOutput() const {
  static int i = 0;
  i++;
return _impl.Get<std::shared_ptr<deferrednode::IMPL>>()->_rtgLaccum->GetMrt(0);
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
