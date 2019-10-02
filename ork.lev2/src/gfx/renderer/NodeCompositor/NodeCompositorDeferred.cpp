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

#include "DeferredMaterial.inl"
#include "NodeCompositorDeferred.h"

ImplementReflectionX(ork::lev2::DeferredCompositingNode,
                     "DeferredCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork {
namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNode::describeX(class_t *c) {
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

  void next() {
    float x = float((rand() % 4096) - 2048);
    float z = float((rand() % 4096) - 2048);
    float y = float(100 + ((rand() % 200) - 100));
    _dst = fvec3(x, y, z);
    _counter = 200 + rand() % 200;
  }
};

struct IMPL {
  ///////////////////////////////////////
  IMPL() : _camname(AddPooledString("Camera")) {
    _layername = "All"_pool;

    for (int i = 0; i < 128; i++) {

      PointLight p;
      p.next();
      p._color.x = float(rand() & 0xff) / 128.0;
      p._color.y = float(rand() & 0xff) / 128.0;
      p._color.z = float(rand() & 0xff) / 128.0;
      p._radius = 50.0f;
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
  void init(lev2::GfxTarget *pTARG) {
    pTARG->debugPushGroup("Deferred::rendeinitr");
    if (nullptr == _rtgGbuffer) {
      //////////////////////////////////////////////////////////////
      _lightingmtl.Init(pTARG);
      //////////////////////////////////////////////////////////////
      _rtgGbuffer = new RtGroup(pTARG, 8, 8, NUMSAMPLES);
      auto buf0 = new RtBuffer(_rtgGbuffer, lev2::ETGTTYPE_MRT0,
                               lev2::EBUFFMT_RGBA8, 8, 8);
      auto buf1 = new RtBuffer(_rtgGbuffer, lev2::ETGTTYPE_MRT1,
                               lev2::EBUFFMT_RGB10A2, 8, 8);
      // auto buf2        = new RtBuffer(_rtgGbuffer, lev2::ETGTTYPE_MRT2,
      // lev2::EBUFFMT_RGBA32F, 8, 8);
      buf0->_debugName = "DeferredRtAlbAo";
      buf1->_debugName = "DeferredRRufMtl";
      // buf2->_debugName = "DeferredRtNormalDist";
      _rtgGbuffer->SetMrt(0, buf0);
      _rtgGbuffer->SetMrt(1, buf1);
      //_rtgGbuffer->SetMrt(2, buf2);
      //////////////////////////////////////////////////////////////
      _rtgLaccum = new RtGroup(pTARG, 8, 8, NUMSAMPLES);
      auto bufLA = new RtBuffer(_rtgLaccum, lev2::ETGTTYPE_MRT0,
                                lev2::EBUFFMT_RGBA16F, 8, 8);
      bufLA->_debugName = "DeferredLightAccum";
      _rtgLaccum->SetMrt(0, bufLA);
      //////////////////////////////////////////////////////////////
    }
    pTARG->debugPopGroup();
  }
  ///////////////////////////////////////
  void _render(DeferredCompositingNode *node, CompositorDrawData &drawdata) {
    FrameRenderer &framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData &RCFD = framerenderer.framedata();
    auto CIMPL = drawdata._cimpl;
    auto targ = RCFD.GetTarget();
    auto FXI = targ->FXI();
    auto &ddprops = drawdata._properties;
    SRect tgt_rect(0, 0, targ->GetW(), targ->GetH());
    //////////////////////////////////////////////////////
    // Resize RenderTargets
    //////////////////////////////////////////////////////
    int newwidth = ddprops["OutputWidth"_crcu].Get<int>();
    int newheight = ddprops["OutputHeight"_crcu].Get<int>();
    if (_rtgGbuffer->GetW() != newwidth or _rtgGbuffer->GetH() != newheight) {
      _rtgGbuffer->Resize(newwidth, newheight);
      _rtgLaccum->Resize(newwidth, newheight);
      _width = newwidth;
      _height = newheight;
    }
    //////////////////////////////////////////////////////
    auto irenderer = ddprops["irenderer"_crcu].Get<lev2::IRenderer *>();
    //////////////////////////////////////////////////////
    targ->debugPushGroup("Deferred::render");
    RtGroupRenderTarget rtgbuf(_rtgGbuffer);
    {
      targ->FBI()->PushRtGroup(_rtgGbuffer);
      targ->FBI()->SetAutoClear(false); // explicit clear
      targ->BeginFrame();
      /////////////////////////////////////////////////////////////////////////////////////////
      auto DB = RCFD.GetDB();
      auto CPD = CIMPL->topCPD();
      bool is_stereo_1pass = CPD.isStereoOnePass();
      CPD._clearColor = node->_clearColor;
      CPD.mpLayerName = &_layername;
      CPD._irendertarget = &rtgbuf;
      CPD.SetDstRect(tgt_rect);
      CPD._passID = "defgbuffer1"_crcu;
      fvec3 campos_mono = CPD.monoCamPos(fmtx4());
      fmtx4 IVPL, IVPR;
      if (is_stereo_1pass) {
        auto L = CPD._stereoCameraMatrices->_left;
        auto R = CPD._stereoCameraMatrices->_right;
        IVPL.inverseOf(L->_vmatrix * L->_pmatrix);
        IVPR.inverseOf(R->_vmatrix * R->_pmatrix);
      } else {
        auto M = CPD._cameraMatrices;
        IVPL.inverseOf(M->_vmatrix * M->_pmatrix);
        IVPR.inverseOf(M->_vmatrix * M->_pmatrix);
      }
      ///////////////////////////////////////////////////////////////////////////
      if (DB) {
        ///////////////////////////////////////////////////////////////////////////
        // DrawableBuffer -> RenderQueue enqueue
        ///////////////////////////////////////////////////////////////////////////
        for (const PoolString &layer_name : CPD.getLayerNames()) {
          targ->debugMarker(FormatString(
              "Deferred::renderEnqueuedScene::layer<%s>", layer_name.c_str()));
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
      CPD._irendertarget = &rtlaccum;
      CPD._cameraMatrices = nullptr;
      CPD._stereoCameraMatrices = nullptr;
      CPD._stereo1pass = false;
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
      _lightingmtl._ivp[0] = IVPL;
      _lightingmtl._ivp[1] = IVPR;
      _lightingmtl._albedoAoMap = _rtgGbuffer->GetMrt(0)->GetTexture();
      _lightingmtl._normalLitmodelMap = _rtgGbuffer->GetMrt(1)->GetTexture();
      _lightingmtl._depthMap = _rtgGbuffer->_depthTexture;
      _lightingmtl._invviewsize =
          fvec2(1.0 / float(_width), 1.0f / float(_height));
      _lightingmtl.mRasterState.SetBlending(EBLENDING_OFF);
      _lightingmtl.mRasterState.SetDepthTest(EDEPTHTEST_OFF);
      _lightingmtl.mRasterState.SetCullTest(ECULLTEST_PASS_BACK);
      _lightingmtl._lightcolor = fvec3(0.8, 0.7, 0.2);
      _lightingmtl._lightpos = campos_mono;
      _lightingmtl._lightradius = 100.0f;
      _lightingmtl.mRasterState.SetBlending(EBLENDING_OFF);
      _lightingmtl.mRasterState.SetDepthTest(EDEPTHTEST_OFF);
      FXI->BindTechnique(_lightingmtl._shader,
                         is_stereo_1pass ? _lightingmtl._tekBaseLightingStereo
                                         : _lightingmtl._tekBaseLighting);
      // fbi->PushViewport(ViewportRect);
      // fbi->PushScissor(ViewportRect);
      _lightingmtl.begin(RCFD);
      this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1));
      _lightingmtl.end(RCFD);
      CIMPL->popCPD();
      //////////////////////////////////////////////////////////////////
      // point lighting
      //  todo : batch multiple lights together
      //   compute screen aligned quad for batch..
      //////////////////////////////////////////////////////////////////
      static float ftime = 0.0f;
      CPD = CIMPL->topCPD();
      CPD.SetDstRect(vprect);
      CPD._irendertarget = &rtlaccum;

      CIMPL->pushCPD(CPD);

      _lightingmtl.mRasterState.SetBlending(EBLENDING_ADDITIVE);
      _lightingmtl.mRasterState.SetDepthTest(EDEPTHTEST_OFF);
      _lightingmtl.mRasterState.SetCullTest(ECULLTEST_PASS_BACK);
      targ->PushMaterial(&_lightingmtl);
      targ->FBI()->PushViewport(vprect); // stereo viewport
      FXI->BindTechnique(_lightingmtl._shader,
                         is_stereo_1pass ? _lightingmtl._tekPointLightingStereo
                                         : _lightingmtl._tekPointLighting);
      _lightingmtl.begin(RCFD);
      for (auto &pl : _pointlights) {
        _lightingmtl._lightmatrix.ComposeMatrix(pl._pos, fquat(), pl._radius);
        if (is_stereo_1pass) {
          auto L = CPD._stereoCameraMatrices->_left;
          auto R = CPD._stereoCameraMatrices->_right;
          fmtx4 mvpL = _lightingmtl._lightmatrix * (L->_vmatrix * L->_pmatrix);
          fmtx4 mvpR = _lightingmtl._lightmatrix * (R->_vmatrix * R->_pmatrix);
          FXI->BindParamMatrix(_lightingmtl._shader, _lightingmtl._parMatMVPL,
                               mvpL);
          FXI->BindParamMatrix(_lightingmtl._shader, _lightingmtl._parMatMVPR,
                               mvpR);
        } else {
          auto M = CPD._cameraMatrices;
          fmtx4 mvp = _lightingmtl._lightmatrix * (M->_vmatrix * M->_pmatrix);
          FXI->BindParamMatrix(_lightingmtl._shader, _lightingmtl._parMatMVPC,
                               mvp);
        }
        FXI->BindParamVect4(_lightingmtl._shader, _lightingmtl._parLightPosR,
                            fvec4(pl._pos, pl._radius));
        FXI->BindParamVect3(_lightingmtl._shader, _lightingmtl._parLightColor,
                            pl._color);
        FXI->CommitParams();
        targ->GBI()->DrawPrimitiveEML(GfxPrimitives::GetFullSphere());
        //////////////////////////
        if (pl._counter < 1) {
          pl.next();
        } else {
          fvec3 delta = pl._dst - pl._pos;
          pl._pos += delta.Normal() * 0.5;
          pl._counter--;
        }
      }
      _lightingmtl.end(RCFD);
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
  DeferredMaterial _lightingmtl;
  RtGroup *_rtgGbuffer = nullptr;
  RtGroup *_rtgLaccum = nullptr;
  fmtx4 _viewOffsetMatrix;
  int _width = 0;
  int _height = 0;
  std::vector<PointLight> _pointlights;
};
} // namespace deferrednode

///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNode::DeferredCompositingNode() {
  _impl = std::make_shared<deferrednode::IMPL>();
}
///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNode::~DeferredCompositingNode() {}
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNode::DoInit(lev2::GfxTarget *pTARG, int iW, int iH) {
  _impl.Get<std::shared_ptr<deferrednode::IMPL>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNode::DoRender(CompositorDrawData &drawdata) {
  auto impl = _impl.Get<std::shared_ptr<deferrednode::IMPL>>();
  impl->_render(this, drawdata);
}
///////////////////////////////////////////////////////////////////////////////
RtBuffer *DeferredCompositingNode::GetOutput() const {
  static int i = 0;
  i++;
  return _impl.Get<std::shared_ptr<deferrednode::IMPL>>()->_rtgLaccum->GetMrt(
      0);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace lev2
} // namespace ork
