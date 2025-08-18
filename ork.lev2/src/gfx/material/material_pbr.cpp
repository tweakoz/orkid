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

  _tek_FWD_CT_NM_RI_NI_MO = fxi->technique(_shader, "FWD_CT_NM_RI_NI_MO"s + _shader_suffix);
  _tek_FWD_CV_NM_RI_NI_MO = fxi->technique(_shader, "FWD_CV_NM_RI_NI_MO"s + _shader_suffix);
  _tek_FWD_CT_NM_RI_IN_MO = fxi->technique(_shader, "FWD_CT_NM_RI_IN_MO"s + _shader_suffix);
  _tek_FWD_CT_NM_RI_NI_ST = fxi->technique(_shader, "FWD_CT_NM_RI_NI_ST"s + _shader_suffix);
  _tek_FWD_CT_NM_RI_IN_ST = fxi->technique(_shader, "FWD_CT_NM_RI_IN_ST"s + _shader_suffix);

  _tek_FWD_CT_NM_SK_NI_MO = fxi->technique(_shader, "FWD_CT_NM_SK_NI_MO"s + _shader_suffix);
  _tek_FWD_CT_NM_SK_IN_MO = fxi->technique(_shader, "FWD_CT_NM_SK_IN_MO"s + _shader_suffix);
  _tek_FWD_CT_NM_SK_NI_ST = fxi->technique(_shader, "FWD_CT_NM_SK_NI_ST"s + _shader_suffix);
  _tek_FWD_CT_NM_SK_IN_ST = fxi->technique(_shader, "FWD_CT_NM_SK_IN_ST"s + _shader_suffix);

  _tek_FWD_DEPTHPREPASS_RI_IN_MO = fxi->technique(_shader, "FWD_DEPTHPREPASS_RI_IN_MO"s + _shader_suffix);
  _tek_FWD_DEPTHPREPASS_RI_NI_MO = fxi->technique(_shader, "FWD_DEPTHPREPASS_RI_NI_MO"s + _shader_suffix);
  _tek_FWD_DEPTHPREPASS_SK_IN_MO = fxi->technique(_shader, "FWD_DEPTHPREPASS_SK_IN_MO"s + _shader_suffix);
  _tek_FWD_DEPTHPREPASS_SK_NI_MO = fxi->technique(_shader, "FWD_DEPTHPREPASS_SK_NI_MO"s + _shader_suffix);

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
  _paramVL                = fxi->parameter(_shader, "v_l");
  _paramVR                = fxi->parameter(_shader, "v_r");
  _paramVPL               = fxi->parameter(_shader, "vp_l");
  _paramVPR               = fxi->parameter(_shader, "vp_r");
  _paramIVPL              = fxi->parameter(_shader, "inv_vp_l");
  _paramIVPR              = fxi->parameter(_shader, "inv_vp_r");
  _paramIVP               = fxi->parameter(_shader, "inv_vp");
  _paramMVP               = fxi->parameter(_shader, "mvp");
  _paramMVPL              = fxi->parameter(_shader, "mvp_l");
  _paramMVPR              = fxi->parameter(_shader, "mvp_r");
  _paramMV                = fxi->parameter(_shader, "mv");
  _paramMROT              = fxi->parameter(_shader, "mrot");
  _paramMapCNMREA         = fxi->parameter(_shader, "CNMREA");
  
  _paramDppZBias          = fxi->parameter(_shader, "DppZBias");
  _parMapLightMapArray    = fxi->parameter(_shader, "LightMapArray");
  _paramLightMapColors    = fxi->parameter(_shader, "LightMapColors");
  
  _parInvViewSize         = fxi->parameter(_shader, "InvViewportSize");
  _parMetallicFactor      = fxi->parameter(_shader, "MetallicFactor");
  _parRoughnessFactor     = fxi->parameter(_shader, "RoughnessFactor");
  _parRoughnessPower      = fxi->parameter(_shader, "RoughnessPower" );
  _parModColor            = fxi->parameter(_shader, "ModColor");
  _parPickID              = fxi->parameter(_shader, "obj_pickID");
  _paramInstanceMatrixMap = fxi->parameter(_shader, "InstanceMatrices");
  _paramInstanceIdMap     = fxi->parameter(_shader, "InstanceIds");
  _paramInstanceColorMap  = fxi->parameter(_shader, "InstanceColors");

  _parBoneBlock = fxi->uniformBlock(_shader, "ub_vtx_boneblock");
  // fwd

  _paramEyePostion    = fxi->parameter(_shader, "EyePostion");
  _paramEyePostionL   = fxi->parameter(_shader, "EyePostionL");
  _paramEyePostionR   = fxi->parameter(_shader, "EyePostionR");
  _paramAmbientLevel  = fxi->parameter(_shader, "AmbientLevel");
  _paramDiffuseLevel  = fxi->parameter(_shader, "DiffuseLevel");
  _paramSpecularLevel = fxi->parameter(_shader, "SpecularLevel");
  _paramSkyboxLevel   = fxi->parameter(_shader, "SkyboxLevel");

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
  _parMapDiffuseEnv       = fxi->parameter(_shader, "MapDiffuseEnv");
  _parMapBrdfIntegration  = fxi->parameter(_shader, "MapBrdfIntegration");
  _parEnvironmentMipBias  = fxi->parameter(_shader, "EnvironmentMipBias");
  _parEnvironmentMipScale = fxi->parameter(_shader, "EnvironmentMipScale");
  _parDepthFogDistance    = fxi->parameter(_shader, "DepthFogDistance");
  _parDepthFogPower       = fxi->parameter(_shader, "DepthFogPower");

  _parUnTexPointLightsCount = fxi->parameter(_shader, "point_light_count");
  _parUnTexPointLightsData  = fxi->uniformBlock(_shader, "ub_frg_fwd_lighting");

  _parTexSpotLightsCount = fxi->parameter(_shader, "spot_light_count");

  //_parLightCookies = fxi->parameter(_shader, "light_cookies");
  _parLightColorCookies = fxi->parameter(_shader, "light_cookie_colors");
  _parLightDepthCookies = fxi->parameter(_shader, "light_cookie_depths");

  _parProbeReflection = fxi->parameter(_shader, "reflectionPROBE");
  _parProbeIrradiance = fxi->parameter(_shader, "irradiancePROBE");

  // printf( "_parLightCookies<%p>\n", _parLightCookies );

  //

  OrkAssert(_parBoneBlock != nullptr);

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
    // printf( "I<%d>: ", i );
    // Matrices[i].dump("bonemtx");
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
