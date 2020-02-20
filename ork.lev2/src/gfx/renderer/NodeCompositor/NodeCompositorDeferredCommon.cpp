////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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
#include <ork/lev2/gfx/material_pbr.inl>

#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorDeferred.h>

namespace ork::lev2::deferrednode {
///////////////////////////////////////////////////////////////////////////////

DeferredContext::DeferredContext(RenderCompositingNode* node, std::string shadername, int numlights)
    : _node(node) {
  ///////////
  _rtbDepthCluster = new RtBuffer(lev2::ERTGSLOT0, lev2::EBUFFMT_R32UI, 8, 8);
  _rtbLightAccum   = new RtBuffer(lev2::ERTGSLOT0, lev2::EBUFFMT_RGBA16F, 8, 8);
  _rtbAlbAo        = new RtBuffer(lev2::ERTGSLOT0, lev2::EBUFFMT_RGBA8, 8, 8);
  _rtbNormalDist   = new RtBuffer(lev2::ERTGSLOT1, lev2::EBUFFMT_RGB10A2, 8, 8);
  _rtbRufMtl       = new RtBuffer(lev2::ERTGSLOT2, lev2::EBUFFMT_RGBA8, 8, 8);
  ///////////
  _rtbAlbAo->_debugName        = "DeferredRtAlbAo";
  _rtbNormalDist->_debugName   = "DeferredRtNormalDist";
  _rtbRufMtl->_debugName       = "DeferredRtRufMtl";
  _rtbDepthCluster->_debugName = "DeferredDepthCluster";
  _rtbLightAccum->_debugName   = "DeferredLightAccum";
  ///////////
  _rtbDepthCluster->_texture->TexSamplingMode().PresetPointAndClamp();
  _rtbLightAccum->_texture->TexSamplingMode().PresetPointAndClamp();
  _rtbAlbAo->_texture->TexSamplingMode().PresetPointAndClamp();
  _rtbNormalDist->_texture->TexSamplingMode().PresetPointAndClamp();
  _rtbRufMtl->_texture->TexSamplingMode().PresetPointAndClamp();
  ///////////
  _shadername = shadername;
  _layername  = "All"_pool;

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

DeferredContext::~DeferredContext() {
}

///////////////////////////////////////////////////////////////////////////////

lev2::Texture* DeferredContext::brdfIntegrationTexture() const {
  return _brdfIntegrationMap;
}

///////////////////////////////////////////////////////////////////////////////
// deferred layout
// rt0/GL_RGBA8    (32,32)  - albedo,ao (primary color)
// rt1/GL_RGB10_A2 (32,64)  - normal,model
// rt2/GL_RGBA8    (32,96)  - mtl,ruf,aux1,aux2
// rt3/GL_R32F     (32,128) - depth
///////////////////////////////////////

void DeferredContext::gpuInit(Context* target) {
  target->debugPushGroup("Deferred::rendeinitr");
  auto FXI = target->FXI();
  if (nullptr == _rtgGbuffer) {
    _brdfIntegrationMap = PBRMaterial::brdfIntegrationMap(target);
    //////////////////////////////////////////////////////////////
    _lightingmtl.gpuInit(target, _shadername);
    _tekBaseLighting       = _lightingmtl.technique("baselight");
    _tekBaseLightingStereo = _lightingmtl.technique("baselight_stereo");
    //
    _tekPointLightingUntextured       = _lightingmtl.technique("pointlight_untextured");
    _tekPointLightingTextured         = _lightingmtl.technique("pointlight_textured");
    _tekPointLightingUntexturedStereo = _lightingmtl.technique("pointlight_untextured_stereo");
    _tekPointLightingTexturedStereo   = _lightingmtl.technique("pointlight_textured_stereo");
    //
    _tekSpotLightingUntextured             = _lightingmtl.technique("spotlight_untextured");
    _tekSpotLightingTextured               = _lightingmtl.technique("spotlight_textured");
    _tekSpotLightingUntexturedStereo       = _lightingmtl.technique("spotlight_untextured_stereo");
    _tekSpotLightingTexturedStereo         = _lightingmtl.technique("spotlight_textured_stereo");
    _tekSpotLightingTexturedShadowed       = _lightingmtl.technique("spotlight_textured_shadowed");
    _tekSpotLightingTexturedShadowedStereo = _lightingmtl.technique("spotlight_textured_shadowed_stereo");
    //
    _tekSpotDecalingTexturedStereo = _lightingmtl.technique("spotdecal_textured_stereo");
    _tekSpotDecalingTextured       = _lightingmtl.technique("spotdecal_textured");
    //
    _tekDownsampleDepthCluster    = _lightingmtl.technique("downsampledepthcluster");
    _tekEnvironmentLighting       = _lightingmtl.technique("environmentlighting");
    _tekEnvironmentLightingStereo = _lightingmtl.technique("environmentlighting_stereo");
    //////////////////////////////////////////////////////////////
    // init lightblock
    //////////////////////////////////////////////////////////////
    _lightblock = _lightingmtl.paramBlock("ub_light");
    //////////////////////////////////////////////////////////////
    _parMatIVPArray        = _lightingmtl.param("IVPArray");
    _parMatVArray          = _lightingmtl.param("VArray");
    _parMatPArray          = _lightingmtl.param("PArray");
    _parMapGBufAlbAo       = _lightingmtl.param("MapAlbedoAo");
    _parMapGBufNrmL        = _lightingmtl.param("MapNormalL");
    _parMapDepth           = _lightingmtl.param("MapDepth");
    _parMapShadowDepth     = _lightingmtl.param("MapShadowDepth");
    _parMapGBufRufMtlAlpha = _lightingmtl.param("MapRufMtlAlpha");
    _parMapDepthCluster    = _lightingmtl.param("MapDepthCluster");
    _parLightCookieTexture = _lightingmtl.param("MapLightingCookie");
    _parMapSpecularEnv     = _lightingmtl.param("MapSpecularEnv");
    _parMapDiffuseEnv      = _lightingmtl.param("MapDiffuseEnv");
    _parMapBrdfIntegration = _lightingmtl.param("MapBrdfIntegration");

    _parInvViewSize         = _lightingmtl.param("InvViewportSize");
    _parTime                = _lightingmtl.param("Time");
    _parNumLights           = _lightingmtl.param("NumLights");
    _parTileDim             = _lightingmtl.param("TileDim");
    _parNearFar             = _lightingmtl.param("NearFar");
    _parZndc2eye            = _lightingmtl.param("Zndc2eye");
    _parEnvironmentMipBias  = _lightingmtl.param("EnvironmentMipBias");
    _parEnvironmentMipScale = _lightingmtl.param("EnvironmentMipScale");
    _parSpecularLevel       = _lightingmtl.param("SpecularLevel");
    _parDiffuseLevel        = _lightingmtl.param("DiffuseLevel");
    _parAmbientLevel        = _lightingmtl.param("AmbientLevel");
    _parSkyboxLevel         = _lightingmtl.param("SkyboxLevel");
    _parDepthFogDistance    = _lightingmtl.param("DepthFogDistance");
    _parDepthFogPower       = _lightingmtl.param("DepthFogPower");
    _parShadowParams        = _lightingmtl.param("ShadowParams");
    //////////////////////////////////////////////////////////////
    _rtgGbuffer             = new RtGroup(target, 8, 8, 1);
    _rtgGbuffer->_autoclear = false;
    _rtgDecal               = new RtGroup(target, 8, 8, 1);
    _rtgDecal->_needsDepth  = false;
    _rtgGbuffer->SetMrt(0, _rtbAlbAo);
    _rtgGbuffer->SetMrt(1, _rtbNormalDist);
    _rtgGbuffer->SetMrt(2, _rtbRufMtl);
    _rtgDecal->SetMrt(0, _rtbAlbAo);
    _gbuffRT = new RtGroupRenderTarget(_rtgGbuffer);
    _decalRT = new RtGroupRenderTarget(_rtgDecal);
    //////////////////////////////////////////////////////////////
    _rtgDepthCluster = new RtGroup(target, 8, 8, 1);
    _rtgDepthCluster->SetMrt(0, _rtbDepthCluster);
    _clusterRT = new RtGroupRenderTarget(_rtgDepthCluster);
    //////////////////////////////////////////////////////////////
    _rtgLaccum             = new RtGroup(target, 8, 8, 1);
    _rtgLaccum->_autoclear = false;
    _rtgLaccum->SetMrt(0, _rtbLightAccum);
    _accumRT = new RtGroupRenderTarget(_rtgLaccum);
    //////////////////////////////////////////////////////////////
    _whiteTexture = asset::AssetManager<lev2::TextureAsset>::Load("data://effect_textures/white")->GetTexture();
  }
  target->debugPopGroup();
}

///////////////////////////////////////////////////////////////////////////////

void DeferredContext::renderGbuffer(CompositorDrawData& drawdata, const ViewData& VD) {
  auto CIMPL                   = drawdata._cimpl;
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  RenderContextFrameData& RCFD = framerenderer.framedata();
  auto targ                    = drawdata.context();
  auto FBI                     = targ->FBI();
  auto& ddprops                = drawdata._properties;
  auto irenderer               = ddprops["irenderer"_crcu].Get<lev2::IRenderer*>();
  ViewportRect tgt_rect(0, 0, _rtgGbuffer->GetW(), _rtgGbuffer->GetH());
  ViewportRect mrt_rect(0, 0, _rtgGbuffer->GetW(), _rtgGbuffer->GetH());
  ///////////////////////////////////////////////////////////////////////////
  FBI->PushRtGroup(_rtgGbuffer);
  FBI->SetAutoClear(false); // explicit clear
  targ->beginFrame();
  ///////////////////////////////////////////////////////////////////////////
  const auto TOPCPD  = CIMPL->topCPD();
  auto CPD           = TOPCPD;
  CPD._clearColor    = _clearColor;
  CPD.mpLayerName    = &_layername;
  CPD._irendertarget = _gbuffRT;
  CPD.SetDstRect(tgt_rect);
  CPD.SetMrtRect(mrt_rect);
  CPD._passID = "defgbuffer1"_crcu;
  ///////////////////////////////////////////////////////////////////////////
  auto DB = RCFD.GetDB();
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
  targ->endFrame();
  FBI->PopRtGroup();
}

///////////////////////////////////////////////////////////////////////////////

const uint32_t* DeferredContext::captureDepthClusters(const CompositorDrawData& drawdata, const ViewData& VD) {
  auto CIMPL                   = drawdata._cimpl;
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  RenderContextFrameData& RCFD = framerenderer.framedata();
  auto targ                    = drawdata.context();
  auto FBI                     = targ->FBI();
  auto this_buf                = FBI->GetThisBuffer();
  auto vprect                  = ViewportRect(0, 0, _clusterW, _clusterH);
  auto quadrect                = SRect(0, 0, _clusterW, _clusterH);
  auto tgt_rect                = targ->mainSurfaceRectAtOrigin();

  const auto TOPCPD = CIMPL->topCPD();
  auto CPD          = TOPCPD;
  CPD._clearColor   = _clearColor;
  CPD.mpLayerName   = &_layername;
  CPD.SetDstRect(tgt_rect);
  CPD._passID = "defcluster"_crcu;
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
    targ->beginFrame();
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
    targ->endFrame();
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
  auto& ddprops = drawdata._properties;
  //////////////////////////////////////////////////////
  // Resize RenderTargets
  //////////////////////////////////////////////////////
  int newwidth  = ddprops["OutputWidth"_crcu].Get<int>();
  int newheight = ddprops["OutputHeight"_crcu].Get<int>();
  if (_rtgGbuffer->GetW() != newwidth or _rtgGbuffer->GetH() != newheight) {
    printf("newwidth<%d>\n", newwidth);
    printf("newheight<%d>\n", newheight);
    _width    = newwidth;
    _height   = newheight;
    _clusterW = (newwidth + KTILEDIMXY - 1) / KTILEDIMXY;
    _clusterH = (newheight + KTILEDIMXY - 1) / KTILEDIMXY;
    _rtgGbuffer->Resize(newwidth, newheight);
    _rtgLaccum->Resize(newwidth, newheight);
    _rtgDecal->Resize(newwidth, newheight);
    _rtgDepthCluster->Resize(_clusterW, _clusterH);
  }
}

///////////////////////////////////////////////////////////////////////////////

void DeferredContext::updateDebugLights(const ViewData& VD) {
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
    pl->_aabox         = sph.projectedBounds(VD.VPL);
    const auto& boxmin = pl->_aabox.Min();
    const auto& boxmax = pl->_aabox.Max();
    pl->_aamin         = ((boxmin + fvec3(1, 1, 1)) * 0.5);
    pl->_aamax         = ((boxmax + fvec3(1, 1, 1)) * 0.5);
    pl->_minX          = int(floor(pl->_aamin.x * KTILEMAXX));
    pl->_maxX          = int(ceil(pl->_aamax.x * KTILEMAXX));
    pl->_minY          = int(floor(pl->_aamin.y * KTILEMAXY));
    pl->_maxY          = int(ceil(pl->_aamax.y * KTILEMAXY));
    pl->dist2cam       = (pl->_pos - VD._camposmono).Mag();
    pl->_minZ          = pl->dist2cam - pl->_radius; // Zndc2eye.x / (pl->_aabox.Min().z - Zndc2eye.y);
    pl->_maxZ          = pl->dist2cam + pl->_radius; // Zndc2eye.x / (pl->_aabox.Max().z - Zndc2eye.y);
  }
}

///////////////////////////////////////////////////////////////////////////////

void DeferredContext::bindViewParams(const ViewData& VD) {
  _lightingmtl.bindParamMatrixArray(_parMatIVPArray, VD._ivp, 2);
  _lightingmtl.bindParamMatrixArray(_parMatVArray, VD._v, 2);
  _lightingmtl.bindParamMatrixArray(_parMatPArray, VD._p, 2);
  _lightingmtl.bindParamVec2(_parZndc2eye, VD._zndc2eye);
  _lightingmtl.bindParamVec2(_parNearFar, fvec2(DeferredContext::KNEAR, DeferredContext::KFAR));
  _lightingmtl.bindParamVec2(_parInvViewSize, fvec2(1.0 / float(_width), 1.0f / float(_height)));
}

void DeferredContext::bindRasterState(Context* ctx, ECullTest culltest, EDepthTest depthtest, EBlending blending) {
  _lightingmtl._rasterstate.SetBlending(blending);
  _lightingmtl._rasterstate.SetDepthTest(depthtest);
  _lightingmtl._rasterstate.SetCullTest(culltest);
  ctx->RSI()->BindRasterState(_lightingmtl._rasterstate);
}

///////////////////////////////////////////////////////////////////////////////

void DeferredContext::renderBaseLighting(CompositorDrawData& drawdata, const ViewData& VD) {
  /////////////////////////////////////////////////////////////////
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  RenderContextFrameData& RCFD = framerenderer.framedata();
  auto CIMPL                   = drawdata._cimpl;
  auto targ                    = drawdata.context();
  auto FBI                     = targ->FBI();
  auto this_buf                = FBI->GetThisBuffer();
  auto RSI                     = targ->RSI();
  auto DWI                     = targ->DWI();
  const auto TOPCPD            = CIMPL->topCPD();
  _accumCPD                    = TOPCPD;
  _decalCPD                    = TOPCPD;
  /////////////////////////////////////////////////////////////////
  auto vprect   = ViewportRect(0, 0, _width, _height);
  auto quadrect = SRect(0, 0, _width, _height);
  _accumCPD.SetDstRect(vprect);
  _accumCPD._irendertarget        = _accumRT;
  _accumCPD._cameraMatrices       = nullptr;
  _accumCPD._stereoCameraMatrices = nullptr;
  _accumCPD._stereo1pass          = false;
  _decalCPD.SetDstRect(vprect);
  _decalCPD._irendertarget        = _decalRT;
  _decalCPD._cameraMatrices       = nullptr;
  _decalCPD._stereoCameraMatrices = nullptr;
  _decalCPD._stereo1pass          = false;
  CIMPL->pushCPD(_accumCPD); // base lighting
  FBI->PushRtGroup(_rtgLaccum);
  FBI->rtGroupClear(_rtgLaccum);
  //////////////////////////////////////////////////////////////////
  // base lighting
  //////////////////////////////////////////////////////////////////
  targ->debugPushGroup("Deferred::BaseLighting");
  _lightingmtl.bindTechnique(VD._isStereo ? _tekBaseLightingStereo : _tekBaseLighting);
  _lightingmtl.begin(RCFD);
  //////////////////////////////////////////////////////
  bindViewParams(VD);
  bindRasterState(targ, ECULLTEST_PASS_BACK, EDEPTHTEST_OFF, EBLENDING_OFF);
  //////////////////////////////////////////////////////
  _lightingmtl.bindParamCTex(_parMapGBufAlbAo, _rtgGbuffer->GetMrt(0)->texture());
  _lightingmtl.bindParamCTex(_parMapGBufNrmL, _rtgGbuffer->GetMrt(1)->texture());
  _lightingmtl.bindParamCTex(_parMapDepth, _rtgGbuffer->_depthTexture);
  _lightingmtl.commit();
  DWI->quad2DEMLTiled(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 0, 0), 2);
  _lightingmtl.end(RCFD);
  CIMPL->popCPD();       // base lighting
  targ->debugPopGroup(); // BaseLighting
  FBI->PopRtGroup();     // deferredRtg
}

