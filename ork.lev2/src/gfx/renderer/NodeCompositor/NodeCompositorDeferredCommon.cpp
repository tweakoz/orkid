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

namespace ork::lev2::deferrednode {
///////////////////////////////////////////////////////////////////////////////

DeferredContext::DeferredContext(RenderCompositingNode* node, std::string shadername, int numlights) : _node(node) {

  _shadername = shadername;
  _layername = "All"_pool;

  for (int i = 0; i < numlights; i++) {

    auto p = new PointLight;
    p->next();
    p->_color.x = float(rand() & 0xff) / 256.0;
    p->_color.y = float(rand() & 0xff) / 256.0;
    p->_color.z = float(rand() & 0xff) / 256.0;
    p->_radius  = 16 + float(rand() & 0xff) / 2.0;
    _pointlights.push_back(p);
  }
}

///////////////////////////////////////////////////////////////////////////////

DeferredContext::~DeferredContext() {}

///////////////////////////////////////////////////////////////////////////////
// deferred layout
// rt0/GL_RGBA8    (32,32)  - albedo,ao (primary color)
// rt1/GL_RGB10_A2 (32,64)  - normal,model
// rt2/GL_RGBA8    (32,96)  - mtl,ruf,aux1,aux2
// rt3/GL_R32F     (32,128) - depth
///////////////////////////////////////

void DeferredContext::gpuInit(GfxTarget* target) {
  target->debugPushGroup("Deferred::rendeinitr");
  auto FXI = target->FXI();
  if (nullptr == _rtgGbuffer) {
    //////////////////////////////////////////////////////////////
    _lightingmtl.gpuInit(target, _shadername);
    _tekBaseLighting           = _lightingmtl.technique("baselight");
    _tekPointLighting          = _lightingmtl.technique("pointlight");
    _tekBaseLightingStereo     = _lightingmtl.technique("baselight_stereo");
    _tekPointLightingStereo    = _lightingmtl.technique("pointlight_stereo");
    _tekDownsampleDepthCluster = _lightingmtl.technique("downsampledepthcluster");
    _tekDebugNormal = _lightingmtl.technique("debugnormal");
//////////////////////////////////////////////////////////////
    // init lightblock
    //////////////////////////////////////////////////////////////
    _lightblock  = _lightingmtl.paramBlock("ub_light");
    //////////////////////////////////////////////////////////////
    _parMatIVPArray    = _lightingmtl.param("IVPArray");
    _parMatVArray      = _lightingmtl.param("VArray");
    _parMatPArray      = _lightingmtl.param("PArray");
    _parMapGBufAlbAo   = _lightingmtl.param("MapAlbedoAo");
    _parMapGBufNrmL    = _lightingmtl.param("MapNormalL");
    _parMapDepth       = _lightingmtl.param("MapDepth");
    _parMapGBufRufMtlAlpha = _lightingmtl.param("MapRufMtlAlpha");
    _parMapDepthCluster = _lightingmtl.param("MapDepthCluster");
    _parMapEnvironment = _lightingmtl.param("MapEnvironment");

    _parInvViewSize    = _lightingmtl.param("InvViewportSize");
    _parTime           = _lightingmtl.param("Time");
    _parNumLights      = _lightingmtl.param("NumLights");
    _parTileDim        = _lightingmtl.param("TileDim");
    _parNearFar        = _lightingmtl.param("NearFar");
    _parZndc2eye       = _lightingmtl.param("Zndc2eye");
    //////////////////////////////////////////////////////////////
    _rtgGbuffer      = new RtGroup(target, 8, 8, 1);
    auto buf0        = new RtBuffer(_rtgGbuffer, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA8, 8, 8);
    auto buf1        = new RtBuffer(_rtgGbuffer, lev2::ETGTTYPE_MRT1, lev2::EBUFFMT_RGB10A2, 8, 8);
    auto buf2        = new RtBuffer(_rtgGbuffer, lev2::ETGTTYPE_MRT2, lev2::EBUFFMT_RGBA8, 8, 8);
    buf0->_debugName = "DeferredRtAlbAo";
    buf1->_debugName = "DeferredRtNormalDist";
    buf2->_debugName = "DeferredRtRufMtl";
    _rtgGbuffer->SetMrt(0, buf0);
    _rtgGbuffer->SetMrt(1, buf1);
    _rtgGbuffer->SetMrt(2, buf2);
    _gbuffRT = new RtGroupRenderTarget(_rtgGbuffer);
    //////////////////////////////////////////////////////////////
    _rtgDepthCluster      = new RtGroup(target, 8, 8, 1);
    auto bufD        = new RtBuffer(_rtgDepthCluster, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_R32UI, 8, 8);
    bufD->_debugName = "DeferredDepthCluster";
    _rtgDepthCluster->SetMrt(0, bufD);
    _clusterRT = new RtGroupRenderTarget(_rtgDepthCluster);
    //////////////////////////////////////////////////////////////
    _rtgLaccum        = new RtGroup(target, 8, 8, 1);
    auto bufLA        = new RtBuffer(_rtgLaccum, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA16F, 8, 8);
    bufLA->_debugName = "DeferredLightAccum";
    _rtgLaccum->SetMrt(0, bufLA);
    _accumRT = new RtGroupRenderTarget(_rtgLaccum);
    //////////////////////////////////////////////////////////////
    
  }
  target->debugPopGroup();
}

///////////////////////////////////////////////////////////////////////////////

void DeferredContext::renderGbuffer(CompositorDrawData& drawdata, const ViewData& VD) {
  auto CIMPL                   = drawdata._cimpl;
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  RenderContextFrameData& RCFD = framerenderer.framedata();
  auto targ                    = RCFD.GetTarget();
  auto FBI                     = targ->FBI();
  auto& ddprops                = drawdata._properties;
  auto irenderer = ddprops["irenderer"_crcu].Get<lev2::IRenderer*>();
  SRect tgt_rect(0, 0, targ->GetW(), targ->GetH());
  ///////////////////////////////////////////////////////////////////////////
  FBI->PushRtGroup(_rtgGbuffer);
  FBI->SetAutoClear(false); // explicit clear
  targ->BeginFrame();
  ///////////////////////////////////////////////////////////////////////////
  const auto TOPCPD = CIMPL->topCPD();
  auto CPD = TOPCPD;
  CPD._clearColor      = _clearColor;
  CPD.mpLayerName      = &_layername;
  CPD._irendertarget   = _gbuffRT;
  CPD.SetDstRect(tgt_rect);
  CPD._passID       = "defgbuffer1"_crcu;
  ///////////////////////////////////////////////////////////////////////////
  auto DB              = RCFD.GetDB();
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
    FBI->Clear(_clearColor, 1.0f);
    irenderer->drawEnqueuedRenderables();
    framerenderer.renderMisc();
    targ->debugPopGroup(); // drawenq
    CIMPL->popCPD();
  }
  /////////////////////////////////////////////////////////////////////////////////////////
  targ->EndFrame();
  FBI->PopRtGroup();

}

