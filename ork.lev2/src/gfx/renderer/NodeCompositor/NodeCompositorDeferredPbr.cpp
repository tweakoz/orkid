////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <algorithm>
#include <ork/pch.h>
#include <ork/rtti/Class.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/mutex.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/kernel/datacache.h>
#include <ork/gfx/brdf.inl>
#include <ork/gfx/dds.h>
//#include <ork/gfx/image.inl>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/texman.h>

#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorDeferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/CpuLightProcessor.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/SimpleLightProcessor.h>
#include <ork/profiling.inl>
#include <ork/asset/Asset.inl>

ImplementReflectionX(ork::lev2::deferrednode::DeferredCompositingNodePbr, "DeferredCompositingNodePbr");

// fvec3 LightColor
// fvec4 LightPosR 16byte
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::deferrednode {
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNodePbr::describeX(class_t* c) {

  using namespace asset;

  class_t::CreateClassAlias("DeferredCompositingNodeDebugNormal", c);

  c->directProperty("ClearColor", &DeferredCompositingNodePbr::_clearColor);
  c->directProperty("AmbientLevel", &DeferredCompositingNodePbr::_ambientLevel);
  c->floatProperty("EnvironmentIntensity", float_range{0, 100}, &DeferredCompositingNodePbr::_environmentIntensity);
  c->floatProperty("EnvironmentMipBias", float_range{0, 12}, &DeferredCompositingNodePbr::_environmentMipBias);
  c->floatProperty("EnvironmentMipScale", float_range{0, 100}, &DeferredCompositingNodePbr::_environmentMipScale);
  c->floatProperty("DiffuseLevel", float_range{0, 10}, &DeferredCompositingNodePbr::_diffuseLevel);
  c->floatProperty("SpecularLevel", float_range{0, 10}, &DeferredCompositingNodePbr::_specularLevel);
  c->floatProperty("SkyboxLevel", float_range{0, 10}, &DeferredCompositingNodePbr::_skyboxLevel);
  c->floatProperty("DepthFogDistance", float_range{0.1, 5000}, &DeferredCompositingNodePbr::_depthFogDistance);
  c->floatProperty("DepthFogPower", float_range{0.01, 100.0}, &DeferredCompositingNodePbr::_depthFogPower);

  c->accessorProperty(
       "EnvironmentTexture", //
       &DeferredCompositingNodePbr::_readEnvTexture,
       &DeferredCompositingNodePbr::_writeEnvTexture)
      ->annotate<ConstString>("editor.class", "ged.factory.assetlist")
      ->annotate<ConstString>("editor.assettype", "lev2tex")
      ->annotate<ConstString>("editor.assetclass", "lev2tex")
      ->annotate<asset::vars_gen_t>(
          "asset.deserialize.vargen", //
          [](ork::object_ptr_t obj) -> asset::vars_ptr_t {
            auto node = std::dynamic_pointer_cast<DeferredCompositingNodePbr>(obj);
            OrkAssert(node);
            OrkAssert(false);
            return node->_pbrcommon->_texAssetVarMap;
          });
}

void DeferredCompositingNodePbr::_readEnvTexture(asset::asset_ptr_t& tex) const {
  tex = _environmentTextureAsset;
}

void DeferredCompositingNodePbr::setEnvTexturePath(file::Path path) {
  auto envl_asset = asset::AssetManager<TextureAsset>::load(path.c_str());
  OrkAssert(false);
  // TODO - inject asset postload ops ()
}

void DeferredCompositingNodePbr::_writeEnvTexture(asset::asset_ptr_t const& tex) {
  asset::vars_constptr_t old_varmap;
  if(_environmentTextureAsset){
    old_varmap = _environmentTextureAsset->_varmap;
    //printf("OLD <%p:%s>\n\n", _environmentTextureAsset.get(),_environmentTextureAsset->name().c_str());
  }
  //printf("NEW <%p:%s>\n\n", tex.get(),tex->name().c_str());

  _environmentTextureAsset = tex;
  if (nullptr == _environmentTextureAsset)
    return;
  _environmentTextureAsset->_varmap = _pbrcommon->_texAssetVarMap;
}

lev2::texture_ptr_t DeferredCompositingNodePbr::envSpecularTexture() const {
  return _pbrcommon->_filtenvSpecularMap;
}
lev2::texture_ptr_t DeferredCompositingNodePbr::envDiffuseTexture() const {
  return _pbrcommon->_filtenvDiffuseMap;
}

///////////////////////////////////////////////////////////////////////////////
struct PbrNodeImpl {
  static const int KMAXLIGHTS = 32;
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  PbrNodeImpl(DeferredCompositingNodePbr* node)
      : _camname(AddPooledString("Camera"))
      , _context(node, "orkshader://deferred", KMAXLIGHTS)
      , _lightProcessor(_context, node) {
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ~PbrNodeImpl() {
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void init(lev2::Context* context) {
    _context.gpuInit(context);
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void _render(DeferredCompositingNodePbr* node, CompositorDrawData& drawdata) {
    _timer.Start();
    EASY_BLOCK("pbr-_render");
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto targ                    = RCFD.GetTarget();
    auto CIMPL                   = drawdata._cimpl;
    auto FBI                     = targ->FBI();
    auto this_buf                = FBI->GetThisBuffer();
    auto RSI                     = targ->RSI();
    auto DWI                     = targ->DWI();
    const auto TOPCPD            = CIMPL->topCPD();
    /////////////////////////////////////////////////
    RCFD.setUserProperty("rtg_gbuffer"_crc,_context._rtgGbuffer);
    RCFD.setUserProperty("rtb_gbuffer"_crc,_context._rtbGbuffer);
    RCFD.setUserProperty("rtb_accum"_crc,_context._rtbLightAccum );
    RCFD._renderingmodel = node->_renderingmodel;
    //////////////////////////////////////////////////////
    _context.renderUpdate(drawdata);
    auto VD = drawdata.computeViewData();
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
    auto vprect   = ViewportRect(0, 0, _context._width, _context._height);
    auto quadrect = SRect(0, 0, _context._width, _context._height);
    _context._accumCPD.SetDstRect(vprect);
    _context._accumCPD._irendertarget        = _context._accumRT;
    _context._accumCPD._cameraMatrices       = nullptr;
    _context._accumCPD._stereoCameraMatrices = nullptr;
    _context._accumCPD._stereo1pass          = false;
    _context._specularLevel                  = node->specularLevel() * node->environmentIntensity();
    _context._diffuseLevel                   = node->diffuseLevel() * node->environmentIntensity();
    _context._depthFogDistance               = node->depthFogDistance();
    _context._depthFogPower                  = node->depthFogPower();
    float skybox_level                       = node->skyboxLevel() * node->environmentIntensity();
    CIMPL->pushCPD(_context._accumCPD); // base lighting
    FBI->SetAutoClear(true);
    FBI->PushRtGroup(_context._rtgLaccum.get());
    // targ->beginFrame();
    FBI->Clear(fvec4(0.1, 0.2, 0.3, 1), 1.0f);
    //////////////////////////////////////////////////////////////////
    if (auto lmgr = CIMPL->lightManager()) {
      EASY_BLOCK("lights-1");
      lmgr->enumerateInPass(_context._accumCPD, _enumeratedLights);
      _lightProcessor.gpuUpdate(drawdata, VD, _enumeratedLights);
      auto& lights = _enumeratedLights._enumeratedLights;
      if (lights.size())
        _lightProcessor.renderDecals(drawdata, VD, _enumeratedLights);
    }
    //////////////////////////////////////////////////////////////////
    // base lighting (environent IBL lighting)
    //////////////////////////////////////////////////////////////////
    targ->debugPushGroup("Deferred::BaseLighting");
    _context._lightingmtl._rasterstate.SetBlending(Blending::OFF);
    _context._lightingmtl._rasterstate.SetDepthTest(EDEPTHTEST_OFF);
    _context._lightingmtl._rasterstate.SetCullTest(ECULLTEST_PASS_BACK);


    int pbr_model = RCFD.getUserProperty("pbr_model"_crc).get<int>();

    switch( pbr_model ){
      case 1:
      _context._lightingmtl.begin(
          is_stereo //
              ? _context._tekEnvironmentLightingSDFStereo
              : _context._tekEnvironmentLightingSDF,
          RCFD);
        break;
      case 0:
      default:
      _context._lightingmtl.begin(
          is_stereo //
              ? _context._tekEnvironmentLightingStereo
              : _context._tekEnvironmentLighting,
          RCFD);
        break;
    }
    //////////////////////////////////////////////////////

    _context._lightingmtl.bindParamFloat(_context._parDepthFogDistance, 1.0f / node->depthFogDistance());
    _context._lightingmtl.bindParamFloat(_context._parDepthFogPower, node->depthFogPower());

    /////////////////////////

    _context._lightingmtl.bindParamCTex(_context._parMapGBuf, _context._rtgGbuffer->GetMrt(0)->texture());

    //_context._lightingmtl.bindParamCTex(_context._parMapGBufAlbAo, _context._rtgGbuffer->GetMrt(0)->texture());
    //_context._lightingmtl.bindParamCTex(_context._parMapGBufNrmL, _context._rtgGbuffer->GetMrt(1)->texture());
    //_context._lightingmtl.bindParamCTex(_context._parMapGBufRufMtlAlpha, _context._rtgGbuffer->GetMrt(2)->texture());
    _context._lightingmtl.bindParamCTex(_context._parMapDepth, _context._rtgGbuffer->_depthTexture);

    _context._lightingmtl.bindParamCTex(_context._parMapSpecularEnv, node->envSpecularTexture().get());
    _context._lightingmtl.bindParamCTex(_context._parMapDiffuseEnv, node->envDiffuseTexture().get());

    OrkAssert(_context.brdfIntegrationTexture() != nullptr);
    _context._lightingmtl.bindParamCTex(_context._parMapBrdfIntegration, _context.brdfIntegrationTexture().get());

    _context._lightingmtl.bindParamCTex(_context._parMapVolTexA, _context._voltexA->_texture.get());

    /////////////////////////
    _context._lightingmtl.bindParamFloat(_context._parSkyboxLevel, skybox_level);
    _context._lightingmtl.bindParamVec3(_context._parAmbientLevel, node->ambientLevel());
    _context._lightingmtl.bindParamFloat(_context._parSpecularLevel, _context._specularLevel);
    _context._lightingmtl.bindParamFloat(_context._parDiffuseLevel, _context._diffuseLevel);
    /////////////////////////
    _context._lightingmtl.bindParamFloat(_context._parEnvironmentMipBias, node->environmentMipBias());
    _context._lightingmtl.bindParamFloat(_context._parEnvironmentMipScale, node->environmentMipScale());
    /////////////////////////
    _context._lightingmtl._rasterstate.SetZWriteMask(false);
    _context._lightingmtl._rasterstate.SetDepthTest(EDEPTHTEST_OFF);
    _context._lightingmtl._rasterstate.SetAlphaTest(EALPHATEST_OFF);
    /////////////////////////
    _context.bindViewParams(VD);
    /////////////////////////
    _context._lightingmtl.commit();
    RSI->BindRasterState(_context._lightingmtl._rasterstate);
    DWI->quad2DEMLTiled(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 0, 0), 16);
    _context._lightingmtl.end(RCFD);
    targ->debugPopGroup(); // BaseLighting

    /////////////////////////////////
    // Dynamic Lighting
    /////////////////////////////////

    if (auto lmgr = CIMPL->lightManager()) {
      EASY_BLOCK("lights-2");
      if (_enumeratedLights._enumeratedLights.size())
        _lightProcessor.renderLights(drawdata, VD, _enumeratedLights);
    }

    /////////////////////////////////
    // end frame
    /////////////////////////////////

    CIMPL->popCPD(); // base lighting
    // targ->endFrame();
    FBI->PopRtGroup(); // deferredRtg

    targ->debugPopGroup(); // "Deferred::LightAccum"
    targ->debugPopGroup(); // "Deferred::render"
     //float totaltime = _timer.SecsSinceStart();
     //printf( "Deferred::_render totaltime<%g>\n", totaltime );
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  PoolString _camname;

  DeferredContext _context;
  int _sequence = 0;
  std::atomic<int> _lightjobcount;
  ork::Timer _timer;
  EnumeratedLights _enumeratedLights;
  SimpleLightProcessor _lightProcessor;

}; // IMPL

///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNodePbr::DeferredCompositingNodePbr() {
  _renderingmodel = RenderingModel(ERenderModelID::DEFERRED_PBR);
  _impl           = std::make_shared<PbrNodeImpl>(this);
  _pbrcommon = std::make_shared<pbr::CommonStuff>();
}

///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNodePbr::~DeferredCompositingNodePbr() {
}
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNodePbr::doGpuInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.get<std::shared_ptr<PbrNodeImpl>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNodePbr::DoRender(CompositorDrawData& drawdata) {
  EASY_BLOCK("pbr-DoRender");
  auto impl = _impl.get<std::shared_ptr<PbrNodeImpl>>();
  impl->_render(this, drawdata);
}
///////////////////////////////////////////////////////////////////////////////
rtbuffer_ptr_t DeferredCompositingNodePbr::GetOutput() const {
  auto& CTX = _impl.get<std::shared_ptr<PbrNodeImpl>>()->_context;
  return CTX._rtbLightAccum;
}
///////////////////////////////////////////////////////////////////////////////
rtgroup_ptr_t DeferredCompositingNodePbr::GetOutputGroup() const {
  auto& CTX = _impl.get<std::shared_ptr<PbrNodeImpl>>()->_context;
  return CTX._rtgGbuffer;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::deferrednode
