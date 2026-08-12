////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/util/crc.h>
#include <ork/file/path.h>
#include <ork/file/chunkfile.inl>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/gfx/brdf.inl>
#include <ork/pch.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <OpenImageIO/imageio.h>
#include <ork/kernel/datacache.h>
#include <ork/reflect/properties/registerX.inl>
//
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>
#include <ork/util/logger.h>

OIIO_NAMESPACE_USING

ImplementReflectionX(ork::lev2::PBRMaterial, "PBRMaterial");

namespace ork::lev2 {

static logchannel_ptr_t logchan_pbr = logger()->configureChannel("mtlpbr", fvec3(0.8, 0.8, 0.1), true);

///////////////////////////////////////////////////////////////////////////////

struct GlobalDefaultMaterial {
  GlobalDefaultMaterial(Context* ctx) {
    _material = std::make_shared<PBRMaterial>(ctx);
  }
  pbrmaterial_ptr_t _material;
};

pbrmaterial_ptr_t default3DMaterial(Context* ctx) {
  static GlobalDefaultMaterial _gdm(ctx);
  return _gdm._material;
}

///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::describeX(class_t* c) {

  /////////////////////////////////////////////////////////////////
  // chunkfile::materialreader_t
  //  this is a callback invoked from the xgm model reader
  //  when a material is encountered in the xgm file
  //  it is in callback form so the model reader can fork
  //  based on material types..
  /////////////////////////////////////////////////////////////////

  chunkfile::materialreader_t reader = [](chunkfile::XgmMaterialReaderContext& ctx) -> material_ptr_t {
    return PBRMaterial::_xgmReader(ctx);
  };

  /////////////////////////////////////////////////////////////////
  // chunkfile::materialreader_t
  //  this is a callback invoked from the xgm model writer
  //  when a material is encountered in a model being written to a xgm file
  //  it is in callback form so the model writer can fork
  //  based on material types..
  /////////////////////////////////////////////////////////////////

  chunkfile::materialwriter_t writer = [](chunkfile::XgmMaterialWriterContext& ctx) {
    PBRMaterial::_xgmWriter(ctx);
  };

  /////////////////////////////////////////////////////////////////
  // attach reader and writer to the material class reflection annotations
  /////////////////////////////////////////////////////////////////

  c->annotate("xgm.writer", writer);
  c->annotate("xgm.reader", reader);
}

///////////////////////////////////////////////////////////////////////////////

PBRMaterial::PBRMaterial(Context* targ)
    : PBRMaterial() {
  gpuInit(targ);
}

///////////////////////////////////////////////////////////////////////////////

PBRMaterial::PBRMaterial()
    : _baseColor(1, 1, 1) {
  _vars = std::make_shared<varmap::VarMap>();
  _rasterstate->setBlendingMacro(BlendingMacro::OFF);
  _rasterstate->setDepthTest(EDepthTest::LEQUALS);
  _rasterstate->setWriteMaskZ(true);
  _rasterstate->setCullTest(ECullTest::PASS_FRONT);
  miNumPasses = 1;
  _shaderpath = "orkshader://pbr";
  // printf( "new PBRMaterial<%p>\n", this );
}

///////////////////////////////////////////////////////////////////////////////

PBRMaterial::~PBRMaterial() {
  // Pointer-keyed cache eviction — see FxPipelineCacheImpl::removeCache. Skipping
  // this leaves a cache reachable by the next material that lands on this
  // address, whose pipelines then rebind THIS material's freed resources.
  auto dead_cache = _evictPbrPipelineCache(this);
  if (dead_cache and _initialTarget) {
    // Its pipelines may hold the last ref to GPU-owning binds; drop them on the
    // owning context's thread, past the frames still in flight.
    constexpr int kDelayFrames = 3; // > MAX_FRAMES_IN_FLIGHT
    _initialTarget->enqueueDelayedDestroy([dead_cache]() {}, kDelayFrames);
  }
}

///////////////////////////////////////////////////////////////////////////////

pbrmaterial_ptr_t PBRMaterial::clone() const {
  auto copy = std::make_shared<PBRMaterial>();
  *copy     = *this;
  copy->_initialTarget = nullptr;
  return copy;
}