///////////////////////////////////////////////////////////////////////////////

const uint32_t* DeferredContext::captureDepthClusters(CompositorDrawData& drawdata,const ViewData& VD) {
  auto CIMPL                   = drawdata._cimpl;
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  RenderContextFrameData& RCFD = framerenderer.framedata();
  auto targ                    = RCFD.GetTarget();
  auto FBI                     = targ->FBI();
    auto this_buf                = FBI->GetThisBuffer();
  auto vprect   = SRect(0, 0, _clusterW, _clusterH);
  auto quadrect = SRect(0, 0, _clusterW, _clusterH);
  auto tgt_rect = SRect(0, 0, targ->GetW(), targ->GetH());

  const auto TOPCPD = CIMPL->topCPD();
  auto CPD = TOPCPD;
  CPD._clearColor      = _clearColor;
  CPD.mpLayerName      = &_layername;
  CPD.SetDstRect(tgt_rect);
  CPD._passID       = "defcluster"_crcu;
  CPD.SetDstRect(vprect);
  CPD._irendertarget        = _clusterRT;
  CPD._cameraMatrices       = nullptr;
  CPD._stereoCameraMatrices = nullptr;
  CPD._stereo1pass          = false;
  CIMPL->pushCPD(CPD); // findclusters
  targ->debugPushGroup("Deferred::findclusters");
  {
    FBI->SetAutoClear(true);
    FBI->PushRtGroup(_rtgDepthCluster);
    targ->BeginFrame();
    _lightingmtl.bindTechnique(_tekDownsampleDepthCluster);
    _lightingmtl.begin(RCFD);
    _lightingmtl.bindParamInt(_parTileDim, KTILEDIMXY);
    _lightingmtl.bindParamCTex(_parMapDepth, _rtgGbuffer->_depthTexture);
    _lightingmtl.bindParamVec2(_parNearFar, fvec2(KNEAR, KFAR));
    _lightingmtl.bindParamVec2(_parZndc2eye, VD._zndc2eye);
    _lightingmtl.bindParamVec2(_parInvViewSize, fvec2(1.0 / float(_width), 1.0f / float(_height)));
    _lightingmtl._rasterstate.SetBlending(EBLENDING_OFF);
    _lightingmtl._rasterstate.SetDepthTest(EDEPTHTEST_OFF);
    _lightingmtl._rasterstate.SetCullTest(ECULLTEST_PASS_BACK);
    _lightingmtl.commit();
    this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 0, 0));
    _lightingmtl.end(RCFD);
    targ->EndFrame();
    FBI->PopRtGroup();
  }
  targ->debugPopGroup(); // findclusters
  CIMPL->popCPD();       // findclusters

  bool captureok = FBI->capture(*_rtgDepthCluster, 0, &_clustercapture);
  assert(captureok);
  return (const uint32_t*)_clustercapture._data;
}

