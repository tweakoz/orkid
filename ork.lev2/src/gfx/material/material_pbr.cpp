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

OIIO_NAMESPACE_USING

ImplementReflectionX(ork::lev2::PBRMaterial, "PBRMaterial");

namespace ork::lev2 {

//////////////////////////////////////////////////////

PBRMaterial::PBRMaterial()
    : _baseColor(1, 1, 1) {
  _rasterstate.SetShadeModel(ESHADEMODEL_SMOOTH);
  _rasterstate.SetAlphaTest(EALPHATEST_OFF);
  _rasterstate.SetBlending(EBLENDING_OFF);
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

  chunkfile::materialreader_t reader = [](chunkfile::XgmMaterialReaderContext& ctx) -> ork::lev2::GfxMaterial* {
    auto targ             = ctx._varmap.typedValueForKey<Context*>("gfxtarget").value();
    auto txi              = targ->TXI();
    const auto& embtexmap = ctx._varmap.typedValueForKey<embtexmap_t>("embtexmap").value();

    int istring = 0;

    ctx._inputStream->GetItem(istring);
    auto materialname = ctx._reader.GetString(istring);

    ctx._inputStream->GetItem(istring);
    auto texbasename = ctx._reader.GetString(istring);
    auto mtl         = new PBRMaterial;
    mtl->SetName(AddPooledString(materialname));
    //printf("materialName<%s>\n", materialname);
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
        //printf("find tex channel<%s> channel<%s> .. ", token, texname);
        auto itt = embtexmap.find(texname);
        assert(itt != embtexmap.end());
        auto embtex    = itt->second;
        auto tex       = new lev2::Texture;
        auto datablock = std::make_shared<DataBlock>(embtex->_srcdata, embtex->_srcdatalen);
        bool ok        = txi->LoadTexture(tex, datablock);
        assert(ok);
        //printf(" embtex<%p> datablock<%p> len<%zu>\n", embtex, datablock.get(), datablock->length());
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
    auto pbrmtl = static_cast<const PBRMaterial*>(ctx._material);

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

void PBRMaterial::Init(Context* targ) /*final*/ {
  assert(_initialTarget == nullptr);
  _initialTarget = targ;
  auto fxi       = targ->FXI();
  auto shass     = ork::asset::AssetManager<FxShaderAsset>::Load("orkshader://pbr");
  _shader        = shass->GetFxShader();

  _tekRigidGBUFFER              = fxi->technique(_shader, "rigid_gbuffer");
  _tekRigidGBUFFER_N            = fxi->technique(_shader, "rigid_gbuffer_n");
  _tekRigidGBUFFER_N_STEREO     = fxi->technique(_shader, "rigid_gbuffer_n_stereo");
  _tekRigidGBUFFER_N_TEX_STEREO = fxi->technique(_shader, "rigid_gbuffer_n_tex_stereo");
  _tekRigidPICKING              = fxi->technique(_shader, "picking_rigid");

  _tekRigidGBUFFER_SKINNED_N = fxi->technique(_shader, "skinned_gbuffer_n");

  _paramMVP           = fxi->parameter(_shader, "mvp");
  _paramMVPL          = fxi->parameter(_shader, "mvp_l");
  _paramMVPR          = fxi->parameter(_shader, "mvp_r");
  _paramMV            = fxi->parameter(_shader, "mv");
  _paramMROT          = fxi->parameter(_shader, "mrot");
  _paramMapColor      = fxi->parameter(_shader, "ColorMap");
  _paramMapNormal     = fxi->parameter(_shader, "NormalMap");
  _paramMapMtlRuf     = fxi->parameter(_shader, "MtlRufMap");
  _parInvViewSize     = fxi->parameter(_shader, "InvViewportSize");
  _parMetallicFactor  = fxi->parameter(_shader, "MetallicFactor");
  _parRoughnessFactor = fxi->parameter(_shader, "RoughnessFactor");
  _parModColor        = fxi->parameter(_shader, "ModColor");
  _parBoneMatrices    = fxi->parameter(_shader, "BoneMatrices");

  assert(_paramMapNormal != nullptr);
  assert(_parBoneMatrices != nullptr);

  if (_texColor == nullptr) {
    _texColor = asset::AssetManager<lev2::TextureAsset>::Load("data://effect_textures/white")->GetTexture();
    //printf("substituted white for non-existant color texture\n");
    OrkAssert(_texColor != nullptr);
  }
  if (_texNormal == nullptr) {
    _texNormal = asset::AssetManager<lev2::TextureAsset>::Load("data://effect_textures/default_normal")->GetTexture();
    //printf("substituted blue for non-existant normal texture\n");
    OrkAssert(_texNormal != nullptr);
  }
  if (_texMtlRuf == nullptr) {
    _texMtlRuf = asset::AssetManager<lev2::TextureAsset>::Load("data://effect_textures/white")->GetTexture();
    //printf("substituted white for non-existant mtlrufao texture\n");
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
    tek = is_skinned ? _tekRigidGBUFFER_SKINNED_N : _tekRigidGBUFFER_N;
    if (is_skinned) {
    }
  }

  fxi->BindTechnique(_shader, tek);

  int numpasses = fxi->BeginBlock(_shader, RCID);
  assert(numpasses == 1);
  return numpasses;
}

////////////////////////////////////////////

void PBRMaterial::EndBlock(Context* targ) {
  auto fxi = targ->FXI();
  fxi->EndBlock(_shader);
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
  fxi->BindPass(_shader, 0);

  fvec4 modcolor = _baseColor;
  if (is_picking) {
    modcolor = targ->RefModColor();
    // printf("modcolor<%g %g %g %g>\n", modcolor.x, modcolor.y, modcolor.z, modcolor.w);
  } else {
    fxi->BindParamCTex(_shader, _paramMapColor, _texColor);
    fxi->BindParamCTex(_shader, _paramMapNormal, _texNormal);
    fxi->BindParamCTex(_shader, _paramMapMtlRuf, _texMtlRuf);
    fxi->BindParamFloat(_shader, _parMetallicFactor, _metallicFactor);
    fxi->BindParamFloat(_shader, _parRoughnessFactor, _roughnessFactor);
    auto brdfintegtex = PBRMaterial::brdfIntegrationMap(targ);
    const auto& drect = CPD.GetDstRect();
    const auto& mrect = CPD.GetMrtRect();
    float w           = mrect._w;
    float h           = mrect._h;
    fxi->BindParamVect2(_shader, _parInvViewSize, fvec2(1.0 / w, 1.0f / h));
  }

  fxi->BindParamVect4(_shader, _parModColor, modcolor);
  fxi->BindParamMatrix(_shader, _paramMV, mvmtx);

  if (CPD.isStereoOnePass() and CPD._stereoCameraMatrices) {
    auto stereomtx = CPD._stereoCameraMatrices;
    auto MVPL      = stereomtx->MVPL(world);
    auto MVPR      = stereomtx->MVPR(world);
    fxi->BindParamMatrix(_shader, _paramMVPL, MVPL);
    fxi->BindParamMatrix(_shader, _paramMVPR, MVPR);
    fxi->BindParamMatrix(_shader, _paramMROT, (world).rotMatrix33());
  } else {
    auto mcams = CPD._cameraMatrices;
    auto MVP   = world * mcams->_vmatrix * mcams->_pmatrix;
    fxi->BindParamMatrix(_shader, _paramMVP, MVP);
    fxi->BindParamMatrix(_shader, _paramMROT, (world).rotMatrix33());
  }
  rsi->BindRasterState(_rasterstate);
  fxi->CommitParams();
  return true;
}

////////////////////////////////////////////

void PBRMaterial::EndPass(Context* targ) {
  targ->FXI()->EndPass(_shader);
}

///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::BindMaterialInstItem(MaterialInstItem* pitem) const {
  ///////////////////////////////////
  MaterialInstItemMatrixBlock* mtxblockitem = rtti::autocast(pitem);

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

  MaterialInstItemMatrixBlock* mtxblockitem = rtti::autocast(pitem);

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
  fxi->BindParamMatrixArray(shader, _pbrmaterial->_parBoneMatrices, Matrices, (int)inumbones);
  fxi->CommitParams();
}

////////////////////////////////////////////

void PBRMaterial::Update() {
}

////////////////////////////////////////////

void PBRMaterial::setupCamera(const RenderContextFrameData& RCFD) {
  auto target     = RCFD.mpTarget;
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
    FXI->BindParamMatrix(_shader, _paramMVPL, MVPL);
    FXI->BindParamMatrix(_shader, _paramMVPR, MVPR);
  } else if (CPD._cameraMatrices) {
    auto mcams = CPD._cameraMatrices;
    auto MVP   = world * mcams->_vmatrix * mcams->_pmatrix;
    FXI->BindParamMatrix(_shader, _paramMVP, MVP);
  } else {
    auto MVP = MTXI->RefMVPMatrix();
    FXI->BindParamMatrix(_shader, _paramMVP, MVP);
  }
}

////////////////////////////////////////////

void PBRMaterial::begin(const RenderContextFrameData& RCFD) {
}

////////////////////////////////////////////

void PBRMaterial::end(const RenderContextFrameData& RCFD) {
}

} // namespace ork::lev2
