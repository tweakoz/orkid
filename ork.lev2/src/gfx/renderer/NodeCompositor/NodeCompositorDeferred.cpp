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

struct PointLight {
  fvec3 _pos;
  fvec3 _dst;
  fvec3 _color;
  float _radius;
  int _counter = 0;

  void next(){
      float x = float((rand()%2048)-1024);
      float z = float((rand()%2048)-1024);
      float y = float(100+((rand()%200)-100));
      _dst     = fvec3(x, y, z);
      _counter = 200+rand()%200;
  }
};

struct IMPL {
  ///////////////////////////////////////
  IMPL()
      : _camname(AddPooledString("Camera")) {
    _layername = "All"_pool;

    for (int i = 0; i < 64; i++) {

      PointLight p;
      p.next();
      p._color.x = float(rand() & 0xff) / 128.0;
      p._color.y = float(rand() & 0xff) / 128.0;
      p._color.z = float(rand() & 0xff) / 128.0;
      p._radius  = 50.0f;
      _pointlights.push_back(p);
    }
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
      //////////////////////////////////////////////////////////////
      _baselightmtl.SetUserFx("orkshader://deferred", "baselight");
      _baselightmtl.Init(pTARG);
      //////////////////////////////////////////////////////////////
      _pointlightmtl.SetUserFx("orkshader://deferred", "pointlight");
      _pointlightmtl.Init(pTARG);
      //////////////////////////////////////////////////////////////
      _rtgGbuffer      = new RtGroup(pTARG, 8, 8, NUMSAMPLES);
      auto buf0        = new RtBuffer(_rtgGbuffer, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA8, 8, 8);
      auto buf1        = new RtBuffer(_rtgGbuffer, lev2::ETGTTYPE_MRT1, lev2::EBUFFMT_RGB10A2, 8, 8);
      auto buf2        = new RtBuffer(_rtgGbuffer, lev2::ETGTTYPE_MRT2, lev2::EBUFFMT_RGBA32F, 8, 8);
      buf0->_debugName = "DeferredRtAlbAo";
      buf1->_debugName = "DeferredRRufMtl";
      buf2->_debugName = "DeferredRtNormalDist";
      _rtgGbuffer->SetMrt(0, buf0);
      _rtgGbuffer->SetMrt(1, buf1);
      _rtgGbuffer->SetMrt(2, buf2);
      //////////////////////////////////////////////////////////////
      _rtgLaccum        = new RtGroup(pTARG, 8, 8, NUMSAMPLES);
      auto bufLA        = new RtBuffer(_rtgLaccum, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA16F, 8, 8);
      bufLA->_debugName = "DeferredLightAccum";
      _rtgLaccum->SetMrt(0, bufLA);
      //////////////////////////////////////////////////////////////
    }
    pTARG->debugPopGroup();
  }
  ///////////////////////////////////////
  void _render(DeferredCompositingNode* node, CompositorDrawData& drawdata) {
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto CIMPL                   = drawdata._cimpl;
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
      _rtgLaccum->Resize(newwidth, newheight);
      _width  = newwidth;
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
      auto DB            = RCFD.GetDB();
      auto CPD           = CIMPL->topCPD();
      CPD._clearColor    = node->_clearColor;
      CPD.mpLayerName    = &_layername;
      CPD._irendertarget = &rtgbuf;
      CPD.SetDstRect(tgt_rect);
      CPD._passID       = "defgbuffer1"_crcu;
      fvec3 campos_mono = CPD.monoCamPos(fmtx4());
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
      CPD._irendertarget        = &rtlaccum;
      CPD._cameraMatrices       = nullptr;
      CPD._stereoCameraMatrices = nullptr;
      CPD._stereo1pass          = false;
      CIMPL->pushCPD(CPD);
      targ->debugPushGroup("PtxCompositingNode::to_output");
      targ->FBI()->SetAutoClear(false);
      targ->FBI()->PushRtGroup(_rtgLaccum);
      targ->BeginFrame();
      targ->FBI()->Clear(fvec4(0.1, 0.2, 0.3, 1), 1.0f);
      auto this_buf = targ->FBI()->GetThisBuffer();
      //////////////////////////////////////////////////////////////////
      // base lighting
      //////////////////////////////////////////////////////////////////
      fvec4 vtxcolor(1.0f, 1.0f, 1.0f, 1.0f);
      _baselightmtl.SetAuxMatrix(fmtx4::Identity);
      _baselightmtl.SetTexture(_rtgGbuffer->GetMrt(0)->GetTexture());
      _baselightmtl.SetTexture2(_rtgGbuffer->GetMrt(1)->GetTexture());
      _baselightmtl.SetTexture3(_rtgGbuffer->GetMrt(2)->GetTexture());
      _baselightmtl.SetUser0(node->_fogColor);
      _baselightmtl.SetUser1(campos_mono);
      _baselightmtl.SetColorMode(GfxMaterial3DSolid::EMODE_USER);
      _baselightmtl.mRasterState.SetBlending(EBLENDING_OFF);
      _baselightmtl.mRasterState.SetDepthTest(EDEPTHTEST_OFF);
      this_buf->RenderMatOrthoQuad(vprect,
                                   quadrect,
                                   &_baselightmtl,
                                   0.0f,
                                   1.0f, // u0 v0
                                   1.0f,
                                   0.0f, // u1 v1
                                   nullptr,
                                   vtxcolor);
      CIMPL->popCPD();
      //////////////////////////////////////////////////////////////////
      // point lighting
      //////////////////////////////////////////////////////////////////
      static float ftime = 0.0f;
      CPD                = CIMPL->topCPD();
      CPD.SetDstRect(vprect);
      CPD._irendertarget        = &rtlaccum;
      CIMPL->pushCPD(CPD);
      _pointlightmtl.SetAuxMatrix(fmtx4::Identity);
      _pointlightmtl.SetTexture(_rtgGbuffer->GetMrt(0)->GetTexture());
      _pointlightmtl.SetTexture2(_rtgGbuffer->GetMrt(1)->GetTexture());
      _pointlightmtl.SetTexture3(_rtgGbuffer->GetMrt(2)->GetTexture());
      _pointlightmtl.SetUser0(node->_fogColor);
      _pointlightmtl.SetUser2(fvec4(1.0/float(_width),1.0f/float(_height),0,0));
      _pointlightmtl.SetColorMode(GfxMaterial3DSolid::EMODE_USER);
      _pointlightmtl.mRasterState.SetBlending(EBLENDING_ADDITIVE);
      _pointlightmtl.mRasterState.SetDepthTest(EDEPTHTEST_OFF);
      _pointlightmtl.mRasterState.SetCullTest(ECULLTEST_PASS_BACK);
      targ->PushMaterial(&_pointlightmtl);
      targ->FBI()->PushViewport(vprect); // stereo viewport
      for( auto& pl : _pointlights ){
        _pointlightmtl.SetUser0(pl._color);
        _pointlightmtl.SetUser1(fvec4(pl._pos, pl._radius));
        _pointlightmtl.SetAuxMatrix(fmtx4::Identity);
        //////////////////////////
        fmtx4 LightMtx;
        LightMtx.ComposeMatrix(pl._pos, fquat(), pl._radius);
        targ->MTXI()->PushMMatrix(LightMtx);
        targ->GBI()->DrawPrimitive(GfxPrimitives::GetFullSphere());
        targ->MTXI()->PopMMatrix();
        //////////////////////////
        if( pl._counter < 1 ){
          pl.next();
        }
        else {
          fvec3 delta = pl._dst-pl._pos;
          pl._pos += delta.Normal()*0.5;
          pl._counter --;
        }
      }
      targ->FBI()->PopViewport();
      targ->PopMaterial();
      ftime += 0.01f;
      //////////////////////////////////////////////////////////////////
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
  ork::lev2::GfxMaterial3DSolid _baselightmtl;
  ork::lev2::GfxMaterial3DSolid _pointlightmtl;
  RtGroup* _rtgGbuffer = nullptr;
  RtGroup* _rtgLaccum  = nullptr;
  fmtx4 _viewOffsetMatrix;
  int _width  = 0;
  int _height = 0;
  std::vector<PointLight> _pointlights;
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
