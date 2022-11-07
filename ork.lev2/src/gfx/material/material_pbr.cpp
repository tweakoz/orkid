////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/gfx/brdf.inl>
#include <ork/pch.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <OpenImageIO/imageio.h>
#include <ork/kernel/datacache.h>
#include <ork/reflect/properties/registerX.inl>

OIIO_NAMESPACE_USING

ImplementReflectionX(ork::lev2::PBRMaterial, "PBRMaterial");

namespace ork::lev2 {

material_ptr_t default3DMaterial() {
  return std::make_shared<PBRMaterial>(lev2::contextForCurrentThread());
}

//////////////////////////////////////////////////////

PBRMaterial::PBRMaterial(Context* targ)
    : PBRMaterial() {
  gpuInit(targ);
}

PBRMaterial::PBRMaterial()
    : _baseColor(1, 1, 1) {
  _rasterstate.SetShadeModel(ESHADEMODEL_SMOOTH);
  _rasterstate.SetAlphaTest(EALPHATEST_OFF);
  _rasterstate.SetBlending(Blending::OFF);
  _rasterstate.SetDepthTest(EDEPTHTEST_LEQUALS);
  _rasterstate.SetZWriteMask(true);
  _rasterstate.SetCullTest(ECULLTEST_PASS_FRONT);
  miNumPasses = 1;
  _shaderpath = "orkshader://pbr";
}

////////////////////////////////////////////

PBRMaterial::~PBRMaterial() {
}

/////////////////////////////////////////////////////////////////////////

PbrMatrixBlockApplicator* PbrMatrixBlockApplicator::getApplicator() {
  static PbrMatrixBlockApplicator* _gapplicator = new PbrMatrixBlockApplicator;
  return _gapplicator;
}

void PBRMaterial::describeX(class_t* c) {

  /////////////////////////////////////////////////////////////////

  chunkfile::materialreader_t reader = [](chunkfile::XgmMaterialReaderContext& ctx) -> ork::lev2::material_ptr_t {
    auto targ             = ctx._varmap->typedValueForKey<Context*>("gfxtarget").value();
    auto txi              = targ->TXI();
    const auto& embtexmap = ctx._varmap->typedValueForKey<embtexmap_t>("embtexmap").value();

    int istring = 0;

    ctx._inputStream->GetItem(istring);
    auto materialname = ctx._reader.GetString(istring);

    ctx._inputStream->GetItem(istring);
    auto texbasename = ctx._reader.GetString(istring);
    auto mtl         = std::make_shared<PBRMaterial>();
    mtl->SetName(AddPooledString(materialname));
    // printf("materialName<%s>\n", materialname);
    ctx._inputStream->GetItem(istring);
    auto begintextures = ctx._reader.GetString(istring);
    assert(0 == strcmp(begintextures, "begintextures"));
    bool done = false;
    while (false == done) {
      ctx._inputStream->GetItem(istring);
      auto token = ctx._reader.GetString(istring);
      if (0 == strcmp(token, "endtextures"))
        done = true;
      else {
        ctx._inputStream->GetItem(istring);
        auto texname = ctx._reader.GetString(istring);
        // printf("find tex channel<%s> channel<%s> .. ", token, texname);
        auto itt = embtexmap.find(texname);
        assert(itt != embtexmap.end());
        auto embtex    = itt->second;
        auto tex       = std::make_shared<lev2::Texture>();
        auto datablock = std::make_shared<DataBlock>(embtex->_srcdata, embtex->_srcdatalen);
        bool ok        = txi->LoadTexture(tex, datablock);
        assert(ok);
        // printf(" embtex<%p> datablock<%p> len<%zu>\n", embtex, datablock.get(), datablock->length());
        if (0 == strcmp(token, "colormap")) {
          mtl->_texColor = tex;
        }
        if (0 == strcmp(token, "normalmap")) {
          mtl->_texNormal = tex;
        }
        if (0 == strcmp(token, "mtlrufmap")) {
          mtl->_texMtlRuf = tex;
        }
        if (0 == strcmp(token, "emissivemap")) {
          mtl->_texEmissive = tex;
        }
      }
    }
    ctx._inputStream->GetItem<float>(mtl->_metallicFactor);
    ctx._inputStream->GetItem<float>(mtl->_roughnessFactor);
    ctx._inputStream->GetItem<fvec4>(mtl->_baseColor);

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
  };

  /////////////////////////////////////////////////////////////////

  c->annotate("xgm.writer", writer);
  c->annotate("xgm.reader", reader);
}

////////////////////////////////////////////

void PBRMaterial::gpuInit(Context* targ) /*final*/ {
  if (_initialTarget)
    return;

  _initialTarget = targ;
  auto fxi       = targ->FXI();

  _asset_shader = ork::asset::AssetManager<FxShaderAsset>::load(_shaderpath);
  _shader       = _asset_shader->GetFxShader();

  // specials

  _tek_GBU_DB_NM_NI_MO = fxi->technique(_shader, "GBU_DB_NM_NI_MO");

  _tek_GBU_CF_IN_MO = fxi->technique(_shader, "GBU_CF_IN_MO");
  _tek_GBU_CF_NI_MO = fxi->technique(_shader, "GBU_CF_NI_MO");

  _tek_PIK_RI_IN = fxi->technique(_shader, "PIK_RI_IN");
  _tek_PIK_RI_NI = fxi->technique(_shader, "PIK_RI_NI");

  // forwards

  _tek_FWD_CT_NM_RI_NI_MO = fxi->technique(_shader, "FWD_CT_NM_RI_NI_MO");
  _tek_FWD_CT_NM_RI_IN_MO = fxi->technique(_shader, "FWD_CT_NM_RI_IN_MO");

  // deferreds

  _tek_GBU_CM_NM_RI_NI_MO = fxi->technique(_shader, "GBU_CM_NM_RI_NI_MO");
  _tek_GBU_CM_NM_SK_NI_MO = fxi->technique(_shader, "GBU_CM_NM_SK_NI_MO");
  _tek_GBU_CM_NM_RI_NI_ST = fxi->technique(_shader, "GBU_CM_NM_RI_NI_ST");


  _tek_GBU_CT_NM_RI_IN_MO = fxi->technique(_shader, "GBU_CT_NM_RI_IN_MO");
  _tek_GBU_CT_NM_RI_IN_ST = fxi->technique(_shader, "GBU_CT_NM_RI_IN_ST");
  _tek_GBU_CT_NM_RI_NI_ST = fxi->technique(_shader, "GBU_CT_NM_RI_NI_ST");
  _tek_GBU_CT_NM_RI_NI_MO = fxi->technique(_shader, "GBU_CT_NM_RI_NI_MO");

  _tek_GBU_CT_NM_SK_IN_MO = fxi->technique(_shader, "GBU_CT_NM_SK_IN_MO");

  _tek_GBU_CT_NM_SK_NI_MO = fxi->technique(_shader, "GBU_CT_NM_SK_NI_MO");

  _tek_GBU_CT_NV_RI_NI_MO = fxi->technique(_shader, "GBU_CT_NV_RI_NI_MO");

  _tek_GBU_CV_NM_RI_NI_MO = fxi->technique(_shader, "GBU_CV_NM_RI_NI_MO");


  OrkAssert(_tek_GBU_CT_NM_RI_NI_ST);
  OrkAssert(_tek_GBU_CT_NM_RI_IN_ST);
  OrkAssert(_tek_GBU_CT_NM_RI_IN_MO);
  OrkAssert(_tek_GBU_CT_NM_RI_NI_MO);
  OrkAssert(_tek_FWD_CT_NM_RI_NI_MO);
  OrkAssert(_tek_FWD_CT_NM_RI_IN_MO);

  // parameters

  _paramM                 = fxi->parameter(_shader, "m");
  _paramVP                = fxi->parameter(_shader, "vp");
  _paramMVP               = fxi->parameter(_shader, "mvp");
  _paramMVPL              = fxi->parameter(_shader, "mvp_l");
  _paramMVPR              = fxi->parameter(_shader, "mvp_r");
  _paramMV                = fxi->parameter(_shader, "mv");
  _paramMROT              = fxi->parameter(_shader, "mrot");
  _paramMapColor          = fxi->parameter(_shader, "ColorMap");
  _paramMapNormal         = fxi->parameter(_shader, "NormalMap");
  _paramMapMtlRuf         = fxi->parameter(_shader, "MtlRufMap");
  _parInvViewSize         = fxi->parameter(_shader, "InvViewportSize");
  _parMetallicFactor      = fxi->parameter(_shader, "MetallicFactor");
  _parRoughnessFactor     = fxi->parameter(_shader, "RoughnessFactor");
  _parModColor            = fxi->parameter(_shader, "ModColor");
  _parBoneMatrices        = fxi->parameter(_shader, "BoneMatrices");
  _paramInstanceMatrixMap = fxi->parameter(_shader, "InstanceMatrices");
  _paramInstanceIdMap     = fxi->parameter(_shader, "InstanceIds");
  _paramInstanceColorMap  = fxi->parameter(_shader, "InstanceColors");
  
  // fwd

  _paramEyePostion        = fxi->parameter(_shader, "EyePostion");
  _paramAmbientLevel      = fxi->parameter(_shader, "AmbientLevel");
  _paramDiffuseLevel      = fxi->parameter(_shader, "DiffuseLevel");
  _paramSpecularLevel      = fxi->parameter(_shader, "SpecularLevel");
  _paramSkyboxLevel      = fxi->parameter(_shader, "SkyboxLevel");

  _parMapSpecularEnv = fxi->parameter(_shader, "MapSpecularEnv");
  _parMapDiffuseEnv = fxi->parameter(_shader, "MapDiffuseEnv");
  _parMapBrdfIntegration = fxi->parameter(_shader, "MapBrdfIntegration");
  _parEnvironmentMipBias = fxi->parameter(_shader, "EnvironmentMipBias");
  _parEnvironmentMipScale = fxi->parameter(_shader, "EnvironmentMipScale");

  //

  assert(_paramMapNormal != nullptr);
  assert(_parBoneMatrices != nullptr);

  if (_texColor == nullptr) {
    _asset_texcolor = asset::AssetManager<lev2::TextureAsset>::load("src://effect_textures/white");
    _texColor       = _asset_texcolor->GetTexture();
    // printf("substituted white for non-existant color texture\n");
    OrkAssert(_texColor != nullptr);
  }
  if (_texNormal == nullptr) {
    _asset_texnormal = asset::AssetManager<lev2::TextureAsset>::load("src://effect_textures/default_normal");
    _texNormal       = _asset_texnormal->GetTexture();
    // printf("substituted blue for non-existant normal texture\n");
    OrkAssert(_texNormal != nullptr);
  }
  if (_texMtlRuf == nullptr) {
    _asset_mtlruf = asset::AssetManager<lev2::TextureAsset>::load("src://effect_textures/white");
    _texMtlRuf    = _asset_mtlruf->GetTexture();
    // printf("substituted white for non-existant mtlrufao texture\n");
    OrkAssert(_texMtlRuf != nullptr);
  }
  if (_texEmissive) {
    //_asset_emissive = _asset_texcolor;
    //_texEmissive       = _asset_emissive->GetTexture();
    printf("substituted white for non-existant color texture\n");
    // OrkAssert(_texEmissive != nullptr);
    forceEmissive();
    //_asset_texcolor = asset::AssetManager<lev2::TextureAsset>::load("src://effect_textures/white");
    _texColor = _texEmissive;
  }
}

void PBRMaterial::forceEmissive() {
  // to force emissive set normal map to black
  // shader will interpret as emissive
  _asset_texnormal = asset::AssetManager<lev2::TextureAsset>::load("src://effect_textures/black");
  _texNormal       = _asset_texnormal->GetTexture();
  OrkAssert(_texNormal != nullptr);
}

////////////////////////////////////////////

fxinstance_ptr_t PBRMaterial::_createFxStateInstance(FxStateInstanceConfig& cfg) const {

  cfg.dump();

  auto fxinst = std::make_shared<FxStateInstance>(cfg);

  switch (_variant) {
    case 0: { // STANDARD VARIANT
      switch (cfg._rendering_model) {
        //////////////////////////////////////////
        case ERenderModelID::PICKING: {
          OrkAssert(cfg._stereo==false);
          if (cfg._instanced) {
            fxinst->_technique = _tek_PIK_RI_IN;
          }
          ////////////////
          else { // non-instanced
            if (cfg._skinned)
              fxinst->_technique = nullptr;
            else // rigid
              fxinst->_technique = _tek_PIK_RI_NI;
            ////////////////
          }
          OrkAssert(fxinst->_technique!=nullptr);
          fxinst->_params[_paramMVP] = "RCFD_Camera_Pick"_crcsh;
          break;
        }
        //////////////////////////////////////////
        case ERenderModelID::DEFERRED_PBR: {
          if (cfg._stereo) {                                     // stereo
            if (cfg._instanced) {                                // stereo-instanced
              fxinst->_technique = cfg._skinned                  //
                                       ? _tek_GBU_CT_NM_SK_IN_ST //
                                       : _tek_GBU_CT_NM_RI_IN_ST;
              OrkAssert(fxinst->_technique!=nullptr);
            } else {                                             // stereo-non-instanced
              fxinst->_technique = cfg._skinned                  //
                                       ? _tek_GBU_CT_NM_SK_NI_ST //
                                       : _tek_GBU_CT_NM_RI_NI_ST;
              OrkAssert(fxinst->_technique!=nullptr);
            }
            fxinst->_params[_paramMVPL] = "RCFD_Camera_MVP_Left"_crcsh;
            fxinst->_params[_paramMVPR] = "RCFD_Camera_MVP_Right"_crcsh;
          } else {                                               // mono
            if (cfg._instanced) {                                // mono-instanced
              fxinst->_technique = cfg._skinned                  //
                                       ? _tek_GBU_CT_NM_SK_IN_MO //
                                       : _tek_GBU_CT_NM_RI_IN_MO;
              OrkAssert(fxinst->_technique!=nullptr);
            } else {                                             // mono-non-instanced
              fxinst->_technique = cfg._skinned                  //
                                       ? _tek_GBU_CT_NM_SK_NI_MO //
                                       : _tek_GBU_CT_NM_RI_NI_MO;
              OrkAssert(fxinst->_technique!=nullptr);
            }
            fxinst->_params[_paramMVP] = "RCFD_Camera_MVP_Mono"_crcsh;
          }
          OrkAssert(fxinst->_technique!=nullptr);
          break;
        } // ERenderModelID::DEFERRED_PBR
        //////////////////////////////////////////
        case ERenderModelID::FORWARD_UNLIT:
          OrkAssert(false);
          break;
        case ERenderModelID::FORWARD_PBR:
          if (cfg._instanced and not cfg._skinned and not cfg._stereo) {
              fxinst->_technique = _tek_FWD_CT_NM_RI_IN_MO;
              fxinst->_params[_paramMVP] = "RCFD_Camera_MVP_Mono"_crcsh;
              fxinst->_params[_paramEyePostion] = "EyePosition"_crcsh;
              fxinst->_params[_paramAmbientLevel] = "AmbientLevel"_crcsh;
              fxinst->_params[_paramDiffuseLevel] = "DiffuseLevel"_crcsh;
              fxinst->_params[_paramSpecularLevel] = "SpecularLevel"_crcsh;
              fxinst->_params[_paramSkyboxLevel] = "SkyboxLevel"_crcsh;

              fxinst->_params[_parEnvironmentMipBias] = "EnvironmentMipBias"_crcsh;
              fxinst->_params[_parEnvironmentMipScale] = "EnvironmentMipScale"_crcsh;

              fxinst->_params[_parMapSpecularEnv] = "MapSpecularEnv"_crcsh;
              fxinst->_params[_parMapDiffuseEnv] = "MapDiffuseEnv"_crcsh;
              fxinst->_params[_parMapBrdfIntegration] = "MapBrdfIntegration"_crcsh;
          }
          OrkAssert(fxinst->_technique!=nullptr);
          break;
        default:
          OrkAssert(false);
          break;
        //////////////////////////////////////////
      }  //switch (cfg._rendering_model) {
      break;
    } // case 0: // STANDARD VARIANT
    //////////////////////////////////////////
    case "normalviz"_crcu:
      OrkAssert(false);
      break;
    //////////////////////////////////////////
    case "vertexcolor"_crcu:
      OrkAssert(false);
      break;
    //////////////////////////////////////////
    case "font"_crcu:
      OrkAssert(false);
      break;
    //////////////////////////////////////////
    case "font-instanced"_crcu:
      OrkAssert(false);
      break;
    //////////////////////////////////////////
    default:
      OrkAssert(false);
      break;
  }

  OrkAssert(fxinst->_technique != nullptr);

  fxinst->_params[_paramMROT] = "RCFD_Model_Rot"_crcsh;

  fxinst->_params[_paramMapColor]  = _texColor;
  fxinst->_params[_paramMapNormal] = _texNormal;
  fxinst->_params[_paramMapMtlRuf] = _texMtlRuf;

  fxinst->_params[_parMetallicFactor]  = _metallicFactor;
  fxinst->_params[_parRoughnessFactor] = _roughnessFactor;
  fxinst->_params[_parModColor]        = fvec4(1, 1, 1, 1);

  fxinst->_parInstanceMatrixMap = _paramInstanceMatrixMap;
  fxinst->_parInstanceIdMap     = _paramInstanceIdMap;
  fxinst->_parInstanceColorMap  = _paramInstanceColorMap;
  fxinst->_material = (GfxMaterial*) this;

  return fxinst;
}

///////////////////////////////////////////////////////////////////////////////

fxinstancelut_ptr_t PBRMaterial::createFxStateInstanceLut() const {
  fxinstancelut_ptr_t fxlut = std::make_shared<FxStateInstanceLut>();

  FxStateInstanceConfig config;

  printf( "fxlut<%p> createFxStateInstanceLut\n", fxlut.get() );

  /////////////////////
  // picking
  /////////////////////

  config._rendering_model = ERenderModelID::PICKING;

  config._stereo    = false;
  config._instanced = false;
  config._skinned   = false;
  fxlut->assignfxinst(config, _createFxStateInstance(config));

  /////////////////////
  // deferred PBR
  /////////////////////

  config._rendering_model = ERenderModelID::DEFERRED_PBR;

  config._stereo    = false;
  config._instanced = false;
  config._skinned   = false;
  fxlut->assignfxinst(config, _createFxStateInstance(config));

  config._stereo    = false;
  config._instanced = true;
  config._skinned   = false;
  fxlut->assignfxinst(config, _createFxStateInstance(config));

  config._stereo    = true;
  config._instanced = false;
  config._skinned   = false;
  fxlut->assignfxinst(config, _createFxStateInstance(config));

  config._stereo    = true;
  config._instanced = true;
  config._skinned   = false;
  fxlut->assignfxinst(config, _createFxStateInstance(config));

  /////////////////////
  // forward PBR
  /////////////////////

  config._rendering_model = ERenderModelID::FORWARD_PBR;

  config._stereo    = false;
  config._instanced = true;
  config._skinned   = false;
  fxlut->assignfxinst(config, _createFxStateInstance(config));

  return fxlut;
}

////////////////////////////////////////////

int PBRMaterial::BeginBlock(Context* context, const RenderContextInstData& RCID) {
  auto fxi   = context->FXI();
  auto fxlut = RCID._fx_instance_lut;
  OrkAssert(fxlut);
  auto fxinstance = fxlut->findfxinst(RCID);
  OrkAssert(fxinstance);
  auto tek = fxinstance->_technique;
  OrkAssert(tek);

  int numpasses = fxi->BeginBlock(tek, RCID);
  assert(numpasses == 1);
  return numpasses;
}

////////////////////////////////////////////

void PBRMaterial::EndBlock(Context* context) {
  auto fxi = context->FXI();
  fxi->EndBlock();
}

////////////////////////////////////////////

void PBRMaterial::gpuUpdate(Context* context) {
  GfxMaterial::gpuUpdate(context);
  // auto modcolor = context->RefModColor();
  // auto fxi    = context->FXI();
  // fxi->BindParamVect4(_parModColor, modcolor*_baseColor);
}

////////////////////////////////////////////

bool PBRMaterial::BeginPass(Context* targ, int iPass) {
  // printf( "_name<%s>\n", mMaterialName.c_str() );

  auto fxi    = targ->FXI();
  auto rsi    = targ->RSI();
  auto mtxi   = targ->MTXI();
  auto mvpmtx = mtxi->RefMVPMatrix();
  auto rotmtx = mtxi->RefR3Matrix();
  auto mvmtx  = mtxi->RefMVMatrix();
  auto vmtx   = mtxi->RefVMatrix();
  auto pmtx   = mtxi->RefPMatrix();
  auto vpmtx  = fmtx4::multiply_ltor(vmtx, pmtx);

  // vmtx.dump("vmtx");
  const RenderContextInstData* RCID  = targ->GetRenderContextInstData();
  const RenderContextFrameData* RCFD = targ->topRenderContextFrameData();
  const auto& CPD                    = RCFD->topCPD();
  bool is_picking                    = CPD.isPicking();
  const auto& world                  = mtxi->RefMMatrix();
  fxi->BindPass(0);

  fvec4 modcolor = _baseColor;
  if (is_picking) {
    modcolor = targ->RefModColor();
    // printf("modcolor<%g %g %g %g>\n", modcolor.x, modcolor.y, modcolor.z, modcolor.w);
  } else {
    modcolor = _baseColor * targ->RefModColor();
    fxi->BindParamCTex(_paramMapColor, _texColor.get());
    fxi->BindParamCTex(_paramMapNormal, _texNormal.get());
    fxi->BindParamCTex(_paramMapMtlRuf, _texMtlRuf.get());
    fxi->BindParamFloat(_parMetallicFactor, _metallicFactor);
    fxi->BindParamFloat(_parRoughnessFactor, _roughnessFactor);
    auto brdfintegtex = PBRMaterial::brdfIntegrationMap(targ);
    const auto& drect = CPD.GetDstRect();
    const auto& mrect = CPD.GetMrtRect();
    float w           = mrect._w;
    float h           = mrect._h;
    fxi->BindParamVect2(_parInvViewSize, fvec2(1.0 / w, 1.0f / h));
  }

  fxi->BindParamVect4(_parModColor, modcolor);
  fxi->BindParamMatrix(_paramMV, mvmtx);

  if (CPD.isStereoOnePass() and CPD._stereoCameraMatrices) {
    auto stereomtx = CPD._stereoCameraMatrices;
    auto MVPL      = stereomtx->MVPL(world);
    auto MVPR      = stereomtx->MVPR(world);
    fxi->BindParamMatrix(_paramMVPL, MVPL);
    fxi->BindParamMatrix(_paramMVPR, MVPR);
    fxi->BindParamMatrix(_paramMROT, (world).rotMatrix33());


  } else {
    auto mcams = CPD._cameraMatrices;
    auto VP    = fmtx4::multiply_ltor(mcams->_vmatrix, mcams->_pmatrix);
    auto MVP   = fmtx4::multiply_ltor(world, VP);
    fxi->BindParamMatrix(_paramMVP, MVP);
    fxi->BindParamMatrix(_paramVP, VP);
    fxi->BindParamMatrix(_paramMROT, (world).rotMatrix33());

    auto eye_pos = mcams->_vmatrix.inverse().translation();

    printf( "eye_pos<%g %g %g>\n", eye_pos.x, eye_pos.y, eye_pos.z );
    fxi->BindParamVect3(_paramEyePostion,eye_pos);

  }


  switch (_variant) {
    case "font-instanced"_crcu:
      break;
    default:
      break;
  }

  rsi->BindRasterState(_rasterstate);
  fxi->CommitParams();
  return true;
}

void PBRMaterial::UpdateMVPMatrix(Context* context) {
  auto fxi                           = context->FXI();
  auto rsi                           = context->RSI();
  auto mtxi                          = context->MTXI();
  const RenderContextInstData* RCID  = context->GetRenderContextInstData();
  const RenderContextFrameData* RCFD = context->topRenderContextFrameData();
  const auto& CPD                    = RCFD->topCPD();
  if (CPD.isStereoOnePass() and CPD._stereoCameraMatrices) {
  } else {
    auto mcams        = CPD._cameraMatrices;
    const auto& world = mtxi->RefMMatrix();
    auto MVP          = fmtx4::multiply_ltor(world, mcams->_vmatrix, mcams->_pmatrix);
    fxi->BindParamMatrix(_paramMVP, MVP);
  }
}

void PBRMaterial::UpdateMMatrix(Context* context) {
  auto fxi          = context->FXI();
  auto mtxi         = context->MTXI();
  const auto& world = mtxi->RefMMatrix();
  fxi->BindParamMatrix(_paramM, world);
}

////////////////////////////////////////////

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

void PbrMatrixBlockApplicator::ApplyToTarget(Context* targ) // virtual
{
  auto fxi                           = targ->FXI();
  auto mtxi                          = targ->MTXI();
  const RenderContextInstData* RCID  = targ->GetRenderContextInstData();
  const RenderContextFrameData* RCFD = targ->topRenderContextFrameData();
  const auto& CPD                    = RCFD->topCPD();
  const auto& world                  = mtxi->RefMMatrix();
  const auto& drect                  = CPD.GetDstRect();
  const auto& mrect                  = CPD.GetMrtRect();
  FxShader* shader                   = _pbrmaterial->_shader;
  size_t inumbones                   = _matrixblock->GetNumMatrices();
  const fmtx4* Matrices              = _matrixblock->GetMatrices();

  if (0)
    for (int i = 0; i < inumbones; i++) {
      const auto& b = Matrices[i];
      b.dump(FormatString("pbr-bone<%d>", i));
    }
  fxi->BindParamMatrixArray(_pbrmaterial->_parBoneMatrices, Matrices, (int)inumbones);
  fxi->CommitParams();
}

////////////////////////////////////////////

void PBRMaterial::Update() {
}

////////////////////////////////////////////

void PBRMaterial::setupCamera(const RenderContextFrameData& RCFD) {
  auto target     = RCFD._target;
  auto MTXI       = target->MTXI();
  auto FXI        = target->FXI();
  auto CIMPL      = RCFD._cimpl;
  const auto& CPD = CIMPL->topCPD();
  bool is_stereo  = CPD.isStereoOnePass();

  const auto& world = MTXI->RefMMatrix();
  if (is_stereo and CPD._stereoCameraMatrices) {
    auto stereomtx = CPD._stereoCameraMatrices;
    auto MVPL      = stereomtx->MVPL(world);
    auto MVPR      = stereomtx->MVPR(world);
    // todo fix for stereo..
    FXI->BindParamMatrix(_paramMVPL, MVPL);
    FXI->BindParamMatrix(_paramMVPR, MVPR);
  } else if (CPD._cameraMatrices) {
    auto mcams = CPD._cameraMatrices;
    auto MVP   = fmtx4::multiply_ltor(world, mcams->_vmatrix, mcams->_pmatrix);
    FXI->BindParamMatrix(_paramMVP, MVP);
  } else {
    auto MVP = MTXI->RefMVPMatrix();
    FXI->BindParamMatrix(_paramMVP, MVP);
  }
}

////////////////////////////////////////////

void PBRMaterial::begin(const RenderContextFrameData& RCFD) {
}

////////////////////////////////////////////

void PBRMaterial::end(const RenderContextFrameData& RCFD) {
}

////////////////////////////////////////////

} // namespace ork::lev2
