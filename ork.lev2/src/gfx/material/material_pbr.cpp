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
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/shadman.h>
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

static logchannel_ptr_t logchan_pbr = logger()->createChannel("mtlpbr", fvec3(0.8, 0.8, 0.1), true);

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

  chunkfile::materialreader_t reader = [](chunkfile::XgmMaterialReaderContext& ctx) -> ork::lev2::material_ptr_t {
    auto targ             = ctx._varmap->typedValueForKey<Context*>("gfxtarget").value();
    auto txi              = targ->TXI();
    const auto& embtexmap = ctx._varmap->typedValueForKey<embtexmap_t>("embtexmap").value();

    for (auto item : embtexmap) {
      logchan_pbr->log("embtex<%s>", item.first.c_str());
    }

    int istring = 0;

    ctx._inputStream->GetItem(istring);
    auto materialname = ctx._reader.GetString(istring);

    ctx._inputStream->GetItem(istring);
    auto texbasename = ctx._reader.GetString(istring);
    auto mtl         = std::make_shared<PBRMaterial>();
    mtl->_vars->makeValueForKey<bool>("from_xgm") = true;
    mtl->SetName(AddPooledString(materialname));
    logchan_pbr->log("read.xgm: materialName<%s>", materialname);
    ctx._inputStream->GetItem(istring);
    auto begintextures = ctx._reader.GetString(istring);
    OrkAssert(0 == strcmp(begintextures, "begintextures"));
    bool done = false;
    while (false == done) {
      ctx._inputStream->GetItem(istring);
      auto token = ctx._reader.GetString(istring);
      if (0 == strcmp(token, "endtextures"))
        done = true;
      else {
        ctx._inputStream->GetItem(istring);
        auto texname = ctx._reader.GetString(istring);
        logchan_pbr->log("read.xgm: find tex channel<%s> texname<%s> .. ", token, texname);
        auto itt = embtexmap.find(texname);
        OrkAssert(itt != embtexmap.end());
        auto embtex = itt->second;
        logchan_pbr->log("read.xgm: embtex<%p> data<%p> len<%zu>", embtex.get(), embtex->_srcdata, embtex->_srcdatalen);
        auto tex = std::make_shared<lev2::Texture>();
        // crashes here...
        auto datablock = std::make_shared<DataBlock>(embtex->_srcdata, embtex->_srcdatalen);
        bool ok        = txi->LoadTexture(tex, datablock);
        OrkAssert(ok);
        logchan_pbr->log(" embtex<%p> datablock<%p> len<%zu>", embtex.get(), datablock.get(), datablock->length());
        logchan_pbr->log(" token<%s>", token);
        if (0 == strcmp(token, "colormap")) {
          mtl->_texColor     = tex;
          mtl->_colorMapName = texname;
        }
        if (0 == strcmp(token, "normalmap")) {
          mtl->_texNormal     = tex;
          mtl->_normalMapName = texname;
        }
        if (0 == strcmp(token, "mtlrufmap")) {
          mtl->_texMtlRuf     = tex;
          mtl->_mtlRufMapName = texname;
        }
        if (0 == strcmp(token, "emissivemap")) {
          mtl->_texEmissive     = tex;
          mtl->_emissiveMapName = texname;
        }
      }
    }
    ctx._inputStream->GetItem<float>(mtl->_metallicFactor);
    ctx._inputStream->GetItem<float>(mtl->_roughnessFactor);
    ctx._inputStream->GetItem<fvec4>(mtl->_baseColor);
    // logchan_pbr->log("read.xgm: basecolor<%g %g %g>", mtl->_baseColor.x,mtl->_baseColor.y,mtl->_baseColor.z);

    if (auto try_ov = ctx._varmap->typedValueForKey<std::string>("override.shader.gbuf")) {
      const auto& ov_val = try_ov.value();
      if (ov_val == "normalviz") {
        mtl->_variant = "normalviz"_crcu;
      }
    }

    return mtl;
  };

  /////////////////////////////////////////////////////////////////

  chunkfile::materialwriter_t writer = [](chunkfile::XgmMaterialWriterContext& ctx) {
    auto pbrmtl = std::static_pointer_cast<const PBRMaterial>(ctx._material);

    int istring = ctx._writer.stringIndex(pbrmtl->mMaterialName.c_str());
    ctx._outputStream->AddItem(istring);

    istring = ctx._writer.stringIndex(pbrmtl->_textureBaseName.c_str());
    ctx._outputStream->AddItem(istring);

    auto dotex = [&](std::string channelname, std::string texname) {
      // logchan_pbr->log("write.xgm: tex channel<%s> texname<%s>", channelname.c_str(), texname.c_str());
      if (texname.length()) {
        istring = ctx._writer.stringIndex(channelname.c_str());
        ctx._outputStream->AddItem(istring);
        istring = ctx._writer.stringIndex(texname.c_str());
        ctx._outputStream->AddItem(istring);
      }
    };
    istring = ctx._writer.stringIndex("begintextures");
    ctx._outputStream->AddItem(istring);
    dotex("colormap", pbrmtl->_colorMapName);
    dotex("normalmap", pbrmtl->_normalMapName);
    dotex("amboccmap", pbrmtl->_amboccMapName);
    dotex("emissivemap", pbrmtl->_emissiveMapName);
    dotex("mtlrufmap", pbrmtl->_mtlRufMapName);
    istring = ctx._writer.stringIndex("endtextures");
    ctx._outputStream->AddItem(istring);

    ctx._outputStream->AddItem<float>(pbrmtl->_metallicFactor);
    ctx._outputStream->AddItem<float>(pbrmtl->_roughnessFactor);
    ctx._outputStream->AddItem<fvec4>(pbrmtl->_baseColor);
    // logchan_pbr->log("write.xgm: _metallicFactor<%g>", pbrmtl->_metallicFactor);
    // logchan_pbr->log("write.xgm: _roughnessFactor<%g>", pbrmtl->_roughnessFactor);
    // logchan_pbr->log(
    //   "write.xgm: _baseColor<%g %g %g %g>", //
    // pbrmtl->_baseColor.x,                 //
    // pbrmtl->_baseColor.y,                 //
    // pbrmtl->_baseColor.z,                 //
    // pbrmtl->_baseColor.w);
  };

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
  _rasterstate.SetShadeModel(ESHADEMODEL_SMOOTH);
  _rasterstate.SetAlphaTest(EALPHATEST_OFF);
  _rasterstate.SetBlending(Blending::OFF);
  _rasterstate.SetDepthTest(EDepthTest::LEQUALS);
  _rasterstate.SetZWriteMask(true);
  _rasterstate.SetCullTest(ECullTest::PASS_FRONT);
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

  /*copy->_asset_shader = _asset_shader;
  copy->_asset_texcolor = _asset_texcolor;
  copy->_asset_texnormal = _asset_texnormal;
  copy->_asset_mtlruf = _asset_mtlruf;
  copy->_asset_emissive = _asset_emissive;

  copy->_texColor = _texColor;
  copy->_texNormal = _texNormal;
  copy->_texMtlRuf = _texMtlRuf;
  copy->_texEmissive = _texEmissive;
  copy->_textureBaseName = _textureBaseName;

  copy->_colorMapName = _colorMapName;
  copy->_normalMapName = _normalMapName;
  copy->_mtlRufMapName = _mtlRufMapName;
  copy->_amboccMapName = _amboccMapName;
  copy->_emissiveMapName = _emissiveMapName;
  copy->_shaderpath = _shaderpath;

  copy->_metallicFactor = _metallicFactor;
  copy->_roughnessFactor = _roughnessFactor;
  copy->_baseColor = _baseColor;
  copy->_textureBaseName = _textureBaseName;

  copy->_variant = _variant;
  copy->mMaterialName = mMaterialName;
  copy->_varmap = _varmap;

  // TODO - flyweight clones

  if(_initialTarget){
    //copy->gpuInit(_initialTarget);
  }*/
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

  // printf( "PBRMaterial::gpuInit<%p> _shaderpath<%s>\n", this, _shaderpath.c_str() );
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
  _paramP                 = fxi->parameter(_shader, "MatP");
  _paramIP                = fxi->parameter(_shader, "MatInvP");
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
  _paramMapColor          = fxi->parameter(_shader, "ColorMap");
  _paramMapNormal         = fxi->parameter(_shader, "NormalMap");
  _paramMapMtlRuf         = fxi->parameter(_shader, "MtlRufMap");
  _paramMapEmissive       = fxi->parameter(_shader, "EmissiveMap");
  _parMapAmbOcc           = fxi->parameter(_shader, "AmbOccMap");
  _parMapLightMap         = fxi->parameter(_shader, "LightMap");
  _parInvViewSize         = fxi->parameter(_shader, "InvViewportSize");
  _parMetallicFactor      = fxi->parameter(_shader, "MetallicFactor");
  _parRoughnessFactor     = fxi->parameter(_shader, "RoughnessFactor");
  _parModColor            = fxi->parameter(_shader, "ModColor");
  _parPickID              = fxi->parameter(_shader, "obj_pickID");
  _paramInstanceMatrixMap = fxi->parameter(_shader, "InstanceMatrices");
  _paramInstanceIdMap     = fxi->parameter(_shader, "InstanceIds");
  _paramInstanceColorMap  = fxi->parameter(_shader, "InstanceColors");
  _paramInstanceBlock  = fxi->parameterBlock(_shader, "ub_instancing");

  _parBoneBlock = fxi->parameterBlock(_shader, "ub_vtx_boneblock");

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
  _parMapDiffuseEnv       = fxi->parameter(_shader, "MapDiffuseEnv");
  _parMapBrdfIntegration  = fxi->parameter(_shader, "MapBrdfIntegration");
  _parEnvironmentMipBias  = fxi->parameter(_shader, "EnvironmentMipBias");
  _parEnvironmentMipScale = fxi->parameter(_shader, "EnvironmentMipScale");
  _parDepthFogDistance    = fxi->parameter(_shader, "DepthFogDistance");
  _parDepthFogPower       = fxi->parameter(_shader, "DepthFogPower");

  _parUnTexPointLightsCount = fxi->parameter(_shader, "point_light_count");
  _parUnTexPointLightsData  = fxi->parameterBlock(_shader, "ub_frg_fwd_lighting");

  _parTexSpotLightsCount = fxi->parameter(_shader, "spot_light_count");

  //_parLightCookies = fxi->parameter(_shader, "light_cookies");
  _parLightCookie0 = fxi->parameter(_shader, "light_cookie0");
  _parLightCookie1 = fxi->parameter(_shader, "light_cookie1");
  _parLightCookie2 = fxi->parameter(_shader, "light_cookie2");
  _parLightCookie3 = fxi->parameter(_shader, "light_cookie3");
  //_parLightCookie4 = fxi->parameter(_shader, "light_cookie4");
  //_parLightCookie5 = fxi->parameter(_shader, "light_cookie5");
  //_parLightCookie6 = fxi->parameter(_shader, "light_cookie6");
  //_parLightCookie7 = fxi->parameter(_shader, "light_cookie7");

  _parProbeReflection = fxi->parameter(_shader, "reflectionPROBE");
  _parProbeIrradiance = fxi->parameter(_shader, "irradiancePROBE");

  // printf( "_parLightCookies<%p>\n", _parLightCookies );

  //

  OrkAssert(_paramMapNormal != nullptr);
  OrkAssert(_parBoneBlock != nullptr);

  // printf( "_texColor<%p>\n", _texColor.get() );
  // printf( "_texNormal<%p>\n", _texNormal.get() );
  // printf( "_texMtlRuf<%p>\n", _texMtlRuf.get() );

  _texBlack = targ->TXI()->createColorTexture(fvec4(0, 0, 0, 1), 32, 32);
  _texCubeBlack = targ->TXI()->createColorCubeTexture(fvec4(0, 0, 0, 1), 32,32);

  if (_texColor == nullptr) {
    auto loadreq         = std::make_shared<asset::LoadRequest>();
    loadreq->_asset_path = "src://effect_textures/white";
    _asset_texcolor      = asset::AssetManager<lev2::TextureAsset>::load(loadreq);
    _texColor            = _asset_texcolor->GetTexture();
    // logchan_pbr->log("substituted white for non-existant color texture");
    OrkAssert(_texColor != nullptr);
  }
  if (_texNormal == nullptr) {
    static auto defntex = targ->TXI()->createColorTexture(fvec4(0.5, 0.5, 1, 1), 8, 8);
    defntex->_debugName = "default_normal";
    _texNormal          = defntex;
    OrkAssert(_texNormal != nullptr);
  }
  if (_texMtlRuf == nullptr) {

    static auto defmrtex = targ->TXI()->createColorTexture(fvec4(1, 1, 0, 1), 8, 8);
    _texMtlRuf           = defmrtex;

    if (_metallicFactor != 0.0f) {
      static auto metallictex = targ->TXI()->createColorTexture(fvec4(1, 0, 1, 1), 8, 8);
      metallictex->_debugName = "default_metallicroughness";
      _texMtlRuf              = metallictex;
    }

    OrkAssert(_texMtlRuf != nullptr);
  }
  if (_texEmissive == nullptr) {
    static auto defemitex = targ->TXI()->createColorTexture(fvec4(0, 0, 0, 0), 8, 8);
    defemitex->_debugName = "default_emissive";
    _texEmissive          = defemitex;
  }
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

