////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <algorithm>
#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/mutex.h>
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

ImplementReflectionX(ork::lev2::DeferredCompositingNode, "DeferredCompositingNode");

// fvec3 LightColor
// fvec4 LightPosR 16byte
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
  int _counter    = 0;
  float _mindepth = 0;
  AABox _aabox;
  fvec3 _aamin, _aamax;
  int _minX, _minY;
  int _maxX, _maxY;
  float _minZ, _maxZ;

  void next() {
    float x  = float((rand() & 0x3fff) - 0x2000);
    float z  = float((rand() & 0x3fff) - 0x2000);
    float y  = float((rand() & 0x1fff) - 0x1000);
    _dst     = fvec3(x, y, z);
    _counter = 256 + rand() & 0xff;
  }
};

struct IMPL {
  static constexpr size_t KMAXLIGHTS = 2048;
#if defined(ENABLE_COMPUTE_SHADERS)
  static constexpr int KTILEDIMXY = 64;
#else
  static constexpr int KTILEDIMXY = 64;
#endif
  static constexpr size_t KMAXLIGHTSPERCHUNK = 32768 / sizeof(fvec4);
  static constexpr int KMAXNUMTILESX         = 512;
  static constexpr int KMAXNUMTILESY         = 256;
  static constexpr int KMAXTILECOUNT         = KMAXNUMTILESX * KMAXNUMTILESY;
  static constexpr float KNEAR               = 0.1f;
  static constexpr float KFAR                = 100000.0f;
  ///////////////////////////////////////
  IMPL()
      : _camname(AddPooledString("Camera"))
      , _actlmutex("activelightsmutex")
      {
    _layername = "All"_pool;

#if defined(ENABLE_COMPUTE_SHADERS)
    const int knumlights = KMAXLIGHTS;
#else
    const int knumlights = KMAXLIGHTS;
#endif

    assert(knumlights <= KMAXLIGHTS);

    for (int i = 0; i < knumlights; i++) {

      PointLight p;
      p.next();
      p._color.x = float(rand() & 0xff) / 256.0;
      p._color.y = float(rand() & 0xff) / 256.0;
      p._color.z = float(rand() & 0xff) / 256.0;
      p._radius  = 16 + float(rand() & 0xff) / 2.0;
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
    auto FXI = pTARG->FXI();
    if (nullptr == _rtgGbuffer) {
      for (int i = 0; i < KMAXTILECOUNT; i++) {
        _lighttiles.push_back(pllist_t());
      }
      //////////////////////////////////////////////////////////////
      _lightingmtl.gpuInit(pTARG, "orkshader://deferred");
      _tekBaseLighting          = _lightingmtl.technique("baselight");
      _tekPointLighting         = _lightingmtl.technique("pointlight");
      _tekBaseLightingStereo    = _lightingmtl.technique("baselight_stereo");
      _tekPointLightingStereo   = _lightingmtl.technique("pointlight_stereo");
      _tekDownsampleDepthMinMax = _lightingmtl.technique("downsampledepthminmax");
      //////////////////////////////////////////////////////////////
      // init lightblock
      //////////////////////////////////////////////////////////////
      _lightbuffer = pTARG->FXI()->createParamBuffer(65536);
      _lightblock  = _lightingmtl.paramBlock("ub_light");
      auto mapped  = FXI->mapParamBuffer(_lightbuffer);
      size_t base  = 0;
      for (int i = 0; i < KMAXLIGHTSPERCHUNK; i++)
        mapped->ref<fvec3>(base + i * sizeof(fvec4)) = fvec3(0, 0, 0);
      base += KMAXLIGHTSPERCHUNK * sizeof(fvec4);
      for (int i = 0; i < KMAXLIGHTSPERCHUNK; i++)
        mapped->ref<fvec4>(base + i * sizeof(fvec4)) = fvec4();
      mapped->unmap();
      //////////////////////////////////////////////////////////////
      _parMatIVPArray    = _lightingmtl.param("IVPArray");
      _parMatVArray      = _lightingmtl.param("VArray");
      _parMatPArray      = _lightingmtl.param("PArray");
      _parMapGBufAlbAo   = _lightingmtl.param("MapAlbedoAo");
      _parMapGBufNrmL    = _lightingmtl.param("MapNormalL");
      _parMapDepth       = _lightingmtl.param("MapDepth");
      _parMapDepthMinMax = _lightingmtl.param("MapDepthMinMax");
      _parInvViewSize    = _lightingmtl.param("InvViewportSize");
      _parTime           = _lightingmtl.param("Time");
      _parNumLights      = _lightingmtl.param("NumLights");
      _parTileDim        = _lightingmtl.param("TileDim");
      _parNearFar        = _lightingmtl.param("NearFar");
      _parZndc2eye       = _lightingmtl.param("Zndc2eye");
      //////////////////////////////////////////////////////////////
      _rtgGbuffer      = new RtGroup(pTARG, 8, 8, NUMSAMPLES);
      auto buf0        = new RtBuffer(_rtgGbuffer, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA8, 8, 8);
      auto buf1        = new RtBuffer(_rtgGbuffer, lev2::ETGTTYPE_MRT1, lev2::EBUFFMT_RGB10A2, 8, 8);
      buf0->_debugName = "DeferredRtAlbAo";
      buf1->_debugName = "DeferredRtNormalDist";
      _rtgGbuffer->SetMrt(0, buf0);
      _rtgGbuffer->SetMrt(1, buf1);
      //////////////////////////////////////////////////////////////
      _rtgMinMaxD      = new RtGroup(pTARG, 8, 8, NUMSAMPLES);
      auto bufD        = new RtBuffer(_rtgMinMaxD, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_R32UI, 8, 8);
      bufD->_debugName = "DeferredMinMaxD";
      _rtgMinMaxD->SetMrt(0, bufD);
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
    //_timer.Start();
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto CIMPL                   = drawdata._cimpl;
    auto targ                    = RCFD.GetTarget();
    auto FBI                     = targ->FBI();
    auto FXI                     = targ->FXI();
    auto TXI                     = targ->TXI();
    auto RSI                     = targ->RSI();
    auto GBI                     = targ->GBI();
    auto& ddprops                = drawdata._properties;
    auto this_buf                = FBI->GetThisBuffer();
    SRect tgt_rect(0, 0, targ->GetW(), targ->GetH());
    //////////////////////////////////////////////////////
    // Resize RenderTargets
    //////////////////////////////////////////////////////
    int newwidth  = ddprops["OutputWidth"_crcu].Get<int>();
    int newheight = ddprops["OutputHeight"_crcu].Get<int>();
    if (_rtgGbuffer->GetW() != newwidth or _rtgGbuffer->GetH() != newheight) {
      _width   = newwidth;
      _height  = newheight;
      _minmaxW = (newwidth + KTILEDIMXY - 1) / KTILEDIMXY;
      _minmaxH = (newheight + KTILEDIMXY - 1) / KTILEDIMXY;
      _rtgGbuffer->Resize(newwidth, newheight);
      _rtgLaccum->Resize(newwidth, newheight);
      _rtgMinMaxD->Resize(_minmaxW, _minmaxH);
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
      const auto TOPCPD    = CIMPL->topCPD();
      auto CPD             = TOPCPD;
      bool is_stereo_1pass = CPD.isStereoOnePass();
      CPD._clearColor      = node->_clearColor;
      CPD.mpLayerName      = &_layername;
      CPD._irendertarget   = &rtgbuf;
      CPD.SetDstRect(tgt_rect);
      CPD._passID       = "defgbuffer1"_crcu;
      fvec3 campos_mono = CPD.monoCamPos(fmtx4());
      fmtx4 IVPL, IVPR, IVPM;
      fmtx4 VL, VR, VM;
      fmtx4 PL, PR, PM;
      fmtx4 VPL, VPR, VPM;
      if (is_stereo_1pass) {
        auto L = CPD._stereoCameraMatrices->_left;
        auto R = CPD._stereoCameraMatrices->_right;
        auto M = CPD._stereoCameraMatrices->_mono;
        VL     = L->_vmatrix;
        VR     = R->_vmatrix;
        VM     = M->_vmatrix;
        PL     = L->_pmatrix;
        PR     = R->_pmatrix;
        PM     = M->_pmatrix;
        VPL    = VL * PL;
        VPR    = VR * PR;
        VPM    = VM * PM;
      } else {
        auto M = CPD._cameraMatrices;
        VM     = M->_vmatrix;
        PM     = M->_pmatrix;
        VL     = VM;
        VR     = VM;
        PL     = PM;
        PR     = PM;
        VPM    = VM * PM;
        VPL    = VPM;
        VPR    = VPM;
      }
      IVPM.inverseOf(VPM);
      IVPL.inverseOf(VPL);
      IVPR.inverseOf(VPR);
      _v[0]   = VL;
      _v[1]   = VR;
      _p[0]   = PL; //_p[0].Transpose();
      _p[1]   = PR; //_p[1].Transpose();
      _ivp[0] = IVPL;
      _ivp[1] = IVPR;
      fmtx4 IVL;
      IVL.inverseOf(VL);
      campos_mono = IVL.GetColumn(3).xyz();
      // printf( "campos<%g %g %g>\n", campos_mono.x, campos_mono.y, campos_mono.z );
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
        CIMPL->pushCPD(CPD); // drawenq
        targ->debugPushGroup("toolvp::DrawEnqRenderables");
        FBI->Clear(node->_clearColor, 1.0f);
        irenderer->drawEnqueuedRenderables();
        framerenderer.renderMisc();
        targ->debugPopGroup(); // drawenq
        CIMPL->popCPD();
      }
      /////////////////////////////////////////////////////////////////////////////////////////
      targ->EndFrame();
      FBI->PopRtGroup();
      //////////////////////////////////////////////////////
      auto Zndc2eye = fvec2(_p[0].GetElemXY(3, 2), _p[0].GetElemXY(2, 2));
      /////////////////////////////////////////////////////////////////////////////////////////
      // gen minmax tile depth image
      /////////////////////////////////////////////////////////////////////////////////////////
      RtGroupRenderTarget rtlminmaxd(_rtgMinMaxD);
      auto vprect   = SRect(0, 0, _minmaxW, _minmaxH);
      auto quadrect = SRect(0, 0, _minmaxW, _minmaxH);
      CPD.SetDstRect(vprect);
      CPD._irendertarget        = &rtlminmaxd;
      CPD._cameraMatrices       = nullptr;
      CPD._stereoCameraMatrices = nullptr;
      CPD._stereo1pass          = false;
      CIMPL->pushCPD(CPD); // MinMaxD
      targ->debugPushGroup("Deferred::minmaxd");
      {
        FBI->SetAutoClear(true);
        FBI->PushRtGroup(_rtgMinMaxD);
        targ->BeginFrame();
        _lightingmtl.bindTechnique(_tekDownsampleDepthMinMax);
        _lightingmtl.begin(RCFD);
        _lightingmtl.bindParamInt(_parTileDim, KTILEDIMXY);
        _lightingmtl.bindParamCTex(_parMapDepth, _rtgGbuffer->_depthTexture);
        _lightingmtl.bindParamVec2(_parNearFar, fvec2(KNEAR, KFAR));
        _lightingmtl.bindParamVec2(_parZndc2eye, Zndc2eye);
        _lightingmtl.bindParamVec2(_parInvViewSize, fvec2(1.0 / float(_width), 1.0f / float(_height)));
        _lightingmtl.mRasterState.SetBlending(EBLENDING_OFF);
        _lightingmtl.mRasterState.SetDepthTest(EDEPTHTEST_OFF);
        _lightingmtl.mRasterState.SetCullTest(ECULLTEST_PASS_BACK);
        _lightingmtl.commit();
        this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 0, 0));
        _lightingmtl.end(RCFD);
        targ->EndFrame();
        FBI->PopRtGroup();
      }
      targ->debugPopGroup(); // MinMaxD
      CIMPL->popCPD();       // MinMaxD

      bool captureok = FBI->capture(*_rtgMinMaxD, 0, &_depthcapture);
      assert(captureok);
      auto minmaxdepthbase = (const uint32_t*)_depthcapture._data;
      /////////////////////////////////////////////////////////////////////////////////////////
      // Light Accumulation
      /////////////////////////////////////////////////////////////////////////////////////////
      RtGroupRenderTarget rtlaccum(_rtgLaccum);
      vprect   = SRect(0, 0, _width, _height);
      quadrect = SRect(0, 0, _width, _height);
      CPD.SetDstRect(vprect);
      CPD._irendertarget        = &rtlaccum;
      CPD._cameraMatrices       = nullptr;
      CPD._stereoCameraMatrices = nullptr;
      CPD._stereo1pass          = false;
      CIMPL->pushCPD(CPD); // base lighting
      targ->debugPushGroup("Deferred::LightAccum");
      FBI->SetAutoClear(false);
      FBI->PushRtGroup(_rtgLaccum);
      FBI->SetAutoClear(true);
      targ->BeginFrame();
      FBI->Clear(fvec4(0.1, 0.2, 0.3, 1), 1.0f);
      //////////////////////////////////////////////////////////////////
      // base lighting
      //////////////////////////////////////////////////////////////////
      targ->debugPushGroup("Deferred::BaseLighting");
      _lightingmtl.bindTechnique(is_stereo_1pass ? _tekBaseLightingStereo : _tekBaseLighting);
      _lightingmtl.mRasterState.SetBlending(EBLENDING_OFF);
      _lightingmtl.mRasterState.SetDepthTest(EDEPTHTEST_OFF);
      _lightingmtl.mRasterState.SetCullTest(ECULLTEST_PASS_BACK);
      _lightingmtl.begin(RCFD);
      //////////////////////////////////////////////////////
      _lightingmtl.bindParamMatrixArray(_parMatIVPArray, _ivp, 2);
      _lightingmtl.bindParamMatrixArray(_parMatVArray, _v, 2);
      _lightingmtl.bindParamMatrixArray(_parMatPArray, _p, 2);
      _lightingmtl.bindParamCTex(_parMapGBufAlbAo, _rtgGbuffer->GetMrt(0)->GetTexture());
      _lightingmtl.bindParamCTex(_parMapGBufNrmL, _rtgGbuffer->GetMrt(1)->GetTexture());
      _lightingmtl.bindParamCTex(_parMapDepth, _rtgGbuffer->_depthTexture);
      _lightingmtl.bindParamVec2(_parNearFar, fvec2(KNEAR, KFAR));
      _lightingmtl.bindParamVec2(_parInvViewSize, fvec2(1.0 / float(_width), 1.0f / float(_height)));
      _lightingmtl.commit();
      RSI->BindRasterState(_lightingmtl.mRasterState);
      this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 0, 0));
      _lightingmtl.end(RCFD);
      CIMPL->popCPD();       // base lighting
      targ->debugPopGroup(); // BaseLighting
