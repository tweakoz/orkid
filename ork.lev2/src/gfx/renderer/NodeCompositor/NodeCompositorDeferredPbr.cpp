////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <algorithm>
#include <ork/pch.h>
#include <ork/rtti/Class.h>
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
#include <ork/kernel/datacache.inl>
#include <ork/gfx/brdf.inl>
#include <ork/gfx/dds.h>
//#include <ork/gfx/image.inl>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/texman.h>

#include "NodeCompositorDeferred.h"
#include "CpuLightProcessor.h"

ImplementReflectionX(ork::lev2::deferrednode::DeferredCompositingNodePbr, "DeferredCompositingNodePbr");

// fvec3 LightColor
// fvec4 LightPosR 16byte
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::deferrednode {
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNodePbr::describeX(class_t* c) {

  class_t::CreateClassAlias("DeferredCompositingNodeDebugNormal", c);

  c->memberProperty("ClearColor", &DeferredCompositingNodePbr::_clearColor);
  c->memberProperty("AmbientLevel", &DeferredCompositingNodePbr::_ambientLevel);
  c->floatProperty("EnvironmentIntensity", float_range{0, 100}, &DeferredCompositingNodePbr::_environmentIntensity);
  c->floatProperty("EnvironmentMipBias", float_range{0, 12}, &DeferredCompositingNodePbr::_environmentMipBias);
  c->floatProperty("EnvironmentMipScale", float_range{0, 100}, &DeferredCompositingNodePbr::_environmentMipScale);
  c->floatProperty("DiffuseLevel", float_range{0, 10}, &DeferredCompositingNodePbr::_diffuseLevel);
  c->floatProperty("SpecularLevel", float_range{0, 10}, &DeferredCompositingNodePbr::_specularLevel);
  c->floatProperty("SkyboxLevel", float_range{0, 10}, &DeferredCompositingNodePbr::_skyboxLevel);
  c->floatProperty("DepthFogDistance", float_range{0.1, 5000}, &DeferredCompositingNodePbr::_depthFogDistance);
  c->floatProperty("DepthFogPower", float_range{0.01, 100.0}, &DeferredCompositingNodePbr::_depthFogPower);

  c->accessorProperty(
       "EnvironmentTexture", &DeferredCompositingNodePbr::_readEnvTexture, &DeferredCompositingNodePbr::_writeEnvTexture)
      ->annotate<ConstString>("editor.class", "ged.factory.assetlist")
      ->annotate<ConstString>("editor.assettype", "lev2tex")
      ->annotate<ConstString>("editor.assetclass", "lev2tex");
}

void DeferredCompositingNodePbr::_readEnvTexture(ork::rtti::ICastable*& tex) const {
  tex = _environmentTextureAsset;
}

void DeferredCompositingNodePbr::_writeEnvTexture(ork::rtti::ICastable* const& tex) {
  _environmentTextureAsset = tex ? rtti::autocast(tex) : nullptr;
  if (nullptr == _environmentTextureAsset)
    return;
  printf("WTF1 <%p>\n\n", _environmentTextureAsset);
  ////////////////////////////////////////////////////////////////////////////////
  // irradiance map preprocessor
  ////////////////////////////////////////////////////////////////////////////////
  _environmentTextureAsset->_varmap.makeValueForKey<Texture::proc_t>("postproc") =
      [this](Texture* tex, Context* targ, datablockptr_t datablock) -> datablockptr_t {
    printf(
        "EnvironmentTexture Irradiance PreProcessor tex<%p:%s> datablocklen<%zu>...\n",
        tex,
        tex->_debugName.c_str(),
        datablock->length());
    boost::Crc64 hasher;
    hasher.accumulateString("irradiancemap");
    hasher.accumulateItem<uint64_t>(datablock->hash()); // data content
    hasher.finish();
    uint64_t cachekey = hasher.result();
    auto irrmapdblock = DataBlockCache::findDataBlock(cachekey);
    if (0) { // irrmapdblock) {
      // found in cache
      datablock = irrmapdblock;
    } else {
      DataBlockInputStream istream(datablock);
      // not found in cache, generate
      irrmapdblock = std::make_shared<DataBlock>();
      ///////////////////////////
      _filtenvSpecularMap = PBRMaterial::filterSpecularEnvMap(tex, targ);
      _filtenvDiffuseMap  = PBRMaterial::filterDiffuseEnvMap(tex, targ);
      //////////////////////////////////////////////////////////////
      // DataBlockCache::setDataBlock(cachekey, irrmapdblock);
      datablock = irrmapdblock;
    }
    return datablock;
  };
  ////////////////////////////////////////////////////////////////////////////////
}

lev2::Texture* DeferredCompositingNodePbr::envSpecularTexture() const {
  return _filtenvSpecularMap;
}
lev2::Texture* DeferredCompositingNodePbr::envDiffuseTexture() const {
  return _filtenvDiffuseMap;
}

///////////////////////////////////////////////////////////////////////////////
struct IMPL {
  static const int KMAXLIGHTS = 8;
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  IMPL(DeferredCompositingNodePbr* node)
      : _camname(AddPooledString("Camera"))
      , _context(node, "orkshader://deferred", KMAXLIGHTS)
      , _lightProcessor(_context, node) {
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ~IMPL() {
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void init(lev2::Context* context) {
    _context.gpuInit(context);
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void _render(DeferredCompositingNodePbr* node, CompositorDrawData& drawdata) {
    //_timer.Start();
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto targ                    = RCFD.GetTarget();
    auto CIMPL                   = drawdata._cimpl;
    auto FBI                     = targ->FBI();
    auto this_buf                = FBI->GetThisBuffer();
    auto RSI                     = targ->RSI();
    const auto TOPCPD            = CIMPL->topCPD();
    //////////////////////////////////////////////////////
    _context.renderUpdate(drawdata);
    auto VD = _context.computeViewData(drawdata);
    _context.updateDebugLights(VD);
    _context._clearColor = node->_clearColor;
    /////////////////////////////////////////////////////////////////////////////////////////
    bool is_stereo = VD._isStereo;
    /////////////////////////////////////////////////////////////////////////////////////////
    targ->debugPushGroup("Deferred::render");
    _context.renderGbuffer(drawdata, VD);
    targ->debugPushGroup("Deferred::LightAccum");
    _context._accumCPD = TOPCPD;
    /////////////////////////////////////////////////////////////////
    auto vprect   = SRect(0, 0, _context._width, _context._height);
    auto quadrect = SRect(0, 0, _context._width, _context._height);
    _context._accumCPD.SetDstRect(vprect);
    _context._accumCPD._irendertarget        = _context._accumRT;
    _context._accumCPD._cameraMatrices       = nullptr;
    _context._accumCPD._stereoCameraMatrices = nullptr;
    _context._accumCPD._stereo1pass          = false;
    _context._specularLevel                  = node->specularLevel() * node->environmentIntensity();
    _context._diffuseLevel                   = node->diffuseLevel() * node->environmentIntensity();
    float skybox_level                       = node->skyboxLevel() * node->environmentIntensity();
    CIMPL->pushCPD(_context._accumCPD); // base lighting
    FBI->SetAutoClear(true);
    FBI->PushRtGroup(_context._rtgLaccum);
    targ->beginFrame();
    FBI->Clear(fvec4(0.1, 0.2, 0.3, 1), 1.0f);
    //////////////////////////////////////////////////////////////////
    // base lighting (environent IBL lighting)
    //////////////////////////////////////////////////////////////////
    targ->debugPushGroup("Deferred::BaseLighting");
    _context._lightingmtl.bindTechnique(is_stereo ? _context._tekEnvironmentLightingStereo : _context._tekEnvironmentLighting);
    _context._lightingmtl._rasterstate.SetBlending(EBLENDING_OFF);
    _context._lightingmtl._rasterstate.SetDepthTest(EDEPTHTEST_OFF);
    _context._lightingmtl._rasterstate.SetCullTest(ECULLTEST_PASS_BACK);
    _context._lightingmtl.begin(RCFD);
    //////////////////////////////////////////////////////
    _context._lightingmtl.bindParamMatrixArray(_context._parMatIVPArray, VD._ivp, 2);
    _context._lightingmtl.bindParamMatrixArray(_context._parMatVArray, VD._v, 2);
    _context._lightingmtl.bindParamMatrixArray(_context._parMatPArray, VD._p, 2);
    _context._lightingmtl.bindParamVec2(_context._parZndc2eye, VD._zndc2eye);

    /////////////////////////

    _context._lightingmtl.bindParamFloat(_context._parDepthFogDistance, 1.0f / node->depthFogDistance());
    _context._lightingmtl.bindParamFloat(_context._parDepthFogPower, node->depthFogPower());

    /////////////////////////

    _context._lightingmtl.bindParamCTex(_context._parMapGBufAlbAo, _context._rtgGbuffer->GetMrt(0)->GetTexture());
    _context._lightingmtl.bindParamCTex(_context._parMapGBufNrmL, _context._rtgGbuffer->GetMrt(1)->GetTexture());
    _context._lightingmtl.bindParamCTex(_context._parMapGBufRufMtlAlpha, _context._rtgGbuffer->GetMrt(2)->GetTexture());
    _context._lightingmtl.bindParamCTex(_context._parMapDepth, _context._rtgGbuffer->_depthTexture);

    _context._lightingmtl.bindParamCTex(_context._parMapSpecularEnv, node->envSpecularTexture());
    _context._lightingmtl.bindParamCTex(_context._parMapDiffuseEnv, node->envDiffuseTexture());

    OrkAssert(_context.brdfIntegrationTexture() != nullptr);
    _context._lightingmtl.bindParamCTex(_context._parMapBrdfIntegration, _context.brdfIntegrationTexture());

    /////////////////////////

    _context._lightingmtl.bindParamFloat(_context._parSkyboxLevel, skybox_level);
    _context._lightingmtl.bindParamVec3(_context._parAmbientLevel, node->ambientLevel());
    _context._lightingmtl.bindParamFloat(_context._parSpecularLevel, _context._specularLevel);
    _context._lightingmtl.bindParamFloat(_context._parDiffuseLevel, _context._diffuseLevel);

    /////////////////////////

    _context._lightingmtl.bindParamFloat(_context._parEnvironmentMipBias, node->environmentMipBias());
    _context._lightingmtl.bindParamFloat(_context._parEnvironmentMipScale, node->environmentMipScale());

    /////////////////////////

    _context._lightingmtl.bindParamVec2(_context._parNearFar, fvec2(0.1, 1000));
    _context._lightingmtl.bindParamVec2(
        _context._parInvViewSize, fvec2(1.0 / float(_context._width), 1.0f / float(_context._height)));
    _context._lightingmtl.commit();
    RSI->BindRasterState(_context._lightingmtl._rasterstate);
    this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 0, 0));
    _context._lightingmtl.end(RCFD);
    targ->debugPopGroup(); // BaseLighting

    /////////////////////////////////
    // Dynamic Lighting
    /////////////////////////////////

    if (auto lmgr = CIMPL->lightManager()) {

      // printf("lightmgr<%p>\n", lmgr);
      if (0) {
        lmgr->enumerateInPass(_context._accumCPD, _enumeratedLights);
        auto& lights = _enumeratedLights._enumeratedLights;

        if (lights.size())
          _lightProcessor.render(drawdata, VD, _enumeratedLights);
      }
    }

    /////////////////////////////////
    // end frame
    /////////////////////////////////

    CIMPL->popCPD(); // base lighting
    targ->endFrame();
    FBI->PopRtGroup(); // deferredRtg

    targ->debugPopGroup(); // "Deferred::LightAccum"
    targ->debugPopGroup(); // "Deferred::render"
    // float totaltime = _timer.SecsSinceStart();
    // printf( "Deferred::_render totaltime<%g>\n", totaltime );
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  PoolString _camname;

  DeferredContext _context;
  int _sequence = 0;
  std::atomic<int> _lightjobcount;
  ork::Timer _timer;
  EnumeratedLights _enumeratedLights;
  CpuLightProcessor _lightProcessor;

}; // IMPL

///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNodePbr::DeferredCompositingNodePbr() {
  _impl = std::make_shared<IMPL>(this);
}
///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNodePbr::~DeferredCompositingNodePbr() {
}
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNodePbr::DoInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.Get<std::shared_ptr<IMPL>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNodePbr::DoRender(CompositorDrawData& drawdata) {
  auto impl = _impl.Get<std::shared_ptr<IMPL>>();
  impl->_render(this, drawdata);
}
///////////////////////////////////////////////////////////////////////////////
RtBuffer* DeferredCompositingNodePbr::GetOutput() const {
  static int i = 0;
  i++;
  return _impl.Get<std::shared_ptr<IMPL>>()->_context._rtgLaccum->GetMrt(0);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::deferrednode
