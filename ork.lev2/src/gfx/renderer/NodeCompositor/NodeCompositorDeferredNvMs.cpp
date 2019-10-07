////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <algorithm>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/material_freestyle.inl>

#include "NodeCompositorDeferred.h"

ImplementReflectionX(ork::lev2::DeferredCompositingNodeNvMs,
                     "DeferredCompositingNodeNvMs");

// fvec3 LightColor
// fvec4 LightPosR 16byte
///////////////////////////////////////////////////////////////////////////////
namespace ork {
namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNodeNvMs::describeX(class_t *c) {
  c->memberProperty("ClearColor", &DeferredCompositingNodeNvMs::_clearColor);
  c->memberProperty("FogColor", &DeferredCompositingNodeNvMs::_fogColor);
}
///////////////////////////////////////////////////////////////////////////
constexpr int NUMSAMPLES = 1;
///////////////////////////////////////////////////////////////////////////////
namespace deferrednodenvms {

struct PointLight {
  fvec3 _pos;
  fvec3 _dst;
  fvec3 _color;
  float _radius;
  int _counter = 0;

  void next() {
    float x = float((rand() % 4096) - 2048);
    float z = float((rand() % 4096) - 2048);
    float y = float(100 + ((rand() % 800) - 400));
    _dst = fvec3(x, y, z);
    _counter = 200 + rand() % 200;
  }
};

struct IMPL {
  static constexpr size_t KMAXLIGHTS = 2048;
  static constexpr int KTILEDIMX = 32;
  static constexpr int KTILEDIMY = 16;
  static constexpr int KTILECOUNT = KTILEDIMX*KTILEDIMY;
  static constexpr float KNEAR = 0.1f;
  static constexpr float KFAR = 100000.0f;
  ///////////////////////////////////////
  IMPL() : _camname(AddPooledString("Camera")) {
    _layername = "All"_pool;

    for (int i = 0; i < 256; i++) {

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
    auto FXI = pTARG->FXI();
    if (nullptr == _rtgGbuffer) {
      for( int i=0; i<KTILECOUNT; i++ ){
        _lighttiles.push_back(pllist_t());
      }
      //////////////////////////////////////////////////////////////
      _lightingmtl.gpuInit(pTARG, "orkshader://deferrednvms");
      _tekBaseLighting = _lightingmtl.technique("baselight");
      _tekPointLighting = _lightingmtl.technique("pointlight");
      _tekBaseLightingStereo = _lightingmtl.technique("baselight_stereo");
      _tekPointLightingStereo = _lightingmtl.technique("pointlight_stereo");
      //////////////////////////////////////////////////////////////
      // init lightblock
      //////////////////////////////////////////////////////////////
      _lightbuffer = pTARG->FXI()->createParamBuffer(KMAXLIGHTS*32);
      _lightblock = _lightingmtl.paramBlock("ub_light");
      auto mapped = FXI->mapParamBuffer(_lightbuffer);
          size_t base = 0;
          for(int i=0; i<KMAXLIGHTS; i++)
            mapped->ref<fvec3>(base+i*sizeof(fvec4)) = fvec3(0,0,0);
          base += KMAXLIGHTS*sizeof(fvec4);
          for(int i=0; i<KMAXLIGHTS; i++)
            mapped->ref<fvec4>(base+i*sizeof(fvec4)) = fvec4();
      mapped->unmap();
      //////////////////////////////////////////////////////////////
      _parMatIVPArray = _lightingmtl.param("IVPArray");
      _parMatVArray = _lightingmtl.param("VArray");
      _parMatPArray = _lightingmtl.param("PArray");
      _parMapGBufAlbAo = _lightingmtl.param("MapAlbedoAo");
      _parMapGBufNrmL = _lightingmtl.param("MapNormalL");
      _parMapDepth = _lightingmtl.param("MapDepth");
      _parInvViewSize = _lightingmtl.param("InvViewportSize");
      _parTime = _lightingmtl.param("Time");
      _parNumLights = _lightingmtl.param("NumLights");
      _parNearFar = _lightingmtl.param("NearFar");
      _parZndc2eye = _lightingmtl.param("Zndc2eye");
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
  void _render(DeferredCompositingNodeNvMs *node, CompositorDrawData &drawdata) {
    FrameRenderer &framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData &RCFD = framerenderer.framedata();
    auto CIMPL = drawdata._cimpl;
    auto targ = RCFD.GetTarget();
    auto FBI = targ->FBI();
    auto FXI = targ->FXI();
    auto TXI = targ->TXI();
    auto RSI = targ->RSI();
    auto GBI = targ->GBI();
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
      FBI->PushRtGroup(_rtgGbuffer);
      FBI->SetAutoClear(false); // explicit clear
      targ->BeginFrame();
      /////////////////////////////////////////////////////////////////////////////////////////
      auto DB = RCFD.GetDB();
      const auto TOPCPD = CIMPL->topCPD();
      auto CPD = TOPCPD;
      bool is_stereo_1pass = CPD.isStereoOnePass();
      CPD._clearColor = node->_clearColor;
      CPD.mpLayerName = &_layername;
      CPD._irendertarget = &rtgbuf;
      CPD.SetDstRect(tgt_rect);
      CPD._passID = "defgbuffer1"_crcu;
      fvec3 campos_mono = CPD.monoCamPos(fmtx4());
      fmtx4 IVPL, IVPR, IVPM;
      fmtx4 VL, VR, VM;
      fmtx4 PL, PR, PM;
      fmtx4 VPL, VPR, VPM;
      if (is_stereo_1pass) {
        auto L = CPD._stereoCameraMatrices->_left;
        auto R = CPD._stereoCameraMatrices->_right;
        auto M = CPD._stereoCameraMatrices->_mono;
        VL = L->_vmatrix;
        VR = R->_vmatrix;
        VM = M->_vmatrix;
        PL = L->_pmatrix;
        PR = R->_pmatrix;
        PM = M->_pmatrix;
        VPL = VL * PL;
        VPR = VR * PR;
        VPM = VM * PM;
      } else {
        auto M = CPD._cameraMatrices;
        VM = M->_vmatrix;
        PM = M->_pmatrix;
        VL = VM;
        VR = VM;
        PL = PM;
        PR = PM;
        VPM = VM*PM;
        VPL = VPM;
        VPR = VPM;
      }
      IVPM.inverseOf(VPM);
      IVPL.inverseOf(VPL);
      IVPR.inverseOf(VPR);
      _v[0] = VL;
      _v[1] = VR;
      _p[0] = PL; //_p[0].Transpose();
      _p[1] = PR; //_p[1].Transpose();
      _ivp[0] = IVPL;
      _ivp[1] = IVPR;
      fmtx4 IVL; IVL.inverseOf(VL);
      campos_mono = IVL.GetColumn(3).xyz();
      //printf( "campos<%g %g %g>\n", campos_mono.x, campos_mono.y, campos_mono.z );
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
        FBI->Clear(node->_clearColor, 1.0f);
        irenderer->drawEnqueuedRenderables();
        framerenderer.renderMisc();
        targ->debugPopGroup();
        CIMPL->popCPD();
      }
      /////////////////////////////////////////////////////////////////////////////////////////
      targ->EndFrame();
      FBI->PopRtGroup();
      //TXI->generateMipMaps(_rtgGbuffer->_depthTexture);
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
        targ->debugPushGroup("Deferred::Lighting");
          FBI->SetAutoClear(false);
          FBI->PushRtGroup(_rtgLaccum);
          targ->BeginFrame();
          FBI->Clear(fvec4(0.1, 0.2, 0.3, 1), 1.0f);
          auto this_buf = FBI->GetThisBuffer();
          //////////////////////////////////////////////////////////////////
          // base lighting
          //////////////////////////////////////////////////////////////////
          targ->debugPushGroup("Deferred::BaseLighting");
          _lightingmtl.bindTechnique(is_stereo_1pass?_tekBaseLightingStereo:_tekBaseLighting);
          _lightingmtl.mRasterState.SetBlending(EBLENDING_OFF);
          _lightingmtl.mRasterState.SetDepthTest(EDEPTHTEST_OFF);
          _lightingmtl.mRasterState.SetCullTest(ECULLTEST_PASS_BACK);
          _lightingmtl.begin(RCFD);
          //////////////////////////////////////////////////////
          _lightingmtl.bindParamMatrixArray(_parMatIVPArray, _ivp, 2);
          _lightingmtl.bindParamMatrixArray(_parMatVArray, _v, 2);
          _lightingmtl.bindParamMatrixArray(_parMatPArray, _p, 2);
          _lightingmtl.bindParamCTex(_parMapGBufAlbAo,
                                     _rtgGbuffer->GetMrt(0)->GetTexture());
          _lightingmtl.bindParamCTex(_parMapGBufNrmL,
                                     _rtgGbuffer->GetMrt(1)->GetTexture());
          _lightingmtl.bindParamCTex(_parMapDepth, _rtgGbuffer->_depthTexture);
          _lightingmtl.bindParamVec2(_parNearFar,fvec2(KNEAR,KFAR));
          _lightingmtl.bindParamVec2(
              _parInvViewSize, fvec2(1.0 / float(_width), 1.0f / float(_height)));
          _lightingmtl.commit();
          this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1));
          _lightingmtl.end(RCFD);
        CIMPL->popCPD();
      targ->debugPopGroup(); // BaseLighting
      //////////////////////////////////////////////////////////////////
      // point lighting
      //  todo : batch multiple lights together
      //   compute screen aligned quad for batch..
      // accumulate pointlights
      //////////////////////////////////////////////////////////////////
      targ->debugPushGroup("Deferred::PointLighting");
      _lightingmtl.bindTechnique(is_stereo_1pass?_tekPointLightingStereo:_tekPointLighting);
      _lightingmtl.begin(RCFD);
      //////////////////////////////////////////////////////
      FXI->bindParamBlockBuffer(_lightblock, _lightbuffer);
      //////////////////////////////////////////////////////
      auto Zndc2eye = fvec2(
        _p[0].GetElemXY(3,2),
        _p[0].GetElemXY(2,2)
      );
      //////////////////////////////////////////////////////
      _lightingmtl.bindParamMatrixArray(_parMatIVPArray, _ivp, 2);
      _lightingmtl.bindParamMatrixArray(_parMatVArray, _v, 2);
      _lightingmtl.bindParamMatrixArray(_parMatPArray, _p, 2);
      _lightingmtl.bindParamCTex(_parMapGBufAlbAo,
                                 _rtgGbuffer->GetMrt(0)->GetTexture());
      _lightingmtl.bindParamCTex(_parMapGBufNrmL,
                                 _rtgGbuffer->GetMrt(1)->GetTexture());
      _lightingmtl.bindParamCTex(_parMapDepth, _rtgGbuffer->_depthTexture);
      _lightingmtl.bindParamVec2(_parNearFar,fvec2(KNEAR,KFAR));
      _lightingmtl.bindParamVec2(_parZndc2eye,Zndc2eye);
      _lightingmtl.bindParamVec2(
          _parInvViewSize, fvec2(1.0 / float(_width), 1.0f / float(_height)));
      //////////////////////////////////////////////////
      _lightingmtl.mRasterState.SetCullTest(ECULLTEST_OFF);
      _lightingmtl.mRasterState.SetBlending(EBLENDING_ADDITIVE);
      _lightingmtl.mRasterState.SetDepthTest(EDEPTHTEST_OFF);
      RSI->BindRasterState(_lightingmtl.mRasterState);
      constexpr size_t KPOSPASE = KMAXLIGHTS*sizeof(fvec4);
      //////////////////////////////////////////////////
      // upload lights
      //////////////////////////////////////////////////
      auto mapped = FXI->mapParamBuffer(_lightbuffer,0,_pointlights.size()*32);
          for( size_t lidx=0; lidx<_pointlights.size(); lidx++ ){
            const auto& light = _pointlights[lidx];
            float mindepth = (light._pos-campos_mono).Mag();
            fvec4 colord(light._color,mindepth);
            mapped->ref<fvec4>(lidx*sizeof(fvec4)) = colord;
          }
      mapped->unmap();
      //////////////////////////////////////////////////
      // launch nvmeshshader
      //////////////////////////////////////////////////
      GBI->DrawMeshTasksNV(0,1);
      //////////////////////////////////////////////////
      _lightingmtl.end(RCFD);
      targ->debugPopGroup(); // PointLighting
      //////////////////////////////////////////////////////////////////
      // clear lighttiles
      //////////////////////////////////////////////////////////////////
      for( int i=0; i<KTILECOUNT; i++ ){
        _lighttiles[i].clear();
      }
      //////////////////////////////////////////////////////////////////
      // update pointlights
      //////////////////////////////////////////////////////////////////
      for (auto &pl : _pointlights) {
        if (pl._counter < 1) {
          pl.next();
        } else {
          fvec3 delta = pl._dst - pl._pos;
          pl._pos += delta.Normal() * 0.5;
          pl._counter--;
        }
      }
      /////////////////////////////////////////////////////////////////////////////////////////
      // end frame
      /////////////////////////////////////////////////////////////////////////////////////////
      targ->EndFrame();
      FBI->PopRtGroup();
      targ->debugPopGroup(); // Lighting
      //CIMPL->popCPD();
      /////////////////////////////////////////////////////////////////////////////////////////
    }
    targ->debugPopGroup();
  }
  ///////////////////////////////////////
  PoolString _camname, _layername;
  FreestyleMaterial _lightingmtl;