///////////////////////////////////////////////////////////////////////////////

void DeferredContext::beginPointLighting(CompositorDrawData& drawdata, const ViewData& VD, lev2::Texture* cookietexture) {
  auto CIMPL                   = drawdata._cimpl;
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  RenderContextFrameData& RCFD = framerenderer.framedata();
  auto targ                    = drawdata.context();
  auto FBI                     = targ->FBI();
  auto FXI                     = targ->FXI();
  auto RSI                     = targ->RSI();
  targ->debugPushGroup("Deferred::PointLighting");
  CIMPL->pushCPD(_accumCPD);
  FBI->PushRtGroup(_rtgLaccum);
  const FxShaderTechnique* tek = nullptr;
  if (VD._isStereo) {
    tek = cookietexture ? _tekPointLightingTexturedStereo : _tekPointLightingUntexturedStereo;
  } else {
    tek = cookietexture ? _tekPointLightingTextured : _tekPointLightingUntextured;
  }
  _lightingmtl.bindTechnique(tek);
  _lightingmtl.begin(RCFD);
  //////////////////////////////////////////////////////
  bindViewParams(VD);
  bindRasterState(targ, ECULLTEST_OFF, EDEPTHTEST_OFF, EBLENDING_ADDITIVE);
  //////////////////////////////////////////////////////
  _lightingmtl.bindParamCTex(_parMapGBufAlbAo, _rtgGbuffer->GetMrt(0)->texture());
  _lightingmtl.bindParamCTex(_parMapGBufNrmL, _rtgGbuffer->GetMrt(1)->texture());
  _lightingmtl.bindParamCTex(_parMapGBufRufMtlAlpha, _rtgGbuffer->GetMrt(2)->texture());
  _lightingmtl.bindParamCTex(_parMapDepth, _rtgGbuffer->_depthTexture);
  _lightingmtl.bindParamCTex(_parMapDepthCluster, _rtgDepthCluster->GetMrt(0)->texture());
  _lightingmtl.bindParamCTex(_parMapBrdfIntegration, _brdfIntegrationMap);
  ///////////////////////////
  if (cookietexture)
    _lightingmtl.bindParamCTex(_parLightCookieTexture, cookietexture);
  ///////////////////////////
  _lightingmtl.bindParamFloat(_parSpecularLevel, _specularLevel);
  _lightingmtl.bindParamFloat(_parDiffuseLevel, _diffuseLevel);
  //////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void DeferredContext::endPointLighting(CompositorDrawData& drawdata, const ViewData& VD) {
  auto CIMPL                   = drawdata._cimpl;
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  RenderContextFrameData& RCFD = framerenderer.framedata();
  auto targ                    = drawdata.context();
  auto FBI                     = targ->FBI();
  _lightingmtl.end(RCFD);
  CIMPL->popCPD();       // _accumCPD
  targ->debugPopGroup(); // Deferred::PointLighting
  FBI->PopRtGroup();     // _rtgLaccum
}