//////////////////////////////////////////////////////////////////
// point lighting
//  todo : batch multiple lights together
//   compute screen aligned quad for batch..
// accumulate pointlights
//////////////////////////////////////////////////////////////////
#if 1
      targ->debugPushGroup("Deferred::PointLighting");
      static float ftime = 0.0f;
      CIMPL->pushCPD(CPD);
      _lightingmtl.bindTechnique(is_stereo_1pass ? _tekPointLightingStereo : _tekPointLighting);
      _lightingmtl.begin(RCFD);
      //////////////////////////////////////////////////////
      FXI->bindParamBlockBuffer(_lightblock, _lightbuffer);
      //////////////////////////////////////////////////////
      _lightingmtl.bindParamMatrixArray(_parMatIVPArray, _ivp, 2);
      _lightingmtl.bindParamMatrixArray(_parMatVArray, _v, 2);
      _lightingmtl.bindParamMatrixArray(_parMatPArray, _p, 2);
      _lightingmtl.bindParamCTex(_parMapGBufAlbAo, _rtgGbuffer->GetMrt(0)->GetTexture());
      _lightingmtl.bindParamCTex(_parMapGBufNrmL, _rtgGbuffer->GetMrt(1)->GetTexture());
      _lightingmtl.bindParamCTex(_parMapDepth, _rtgGbuffer->_depthTexture);
      _lightingmtl.bindParamCTex(_parMapDepthMinMax, _rtgMinMaxD->GetMrt(0)->GetTexture());
      _lightingmtl.bindParamVec2(_parNearFar, fvec2(KNEAR, KFAR));
      _lightingmtl.bindParamVec2(_parZndc2eye, Zndc2eye);
      _lightingmtl.bindParamVec2(_parInvViewSize, fvec2(1.0 / float(_width), 1.0f / float(_height)));
      //////////////////////////////////////////////////
      // outside lights
      //////////////////////////////////////////////////
      _lightingmtl.mRasterState.SetCullTest(ECULLTEST_OFF);
      _lightingmtl.mRasterState.SetBlending(EBLENDING_ADDITIVE);
      //_lightingmtl.mRasterState.SetBlending(EBLENDING_OFF);
      _lightingmtl.mRasterState.SetDepthTest(EDEPTHTEST_OFF);
      RSI->BindRasterState(_lightingmtl.mRasterState);
      constexpr size_t KPOSPASE = KMAXLIGHTSPERCHUNK * sizeof(fvec4);
      /////////////////////////////////////
      const int KTILEMAXX = _minmaxW - 1;
      const int KTILEMAXY = _minmaxH - 1;
      if (true) { // tiled lighting

        //float time_tile_in = _timer.SecsSinceStart();

        size_t numltiles = 0;
        for (size_t lidx = 0; lidx < _pointlights.size(); lidx++) {
          auto& light = _pointlights[lidx];
          Sphere sph(light._pos, light._radius);
          light._aabox       = sph.projectedBounds(VPL);
          const auto& boxmin = light._aabox.Min();
          const auto& boxmax = light._aabox.Max();
          light._aamin       = ((boxmin + fvec3(1, 1, 1)) * 0.5);
          light._aamax       = ((boxmax + fvec3(1, 1, 1)) * 0.5);
          light._minX        = int(floor(light._aamin.x * KTILEMAXX));
          light._maxX        = int(ceil(light._aamax.x * KTILEMAXX));
          light._minY        = int(floor(light._aamin.y * KTILEMAXY));
          light._maxY        = int(ceil(light._aamax.y * KTILEMAXY));
          light._minZ        = Zndc2eye.x / (light._aabox.Min().z - Zndc2eye.y);
          light._maxZ        = Zndc2eye.x / (light._aabox.Max().z - Zndc2eye.y);
          light._mindepth    = (light._pos - campos_mono).Mag();
        }

        /////////////////////////////////////
        //float time_tile_cpa = _timer.SecsSinceStart();
        //printf( "Deferred::_render tilecpa time<%g>\n", time_tile_cpa-time_tile_in );
        /////////////////////////////////////
        _lightjobcount = 0;

        for (int iy = 0; iy <= _minmaxH; iy++) {
          for (int ix = 0; ix <= _minmaxW; ix++) {
            int mmpixindex = iy * _minmaxW + ix;
            auto job = [this,mmpixindex,ix,iy,&minmaxdepthbase,&numltiles]() {
              for (size_t lidx = 0; lidx < _pointlights.size(); lidx++) {
                auto& light   = _pointlights[lidx];
                bool overlapx = (ix >= light._minX) and (ix <= light._maxX);
                if (overlapx) {
                  bool overlapy = (iy >= light._minY) and (iy <= light._maxY);
                  if (overlapy) {
                    uint32_t depthsample = minmaxdepthbase[mmpixindex];
                    uint32_t bitindex    = 0;
                    bool overlapZ        = false;
                    while (depthsample != 0 and (false == overlapZ)) {
                      bool have_bit = (depthsample & 1);
                      if (have_bit) {
                        float bitshiftedLO = float(1 << bitindex);
                        float bitshiftedHI = bitshiftedLO + bitshiftedLO;
                        overlapZ |= std::max(light._minZ, bitshiftedLO) <= std::min(light._maxZ, bitshiftedHI);
                      } // if (have_bit) {
                      depthsample >>= 1;
                      bitindex++;
                    } // while(depthsample)
                    if (overlapZ) {
                      _actlmutex.Lock();
                      auto& lt = _lighttiles[mmpixindex];
                      lt.push_back(&light);
                      if (lt.size() == 1)
                        _activetiles.push_back(mmpixindex);
                      _actlmutex.UnLock();
                      numltiles++;
                    } // if( overlapZ )
                  }   // if( overlapY )
                }     // if( overlapX) {
              }       // for (size_t lidx = 0; lidx < _pointlights.size(); lidx++) {
              _lightjobcount--;
            }; // job =
            int jobindex = _lightjobcount++;
            //job();
            ParallelOpQ().push(job);
          }         // for (int ix = 0; ix <= _minmaxW; ix++) {
        }           // for (int iy = 0; iy <= _minmaxH; iy++) {

        /////////////////////////////////////
        //float time_tile_cpb = _timer.SecsSinceStart();
        //printf( "Deferred::_render tilecpb time<%g>\n", time_tile_cpb-time_tile_cpa );
        /////////////////////////////////////

        while(_lightjobcount){
             ParallelOpQ().sync();
        }
        const float KTILESIZX = 2.0f / float(_minmaxW);
        const float KTILESIZY = 2.0f / float(_minmaxH);

        size_t numactive = _activetiles.size();
        size_t actindex  = 0;

        /////////////////////////////////////
        //float time_tile_cpc = _timer.SecsSinceStart();
        //printf( "Deferred::_render tilecpc time<%g>\n", time_tile_cpc-time_tile_cpb );
        /////////////////////////////////////
        size_t numchunks = 0;
        size_t lidxbase  = 0;
        /////////////////////////////////////
        while (numactive) {
          /////////////////////////////////////
          // process a chunk
          /////////////////////////////////////
          bool chunk_done     = false;
          auto mapping        = FXI->mapParamBuffer(_lightbuffer, 0, 65536);
          int chunksize       = 0;
          size_t chunk_offset = 0;
          _chunktiles_pos.clear();
          _chunktiles_uva.clear();
          _chunktiles_uvb.clear();
          /////////////////////////////////////
          while (false == chunk_done) {
            int index              = _activetiles[actindex];
            auto& lightlist        = _lighttiles[index];
            int iy                 = index / _minmaxW;
            int ix                 = index % _minmaxW;
            float T                = float(iy) * KTILESIZY - 1.0f;
            float L                = float(ix) * KTILESIZX - 1.0f;
            size_t numlightsintile = lightlist.size();
            size_t remainingintile = numlightsintile - lidxbase;
            size_t countthisiter   = remainingintile;
            /////////////////////////////////////////////////////////
            // clamp number of lights to that which will
            //  fit into the current chunk
            /////////////////////////////////////////////////////////
            if ((countthisiter + chunksize) > KMAXLIGHTSPERCHUNK) {
              countthisiter = KMAXLIGHTSPERCHUNK - chunksize;
            }
            /////////////////////////////////////////////////////////
            // add item to current chunk
            /////////////////////////////////////////////////////////
            _chunktiles_pos.push_back(fvec4(L, T, KTILESIZX, KTILESIZY));
            _chunktiles_uva.push_back(fvec4(0, 0, 1, 1));
            _chunktiles_uvb.push_back(fvec4(chunksize, countthisiter, 0, 0));
            /////////////////////////////////////////////////////////
            for (size_t lidx = 0; lidx < countthisiter; lidx++) {
              const auto light                             = lightlist[lidxbase+lidx];
              /////////////////////////////////////////////////////////
              // embed chunk's lights into lighting UBO
              /////////////////////////////////////////////////////////
              mapping->ref<fvec4>(chunk_offset)            = fvec4(light->_color, light->_mindepth);
              mapping->ref<fvec4>(KPOSPASE + chunk_offset) = fvec4(light->_pos, light->_radius);
              chunk_offset += sizeof(fvec4);
            }
            /////////////////////////////////////////////////////////
            chunksize+=countthisiter;
            chunk_done = chunksize >= KMAXLIGHTSPERCHUNK;
            /////////////////////////////////////////////////////////
            // advance tile ?
            /////////////////////////////////////////////////////////
            lidxbase += countthisiter;
            if( lidxbase == lightlist.size()) {
              actindex++;
              lidxbase = 0;
              chunk_done |= (actindex >= _activetiles.size());
            }
          }
          /////////////////////////////////////
          // chunk ready, fire it off..
          /////////////////////////////////////
          FXI->unmapParamBuffer(mapping.get());
          //////////////////////////////////////////////////
          // set number of lights for tile
          //////////////////////////////////////////////////
          _lightingmtl.bindParamInt(_parNumLights, chunksize);
          _lightingmtl.commit();
          //////////////////////////////////////////////////
          // accumulate light for tile
          //////////////////////////////////////////////////
          if (is_stereo_1pass) {
            // float L = (float(ix) / float(_minmaxW));
            // this_buf->Render2dQuadEML(fvec4(L - 1.0f, T, KTILESIZX * 0.5, KTILESIZY), fvec4(0, 0, 1, 1));
            // this_buf->Render2dQuadEML(fvec4(L, T, KTILESIZX * 0.5, KTILESIZY), fvec4(0, 0, 1, 1));
          } else {
            if (_chunktiles_pos.size())
              this_buf->Render2dQuadsEML(
                  _chunktiles_pos.size(), _chunktiles_pos.data(), _chunktiles_uva.data(), _chunktiles_uvb.data());
          }
          /////////////////////////////////////
          numactive = _activetiles.size() - actindex;
          numchunks++;
          /////////////////////////////////////
        }
        //float time_tile_out = _timer.SecsSinceStart();
        //printf( "Deferred::_render tiletime<%g>\n", time_tile_out-time_tile_in );

        // printf( "numchunks<%zu>\n", numchunks );
      }
      /////////////////////////////////////
      else {
        for (size_t lidx = 0; lidx < _pointlights.size(); lidx++) {
          auto& light = _pointlights[lidx];
          Sphere sph(light._pos, light._radius);
          auto box  = sph.projectedBounds(VPL);
          auto bmin = ((box.Min() + fvec3(1, 1, 1)) * 0.5);
          auto bmax = ((box.Max() + fvec3(1, 1, 1)) * 0.5);

          light._mindepth = (light._pos - campos_mono).Mag();
        }
        auto mapped = FXI->mapParamBuffer(_lightbuffer);
        for (size_t lidx = 0; lidx < _pointlights.size(); lidx++) {
          const auto& light                                   = _pointlights[lidx];
          mapped->ref<fvec4>(lidx * sizeof(fvec4))            = fvec4(light._color, light._mindepth);
          mapped->ref<fvec4>(KPOSPASE + lidx * sizeof(fvec4)) = fvec4(light._pos, light._radius);
        }
        mapped->unmap();
        _lightingmtl.bindParamInt(_parNumLights, _pointlights.size());
        _lightingmtl.commit();
        this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 0, 0));
      }
      /////////////////////////////////////
      _lightingmtl.end(RCFD);
      targ->debugPopGroup(); // PointLighting