bool PBRMaterial::BeginPass(Context* targ, int iPass) {
  auto fxi = targ->FXI();
  auto rsi = targ->RSI();
  fxi->BindPass(0);
  rsi->BindRasterState(_rasterstate);
  fxi->CommitParams();
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::UpdateMVPMatrix(Context* context) {
  auto fxi                           = context->FXI();
  auto rsi                           = context->RSI();
  auto mtxi                          = context->MTXI();
  const RenderContextInstData* RCID  = context->GetRenderContextInstData();
  auto RCFD = context->topRenderContextFrameData();
  const auto& CPD                    = RCFD->topCPD();
  if (CPD.isStereoOnePass() and CPD._stereoCameraMatrices) {
  } else {
    auto mcams        = CPD._cameraMatrices;
    const auto& world = mtxi->RefMMatrix();
    auto MVP          = fmtx4::multiply_ltor(world, mcams->_vmatrix, mcams->_pmatrix);
    fxi->BindParamMatrix(_paramV, mcams->_vmatrix);
    fxi->BindParamMatrix(_paramMVP, MVP);
  }
}

///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::UpdateMMatrix(Context* context) {
  auto fxi          = context->FXI();
  auto mtxi         = context->MTXI();
  const auto& world = mtxi->RefMMatrix();
  fxi->BindParamMatrix(_paramM, world);
}

///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::EndPass(Context* targ) {
  targ->FXI()->EndPass();
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
  const auto& mrect                  = CPD.GetMrtRect();
  FxShader* shader                   = _pbrmaterial->_shader;
  size_t inumbones                   = _matrixblock->GetNumMatrices();
  const fmtx4* Matrices              = _matrixblock->GetMatrices();
  size_t fmtx4_stride                = sizeof(fmtx4);

  auto bones_buffer = PBRMaterial::boneDataBuffer(context);
  auto bones_mapped = fxi->mapParamBuffer(bones_buffer, 0, inumbones * sizeof(fmtx4));

  // printf( "inumbones<%d>\n", inumbones );

  for (int i = 0; i < inumbones; i++) {
    bones_mapped->ref<fmtx4>(fmtx4_stride * i) = Matrices[i];
    // printf( "I<%d>: ", i );
    // Matrices[i].dump("bonemtx");
  }

  bones_mapped->unmap();

  if (_pbrmaterial->_parBoneBlock) {
    fxi->bindParamBlockBuffer(_pbrmaterial->_parBoneBlock, bones_buffer);
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