///////////////////////////////////////////////////////////////////////////////

void DeferredContext::beginSpotLighting(CompositorDrawData& drawdata, const ViewData& VD, lev2::Texture* cookietexture) {
  auto CIMPL                   = drawdata._cimpl;
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  RenderContextFrameData& RCFD = framerenderer.framedata();
  auto targ                    = drawdata.context();
  auto FBI                     = targ->FBI();
  auto FXI                     = targ->FXI();
  auto RSI                     = targ->RSI();
  targ->debugPushGroup("Deferred::PointLighting");
  CIMPL->pushCPD(_accumCPD);
  FBI->PushRtGroup(_rtgLaccum);
  const FxShaderTechnique* tek = nullptr;
  if (VD._isStereo) {
    tek = cookietexture ? _tekSpotLightingTexturedStereo : _tekSpotLightingUntexturedStereo;
  } else {
    tek = cookietexture ? _tekSpotLightingTextured : _tekSpotLightingUntextured;
  }
  _lightingmtl.bindTechnique(tek);
  _lightingmtl.begin(RCFD);
  //////////////////////////////////////////////////////
  bindViewParams(VD);
  bindRasterState(targ, ECULLTEST_OFF, EDEPTHTEST_OFF, EBLENDING_ADDITIVE);
  //////////////////////////////////////////////////////
  _lightingmtl.bindParamCTex(_parMapGBufAlbAo, _rtgGbuffer->GetMrt(0)->texture());
  _lightingmtl.bindParamCTex(_parMapGBufNrmL, _rtgGbuffer->GetMrt(1)->texture());
  _lightingmtl.bindParamCTex(_parMapGBufRufMtlAlpha, _rtgGbuffer->GetMrt(2)->texture());
  _lightingmtl.bindParamCTex(_parMapDepth, _rtgGbuffer->_depthTexture);
  _lightingmtl.bindParamCTex(_parMapDepthCluster, _rtgDepthCluster->GetMrt(0)->texture());
  _lightingmtl.bindParamCTex(_parMapBrdfIntegration, _brdfIntegrationMap);
  ///////////////////////////
  if (cookietexture)
    _lightingmtl.bindParamCTex(_parLightCookieTexture, cookietexture);
  ///////////////////////////
  _lightingmtl.bindParamFloat(_parSpecularLevel, _specularLevel);
  _lightingmtl.bindParamFloat(_parDiffuseLevel, _diffuseLevel);
  //////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void DeferredContext::endSpotLighting(CompositorDrawData& drawdata, const ViewData& VD) {
  auto CIMPL                   = drawdata._cimpl;
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  RenderContextFrameData& RCFD = framerenderer.framedata();
  auto targ                    = drawdata.context();
  auto FBI                     = targ->FBI();
  _lightingmtl.end(RCFD);
  CIMPL->popCPD();       // _accumCPD
  targ->debugPopGroup(); // Deferred::PointLighting
  FBI->PopRtGroup();     // _rtgLaccum
}