///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::gpuInit(Context* targ) /*final*/ {

  // printf( "PBRMaterial::gpuInit<%p> _initialTarget<%p> targ<%p>\n", this, _initialTarget, targ );

  if (_initialTarget)
    return;

  _initialTarget = targ;
  auto fxi       = targ->FXI();

  auto loadreq = std::make_shared<asset::LoadRequest>();

  //printf( "PBRMaterial::gpuInit<%p> _shaderpath<%s>\n", this, _shaderpath.c_str() );
  loadreq->_asset_path = _shaderpath;

  _as_freestyle = std::make_shared<FreestyleMaterial>();
  _as_freestyle->gpuInit(targ, _shaderpath);
  _asset_shader = _as_freestyle->_shaderasset;
  _shader       = _as_freestyle->_shader;

  // specials

  _tek_GBU_DB_NM_NI_MO = fxi->technique(_shader, "GBU_DB_NM_NI_MO"s + _shader_suffix);

  _tek_GBU_CF_IN_MO = fxi->technique(_shader, "GBU_CF_IN_MO"s + _shader_suffix);
  _tek_GBU_CF_NI_MO = fxi->technique(_shader, "GBU_CF_NI_MO"s + _shader_suffix);

  _tek_PIK_RI_IN = fxi->technique(_shader, "PIK_RI_IN"s + _shader_suffix);
  _tek_PIK_RI_NI = fxi->technique(_shader, "PIK_RI_NI"s + _shader_suffix);
  _tek_PIK_SK_NI = fxi->technique(_shader, "PIK_SK_NI"s + _shader_suffix);

  // forwards

  _tek_FWD_UNLIT_NI_MO = fxi->technique(_shader, "FWD_UNLIT_NI_MO"s + _shader_suffix);

  _tek_FWD_SKYBOX_MO = fxi->technique(_shader, "FWD_SKYBOX_MO"s + _shader_suffix);
  _tek_FWD_SKYBOX_ST = fxi->technique(_shader, "FWD_SKYBOX_ST"s + _shader_suffix);
  _tek_FWD_SKYBOX_PROC = fxi->technique(_shader, "FWD_SKYBOX_PROC"s + _shader_suffix);
  _tek_FWD_SKYBOX_PROC_ST = fxi->technique(_shader, "FWD_SKYBOX_PROC_ST"s + _shader_suffix);

  _tek_FWD_CT_NM_RI_NI_MO = fxi->technique(_shader, "FWD_CT_NM_RI_NI_MO"s + _shader_suffix);
  _tek_FWD_CV_NM_RI_NI_MO = fxi->technique(_shader, "FWD_CV_NM_RI_NI_MO"s + _shader_suffix);
  _tek_FWD_CV_NM_RI_NI_MO_ALPHA = fxi->technique(_shader, "FWD_CV_NM_RI_NI_MO_ALPHA"s + _shader_suffix);
  _tek_FWD_CT_NM_RI_IN_MO = fxi->technique(_shader, "FWD_CT_NM_RI_IN_MO"s + _shader_suffix);
  _tek_FWD_CV_NM_RI_IN_MO = fxi->technique(_shader, "FWD_CV_NM_RI_IN_MO"s + _shader_suffix);
  _tek_FWD_CV_NM_RI_IN_MO_ALPHA = fxi->technique(_shader, "FWD_CV_NM_RI_IN_MO_ALPHA"s + _shader_suffix);
  _tek_FWD_CT_NM_RI_NI_ST = fxi->technique(_shader, "FWD_CT_NM_RI_NI_ST"s + _shader_suffix);
  _tek_FWD_CT_NM_RI_IN_ST = fxi->technique(_shader, "FWD_CT_NM_RI_IN_ST"s + _shader_suffix);
  _tek_FWD_CV_NM_RI_NI_ST = fxi->technique(_shader, "FWD_CV_NM_RI_NI_ST"s + _shader_suffix);
  _tek_FWD_CV_NM_RI_IN_ST = fxi->technique(_shader, "FWD_CV_NM_RI_IN_ST"s + _shader_suffix);
  _tek_FWD_CV_NM_RI_NI_ST_ALPHA = fxi->technique(_shader, "FWD_CV_NM_RI_NI_ST_ALPHA"s + _shader_suffix);
  _tek_FWD_CV_NM_RI_IN_ST_ALPHA = fxi->technique(_shader, "FWD_CV_NM_RI_IN_ST_ALPHA"s + _shader_suffix);

  _tek_FWD_CT_NM_SK_NI_MO = fxi->technique(_shader, "FWD_CT_NM_SK_NI_MO"s + _shader_suffix);
  _tek_FWD_CT_NM_SK_IN_MO = fxi->technique(_shader, "FWD_CT_NM_SK_IN_MO"s + _shader_suffix);
  _tek_FWD_CT_NM_SK_NI_ST = fxi->technique(_shader, "FWD_CT_NM_SK_NI_ST"s + _shader_suffix);
  _tek_FWD_CT_NM_SK_IN_ST = fxi->technique(_shader, "FWD_CT_NM_SK_IN_ST"s + _shader_suffix);

  // SSBO-sourced vertex variant (ptex3d FWD_SSBO_CUSTOM); no suffix — null for materials without it.
  _tek_FWD_SSBO_CUSTOM            = fxi->technique(_shader, "FWD_SSBO_CUSTOM");
  _tek_FWD_SSBO_CUSTOM_ST         = fxi->technique(_shader, "FWD_SSBO_CUSTOM_ST");         // null until the template emits it
  // cloud-shadow fill variant (ptex3d FWD_SUNCOOKIE, unlit surfaces only); null elsewhere.
  _tek_FWD_SUNCOOKIE              = fxi->technique(_shader, "FWD_SUNCOOKIE");
  _tek_FWD_SSBO_CUSTOM_INSTANCED = fxi->technique(_shader, "FWD_SSBO_CUSTOM_INSTANCED");  // null unless ssbo_instanced
  _tek_FWD_SSBO_CUSTOM_INSTANCED_ST = fxi->technique(_shader, "FWD_SSBO_CUSTOM_INSTANCED_ST");
  _tek_FWD_SSBO_CUSTOM_CAPTURE   = fxi->technique(_shader, "FWD_SSBO_CUSTOM_CAPTURE");     // impostor bake (MRT); null unless ssbo
  _tek_FWD_SSBO_CUSTOM_IMPOSTOR  = fxi->technique(_shader, "FWD_SSBO_CUSTOM_IMPOSTOR");    // impostor billboard; null unless impostor=True
  _tek_FWD_SSBO_CUSTOM_IMPOSTOR_ST = fxi->technique(_shader, "FWD_SSBO_CUSTOM_IMPOSTOR_ST"); // its single-pass-stereo peer
  _parImpAlbedo     = fxi->parameter(_shader, "ImpAlbedo");
  _parImpNormal     = fxi->parameter(_shader, "ImpNormal");
  _parImpMetalRough = fxi->parameter(_shader, "ImpMetalRough");
  _parImpCenter     = fxi->parameter(_shader, "ImpCenter");
  _parImpGrid       = fxi->parameter(_shader, "ImpGrid");
  _tek_FWD_SSBO_CUSTOM_DEPTHPREPASS = fxi->technique(_shader, "FWD_SSBO_CUSTOM_DEPTHPREPASS");
  _tek_FWD_SSBO_CUSTOM_INSTANCED_DEPTHPREPASS = fxi->technique(_shader, "FWD_SSBO_CUSTOM_INSTANCED_DEPTHPREPASS"); // E.4
  // ...and their per-view peers (the generated template has emitted these since the ST lowering; null
  //  on a template that predates it, which then falls through to the mono arm as before)
  _tek_FWD_SSBO_CUSTOM_DEPTHPREPASS_ST = fxi->technique(_shader, "FWD_SSBO_CUSTOM_DEPTHPREPASS_ST");
  _tek_FWD_SSBO_CUSTOM_INSTANCED_DEPTHPREPASS_ST = fxi->technique(_shader, "FWD_SSBO_CUSTOM_INSTANCED_DEPTHPREPASS_ST");
  // taskless mesh-shader twins; null unless the vertex source opted into the mesh variant
  _tek_FWD_SSBO_CUSTOM_MESH              = fxi->technique(_shader, "FWD_SSBO_CUSTOM_MESH");
  _tek_FWD_SSBO_CUSTOM_MESH_ST           = fxi->technique(_shader, "FWD_SSBO_CUSTOM_MESH_ST");
  _tek_FWD_SSBO_CUSTOM_MESH_DEPTHPREPASS = fxi->technique(_shader, "FWD_SSBO_CUSTOM_MESH_DEPTHPREPASS");
  _tek_FWD_SSBO_CUSTOM_MESH_DEPTHPREPASS_ST = fxi->technique(_shader, "FWD_SSBO_CUSTOM_MESH_DEPTHPREPASS_ST");
  _tek_FWD_CT_NM_IM_NI_MO        = fxi->technique(_shader, "FWD_CT_NM_IM_NI_MO");  // matrices-only instanced
  _tek_FWD_CT_NM_IM_NI_ST        = fxi->technique(_shader, "FWD_CT_NM_IM_NI_ST");  // ...and its per-view peer

  _tek_FWD_DEPTHPREPASS_RI_IN_MO = fxi->technique(_shader, "FWD_DEPTHPREPASS_RI_IN_MO"s + _shader_suffix);
  _tek_FWD_DEPTHPREPASS_RI_NI_MO = fxi->technique(_shader, "FWD_DEPTHPREPASS_RI_NI_MO"s + _shader_suffix);
  _tek_FWD_DEPTHPREPASS_SK_IN_MO = fxi->technique(_shader, "FWD_DEPTHPREPASS_SK_IN_MO"s + _shader_suffix);
  _tek_FWD_DEPTHPREPASS_SK_NI_MO = fxi->technique(_shader, "FWD_DEPTHPREPASS_SK_NI_MO"s + _shader_suffix);

  _tek_FWD_DEPTHPREPASS_MASKED_RI_NI_MO = fxi->technique(_shader, "FWD_DEPTHPREPASS_MASKED_RI_NI_MO"s + _shader_suffix);
  _tek_FWD_DEPTHPREPASS_MASKED_SK_NI_MO = fxi->technique(_shader, "FWD_DEPTHPREPASS_MASKED_SK_NI_MO"s + _shader_suffix);
  _tek_FWD_DEPTHPREPASS_MASKED_RI_IN_MO = fxi->technique(_shader, "FWD_DEPTHPREPASS_MASKED_RI_IN_MO"s + _shader_suffix);
  _tek_FWD_DEPTHPREPASS_MASKED_RI_NI_ST = fxi->technique(_shader, "FWD_DEPTHPREPASS_MASKED_RI_NI_ST"s + _shader_suffix);
  _tek_FWD_DEPTHPREPASS_MASKED_SK_NI_ST = fxi->technique(_shader, "FWD_DEPTHPREPASS_MASKED_SK_NI_ST"s + _shader_suffix);
  _tek_FWD_DEPTHPREPASS_MASKED_RI_IN_ST = fxi->technique(_shader, "FWD_DEPTHPREPASS_MASKED_RI_IN_ST"s + _shader_suffix);

  _tek_FWD_DEPTHPREPASS_RI_IN_ST = fxi->technique(_shader, "FWD_DEPTHPREPASS_RI_IN_ST"s + _shader_suffix);
  _tek_FWD_DEPTHPREPASS_RI_NI_ST = fxi->technique(_shader, "FWD_DEPTHPREPASS_RI_NI_ST"s + _shader_suffix);
  _tek_FWD_DEPTHPREPASS_SK_IN_ST = fxi->technique(_shader, "FWD_DEPTHPREPASS_SK_IN_ST"s + _shader_suffix);
  _tek_FWD_DEPTHPREPASS_SK_NI_ST = fxi->technique(_shader, "FWD_DEPTHPREPASS_SK_NI_ST"s + _shader_suffix);

  _tek_FWD_CV_EMI_RI_NI_MO = fxi->technique(_shader, "FWD_CV_EMI_RI_NI_MO"s + _shader_suffix);

  // deferreds

  _tek_GBU_CM_NM_RI_NI_MO = fxi->technique(_shader, "GBU_CM_NM_RI_NI_MO"s + _shader_suffix);
  _tek_GBU_CM_NM_SK_NI_MO = fxi->technique(_shader, "GBU_CM_NM_SK_NI_MO"s + _shader_suffix);
  _tek_GBU_CM_NM_RI_NI_ST = fxi->technique(_shader, "GBU_CM_NM_RI_NI_ST"s + _shader_suffix);

  _tek_GBU_CT_NM_RI_IN_MO = fxi->technique(_shader, "GBU_CT_NM_RI_IN_MO"s + _shader_suffix);
  _tek_GBU_CT_NM_RI_IN_ST = fxi->technique(_shader, "GBU_CT_NM_RI_IN_ST"s + _shader_suffix);
  _tek_GBU_CT_NM_RI_NI_ST = fxi->technique(_shader, "GBU_CT_NM_RI_NI_ST"s + _shader_suffix);
  _tek_GBU_CT_NM_RI_NI_MO = fxi->technique(_shader, "GBU_CT_NM_RI_NI_MO"s + _shader_suffix);

  _tek_GBU_CT_NM_SK_IN_MO = fxi->technique(_shader, "GBU_CT_NM_SK_IN_MO"s + _shader_suffix);

  _tek_GBU_CT_NM_SK_NI_MO = fxi->technique(_shader, "GBU_CT_NM_SK_NI_MO"s + _shader_suffix);

  _tek_GBU_CT_NV_RI_NI_MO = fxi->technique(_shader, "GBU_CT_NV_RI_NI_MO"s + _shader_suffix);

  _tek_GBU_CV_EMI_RI_NI_MO = fxi->technique(_shader, "GBU_CV_EMI_RI_NI_MO"s + _shader_suffix);

  // printf( "_tek_GBU_CT_NM_RI_NI_MO<%p>\n", _tek_GBU_CT_NM_RI_NI_MO );
  // printf( "_tek_GBU_CM_NM_RI_NI_MO<%p>\n", _tek_GBU_CM_NM_RI_NI_MO );
  //  OrkAssert(_tek_GBU_CT_NM_RI_NI_ST);
  //  OrkAssert(_tek_GBU_CT_NM_RI_IN_ST);
  //  OrkAssert(_tek_GBU_CT_NM_RI_IN_MO);
  //  OrkAssert(_tek_GBU_CT_NM_RI_NI_MO);
  //  OrkAssert(_tek_FWD_CT_NM_RI_NI_MO);
  //  OrkAssert(_tek_FWD_CT_NM_RI_IN_MO);

  // parameters

  _paramM                 = fxi->parameter(_shader, "m");
  _paramV                 = fxi->parameter(_shader, "v");
  _paramP                 = fxi->parameter(_shader, "p");
  _paramIP                = fxi->parameter(_shader, "inv_p");
  _paramVP                = fxi->parameter(_shader, "vp");
  _paramIV                = fxi->parameter(_shader, "inv_v");
  _paramIVP               = fxi->parameter(_shader, "inv_vp");
  _paramMVP               = fxi->parameter(_shader, "mvp");
  _paramMV                = fxi->parameter(_shader, "mv");
  _paramMROT              = fxi->parameter(_shader, "mrot");
  _paramMVIT              = fxi->parameter(_shader, "mvit");
  _paramMVITROT           = fxi->parameter(_shader, "mvit_rot");
  _paramMapCNMREA         = fxi->parameter(_shader, "CNMREA");
  
  _paramDppZBias          = fxi->parameter(_shader, "DppZBias");
  _paramDppAlphaCutoff    = fxi->parameter(_shader, "DppAlphaCutoff");
  _paramDppCNMREA         = fxi->parameter(_shader, "DppCNMREA");
  _parMapLightMapArray    = fxi->parameter(_shader, "LightMapArray");
  _paramLightMapColors    = fxi->parameter(_shader, "LightMapColors");
  
  _parInvViewSize         = fxi->parameter(_shader, "InvViewportSize");
  _parMetallicFactor      = fxi->parameter(_shader, "MetallicFactor");
  _parRoughnessFactor     = fxi->parameter(_shader, "RoughnessFactor");
  _parRoughnessPower      = fxi->parameter(_shader, "RoughnessPower" );
  _parAlphaCutoff         = fxi->parameter(_shader, "AlphaCutoff");
  _parModColor            = fxi->parameter(_shader, "ModColor");
  _parModAlbedo           = fxi->parameter(_shader, "ModAlbedo");
  _parPickID              = fxi->parameter(_shader, "obj_pickID");
  // matrices-only instancing binds the dynamic single-array block instead of the combined one.
  _parInstanceBlock       = fxi->storageBlock(_shader, _instanceMatricesOnly ? "storage_inst_mtx" : "storage_instancing");

  _parBoneBlock = fxi->uniformBlock(_shader, "ub_vtx_boneblock");
  // fwd

  _paramEyePostion    = fxi->parameter(_shader, "EyePostion");
  _paramAmbientLevel  = fxi->parameter(_shader, "AmbientLevel");
  _paramDiffuseLevel  = fxi->parameter(_shader, "DiffuseLevel");
  _paramSpecularLevel = fxi->parameter(_shader, "SpecularLevel");
  _paramSkyboxLevel   = fxi->parameter(_shader, "SkyboxLevel");
  _paramDiffuseBrdfModel = fxi->parameter(_shader, "DiffuseBrdfModel");

  _paramSSAOTexture     = fxi->parameter(_shader, "SSAOMap");
  _paramSSAOWeight     = fxi->parameter(_shader, "SSAOWeight");
  _paramSSAOPower     = fxi->parameter(_shader, "SSAOPower");
  _paramSSAOBias         = fxi->parameter(_shader, "SSAOBias");
  _paramSSAORadius         = fxi->parameter(_shader, "SSAORadius");
  _paramSSAONumSteps         = fxi->parameter(_shader, "SSAONumSteps");
  _paramSSAONumSamples         = fxi->parameter(_shader, "SSAONumSamples");

  _paramSSAOKernel         = fxi->parameter(_shader, "SSAOKernel");
  _paramSSAOScrNoise         = fxi->parameter(_shader, "SSAOScrNoise");

  _paramMapDepth         = fxi->parameter(_shader, "MapDepth");
  _paramMapLinearDepth         = fxi->parameter(_shader, "MapLinearDepth");
  _paramNearFar = fxi->parameter(_shader, "Zndc2eye");


  _parSpecularMipBias = fxi->parameter(_shader, "SpecularMipBias");

  _parMapSpecularEnv      = fxi->parameter(_shader, "MapSpecularEnv");
  _parMapSpecularRufLevels = fxi->parameter(_shader, "RoughnessLevels");
  _parMapSpecularEnvPrev  = fxi->parameter(_shader, "MapSpecularEnvPrev");
  _parEnvBlendWeight      = fxi->parameter(_shader, "EnvBlendWeight");
  _parEnvCaptureScaleInv  = fxi->parameter(_shader, "EnvCaptureScaleInv");
  _parEnvSH               = fxi->parameter(_shader, "EnvSH");
  _parEnvSHValid          = fxi->parameter(_shader, "EnvSHValid");
  _parMapBrdfIntegration  = fxi->parameter(_shader, "MapBrdfIntegration");
  _parEnvironmentMipBias  = fxi->parameter(_shader, "EnvironmentMipBias");
  _parEnvironmentMipScale = fxi->parameter(_shader, "EnvironmentMipScale");
  _parDepthFogDistance    = fxi->parameter(_shader, "DepthFogDistance");
  _parDepthFogPower       = fxi->parameter(_shader, "DepthFogPower");

  _parUnTexPointLightsCount = fxi->parameter(_shader, "point_light_count");
  _parForwardLightBlock  = fxi->storageBlock(_shader, "storage_fwd_lighting");

  // SKYLIGHT lane A — sun cascade block + sampler (nullptr for shaders
  // without the forward lighting libblocks; binds tolerate nullptr).
  // SINGLE-PASS STEREO — the per-view matrix block the multiview shader variant reads
  // through gl_ViewIndex. Present on every shader the compiler injected it into; null
  // elsewhere, and the bind tolerates null.
  _parStereoBlock  = fxi->uniformBlock(_shader, "ublk_stereo");

  _parSunBlock     = fxi->uniformBlock(_shader, "ublk_sun");
  _parSunShadowMap = fxi->parameter(_shader, "sun_shadow_map");
  _parSunCookie    = fxi->parameter(_shader, "sun_cookie");

  // SKYLIGHT lane B — the ublk_sky_atmo members + LUT samplers the procedural
  // skybox reads (nullptr for shaders without lib_sky).
  _parSkyRadii            = fxi->parameter(_shader, "SkyRadii");
  _parSkySunDirection     = fxi->parameter(_shader, "SkySunDirection");
  _parSkySunIlluminance   = fxi->parameter(_shader, "SkySunIlluminance");
  _parSkyGroundAlbedo     = fxi->parameter(_shader, "SkyGroundAlbedo");
  _parSkySunDisc          = fxi->parameter(_shader, "SkySunDisc");
  _parSkyMoonDirection    = fxi->parameter(_shader, "SkyMoonDirection");
  _parSkyMoonDisc         = fxi->parameter(_shader, "SkyMoonDisc");
  _parSkyMoonAlbedo       = fxi->parameter(_shader, "SkyMoonAlbedo");
  _parSkyNightEmission    = fxi->parameter(_shader, "SkyNightEmission");
  _parSkyMoonIlluminance  = fxi->parameter(_shader, "SkyMoonIlluminance");
  _parSkyViewLut          = fxi->parameter(_shader, "SkyViewLUT");
  _parSkyTransmittanceLut = fxi->parameter(_shader, "SkyTransmittanceLUT");
  // AERIAL PERSPECTIVE — the medium members + multi-scatter LUT the forward
  // haze march reads directly (the skybox reads none of them).
  _parSkyRayleighScatter  = fxi->parameter(_shader, "SkyRayleighScatter");
  _parSkyMieScatter       = fxi->parameter(_shader, "SkyMieScatter");
  _parSkyOzoneAbsorb      = fxi->parameter(_shader, "SkyOzoneAbsorb");
  _parSkyOzoneTent        = fxi->parameter(_shader, "SkyOzoneTent");
  _parSkyMultiScatterLut  = fxi->parameter(_shader, "SkyMultiScatterLUT");
  _parSkyHazeDensity      = fxi->parameter(_shader, "SkyHazeDensity");
  _parSkyHazeScatterTint  = fxi->parameter(_shader, "SkyHazeScatterTint");
  _parSkyHazeInscatterTint = fxi->parameter(_shader, "SkyHazeInscatterTint");
  _parSkyHazeGeom         = fxi->parameter(_shader, "SkyHazeGeom");
  _parSkyCookieParams     = fxi->parameter(_shader, "SkyCookieParams");
  _parSkyCookieBody       = fxi->parameter(_shader, "SkyCookieBody");
  _parSkyCloudCookie      = fxi->parameter(_shader, "SkyCloudCookie");

  _parTexSpotLightsCount = fxi->parameter(_shader, "spot_light_count");

  //_parLightCookies = fxi->parameter(_shader, "light_cookies");
  _parLightColorCookies = fxi->parameter(_shader, "light_cookie_colors");
  _parLightDepthCookies = fxi->parameter(_shader, "light_cookie_depths");

  _parProbeReflection = fxi->parameter(_shader, "reflectionPROBE");
  _parProbeRadiance = fxi->parameter(_shader, "RadiancePROBE");
  _parHasReflectionProbe = fxi->parameter(_shader, "has_reflection_probe");

  // PBR2 Phase 1 — 8 glTF KHR-extension lobe flag + factor lookups.
  // Stays nullptr if the shader pipeline doesn't expose them; bindParam*
  // calls in the lighting lambda tolerate nullptr handles.
  _parHasTransmission           = fxi->parameter(_shader, "has_transmission");
  _parTransmissionFactor        = fxi->parameter(_shader, "transmission_factor");
  _parHasTransmissionRoughness  = fxi->parameter(_shader, "has_transmission_roughness");
  _parTransmissionRoughness     = fxi->parameter(_shader, "transmission_roughness");
  _parHasIor                    = fxi->parameter(_shader, "has_ior");
  _parIor                       = fxi->parameter(_shader, "ior_value");
  _parHasVolume                 = fxi->parameter(_shader, "has_volume");
  _parVolumeThicknessFactor     = fxi->parameter(_shader, "volume_thickness_factor");
  _parHasDiffuseTransmission    = fxi->parameter(_shader, "has_diffuse_transmission");
  _parDiffuseTransmissionFactor = fxi->parameter(_shader, "diffuse_transmission_factor");
  _parHasSpecular               = fxi->parameter(_shader, "has_specular");
  _parSpecularFactor            = fxi->parameter(_shader, "specular_factor");
  _parHasClearcoat              = fxi->parameter(_shader, "has_clearcoat");
  _parClearcoatFactor           = fxi->parameter(_shader, "clearcoat_factor");
  _parHasSheen                  = fxi->parameter(_shader, "has_sheen");
  _parSheenFactor               = fxi->parameter(_shader, "sheen_factor");
  _parHasIridescence            = fxi->parameter(_shader, "has_iridescence");
  _parIridescenceFactor         = fxi->parameter(_shader, "iridescence_factor");

  // PBR2 Phase 2 — color + secondary scalar uniforms.
  _parSheenColor                = fxi->parameter(_shader, "sheen_color");
  _parSpecularColor             = fxi->parameter(_shader, "specular_color");
  _parAttenuationColor          = fxi->parameter(_shader, "attenuation_color");
  _parDiffuseTransmissionColor  = fxi->parameter(_shader, "diffuse_transmission_color");
  _parClearcoatRoughness        = fxi->parameter(_shader, "clearcoat_roughness");
  _parSheenRoughness            = fxi->parameter(_shader, "sheen_roughness");
  _parAttenuationDistance       = fxi->parameter(_shader, "attenuation_distance");
  _parRenderingProbe            = fxi->parameter(_shader, "rendering_probe");

  // PBR2 Phase 3 (P3.D) — subsurface scattering uniforms.
  _parHasSubsurface             = fxi->parameter(_shader, "has_subsurface");
  _parSubsurfaceColor           = fxi->parameter(_shader, "subsurface_color");
  _parSubsurfaceRadius          = fxi->parameter(_shader, "subsurface_radius");
  _parSubsurfaceFactor          = fxi->parameter(_shader, "subsurface_factor");

  // printf( "_parLightCookies<%p>\n", _parLightCookies );

  //

  // Note: _parBoneBlock is optional - only required for skinned meshes
  // The binding code at line ~484 already checks for nullptr

  // printf( "_texColor<%p>\n", _texColor.get() );
  // printf( "_texNormal<%p>\n", _texNormal.get() );
  // printf( "_texMtlRuf<%p>\n", _texMtlRuf.get() );

  /////////////////////////////////////////////////

  if(_texArrayCNMREA == nullptr){
    conformImages();
    assignImages( targ,                  //
                  _image_color,     //
                  _image_normal,    //
                  _image_mtlruf,    //
                  _image_emissive,  //
                  nullptr,  //
                  false);
  }
  /*
  if (_texAmbOcc == nullptr) {
    auto loadreq         = std::make_shared<asset::LoadRequest>();
    loadreq->_asset_path = "src://effect_textures/white";
    _asset_texambocc      = asset::AssetManager<lev2::TextureAsset>::load(loadreq);
    _texAmbOcc            = _asset_texambocc->GetTexture();
    //_activeLightMap = _texAmbOcc; // HACK
    // logchan_pbr->log("substituted white for non-existant color texture");
    OrkAssert(_texAmbOcc != nullptr);
  }
  if (_texNormal == nullptr) {
    static auto defntex = targ->TXI()->createColorTexture(fvec4(0.5, 0.5, 1, 1), 8, 8);
    defntex->_debugName = "default_normal";
    _texNormal          = defntex;
    OrkAssert(_texNormal != nullptr);
  }
  if (_texMtlRuf == nullptr) {
  */

}

