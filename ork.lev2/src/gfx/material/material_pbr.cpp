////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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
  static auto mtl = std::make_shared<PBRMaterial>(GfxEnv::GetRef().loadingContext());
  return mtl;
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
  _rasterstate.SetCullTest(ECULLTEST_OFF);
  miNumPasses = 1;
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
    auto targ             = ctx._varmap.typedValueForKey<Context*>("gfxtarget").value();
    auto txi              = targ->TXI();
    const auto& embtexmap = ctx._varmap.typedValueForKey<embtexmap_t>("embtexmap").value();

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
        auto tex       = new lev2::Texture;
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
      }
    }
    ctx._inputStream->GetItem<float>(mtl->_metallicFactor);
    ctx._inputStream->GetItem<float>(mtl->_roughnessFactor);
    ctx._inputStream->GetItem<fvec4>(mtl->_baseColor);
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
  _asset_shader  = ork::asset::AssetManager<FxShaderAsset>::load("orkshader://pbr");
  _shader        = _asset_shader->GetFxShader();

  _tekRigidPICKING           = fxi->technique(_shader, "picking_rigid");
  _tekRigidPICKING_INSTANCED = fxi->technique(_shader, "picking_rigid_instanced");

  _tekRigidGBUFFER              = fxi->technique(_shader, "rigid_gbuffer");
  _tekRigidGBUFFER_N            = fxi->technique(_shader, "rigid_gbuffer_n");
  _tekRigidGBUFFER_N_STEREO     = fxi->technique(_shader, "rigid_gbuffer_n_stereo");
  _tekRigidGBUFFER_N_TEX_STEREO = fxi->technique(_shader, "rigid_gbuffer_n_tex_stereo");
  _tekRigidGBUFFER_N_SKINNED    = fxi->technique(_shader, "skinned_gbuffer_n");

  _tekRigidGBUFFER_N_INSTANCED        = fxi->technique(_shader, "rigid_gbuffer_n_instanced");
  _tekRigidGBUFFER_N_INSTANCED_STEREO = fxi->technique(_shader, "rigid_gbuffer_n_instanced_stereo");

  _tekSkinnedGBUFFER_N = fxi->technique(_shader, "skinned_gbuffer_n");

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
}

////////////////////////////////////////////

int PBRMaterial::BeginBlock(Context* targ, const RenderContextInstData& RCID) {
  auto fxi                           = targ->FXI();
  const RenderContextFrameData* RCFD = targ->topRenderContextFrameData();
  const auto& CPD                    = RCFD->topCPD();
  bool is_stereo                     = CPD.isStereoOnePass();
  bool is_skinned                    = RCID._isSkinned;
  bool is_picking                    = CPD.isPicking();

  const FxShaderTechnique* tek = _tekRigidGBUFFER;

  if (is_picking) {
    tek = _tekRigidPICKING;
  } else if (is_stereo) {
    if (_stereoVtex)
      tek = _tekRigidGBUFFER_N_TEX_STEREO;
    else
      tek = _tekRigidGBUFFER_N_STEREO;
  } else {
    tek = is_skinned ? _tekRigidGBUFFER_N_SKINNED : _tekRigidGBUFFER_N;
    if (is_skinned) {
    }
  }

  int numpasses = fxi->BeginBlock(tek, RCID);
  assert(numpasses == 1);
  return numpasses;
}

////////////////////////////////////////////

void PBRMaterial::EndBlock(Context* targ) {
  auto fxi = targ->FXI();
  fxi->EndBlock();
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
    fxi->BindParamCTex(_paramMapColor, _texColor);
    fxi->BindParamCTex(_paramMapNormal, _texNormal);
    fxi->BindParamCTex(_paramMapMtlRuf, _texMtlRuf);
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
    auto MVP   = world * mcams->_vmatrix * mcams->_pmatrix;
    fxi->BindParamMatrix(_paramMVP, MVP);
    fxi->BindParamMatrix(_paramMROT, (world).rotMatrix33());
  }
  rsi->BindRasterState(_rasterstate);
  fxi->CommitParams();
  return true;
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
    auto MVP   = world * mcams->_vmatrix * mcams->_pmatrix;
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

fxinstance_ptr_t PBRMaterial::createFxStateInstance(FxStateInstanceConfig& cfg) const {

  auto fxinst = std::make_shared<FxStateInstance>(cfg);

  switch (cfg._base_perm) {
    //////////////////////////////////////////
    case FxStateBasePermutation::PICK:
      if (cfg._instanced_primitive) {
        fxinst->_technique = _tekRigidPICKING_INSTANCED;
      }
      ////////////////
      else { // non-instanced
        if (cfg._skinned)
          fxinst->_technique = nullptr;
        else // rigid
          fxinst->_technique = _tekRigidPICKING;
        ////////////////
      }
      fxinst->_params[_paramMVP] = "RCFD_Camera_Pick"_crcsh;
      break;
    //////////////////////////////////////////
    case FxStateBasePermutation::MONO:
      if (cfg._instanced_primitive) {
        if (cfg._skinned)
          fxinst->_technique = nullptr;
        else { // rigid
          fxinst->_technique = _tekRigidGBUFFER_N_INSTANCED;
        }
      }
      ////////////////
      else { // non-instanced
        if (cfg._skinned)
          fxinst->_technique = _tekRigidGBUFFER_N_SKINNED;
        else // rigid
          fxinst->_technique = _tekRigidGBUFFER_N;
        ////////////////
      }
      fxinst->_params[_paramMVP] = "RCFD_Camera_MVP_Mono"_crcsh;
      break;
    //////////////////////////////////////////
    case FxStateBasePermutation::STEREO:
      if (cfg._instanced_primitive) {
        if (cfg._skinned)
          fxinst->_technique = nullptr;
        else // rigid
          fxinst->_technique = _tekRigidGBUFFER_N_INSTANCED_STEREO;
      }
      ////////////////
      else { // non-instanced
        if (cfg._skinned)
          fxinst->_technique = nullptr;
        else // rigid
          fxinst->_technique = _tekRigidGBUFFER_N_STEREO;
        ////////////////
      }
      fxinst->_params[_paramMVPL] = "RCFD_Camera_MVP_Left"_crcsh;
      fxinst->_params[_paramMVPR] = "RCFD_Camera_MVP_Right"_crcsh;
      break;
      //////////////////////////////////////////
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

  return fxinst;
}

////////////////////////////////////////////

} // namespace ork::lev2