///////////////////////////////////////////////////////////////////////////////

void DeferredContext::beginShadowedSpotLighting(CompositorDrawData& drawdata, const ViewData& VD, lev2::Texture* cookietexture) {
  auto CIMPL                   = drawdata._cimpl;
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  RenderContextFrameData& RCFD = framerenderer.framedata();
  auto targ                    = drawdata.context();
  auto FBI                     = targ->FBI();
  auto FXI                     = targ->FXI();
  auto RSI                     = targ->RSI();
  targ->debugPushGroup("Deferred::PointLighting");
  CIMPL->pushCPD(_accumCPD);
  FBI->PushRtGroup(_rtgLaccum);
  const FxShaderTechnique* tek = nullptr;
  OrkAssert(cookietexture != nullptr);
  tek = VD._isStereo ? _tekSpotLightingTexturedShadowedStereo : _tekSpotLightingTexturedShadowed;
  _lightingmtl.bindTechnique(tek);
  _lightingmtl.begin(RCFD);
  //////////////////////////////////////////////////////
  bindViewParams(VD);
  bindRasterState(targ, ECULLTEST_OFF, EDEPTHTEST_OFF, EBLENDING_ADDITIVE);
  //////////////////////////////////////////////////////
  _lightingmtl.bindParamCTex(_parMapGBufAlbAo, _rtgGbuffer->GetMrt(0)->texture());
  _lightingmtl.bindParamCTex(_parMapGBufNrmL, _rtgGbuffer->GetMrt(1)->texture());
  _lightingmtl.bindParamCTex(_parMapGBufRufMtlAlpha, _rtgGbuffer->GetMrt(2)->texture());
  _lightingmtl.bindParamCTex(_parMapDepth, _rtgGbuffer->_depthTexture);
  _lightingmtl.bindParamCTex(_parMapDepthCluster, _rtgDepthCluster->GetMrt(0)->texture());
  _lightingmtl.bindParamCTex(_parMapBrdfIntegration, _brdfIntegrationMap);
  ///////////////////////////
  if (cookietexture)
    _lightingmtl.bindParamCTex(_parLightCookieTexture, cookietexture);
  ///////////////////////////
  _lightingmtl.bindParamFloat(_parSpecularLevel, _specularLevel);
  _lightingmtl.bindParamFloat(_parDiffuseLevel, _diffuseLevel);
  //////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void DeferredContext::endShadowedSpotLighting(CompositorDrawData& drawdata, const ViewData& VD) {
  auto CIMPL                   = drawdata._cimpl;
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  RenderContextFrameData& RCFD = framerenderer.framedata();
  auto targ                    = drawdata.context();
  auto FBI                     = targ->FBI();
  _lightingmtl.end(RCFD);
  CIMPL->popCPD();       // _accumCPD
  targ->debugPopGroup(); // Deferred::PointLighting
  FBI->PopRtGroup();     // _rtgLaccum
}
///////////////////////////////////////////////////////////////////////////////