void PBRMaterial::forceEmissive() {
  auto loadreq         = std::make_shared<asset::LoadRequest>();
  loadreq->_asset_path = "src://effect_textures/black";
  // to force emissive set normal map to black
  // shader will interpret as emissive
  _asset_texnormal = asset::AssetManager<lev2::TextureAsset>::load(loadreq);
  _texNormal       = _asset_texnormal->GetTexture();
  OrkAssert(_texNormal != nullptr);
}

///////////////////////////////////////////////////////////////////////////////

int PBRMaterial::BeginBlock(Context* context, const RenderContextInstData& RCID) {
  auto fxi     = context->FXI();
  auto fxcache = RCID._pipeline_cache;
  OrkAssert(fxcache);
  auto pipelineance = fxcache->findPipeline(RCID);
  OrkAssert(pipelineance);
  auto tek = pipelineance->_technique;
  OrkAssert(tek);

  int numpasses = fxi->BeginBlock(tek, RCID);
  OrkAssert(numpasses == 1);
  fxi->CommitParams();
  return numpasses;
}

///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::EndBlock(Context* context) {
  auto fxi = context->FXI();
  fxi->EndBlock();
}

///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::gpuUpdate(Context* context) {
  GfxMaterial::gpuUpdate(context);
  // auto fxi    = context->FXI();
}