#endif
      //////////////////////////////////////////////////////////////////
      // clear lighttiles
      //////////////////////////////////////////////////////////////////
      int numtiles = _minmaxW * _minmaxH;
      for (int i = 0; i < numtiles; i++)
        _lighttiles[i].clear();
      _activetiles.clear();
      //////////////////////////////////////////////////////////////////
      // update pointlights
      //////////////////////////////////////////////////////////////////
      for (auto& pl : _pointlights) {
        if (pl._counter < 1) {
          pl.next();
        } else {
          fvec3 delta = pl._dst - pl._pos;
          pl._pos += delta.Normal() * 2.0f;
          pl._counter--;
        }
      }
      /////////////////////////////////////////////////////////////////////////////////////////
      // end frame
      /////////////////////////////////////////////////////////////////////////////////////////
      // ftime += 0.01f;
      targ->EndFrame();
      FBI->PopRtGroup();     // deferredRtg
      targ->debugPopGroup(); // "Deferred::LightAccum"
      CIMPL->popCPD();
      /////////////////////////////////////////////////////////////////////////////////////////
    }
    targ->debugPopGroup(); // "Deferred::render"
    //float totaltime = _timer.SecsSinceStart();
    //printf( "Deferred::_render totaltime<%g>\n", totaltime );
  }
  ///////////////////////////////////////
  PoolString _camname, _layername;
  FreestyleMaterial _lightingmtl;

  const FxShaderTechnique* _tekBaseLighting          = nullptr;
  const FxShaderTechnique* _tekPointLighting         = nullptr;
  const FxShaderTechnique* _tekBaseLightingStereo    = nullptr;
  const FxShaderTechnique* _tekPointLightingStereo   = nullptr;
  const FxShaderTechnique* _tekDownsampleDepthMinMax = nullptr;
  const FxShaderParam* _parMatIVPArray               = nullptr;
  const FxShaderParam* _parMatPArray                 = nullptr;
  const FxShaderParam* _parMatVArray                 = nullptr;
  const FxShaderParam* _parZndc2eye                  = nullptr;
  const FxShaderParam* _parMapGBufAlbAo              = nullptr;
  const FxShaderParam* _parMapGBufNrmL               = nullptr;
  const FxShaderParam* _parMapDepth                  = nullptr;
  const FxShaderParam* _parMapDepthMinMax            = nullptr;
  const FxShaderParam* _parTime                      = nullptr;
  const FxShaderParam* _parNearFar                   = nullptr;
  const FxShaderParam* _parInvViewSize               = nullptr;
  const FxShaderParam* _parInvVpDim                  = nullptr;
  const FxShaderParam* _parNumLights                 = nullptr;
  const FxShaderParam* _parTileDim                   = nullptr;
  const FxShaderParamBlock* _lightblock              = nullptr;
  FxShaderParamBuffer* _lightbuffer                  = nullptr;

  RtGroup* _rtgGbuffer = nullptr;
  RtGroup* _rtgMinMaxD = nullptr;
  RtGroup* _rtgLaccum  = nullptr;
  fmtx4 _viewOffsetMatrix;
  CaptureBuffer _depthcapture;
  int _width   = 0;
  int _height  = 0;
  int _minmaxW = 0;
  int _minmaxH = 0;
  fmtx4 _ivp[2];
  fmtx4 _v[2];
  fmtx4 _p[2];
  std::vector<PointLight> _pointlights;
  typedef std::vector<const PointLight*> pllist_t;
  ork::fixedvector<pllist_t, KMAXTILECOUNT> _lighttiles;
  ork::fixedvector<int, KMAXTILECOUNT> _activetiles;
  ork::fixedvector<int, KMAXTILECOUNT> _chunktiles;
  ork::fixedvector<fvec4, KMAXTILECOUNT> _chunktiles_pos;
  ork::fixedvector<fvec4, KMAXTILECOUNT> _chunktiles_uva;
  ork::fixedvector<fvec4, KMAXTILECOUNT> _chunktiles_uvb;
  std::atomic<int> _lightjobcount;
  ork::Timer _timer;
  ork::mutex _actlmutex;
}; // namespace deferrednode
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