///////////////////////////////////////////////////////////////////////////////

void DeferredContext::renderUpdate(CompositorDrawData& drawdata) {
  auto& ddprops                = drawdata._properties;
  //////////////////////////////////////////////////////
  // Resize RenderTargets
  //////////////////////////////////////////////////////
  int newwidth  = ddprops["OutputWidth"_crcu].Get<int>();
  int newheight = ddprops["OutputHeight"_crcu].Get<int>();
  if (_rtgGbuffer->GetW() != newwidth or _rtgGbuffer->GetH() != newheight) {
    _width   = newwidth;
    _height  = newheight;
    _clusterW = (newwidth + KTILEDIMXY - 1) / KTILEDIMXY;
    _clusterH = (newheight + KTILEDIMXY - 1) / KTILEDIMXY;
    _rtgGbuffer->Resize(newwidth, newheight);
    _rtgLaccum->Resize(newwidth, newheight);
    _rtgDepthCluster->Resize(_clusterW, _clusterH);
  }

}

///////////////////////////////////////////////////////////////////////////////

void DeferredContext::update(const ViewData& VD) {
    const int KTILEMAXX = _clusterW - 1;
    const int KTILEMAXY = _clusterH - 1;
      //////////////////////////////////////////////////////////////////
      // update pointlights
      //////////////////////////////////////////////////////////////////
      for (auto pl : _pointlights) {
        if (pl->_counter < 1) {
          pl->next();
        } else {
          fvec3 delta = pl->_dst - pl->_pos;
          pl->_pos += delta.Normal() * 2.0f;
          pl->_counter--;
        }
      Sphere sph(pl->_pos, pl->_radius);
      pl->_aabox      = sph.projectedBounds(VD.VPL);
      const auto& boxmin = pl->_aabox.Min();
      const auto& boxmax = pl->_aabox.Max();
      pl->_aamin      = ((boxmin + fvec3(1, 1, 1)) * 0.5);
      pl->_aamax      = ((boxmax + fvec3(1, 1, 1)) * 0.5);
      pl->_minX       = int(floor(pl->_aamin.x * KTILEMAXX));
      pl->_maxX       = int(ceil(pl->_aamax.x * KTILEMAXX));
      pl->_minY       = int(floor(pl->_aamin.y * KTILEMAXY));
      pl->_maxY       = int(ceil(pl->_aamax.y * KTILEMAXY));
      pl->dist2cam    = (pl->_pos - VD._camposmono).Mag();
      pl->_minZ       = pl->dist2cam - pl->_radius; // Zndc2eye.x / (pl->_aabox.Min().z - Zndc2eye.y);
      pl->_maxZ       = pl->dist2cam + pl->_radius; // Zndc2eye.x / (pl->_aabox.Max().z - Zndc2eye.y);
      }

}

///////////////////////////////////////////////////////////////////////////////