///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::UpdateMVPMatrix(Context* context) {
  auto fxi                           = context->FXI();
  //auto rsi                           = context->RSI();
  auto mtxi                          = context->MTXI();
  const RenderContextInstData* RCID  = context->GetRenderContextInstData();
  auto RCFD = context->topRenderContextFrameData();
  const auto& CPD                    = RCFD->topCPD();
  if (CPD.isSinglePassStereo() and CPD._stereo_cam_matrices) {
  } else {
    auto mcams        = CPD._mono_cam_matrices;
    const auto& world = mtxi->RefMMatrix();
    auto MVP          = fmtx4::multiply_ltor(world, mcams->_vmatrix, mcams->_pmatrix);
    fxi->bindParamMatrix(_paramV, mcams->_vmatrix);
    fxi->bindParamMatrix(_paramMVP, MVP);
  }
}

///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::UpdateMMatrix(Context* context) {
  auto fxi          = context->FXI();
  auto mtxi         = context->MTXI();
  const auto& world = mtxi->RefMMatrix();
  fxi->bindParamMatrix(_paramM, world);
}

///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::BindMaterialInstItem(MaterialInstItem* pitem) const {
  ///////////////////////////////////
  auto mtxblockitem = dynamic_cast<MaterialInstItemMatrixBlock*>(pitem);

  if (mtxblockitem) {
    // if (hBoneMatrices->GetPlatformHandle()) {
    auto applicator = PbrMatrixBlockApplicator::getApplicator();
    OrkAssert(applicator != 0);
    applicator->_pbrmaterial = this;
    applicator->_matrixblock = mtxblockitem;
    mtxblockitem->SetApplicator(applicator);
    //}
    return;
  }
}