  const FxShaderTechnique *_tekBaseLighting = nullptr;
  const FxShaderTechnique *_tekPointLighting = nullptr;
  const FxShaderTechnique *_tekBaseLightingStereo = nullptr;
  const FxShaderTechnique *_tekPointLightingStereo = nullptr;
  const FxShaderParam *_parMatIVPArray = nullptr;
  const FxShaderParam *_parMatPArray = nullptr;
  const FxShaderParam *_parMatVArray = nullptr;
  const FxShaderParam *_parZndc2eye = nullptr;
  const FxShaderParam *_parMapGBufAlbAo = nullptr;
  const FxShaderParam *_parMapGBufNrmL = nullptr;
  const FxShaderParam *_parMapDepth = nullptr;
  const FxShaderParam *_parTime = nullptr;
  const FxShaderParam *_parNearFar = nullptr;
  const FxShaderParam *_parInvViewSize = nullptr;
  const FxShaderParam *_parInvVpDim = nullptr;
  const FxShaderParam *_parNumLights = nullptr;
  const FxShaderParamBlock *_lightblock = nullptr;
  FxShaderParamBuffer *_lightbuffer = nullptr;

  RtGroup *_rtgGbuffer = nullptr;
  RtGroup *_rtgLaccum = nullptr;
  fmtx4 _viewOffsetMatrix;
  int _width = 0;
  int _height = 0;
  fmtx4 _ivp[2];
  fmtx4 _v[2];
  fmtx4 _p[2];
  std::vector<PointLight> _pointlights;
  Timer _timer;
  typedef std::vector<const PointLight*> pllist_t;
  ork::fixedvector<pllist_t,KTILECOUNT> _lighttiles;
};
} // namespace deferrednodenvms

///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNodeNvMs::DeferredCompositingNodeNvMs() {
  _impl = std::make_shared<deferrednodenvms::IMPL>();
}
///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNodeNvMs::~DeferredCompositingNodeNvMs() {}
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNodeNvMs::DoInit(lev2::GfxTarget *pTARG, int iW, int iH) {
  _impl.Get<std::shared_ptr<deferrednodenvms::IMPL>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNodeNvMs::DoRender(CompositorDrawData &drawdata) {
  auto impl = _impl.Get<std::shared_ptr<deferrednodenvms::IMPL>>();
  impl->_render(this, drawdata);
}
///////////////////////////////////////////////////////////////////////////////
RtBuffer *DeferredCompositingNodeNvMs::GetOutput() const {
  static int i = 0;
  i++;
  return _impl.Get<std::shared_ptr<deferrednodenvms::IMPL>>()->_rtgLaccum->GetMrt(
      0);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace lev2
} // namespace ork