ViewData DeferredContext::computeViewData(CompositorDrawData& drawdata) {
  auto CIMPL                   = drawdata._cimpl;
  const auto TOPCPD = CIMPL->topCPD();
  ViewData VD;
  VD._isStereo = TOPCPD.isStereoOnePass();
  VD._camposmono = TOPCPD.monoCamPos(fmtx4());
  if (VD._isStereo) {
    auto L = TOPCPD._stereoCameraMatrices->_left;
    auto R = TOPCPD._stereoCameraMatrices->_right;
    auto M = TOPCPD._stereoCameraMatrices->_mono;
    VD.VL     = L->_vmatrix;
    VD.VR     = R->_vmatrix;
    VD.VM     = M->_vmatrix;
    VD.PL     = L->_pmatrix;
    VD.PR     = R->_pmatrix;
    VD.PM     = M->_pmatrix;
    VD.VPL    = VD.VL * VD.PL;
    VD.VPR    = VD.VR * VD.PR;
    VD.VPM    = VD.VM * VD.PM;
  } else {
    auto M = TOPCPD._cameraMatrices;
    VD.VM     = M->_vmatrix;
    VD.PM     = M->_pmatrix;
    VD.VL     = VD.VM;
    VD.VR     = VD.VM;
    VD.PL     = VD.PM;
    VD.PR     = VD.PM;
    VD.VPM    = VD.VM * VD.PM;
    VD.VPL    = VD.VPM;
    VD.VPR    = VD.VPM;
  }
  VD.IVPM.inverseOf(VD.VPM);
  VD.IVPL.inverseOf(VD.VPL);
  VD.IVPR.inverseOf(VD.VPR);
  VD._v[0]   = VD.VL;
  VD._v[1]   = VD.VR;
  VD._p[0]   = VD.PL; //_p[0].Transpose();
  VD._p[1]   = VD.PR; //_p[1].Transpose();
  VD._ivp[0] = VD.IVPL;
  VD._ivp[1] = VD.IVPR;
  fmtx4 IVL;
  IVL.inverseOf(VD.VL);
  VD._camposmono = IVL.GetColumn(3).xyz();

  VD._zndc2eye = fvec2(VD._p[0].GetElemXY(3, 2), VD._p[0].GetElemXY(2, 2));
  
  return VD;
}

///////////////////////////////////////////////////////////////////////////////

void DeferredContext::renderBaseLighting( CompositorDrawData& drawdata,
                                          const ViewData& VD ) {
    /////////////////////////////////////////////////////////////////
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto CIMPL                   = drawdata._cimpl;
    auto targ                    = RCFD.GetTarget();
    auto FBI                     = targ->FBI();
    auto this_buf                = FBI->GetThisBuffer();
    auto RSI                     = targ->RSI();
    const auto TOPCPD = CIMPL->topCPD();
    _accumCPD = TOPCPD;
    /////////////////////////////////////////////////////////////////
    auto vprect   = SRect(0, 0, _width, _height);
    auto quadrect = SRect(0, 0, _width, _height);
    _accumCPD.SetDstRect(vprect);
    _accumCPD._irendertarget        = _accumRT;
    _accumCPD._cameraMatrices       = nullptr;
    _accumCPD._stereoCameraMatrices = nullptr;
    _accumCPD._stereo1pass          = false;
    CIMPL->pushCPD(_accumCPD); // base lighting
      FBI->SetAutoClear(true);
      FBI->PushRtGroup(_rtgLaccum);
      targ->BeginFrame();
      FBI->Clear(fvec4(0.1, 0.2, 0.3, 1), 1.0f);
      //////////////////////////////////////////////////////////////////
      // base lighting
      //////////////////////////////////////////////////////////////////
    targ->debugPushGroup("Deferred::BaseLighting");
      _lightingmtl.bindTechnique(VD._isStereo ? _tekBaseLightingStereo : _tekBaseLighting);
      _lightingmtl._rasterstate.SetBlending(EBLENDING_OFF);
      _lightingmtl._rasterstate.SetDepthTest(EDEPTHTEST_OFF);
      _lightingmtl._rasterstate.SetCullTest(ECULLTEST_PASS_BACK);
      _lightingmtl.begin(RCFD);
      //////////////////////////////////////////////////////
      _lightingmtl.bindParamMatrixArray(_parMatIVPArray, VD._ivp, 2);
      _lightingmtl.bindParamMatrixArray(_parMatVArray, VD._v, 2);
      _lightingmtl.bindParamMatrixArray(_parMatPArray, VD._p, 2);
      _lightingmtl.bindParamCTex(_parMapGBufAlbAo, _rtgGbuffer->GetMrt(0)->GetTexture());
      _lightingmtl.bindParamCTex(_parMapGBufNrmL, _rtgGbuffer->GetMrt(1)->GetTexture());
      _lightingmtl.bindParamCTex(_parMapDepth, _rtgGbuffer->_depthTexture);
      _lightingmtl.bindParamVec2(_parNearFar, fvec2(KNEAR, KFAR));
      _lightingmtl.bindParamVec2(_parInvViewSize, fvec2(1.0 / float(_width), 1.0f / float(_height)));
      _lightingmtl.commit();
      RSI->BindRasterState(_lightingmtl._rasterstate);
      this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 0, 0));
      _lightingmtl.end(RCFD);
    CIMPL->popCPD();       // base lighting
    targ->debugPopGroup(); // BaseLighting
    targ->EndFrame();
    FBI->PopRtGroup();     // deferredRtg

}