void DeferredContext::beginSpotDecaling(CompositorDrawData& drawdata, const ViewData& VD, lev2::Texture* cookietexture) {
  auto CIMPL                   = drawdata._cimpl;
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  RenderContextFrameData& RCFD = framerenderer.framedata();
  auto targ                    = drawdata.context();
  auto FBI                     = targ->FBI();
  auto FXI                     = targ->FXI();
  auto RSI                     = targ->RSI();
  targ->debugPushGroup("Deferred::SpotDecaling");
  CIMPL->pushCPD(_decalCPD);
  FBI->PushRtGroup(_rtgDecal);
  const FxShaderTechnique* tek = nullptr;
  if (VD._isStereo) {
    tek = cookietexture ? _tekSpotDecalingTexturedStereo : _tekSpotDecalingTexturedStereo;
  } else {
    tek = cookietexture ? _tekSpotDecalingTextured : _tekSpotDecalingTextured;
  }
  _lightingmtl.bindTechnique(tek);
  _lightingmtl.begin(RCFD);
  //////////////////////////////////////////////////////
  bindViewParams(VD);
  bindRasterState(targ, ECULLTEST_OFF, EDEPTHTEST_OFF, EBLENDING_OFF);
  ///////////////////////////
  _lightingmtl.bindParamCTex(_parMapGBufRufMtlAlpha, _rtgGbuffer->GetMrt(2)->texture());
  _lightingmtl.bindParamCTex(_parMapDepth, _rtgGbuffer->_depthTexture);
  if (cookietexture)
    _lightingmtl.bindParamCTex(_parLightCookieTexture, cookietexture);
  //////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void DeferredContext::endSpotDecaling(CompositorDrawData& drawdata, const ViewData& VD) {
  auto CIMPL = drawdata._cimpl;
  auto& RCFD = drawdata.RCFD();
  auto targ  = drawdata.context();
  auto FBI   = targ->FBI();

  _lightingmtl.end(RCFD);
  CIMPL->popCPD();       // _decalCPD
  targ->debugPopGroup(); // Deferred::SpotDecaling
  FBI->PopRtGroup();     // _rtgDecal
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::deferrednode