///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::UnBindMaterialInstItem(MaterialInstItem* pitem) const {
  ///////////////////////////////////

  auto mtxblockitem = dynamic_cast<MaterialInstItemMatrixBlock*>(pitem);

  if (mtxblockitem) {
    // if (hBoneMatrices->GetPlatformHandle()) {
    auto applicator = static_cast<PbrMatrixBlockApplicator*>(mtxblockitem->mApplicator);
    if (applicator) {
      applicator->_pbrmaterial = nullptr;
      applicator->_matrixblock = nullptr;
    }
    //}
    return;
  }
}

///////////////////////////////////////////////////////////////////////////////

void PbrMatrixBlockApplicator::ApplyToTarget(Context* context) // virtual
{
  auto fxi                           = context->FXI();
  auto mtxi                          = context->MTXI();
  const RenderContextInstData* RCID  = context->GetRenderContextInstData();
  auto RCFD = context->topRenderContextFrameData();
  const auto& CPD                    = RCFD->topCPD();
  const auto& world                  = mtxi->RefMMatrix();
  const auto& drect                  = CPD.GetDstRect();
  const auto& mrect                  = CPD.bufferRect();
  FxShader* shader                   = _pbrmaterial->_shader;
  size_t inumbones                   = _matrixblock->GetNumMatrices();
  const fmtx4* Matrices              = _matrixblock->GetMatrices();
  size_t fmtx4_stride                = sizeof(fmtx4);

  auto bones_buffer = PBRMaterial::boneDataBuffer(context);
  auto bones_mapped = fxi->mapUniformBuffer(bones_buffer, 0, inumbones * sizeof(fmtx4));

  // printf( "inumbones<%d>\n", inumbones );

  for (int i = 0; i < inumbones; i++) {
    bones_mapped->ref<fmtx4>(fmtx4_stride * i) = Matrices[i];
     //printf( "I<%d>: ", i );
     //Matrices[i].dump("bonemtx");
  }

  bones_mapped->unmap();

  if (_pbrmaterial->_parBoneBlock) {
    fxi->bindUniformBuffer(_pbrmaterial->_parBoneBlock, bones_buffer);
  }
}

////////////////////////////////////////////

void PBRMaterial::Update() {
}

////////////////////////////////////////////

void PBRMaterial::begin(const RenderContextFrameData& RCFD) {
}

////////////////////////////////////////////

void PBRMaterial::end(const RenderContextFrameData& RCFD) {
}


////////////////////////////////////////////

} // namespace ork::lev2
