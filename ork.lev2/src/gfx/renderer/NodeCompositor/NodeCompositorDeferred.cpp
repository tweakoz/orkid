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
#include <ork/lev2/gfx/material_freestyle.inl>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>

#include "NodeCompositorDeferred.h"

ImplementReflectionX(ork::lev2::DeferredCompositingNode, "DeferredCompositingNode");

constexpr bool USE_UBO = true;

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

  void next() {
    float x  = float((rand() % 4096) - 2048);
    float z  = float((rand() % 4096) - 2048);
    float y  = float(100 + ((rand() % 200) - 100));
    _dst     = fvec3(x, y, z);
    _counter = 200 + rand() % 200;
  }
};

struct IMPL {
  ///////////////////////////////////////
  IMPL()
      : _camname(AddPooledString("Camera")) {
    _layername = "All"_pool;

    for (int i = 0; i < 128; i++) {

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
      _lightingmtl.gpuInit(pTARG,"orkshader://deferred");
      _tekBaseLighting        = _lightingmtl.technique("baselight");
      _tekBaseLightingStereo  = _lightingmtl.technique("baselight_stereo");
      _tekPointLighting       = _lightingmtl.technique("pointlight");
      _tekPointLightingStereo = _lightingmtl.technique("pointlight_stereo");
      //////////////////////////////////////////////////////////////
      if( USE_UBO ){
        auto block = _lightingmtl.paramBlock("ub_vtx");
        _parMatMVPC      = block->param("MVPC");
        _parMatMVPL      = block->param("MVPL");
        _parMatMVPR      = block->param("MVPR");
      }
      else{
        _parMatMVPC      = _lightingmtl.param("MVPC");
        _parMatMVPL      = _lightingmtl.param("MVPL");
        _parMatMVPR      = _lightingmtl.param("MVPR");
      }
      _parMatIVPArray  = _lightingmtl.param("IVPArray");
      _parLightColor   = _lightingmtl.param("LightColor");
      _parLightPosR    = _lightingmtl.param("LightPosR");
      _parMapGBufAlbAo = _lightingmtl.param("MapAlbedoAo");
      _parMapGBufNrmL  = _lightingmtl.param("MapNormalL");
      _parMapDepth     = _lightingmtl.param("MapDepth");
      _parInvViewSize  = _lightingmtl.param("InvViewportSize");
      _parTime         = _lightingmtl.param("Time");
      //////////////////////////////////////////////////////////////
      _rtgGbuffer = new RtGroup(pTARG, 8, 8, NUMSAMPLES);
      auto buf0   = new RtBuffer(_rtgGbuffer, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA8, 8, 8);
      auto buf1   = new RtBuffer(_rtgGbuffer, lev2::ETGTTYPE_MRT1, lev2::EBUFFMT_RGB10A2, 8, 8);
      // auto buf2        = new RtBuffer(_rtgGbuffer, lev2::ETGTTYPE_MRT2,
      // lev2::EBUFFMT_RGBA32F, 8, 8);
      buf0->_debugName = "DeferredRtAlbAo";
      buf1->_debugName = "DeferredRRufMtl";
      // buf2->_debugName = "DeferredRtNormalDist";
      _rtgGbuffer->SetMrt(0, buf0);
      _rtgGbuffer->SetMrt(1, buf1);
      //_rtgGbuffer->SetMrt(2, buf2);
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
    auto FBI                     = targ->FBI();
    auto FXI                     = targ->FXI();
    auto RSI                     = targ->RSI();
    auto GBI                     = targ->GBI();
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
      FBI->PushRtGroup(_rtgGbuffer);
      FBI->SetAutoClear(false); // explicit clear
      targ->BeginFrame();
      /////////////////////////////////////////////////////////////////////////////////////////
      auto DB              = RCFD.GetDB();
      auto CPD             = CIMPL->topCPD();
      bool is_stereo_1pass = CPD.isStereoOnePass();
      CPD._clearColor      = node->_clearColor;
      CPD.mpLayerName      = &_layername;
      CPD._irendertarget   = &rtgbuf;
      CPD.SetDstRect(tgt_rect);
      CPD._passID       = "defgbuffer1"_crcu;
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
        for (const PoolString& layer_name : CPD.getLayerNames()) {
          targ->debugMarker(FormatString("Deferred::renderEnqueuedScene::layer<%s>", layer_name.c_str()));
          DB->enqueueLayerToRenderQueue(layer_name, irenderer);
        }
        /////////////////////////////////////////////////
        auto MTXI = targ->MTXI();
        CIMPL->pushCPD(CPD);
        targ->debugPushGroup("toolvp::DrawEnqRenderables");
        FBI->Clear(node->_clearColor, 1.0f);
        irenderer->drawEnqueuedRenderables();
        framerenderer.renderMisc();
        targ->debugPopGroup();
        CIMPL->popCPD();
      }
      /////////////////////////////////////////////////////////////////////////////////////////
      targ->EndFrame();
      FBI->PopRtGroup();
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
      FBI->SetAutoClear(false);
      FBI->PushRtGroup(_rtgLaccum);
      targ->BeginFrame();
      FBI->Clear(fvec4(0.1, 0.2, 0.3, 1), 1.0f);
      auto this_buf = FBI->GetThisBuffer();
      //////////////////////////////////////////////////////////////////
      // base lighting
      //////////////////////////////////////////////////////////////////
      _ivp[0] = IVPL;
      _ivp[1] = IVPR;
      _lightingmtl.bindTechnique(is_stereo_1pass ? _tekBaseLightingStereo : _tekBaseLighting);
      _lightingmtl.mRasterState.SetBlending(EBLENDING_OFF);
      _lightingmtl.mRasterState.SetDepthTest(EDEPTHTEST_OFF);
      _lightingmtl.mRasterState.SetCullTest(ECULLTEST_PASS_BACK);
      _lightingmtl.begin(RCFD);
          //////////////////////////////////////////////////////
          if( USE_UBO ){
            auto map_block = _lightingmtl.paramBlock("ub_vtx")->map();
            map_block->setMatrix(_parMatMVPL, fmtx4());
            map_block->setMatrix(_parMatMVPC, fmtx4());
            map_block->setMatrix(_parMatMVPR, fmtx4());
            map_block->unmap();
          }
          else {
            _lightingmtl.bindParamMatrix(_parMatMVPL, fmtx4());
            _lightingmtl.bindParamMatrix(_parMatMVPC, fmtx4());
            _lightingmtl.bindParamMatrix(_parMatMVPR, fmtx4());
          }
          //////////////////////////////////////////////////////
          _lightingmtl.bindParamMatrixArray(_parMatIVPArray, _ivp, 2);
          _lightingmtl.bindParamCTex(_parMapGBufAlbAo, _rtgGbuffer->GetMrt(0)->GetTexture());
          _lightingmtl.bindParamCTex(_parMapGBufNrmL, _rtgGbuffer->GetMrt(1)->GetTexture());
          _lightingmtl.bindParamCTex(_parMapDepth, _rtgGbuffer->_depthTexture);
          _lightingmtl.bindParamVec2(_parInvViewSize, fvec2(1.0 / float(_width), 1.0f / float(_height)));
          _lightingmtl.commit();
          this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1));
      _lightingmtl.end(RCFD);
      CIMPL->popCPD();
      //////////////////////////////////////////////////////////////////
      // point lighting
      //  todo : batch multiple lights together
      //   compute screen aligned quad for batch..
      // accumulate pointlights
      //////////////////////////////////////////////////////////////////
      //_lightingmtl.bindParamVec3(_parLightColor, fvec3(0.8, 0.7, 0.2));
      //_lightingmtl.bindParamVec4(_parLightPosR, fvec4(campos_mono, 100.0f));

      static float ftime = 0.0f;
      CPD                = CIMPL->topCPD();
      CPD.SetDstRect(vprect);
      CPD._irendertarget = &rtlaccum;
      CIMPL->pushCPD(CPD);
      FBI->PushViewport(vprect); // stereo viewport
      _lightingmtl.mRasterState.SetBlending(EBLENDING_ADDITIVE);
      //_lightingmtl.mRasterState.SetBlending(EBLENDING_OFF);
      _lightingmtl.mRasterState.SetDepthTest(EDEPTHTEST_OFF);
      _lightingmtl.mRasterState.SetCullTest(ECULLTEST_PASS_BACK);
      _lightingmtl.bindTechnique(is_stereo_1pass ? _tekPointLightingStereo : _tekPointLighting);
      _lightingmtl.begin(RCFD);
      //////////////////////////////////////////////////////
      if( USE_UBO ){
          auto mapped_block = _lightingmtl.paramBlock("ub_vtx")->map();
          mapped_block->setMatrix(_parMatMVPL, fmtx4());
          mapped_block->setMatrix(_parMatMVPC, fmtx4());
          mapped_block->setMatrix(_parMatMVPR, fmtx4());
          mapped_block->unmap();
        }
        else {
          _lightingmtl.bindParamMatrix(_parMatMVPL, fmtx4());
          _lightingmtl.bindParamMatrix(_parMatMVPC, fmtx4());
          _lightingmtl.bindParamMatrix(_parMatMVPR, fmtx4());
        }
      //////////////////////////////////////////////////////
      _lightingmtl.bindParamMatrixArray(_parMatIVPArray, _ivp, 2);
      _lightingmtl.bindParamCTex(_parMapGBufAlbAo, _rtgGbuffer->GetMrt(0)->GetTexture());
      _lightingmtl.bindParamCTex(_parMapGBufNrmL, _rtgGbuffer->GetMrt(1)->GetTexture());
      _lightingmtl.bindParamCTex(_parMapDepth, _rtgGbuffer->_depthTexture);
      _lightingmtl.bindParamVec2(_parInvViewSize, fvec2(1.0 / float(_width), 1.0f / float(_height)));
      _lightingmtl.bindParamVec3(_parLightColor, fvec3(0.8, 0.7, 0.2));
      _lightingmtl.bindParamVec4(_parLightPosR, fvec4(campos_mono, 100.0f));

      ECullTest eprevculltest = ECULLTEST_OFF;

          for (auto& pl : _pointlights) {


            float dist_to_light = (campos_mono-pl._pos).Mag();
            bool cam_inside_light = dist_to_light<=pl._radius;
            ECullTest cur_cull_test = cam_inside_light ? ECULLTEST_PASS_FRONT : ECULLTEST_PASS_BACK;

            if( eprevculltest != cur_cull_test ){
              _lightingmtl.mRasterState.SetCullTest(cur_cull_test);
              RSI->BindRasterState(_lightingmtl.mRasterState);
            }
            eprevculltest = cur_cull_test;

            fmtx4 LIGHTMTX;
            LIGHTMTX.ComposeMatrix(pl._pos, fquat(), pl._radius);
            if (is_stereo_1pass) {
              auto L     = CPD._stereoCameraMatrices->_left;
              auto R     = CPD._stereoCameraMatrices->_right;
              fmtx4 mvpL = LIGHTMTX * (L->_vmatrix * L->_pmatrix);
              fmtx4 mvpR = LIGHTMTX * (R->_vmatrix * R->_pmatrix);
              _lightingmtl.bindParamMatrix(_parMatMVPL, mvpL);
              _lightingmtl.bindParamMatrix(_parMatMVPR, mvpR);
            } else {
              auto M    = CPD._cameraMatrices;
              fmtx4 mvp = LIGHTMTX * (M->_vmatrix * M->_pmatrix);
              _lightingmtl.bindParamMatrix(_parMatMVPC, mvp);
            }
            _lightingmtl.bindParamVec4(_parLightPosR, fvec4(pl._pos, pl._radius));
            _lightingmtl.bindParamVec3(_parLightColor, pl._color);
            _lightingmtl.commit();
            GBI->DrawPrimitiveEML(GfxPrimitives::GetFullSphere());
          }
      _lightingmtl.end(RCFD);
      FBI->PopViewport();
      ftime += 0.01f;
      //////////////////////////////////////////////////////////////////
      //targ->EndFrame();
      targ->EndFrame();
      FBI->PopRtGroup();
      targ->debugPopGroup();
      CIMPL->popCPD();
      //////////////////////////////////////////////////////////////////
      // update pointlights
      //////////////////////////////////////////////////////////////////
      for (auto& pl : _pointlights) {
        if (pl._counter < 1) {
          pl.next();
        } else {
          fvec3 delta = pl._dst - pl._pos;
          pl._pos += delta.Normal() * 0.5;
          pl._counter--;
        }
      }
      /////////////////////////////////////////////////////////////////////////////////////////
    }
    targ->debugPopGroup();
  }
  ///////////////////////////////////////
  PoolString _camname, _layername;
  FreestyleMaterial _lightingmtl;

  const FxShaderTechnique* _tekBaseLighting        = nullptr;
  const FxShaderTechnique* _tekPointLighting       = nullptr;
  const FxShaderTechnique* _tekBaseLightingStereo  = nullptr;
  const FxShaderTechnique* _tekPointLightingStereo = nullptr;
  const FxShaderParam* _parMatMVPC                 = nullptr;
  const FxShaderParam* _parMatMVPL                 = nullptr;
  const FxShaderParam* _parMatMVPR                 = nullptr;
  const FxShaderParam* _parMatIVPArray             = nullptr;
  const FxShaderParam* _parMapGBufAlbAo            = nullptr;
  const FxShaderParam* _parMapGBufNrmL             = nullptr;
  const FxShaderParam* _parMapDepth                = nullptr;
  const FxShaderParam* _parTime                    = nullptr;
  const FxShaderParam* _parInvViewSize             = nullptr;
  const FxShaderParam* _parInvVpDim                = nullptr;
  const FxShaderParam* _parLightPosR               = nullptr;
  const FxShaderParam* _parLightColor              = nullptr;
  const FxShaderParam* _parLightPos                = nullptr;

  RtGroup* _rtgGbuffer = nullptr;
  RtGroup* _rtgLaccum  = nullptr;
  fmtx4 _viewOffsetMatrix;
  int _width  = 0;
  int _height = 0;
  fmtx4 _ivp[2];
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
