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
#include <ork/kernel/datacache.inl>
#include <ork/gfx/brdf.inl>
#include <ork/gfx/dds.h>
//#include <ork/gfx/image.inl>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/texman.h>

#include "NodeCompositorDeferred.h"

ImplementReflectionX(ork::lev2::deferrednode::DeferredCompositingNodeDebugNormal, "DeferredCompositingNodeDebugNormal");

// fvec3 LightColor
// fvec4 LightPosR 16byte
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::deferrednode {
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNodeDebugNormal::describeX(class_t* c) {
  c->memberProperty("ClearColor", &DeferredCompositingNodeDebugNormal::_clearColor);
  c->memberProperty("FogColor", &DeferredCompositingNodeDebugNormal::_fogColor);
  c->memberProperty("AmbientLevel", &DeferredCompositingNodeDebugNormal::_ambientLevel);
  c->floatProperty("EnvironmentIntensity", float_range{-10, 10}, &DeferredCompositingNodeDebugNormal::_environmentIntensity);
  c->floatProperty("EnvironmentMipBias", float_range{0, 12}, &DeferredCompositingNodeDebugNormal::_environmentMipBias);
  c->floatProperty("EnvironmentMipScale", float_range{0, 100}, &DeferredCompositingNodeDebugNormal::_environmentMipScale);
  c->floatProperty("DiffuseLevel", float_range{-5, 5}, &DeferredCompositingNodeDebugNormal::_diffuseLevel);
  c->floatProperty("SpecularLevel", float_range{-5, 5}, &DeferredCompositingNodeDebugNormal::_specularLevel);

  c->accessorProperty(
       "EnvironmentTexture",
       &DeferredCompositingNodeDebugNormal::_readEnvTexture,
       &DeferredCompositingNodeDebugNormal::_writeEnvTexture)
      ->annotate<ConstString>("editor.class", "ged.factory.assetlist")
      ->annotate<ConstString>("editor.assettype", "lev2tex")
      ->annotate<ConstString>("editor.assetclass", "lev2tex");
}

void DeferredCompositingNodeDebugNormal::_readEnvTexture(ork::rtti::ICastable*& tex) const {
  tex = _environmentTextureAsset;
}

void DeferredCompositingNodeDebugNormal::_writeEnvTexture(ork::rtti::ICastable* const& tex) {
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
      _brdfIntegrationMap = PBRMaterial::brdfIntegrationMap(targ);
      //////////////////////////////////////////////////////////////
      // DataBlockCache::setDataBlock(cachekey, irrmapdblock);
      datablock = irrmapdblock;
    }
    return datablock;
  };
  ////////////////////////////////////////////////////////////////////////////////
}

lev2::Texture* DeferredCompositingNodeDebugNormal::envSpecularTexture() const {
  return _filtenvSpecularMap;
}
lev2::Texture* DeferredCompositingNodeDebugNormal::envDiffuseTexture() const {
  return _filtenvDiffuseMap;
}
lev2::Texture* DeferredCompositingNodeDebugNormal::brdfIntegrationTexture() const {
  return _brdfIntegrationMap;
}

///////////////////////////////////////////////////////////////////////////////
struct IMPL {
  static const int KMAXLIGHTS = 8;
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  IMPL(DeferredCompositingNodeDebugNormal* node)
      : _camname(AddPooledString("Camera"))
      , _context(node, "orkshader://deferred", KMAXLIGHTS) {
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ~IMPL() {
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void init(lev2::Context* target) {
    _context.gpuInit(target);
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void _render(DeferredCompositingNodeDebugNormal* node, CompositorDrawData& drawdata) {
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
    _context.update(VD);
    _context._clearColor = node->_clearColor;
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
    CIMPL->pushCPD(_context._accumCPD); // base lighting
    FBI->SetAutoClear(true);
    FBI->PushRtGroup(_context._rtgLaccum);
    targ->beginFrame();
    FBI->Clear(fvec4(0.1, 0.2, 0.3, 1), 1.0f);
    //////////////////////////////////////////////////////////////////
    // base lighting
    //////////////////////////////////////////////////////////////////
    targ->debugPushGroup("Deferred::BaseLighting");
    _context._lightingmtl.bindTechnique(VD._isStereo ? _context._tekDebugNormalStereo : _context._tekDebugNormal);
    _context._lightingmtl._rasterstate.SetBlending(EBLENDING_OFF);
    _context._lightingmtl._rasterstate.SetDepthTest(EDEPTHTEST_OFF);
    _context._lightingmtl._rasterstate.SetCullTest(ECULLTEST_PASS_BACK);
    _context._lightingmtl.begin(RCFD);
    //////////////////////////////////////////////////////
    _context._lightingmtl.bindParamMatrixArray(_context._parMatIVPArray, VD._ivp, 2);
    _context._lightingmtl.bindParamMatrixArray(_context._parMatVArray, VD._v, 2);
    _context._lightingmtl.bindParamMatrixArray(_context._parMatPArray, VD._p, 2);

    /////////////////////////

    _context._lightingmtl.bindParamCTex(_context._parMapGBufAlbAo, _context._rtgGbuffer->GetMrt(0)->GetTexture());
    _context._lightingmtl.bindParamCTex(_context._parMapGBufNrmL, _context._rtgGbuffer->GetMrt(1)->GetTexture());
    _context._lightingmtl.bindParamCTex(_context._parMapGBufRufMtlAlpha, _context._rtgGbuffer->GetMrt(2)->GetTexture());
    _context._lightingmtl.bindParamCTex(_context._parMapDepth, _context._rtgGbuffer->_depthTexture);

    _context._lightingmtl.bindParamCTex(_context._parMapSpecularEnv, node->envSpecularTexture());
    _context._lightingmtl.bindParamCTex(_context._parMapDiffuseEnv, node->envDiffuseTexture());

    OrkAssert(node->brdfIntegrationTexture() != nullptr);
    _context._lightingmtl.bindParamCTex(_context._parMapBrdfIntegration, node->brdfIntegrationTexture());

    /////////////////////////

    _context._lightingmtl.bindParamFloat(_context._parSkyboxLevel, node->skyboxLevel());
    _context._lightingmtl.bindParamVec3(_context._parAmbientLevel, node->ambientLevel());
    _context._lightingmtl.bindParamFloat(_context._parSpecularLevel, node->specularLevel());
    _context._lightingmtl.bindParamFloat(_context._parDiffuseLevel, node->diffuseLevel());

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
    CIMPL->popCPD();       // base lighting
    targ->debugPopGroup(); // BaseLighting
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
  FxShaderParamBuffer* _lightbuffer = nullptr;
}; // IMPL

///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNodeDebugNormal::DeferredCompositingNodeDebugNormal() {
  _impl = std::make_shared<IMPL>(this);
}
///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNodeDebugNormal::~DeferredCompositingNodeDebugNormal() {
}
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNodeDebugNormal::DoInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.Get<std::shared_ptr<IMPL>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNodeDebugNormal::DoRender(CompositorDrawData& drawdata) {
  auto impl = _impl.Get<std::shared_ptr<IMPL>>();
  impl->_render(this, drawdata);
}
///////////////////////////////////////////////////////////////////////////////
RtBuffer* DeferredCompositingNodeDebugNormal::GetOutput() const {
  static int i = 0;
  i++;
  return _impl.Get<std::shared_ptr<IMPL>>()->_context._rtgLaccum->GetMrt(0);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::deferrednode
