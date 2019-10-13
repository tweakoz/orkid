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

ImplementReflectionX(ork::lev2::DeferredCompositingNode,
                     "DeferredCompositingNode");

// fvec3 LightColor
// fvec4 LightPosR 16byte
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
  float _mindepth = 0;

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

    for (int i = 0; i < 32; i++) {

      PointLight p;
      p.next();
      p._color.x = float(rand() & 0xff) / 128.0;
      p._color.y = float(rand() & 0xff) / 128.0;
      p._color.z = float(rand() & 0xff) / 128.0;
      p._radius = 128.0f;
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
      _lightingmtl.gpuInit(pTARG, "orkshader://deferred");
      _tekBaseLighting = _lightingmtl.technique("baselight");
      _tekPointLighting = _lightingmtl.technique("pointlight");
      _tekBaseLightingStereo = _lightingmtl.technique("baselight_stereo");
      _tekPointLightingStereo = _lightingmtl.technique("pointlight_stereo");
      _tekDownsampleDepthMinMax = _lightingmtl.technique("downsampledepthminmax");
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
      _parMapDepthMinMax = _lightingmtl.param("MapDepthMinMax");
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
      buf0->_debugName = "DeferredRtAlbAo";
      buf1->_debugName = "DeferredRtNormalDist";
      _rtgGbuffer->SetMrt(0, buf0);
      _rtgGbuffer->SetMrt(1, buf1);
      //////////////////////////////////////////////////////////////
      _rtgMinMaxD = new RtGroup(pTARG, 8, 8, NUMSAMPLES);
      auto bufD = new RtBuffer(_rtgMinMaxD, lev2::ETGTTYPE_MRT0,
                               lev2::EBUFFMT_RG32F, 8, 8);
      bufD->_debugName = "DeferredMinMaxD";
      _rtgMinMaxD->SetMrt(0, bufD);
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
    auto FBI = targ->FBI();
    auto FXI = targ->FXI();
    auto TXI = targ->TXI();
    auto RSI = targ->RSI();
    auto GBI = targ->GBI();
    auto &ddprops = drawdata._properties;
    auto this_buf = FBI->GetThisBuffer();
    SRect tgt_rect(0, 0, targ->GetW(), targ->GetH());
    //////////////////////////////////////////////////////
    // Resize RenderTargets
    //////////////////////////////////////////////////////
    int newwidth = ddprops["OutputWidth"_crcu].Get<int>();
    int newheight = ddprops["OutputHeight"_crcu].Get<int>();
    if (_rtgGbuffer->GetW() != newwidth or _rtgGbuffer->GetH() != newheight) {
      _width = newwidth;
      _height = newheight;
      _minmaxW = (newwidth+31)/32;
      _minmaxH = (newheight+31)/32;
      _rtgGbuffer->Resize(newwidth, newheight);
      _rtgLaccum->Resize(newwidth, newheight);
      _rtgMinMaxD->Resize( _minmaxW, _minmaxH );
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
      /////////////////////////////////////////////////////////////////////////////////////////
      // gen minmax tile depth image
      /////////////////////////////////////////////////////////////////////////////////////////



      RtGroupRenderTarget rtlminmaxd(_rtgMinMaxD);
      auto vprect = SRect(0, 0, _minmaxW, _minmaxH);
      auto quadrect = SRect(0, 0, _minmaxW, _minmaxH);
      CPD.SetDstRect(vprect);
      CPD._irendertarget        = &rtlminmaxd;
      CPD._cameraMatrices       = nullptr;
      CPD._stereoCameraMatrices = nullptr;
      CPD._stereo1pass          = false;
      CIMPL->pushCPD(CPD);
      targ->debugPushGroup("Deferred::minmaxd");
      {
        FBI->PushRtGroup(_rtgMinMaxD);
        targ->BeginFrame();
        _lightingmtl.bindTechnique(_tekDownsampleDepthMinMax);
        _lightingmtl.begin(RCFD);
        _lightingmtl.bindParamCTex(_parMapDepth, _rtgGbuffer->_depthTexture);
        _lightingmtl.bindParamVec2(_parNearFar,fvec2(KNEAR,KFAR));
        _lightingmtl.bindParamVec2(
            _parInvViewSize, fvec2(1.0 / float(_width), 1.0f / float(_height)));
        _lightingmtl.mRasterState.SetBlending(EBLENDING_OFF);
        _lightingmtl.mRasterState.SetDepthTest(EDEPTHTEST_OFF);
        _lightingmtl.mRasterState.SetCullTest(ECULLTEST_PASS_BACK);
        _lightingmtl.commit();
        this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1));
        _lightingmtl.end(RCFD);
        targ->EndFrame();
        FBI->PopRtGroup();
      }
      targ->debugPopGroup(); // MinMaxD
      CIMPL->popCPD();       // MinMaxD
      // TXI->generateMipMaps(_rtgGbuffer->_depthTexture);

      /////////////////////////////////////////////////////////////////////////////////////////
      // Light Accumulation
      /////////////////////////////////////////////////////////////////////////////////////////
      RtGroupRenderTarget rtlaccum(_rtgLaccum);
      vprect = SRect(0, 0, _width, _height);
      quadrect = SRect(0, 0, _width, _height);
      CPD.SetDstRect(vprect);
      CPD._irendertarget = &rtlaccum;
      CPD._cameraMatrices = nullptr;
      CPD._stereoCameraMatrices = nullptr;
      CPD._stereo1pass = false;
      CIMPL->pushCPD(CPD);
      targ->debugPushGroup("Deferred::Lighting");
      FBI->SetAutoClear(false);
      FBI->PushRtGroup(_rtgLaccum);
      FBI->SetAutoClear(true);
      targ->BeginFrame();
      FBI->Clear(fvec4(0.1, 0.2, 0.3, 1), 1.0f);
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
      static float ftime = 0.0f;
      CIMPL->pushCPD(CPD);
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
      _lightingmtl.bindParamCTex(_parMapDepthMinMax,
                                 _rtgMinMaxD->GetMrt(0)->GetTexture());
      _lightingmtl.bindParamVec2(_parNearFar,fvec2(KNEAR,KFAR));
      _lightingmtl.bindParamVec2(_parZndc2eye,Zndc2eye);
      _lightingmtl.bindParamVec2(
          _parInvViewSize, fvec2(1.0 / float(_width), 1.0f / float(_height)));
      //////////////////////////////////////////////////
      // outside lights
      //////////////////////////////////////////////////
      _lightingmtl.mRasterState.SetCullTest(ECULLTEST_OFF);
      _lightingmtl.mRasterState.SetBlending(EBLENDING_ADDITIVE);
      //_lightingmtl.mRasterState.SetBlending(EBLENDING_OFF);
      _lightingmtl.mRasterState.SetDepthTest(EDEPTHTEST_OFF);
      RSI->BindRasterState(_lightingmtl.mRasterState);
      constexpr size_t KPOSPASE = KMAXLIGHTS*sizeof(fvec4);
      /////////////////////////////////////
      constexpr int KTILEMAXX = KTILEDIMX-1;
      constexpr int KTILEMAXY = KTILEDIMY-1;
      if(false){ // tiled lighting
        size_t tilelights = 0;
        _timer.Start();
        for( size_t lidx=0; lidx<_pointlights.size(); lidx++ ){
          auto& light = _pointlights[lidx];
          Sphere sph(light._pos,light._radius);
          auto box = sph.projectedBounds(VPL);
          auto bmin = ((box.Min()+fvec3(1,1,1))*0.5);
          auto bmax = ((box.Max()+fvec3(1,1,1))*0.5);

          light._mindepth = (light._pos-campos_mono).Mag();
          //printf( "light._mindepth<%g>\n", light._mindepth );
          int minX = int(floor(bmin.x*(KTILEDIMX-1)));
          int maxX = int(ceil(bmax.x*(KTILEDIMX-1)));
          int minY = int(floor(bmin.y*(KTILEDIMY-1)));
          int maxY = int(ceil(bmax.y*(KTILEDIMY-1)));
          if( (box.Min().z>=-1 and box.Min().z<=1) or
              (box.Max().z>=-1 and box.Max().z<=1) ) {
              for( int iy=minY; iy<=maxY; iy++ ){
                if( iy>=0 and iy<KTILEDIMY ) {
                  for( int ix=minX; ix<=maxX; ix++ ){
                    if( ix>=0 and ix<KTILEDIMX )
                      _lighttiles[iy*KTILEDIMX+ix].push_back(&light);
                  }
                }
              }
          } // if( (box.Min().z>=-1 and box.Min().z<=1) or
        } // for( size_t lidx=0; lidx<_pointlights.size(); lidx++ ){
        float pht1 = _timer.SecsSinceStart();
        printf( "pht1<%g>\n", pht1 );
        const float KTILESIZX = 2.0f/float(KTILEDIMX);
        const float KTILESIZY = 2.0f/float(KTILEDIMY);
        float fnumltot = 0.0f;
        float fnumlcnt = 0.0f;
        for( int iy=0; iy<KTILEDIMY; iy++ ){
            float T = float(iy)*KTILESIZY-1.0f;
            for( int ix=0; ix<KTILEDIMX; ix++ ){
              auto& lightlist = _lighttiles[iy*KTILEDIMX+ix];
              size_t numl = lightlist.size();
              fnumltot += float(numl);
              fnumlcnt += 1.0f;
              //printf( "TILE <%d %d> numl<%zu>\n", ix, iy, numl );
              if( numl ) {
                  //////////////////////////////////////////////////
                  // update light colors for tile
                  //////////////////////////////////////////////////
                  auto mapped = FXI->mapParamBuffer(_lightbuffer,0,numl*sizeof(fvec4));
                      for( size_t lidx=0; lidx<lightlist.size(); lidx++ ){
                        const auto light = lightlist[lidx];
                        fvec4 colord(light->_color,light->_mindepth);
                        mapped->ref<fvec4>(lidx*sizeof(fvec4)) = colord;
                      }
                  mapped->unmap();
                  //////////////////////////////////////////////////
                  // update light positions for tile
                  //////////////////////////////////////////////////
                  mapped = FXI->mapParamBuffer(_lightbuffer,KPOSPASE,numl*sizeof(fvec4));
                  for( size_t lidx=0; lidx<lightlist.size(); lidx++ ){
                    const auto light = lightlist[lidx];
                    mapped->ref<fvec4>(lidx*sizeof(fvec4)) = fvec4(light->_pos,light->_radius);
                  }
                  mapped->unmap();
                  //////////////////////////////////////////////////
                  // set number of lights for tile
                  //////////////////////////////////////////////////
                  _lightingmtl.bindParamInt(_parNumLights,numl);
                  _lightingmtl.commit();
                  //////////////////////////////////////////////////
                  // accumulate light for tile
                  //////////////////////////////////////////////////
                  if( is_stereo_1pass ) {
                    float L = (float(ix)/float(KTILEDIMX));
                    this_buf->Render2dQuadEML(fvec4(L-1.0f,T,KTILESIZX*0.5,KTILESIZY),
                                              fvec4(0, 0, 1, 1));
                    this_buf->Render2dQuadEML(fvec4(L,T,KTILESIZX*0.5,KTILESIZY),
                                            fvec4(0, 0, 1, 1));
                  }
                  else {
                    float L = float(ix)*KTILESIZX-1.0f;
                    this_buf->Render2dQuadEML(fvec4(L,T,KTILESIZX,KTILESIZY),
                                              fvec4(0, 0, 1, 1));
                  }
                } // if numl
            } // for ix
        } // for iy
        float pht2 = _timer.SecsSinceStart();
        printf( "pht2<%g>\n", pht2-pht1 );
        float avg = fnumltot/fnumlcnt;
        printf( "TILE avg lcnt<%g>\n", avg );
      }
      /////////////////////////////////////
      else {
        for( size_t lidx=0; lidx<_pointlights.size(); lidx++ ) {
          auto& light = _pointlights[lidx];
          Sphere sph(light._pos, light._radius);
          auto box  = sph.projectedBounds(VPL);
          auto bmin = ((box.Min() + fvec3(1, 1, 1)) * 0.5);
          auto bmax = ((box.Max() + fvec3(1, 1, 1)) * 0.5);

          light._mindepth = (light._pos - campos_mono).Mag();
        }
        auto mapped = FXI->mapParamBuffer(_lightbuffer);
            for( size_t lidx=0; lidx<_pointlights.size(); lidx++ ){
              const auto& light = _pointlights[lidx];
              mapped->ref<fvec4>(lidx*sizeof(fvec4)) = fvec4(light._color,light._mindepth);
              mapped->ref<fvec4>(KPOSPASE+lidx*sizeof(fvec4)) = fvec4(light._pos,light._radius);
            }
        mapped->unmap();
        _lightingmtl.bindParamInt(_parNumLights,_pointlights.size());
        _lightingmtl.commit();
        this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1));
      }
      /////////////////////////////////////
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
      ftime += 0.01f;
      targ->EndFrame();
      FBI->PopRtGroup();
      targ->debugPopGroup(); // Lighting
      CIMPL->popCPD();
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
  const FxShaderTechnique* _tekDownsampleDepthMinMax = nullptr;
  const FxShaderParam *_parMatIVPArray = nullptr;
  const FxShaderParam *_parMatPArray = nullptr;
  const FxShaderParam *_parMatVArray = nullptr;
  const FxShaderParam *_parZndc2eye = nullptr;
  const FxShaderParam *_parMapGBufAlbAo = nullptr;
  const FxShaderParam *_parMapGBufNrmL = nullptr;
  const FxShaderParam *_parMapDepth = nullptr;
  const FxShaderParam *_parMapDepthMinMax = nullptr;
  const FxShaderParam *_parTime = nullptr;
  const FxShaderParam *_parNearFar = nullptr;
  const FxShaderParam *_parInvViewSize = nullptr;
  const FxShaderParam *_parInvVpDim = nullptr;
  const FxShaderParam *_parNumLights = nullptr;
  const FxShaderParamBlock *_lightblock = nullptr;
  FxShaderParamBuffer *_lightbuffer = nullptr;

  RtGroup *_rtgGbuffer = nullptr;
  RtGroup *_rtgMinMaxD = nullptr;
  RtGroup *_rtgLaccum = nullptr;
  fmtx4 _viewOffsetMatrix;
  int _width = 0;
  int _height = 0;
  int _minmaxW = 0;
  int _minmaxH = 0;
  fmtx4 _ivp[2];
  fmtx4 _v[2];
  fmtx4 _p[2];
  std::vector<PointLight> _pointlights;
  Timer _timer;
  typedef std::vector<const PointLight*> pllist_t;
  ork::fixedvector<pllist_t,KTILECOUNT> _lighttiles;
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