///////////////////////////////////////////////////////////////////////////////

void DeferredContext::beginPointLighting(CompositorDrawData& drawdata, const ViewData& VD){
    auto CIMPL                   = drawdata._cimpl;
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto targ                    = RCFD.GetTarget();
    auto FBI                     = targ->FBI();
    auto FXI                     = targ->FXI();
    auto RSI                     = targ->RSI();
     targ->debugPushGroup("Deferred::PointLighting");
    CIMPL->pushCPD(_accumCPD);
    FBI->SetAutoClear(false);
    FBI->PushRtGroup(_rtgLaccum);
    targ->BeginFrame();
    _lightingmtl.bindTechnique(VD._isStereo ? _tekPointLightingStereo : _tekPointLighting);
    _lightingmtl.begin(RCFD);
    //////////////////////////////////////////////////////
    _lightingmtl.bindParamMatrixArray(_parMatIVPArray, VD._ivp, 2);
    _lightingmtl.bindParamMatrixArray(_parMatVArray, VD._v, 2);
    _lightingmtl.bindParamMatrixArray(_parMatPArray, VD._p, 2);
    _lightingmtl.bindParamCTex(_parMapGBufAlbAo, _rtgGbuffer->GetMrt(0)->GetTexture());
    _lightingmtl.bindParamCTex(_parMapGBufNrmL, _rtgGbuffer->GetMrt(1)->GetTexture());
    _lightingmtl.bindParamCTex(_parMapDepth, _rtgGbuffer->_depthTexture);
    _lightingmtl.bindParamCTex(_parMapDepthCluster, _rtgDepthCluster->GetMrt(0)->GetTexture());
    _lightingmtl.bindParamVec2(_parNearFar, fvec2(DeferredContext::KNEAR, DeferredContext::KFAR));
    _lightingmtl.bindParamVec2(_parZndc2eye, VD._zndc2eye);
    _lightingmtl.bindParamVec2(_parInvViewSize,
                                        fvec2(1.0 / float(_width), 1.0f / float(_height)));
    //////////////////////////////////////////////////
    _lightingmtl._rasterstate.SetCullTest(ECULLTEST_OFF);
    _lightingmtl._rasterstate.SetBlending(EBLENDING_ADDITIVE);
    //_lightingmtl._rasterstate.SetBlending(EBLENDING_OFF);
    _lightingmtl._rasterstate.SetDepthTest(EDEPTHTEST_OFF);
    RSI->BindRasterState(_lightingmtl._rasterstate);

}

///////////////////////////////////////////////////////////////////////////////

void DeferredContext::endPointLighting(CompositorDrawData& drawdata, const ViewData& VD){
    auto CIMPL                   = drawdata._cimpl;
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto targ                    = RCFD.GetTarget();
    auto FBI                     = targ->FBI();
    _lightingmtl.end(RCFD);
    CIMPL->popCPD(); // _accumCPD
    targ->debugPopGroup(); // Deferred::PointLighting
    targ->EndFrame();
    FBI->PopRtGroup();     // _rtgLaccum

}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork { namespace lev2 {
