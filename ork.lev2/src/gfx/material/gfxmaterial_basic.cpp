////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/Array.hpp>
#include <ork/kernel/orkpool.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/gfxanim.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_basic.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/lev2_asset.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::GfxMaterialWiiBasic, "MaterialBasic")
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::WiiMatrixApplicator, "WiiMatrixApplicator")
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::WiiMatrixBlockApplicator, "WiiMatrixBlockApplicator")

namespace ork { namespace lev2 {

void WiiMatrixApplicator::Describe() {}
void WiiMatrixBlockApplicator::Describe() {}

void GfxMaterialWiiBasic::Describe() {}

static bool gbenable = true; // disable since wii port is gone

/////////////////////////////////////////////////////////////////////////

GfxMaterialWiiBasic::GfxMaterialWiiBasic(const char* bastek)
    : mSpecularPower(1.0f)
    , mBasicTechName(bastek)
    , mEmissiveColor(0.0f, 0.0f, 0.0f, 0.0f)
    , hTekModVtxTex(nullptr)
    , hTekModVtxTexStereo(nullptr)
    , hTekMod(nullptr)
    , hMatMV(nullptr)
    , hMatP(nullptr)
    , hWVPMatrix(nullptr)
    , hWVPLMatrix(nullptr)
    , hWVPRMatrix(nullptr)
    , hVPMatrix(nullptr)
    , hWMatrix(nullptr)
    , hIWMatrix(nullptr)
    , hVMatrix(nullptr)
    , hWRotMatrix(nullptr)
    , hDiffuseMapMatrix(nullptr)
    , hNormalMapMatrix(nullptr)
    , hSpecularMapMatrix(nullptr)
    , hBoneMatrices(nullptr)
    , hDiffuseTEX(nullptr)
    , hSpecularTEX(nullptr)
    , hAmbientTEX(nullptr)
    , hNormalTEX(nullptr)
    , hEmissiveColor(nullptr)
    , hWCamLoc(nullptr)
    , hSpecularPower(nullptr)
    , hMODCOLOR(nullptr)
    , hTIME(nullptr)
    , hTopEnvTEX(nullptr)
    , hBotEnvTEX(nullptr) {
  if (gbenable) {
    miNumPasses = 1;
    mRasterState.SetShadeModel(ESHADEMODEL_SMOOTH);
    mRasterState.SetAlphaTest(EALPHATEST_GREATER, 0.5f);
    mRasterState.SetBlending(EBLENDING_OFF);
    mRasterState.SetDepthTest(EDEPTHTEST_LEQUALS);
    mRasterState.SetZWriteMask(true);
    mRasterState.SetCullTest(ECULLTEST_PASS_FRONT);
  }
}

///////////////////////////////////////////////////////////////////////////////

const orkmap<std::string, std::string> GfxMaterialWiiBasic::mBasicTekMap;
const orkmap<std::string, std::string> GfxMaterialWiiBasic::mPickTekMap;

void GfxMaterialWiiBasic::StaticInit() {
  if (gbenable) {
    orkmap<std::string, std::string>& BasicTekMap = const_cast<orkmap<std::string, std::string>&>(mBasicTekMap);
    orkmap<std::string, std::string>& PickTekMap  = const_cast<orkmap<std::string, std::string>&>(mPickTekMap);

    BasicTekMap["/pick"]                          = "tek_modcolor";
    BasicTekMap["/modvtx"]                        = "tek_wnormal";
    BasicTekMap["/modvtx/skinned"]                = "tek_wnormal_skinned";
    BasicTekMap["/lambert/tex"]                   = "tek_lamberttex";
    BasicTekMap["/lambert/tex/skinned"]           = "tek_lamberttex_skinned";
    BasicTekMap["/phong/tex/bump/skinned"]        = "tek_lamberttex_skinned";
    BasicTekMap["/lambert/tex/skinned/stereo"]    = "tek_lamberttex_skinned_stereo";
    BasicTekMap["/phong/tex/bump/skinned/stereo"] = "tek_lamberttex_skinned_stereo";

    PickTekMap["/modvtx"]                 = "tek_pick";
    PickTekMap["/modvtx/skinned"]         = "tek_pick";
    PickTekMap["/lambert/tex"]            = "tek_pick";
    PickTekMap["/lambert/tex/skinned"]    = "tek_pick";
    PickTekMap["/phong/tex/bump/skinned"] = "tek_pick";
  }
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialWiiBasic::Init(ork::lev2::GfxTarget* pTarg) {
  if (gbenable) {
    hModFX = asset::AssetManager<FxShaderAsset>::Load("orkshader://basic")->GetFxShader();

    //////////////////////////////////////////////////////////

    if (mBasicTechName == "") {
      std::string& btek = const_cast<std::string&>(mBasicTechName);
      btek              = "/modvtx";
    }

    printf("GfxMaterialWiiBasic<%p> mBasicTechName<%s>\n", this, mBasicTechName.c_str());

    orkmap<std::string, std::string>::const_iterator itb = mBasicTekMap.find(mBasicTechName);
    orkmap<std::string, std::string>::const_iterator itp = mPickTekMap.find(mBasicTechName);

    OrkAssert(itb != mBasicTekMap.end());
    OrkAssert(itp != mPickTekMap.end());

    printf("GfxMaterialWiiBasic<%p> btek<%s> ptek<%s>\n", this, itb->second.c_str(), itp->second.c_str());

    hTekModVtxTex       = pTarg->FXI()->technique(hModFX, itb->second);
    hTekModVtxTexStereo = pTarg->FXI()->technique(hModFX, (itb->second + "_stereo"));
    hTekMod             = pTarg->FXI()->technique(hModFX, itp->second);

    //////////////////////////////////////////
    // matrices

    hWMatrix  = pTarg->FXI()->parameter(hModFX, "WMatrix");
    hIWMatrix = pTarg->FXI()->parameter(hModFX, "IWMatrix");
    hVMatrix  = pTarg->FXI()->parameter(hModFX, "VMatrix");
    hMatP     = pTarg->FXI()->parameter(hModFX, "PMatrix");
    hMatMV    = pTarg->FXI()->parameter(hModFX, "WVMatrix");
    hVPMatrix = pTarg->FXI()->parameter(hModFX, "VPMatrix");

    hWVPMatrix  = pTarg->FXI()->parameter(hModFX, "WVPMatrix");
    hWVPLMatrix = pTarg->FXI()->parameter(hModFX, "WVPMatrixL");
    hWVPRMatrix = pTarg->FXI()->parameter(hModFX, "WVPMatrixR");

    hWRotMatrix        = pTarg->FXI()->parameter(hModFX, "WRotMatrix");
    hDiffuseMapMatrix  = pTarg->FXI()->parameter(hModFX, "DiffuseMapMatrix");
    hNormalMapMatrix   = pTarg->FXI()->parameter(hModFX, "NormalMapMatrix");
    hSpecularMapMatrix = pTarg->FXI()->parameter(hModFX, "SpecularMapMatrix");

    hBoneMatrices = pTarg->FXI()->parameter(hModFX, "BoneMatrices");

    //////////////////////////////////////////
    // Textures

    hDiffuseTEX  = pTarg->FXI()->parameter(hModFX, "DiffuseMap");
    hSpecularTEX = pTarg->FXI()->parameter(hModFX, "SpecularMap");
    hAmbientTEX  = pTarg->FXI()->parameter(hModFX, "AmbientMap");
    hNormalTEX   = pTarg->FXI()->parameter(hModFX, "NormalMap");

    hTopEnvTEX = pTarg->FXI()->parameter(hModFX, "TopEnvMap");
    hBotEnvTEX = pTarg->FXI()->parameter(hModFX, "BotEnvMap");

    //////////////////////////////////////////
    // data driven material props

    hEmissiveColor = pTarg->FXI()->parameter(hModFX, "EmissiveColor");

    //////////////////////////////////////////
    // misc

    hWCamLoc       = pTarg->FXI()->parameter(hModFX, "WCamLoc");
    hSpecularPower = pTarg->FXI()->parameter(hModFX, "SpecularPower");
    hMODCOLOR      = pTarg->FXI()->parameter(hModFX, "modcolor");
    hTIME          = pTarg->FXI()->parameter(hModFX, "time");

    mLightingInterface.Init(hModFX);
  }
}

///////////////////////////////////////////////////////////////////////////////

static fmtx4 BuildTextureMatrix(const TextureContext& Ctx) {
  fmtx4 MapMatrix;
  MapMatrix.Scale(Ctx.mfRepeatU, Ctx.mfRepeatV, 1.0f);
  return MapMatrix;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static ork::fixed_pool<WiiMatrixBlockApplicator, 1> MtxBlockApplicators;
static ork::fixed_pool<WiiMatrixApplicator, 256> MtxApplicators;

///////////////////////////////////////////////////////////////////////////////

WiiMatrixBlockApplicator::WiiMatrixBlockApplicator(MaterialInstItemMatrixBlock* mtxblockitem, const GfxMaterialWiiBasic* pmat)
    : mMatrixBlockItem(mtxblockitem)
    , mMaterial(pmat) {}

///////////////////////////////////////////////////////////////////////////////

void WiiMatrixBlockApplicator::ApplyToTarget(GfxTarget* pTARG) // virtual
{
  auto fxi  = pTARG->FXI();
  auto mtxi = pTARG->MTXI();

  size_t inumbones      = mMatrixBlockItem->GetNumMatrices();
  const fmtx4* Matrices = mMatrixBlockItem->GetMatrices();
  FxShader* hshader     = mMaterial->hModFX;

  fxi->BindParamMatrix(hshader, mMaterial->hMatMV, mtxi->RefMVMatrix());
  fxi->BindParamMatrix(hshader, mMaterial->hWVPMatrix, mtxi->RefMVPMatrix());
  fxi->BindParamMatrix(hshader, mMaterial->hWMatrix, mtxi->RefMMatrix());

  fmtx4 iwmat;
  iwmat.inverseOf(mtxi->RefMVMatrix());
  fxi->BindParamMatrix(hshader, mMaterial->hIWMatrix, iwmat);
  fxi->BindParamMatrixArray(hshader, mMaterial->hBoneMatrices, Matrices, (int)inumbones);
  fxi->CommitParams();
}

///////////////////////////////////////////////////////////////////////////////

WiiMatrixApplicator::WiiMatrixApplicator(MaterialInstItemMatrix* mtxitem, const GfxMaterialWiiBasic* pmat)
    : mMatrixItem(mtxitem)
    , mMaterial(pmat) {}

void WiiMatrixApplicator::ApplyToTarget(GfxTarget* pTARG) {
  const fmtx4& mtx  = mMatrixItem->GetMatrix();
  FxShader* hshader = mMaterial->hModFX;
  pTARG->FXI()->BindParamMatrix(hshader, mMaterial->hDiffuseMapMatrix, mtx);
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialWiiBasic::BindMaterialInstItem(MaterialInstItem* pitem) const {
  ///////////////////////////////////

  MaterialInstItemMatrixBlock* mtxblockitem = rtti::autocast(pitem);

  if (mtxblockitem) {
    if (hBoneMatrices->GetPlatformHandle()) {
      WiiMatrixBlockApplicator* pyo = MtxBlockApplicators.allocate();
      OrkAssert(pyo != 0);
      new (pyo) WiiMatrixBlockApplicator(mtxblockitem, this);
      mtxblockitem->SetApplicator(pyo);
    }
    return;
  }

  ///////////////////////////////////

  MaterialInstItemMatrix* mtxitem = rtti::autocast(pitem);

  if (mtxitem) {
    WiiMatrixApplicator* pyo = MtxApplicators.allocate();
    OrkAssert(pyo != 0);
    new (pyo) WiiMatrixApplicator(mtxitem, this);
    mtxitem->SetApplicator(pyo);
    return;
  }

  ///////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialWiiBasic::UnBindMaterialInstItem(MaterialInstItem* pitem) const {
  ///////////////////////////////////

  MaterialInstItemMatrixBlock* mtxblockitem = rtti::autocast(pitem);

  if (mtxblockitem) {
    if (hBoneMatrices->GetPlatformHandle()) {
      WiiMatrixBlockApplicator* wiimtxblkapp = rtti::autocast(mtxblockitem->mApplicator);
      if (wiimtxblkapp) {
        MtxBlockApplicators.deallocate(wiimtxblkapp);
      }
    }
    return;
  }

  ///////////////////////////////////

  MaterialInstItemMatrix* mtxitem = rtti::autocast(pitem);

  if (mtxitem) {
    WiiMatrixApplicator* wiimtxapp = rtti::autocast(mtxitem->mApplicator);
    if (wiimtxapp) {
      MtxApplicators.deallocate(wiimtxapp);
    }
    return;
  }

  ///////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialWiiBasic::UpdateMVPMatrix(GfxTarget* pTarg) {
  auto FXI  = pTarg->FXI();
  auto MTXI = pTarg->MTXI();
  FXI->BindParamMatrix(hModFX, hMatMV, MTXI->RefMVMatrix());
  FXI->BindParamMatrix(hModFX, hWVPMatrix, MTXI->RefMVPMatrix());
  FXI->BindParamMatrix(hModFX, hWMatrix, MTXI->RefMMatrix());
  FXI->BindParamMatrix(hModFX, hWRotMatrix, MTXI->RefR3Matrix());
  FXI->CommitParams();
}

///////////////////////////////////////////////////////////////////////////////

bool GfxMaterialWiiBasic::BeginPass(GfxTarget* pTarg, int iPass) {

  auto FXI  = pTarg->FXI();
  auto RSI  = pTarg->RSI();
  auto MTXI = pTarg->MTXI();

  const RenderContextInstData* RCID  = pTarg->GetRenderContextInstData();
  const RenderContextFrameData* RCFD = pTarg->topRenderContextFrameData();
  const auto& CPD = RCFD->topCPD();

  bool bforcenoz = RCID->IsForceNoZWrite();

  const ork::lev2::XgmMaterialStateInst* matinst = RCID->GetMaterialInst();

  bool is_picking                  = CPD.isPicking();

  const TextureContext& DiffuseCtx = GetTexture(ETEXDEST_DIFFUSE);

  const Texture* diftexture = DiffuseCtx.mpTexture;

  fcolor4 ModColor = pTarg->RefModColor();

  mRasterState.SetZWriteMask(!bforcenoz);

  if (is_picking) {
    auto copyofrasterstate = mRasterState;
    copyofrasterstate.SetZWriteMask(true);
    copyofrasterstate.SetDepthTest(EDEPTHTEST_LEQUALS);
    copyofrasterstate.SetCullTest(ECULLTEST_PASS_BACK);
    copyofrasterstate.SetBlending(EBLENDING_OFF);
    copyofrasterstate.SetAlphaTest(EALPHATEST_OFF);
    RSI->BindRasterState(copyofrasterstate);
  } else {
    RSI->BindRasterState(mRasterState);
  }

  FXI->BindPass(hModFX, iPass);
  FXI->BindParamMatrix(hModFX, hMatMV, MTXI->RefMVMatrix());
  FXI->BindParamMatrix(hModFX, hMatP, MTXI->RefPMatrix());
  FXI->BindParamMatrix(hModFX, hVPMatrix, MTXI->RefVPMatrix());
  FXI->BindParamMatrix(hModFX, hVMatrix, MTXI->RefVMatrix());
  FXI->BindParamMatrix(hModFX, hWMatrix, MTXI->RefMMatrix());

  FXI->BindParamMatrix(hModFX, hWRotMatrix, MTXI->RefR3Matrix());
  FXI->BindParamMatrix(hModFX, hDiffuseMapMatrix, BuildTextureMatrix(DiffuseCtx));

  FXI->BindParamVect4(hModFX, hMODCOLOR, ModColor);

  FXI->BindParamCTex(hModFX, hDiffuseTEX, diftexture);

  const auto& world = MTXI->RefMMatrix();
  if (CPD.isStereoOnePass() and CPD._stereoCameraMatrices) {
    auto stereomtx = CPD._stereoCameraMatrices;
    auto MVPL = stereomtx->MVPL(world);
    auto MVPR = stereomtx->MVPR(world);
    FXI->BindParamMatrix(hModFX, hWVPLMatrix, MVPL);
    FXI->BindParamMatrix(hModFX, hWVPRMatrix, MVPR);
  } else {
    auto mcams = CPD._cameraMatrices;
    auto MVP = world
             * mcams->_vmatrix
             * mcams->_pmatrix;
    FXI->BindParamMatrix(hModFX, hWVPMatrix, MVP);
  }
  ////////////////////////////////////////////////////////////

  if (matinst) {
    int inumitems = matinst->GetNumItems();

    if (inumitems) {
      for (int ii = 0; ii < inumitems; ii++) {
        MaterialInstItem* item       = matinst->GetItem(ii);
        MaterialInstApplicator* appl = item->mApplicator;
        if (appl) {
          appl->ApplyToTarget(pTarg);
        }
      }
    }
  }

  ////////////////////////////////////////////////////////////

  mLightingInterface.ApplyLighting(pTarg, iPass);

  FXI->CommitParams();

  return true;
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialWiiBasic::EndPass(GfxTarget* pTarg) { pTarg->FXI()->EndPass(hModFX); }

///////////////////////////////////////////////////////////////////////////////

int GfxMaterialWiiBasic::BeginBlock(GfxTarget* pTarg, const RenderContextInstData& RCID) {
  mRenderContexInstData              = &RCID;
  const RenderContextFrameData* RCFD = pTarg->topRenderContextFrameData();
  const auto& CPD = RCFD->topCPD();


  const fvec3& znormal = CPD.monoCamZnormal();

  bool is_picking                    = CPD.isPicking();
  bool is_stereo                     = CPD.isStereoOnePass();

  const FxShaderTechnique* tek = hTekModVtxTex;

  if (is_picking)
    tek = hTekMod;
  else if (is_stereo) {
    tek = hTekModVtxTexStereo;
  }

  pTarg->FXI()->BindTechnique(hModFX, tek);
  int inumpasses = pTarg->FXI()->BeginBlock(hModFX, RCID);

  mScreenZDir = znormal;

  return inumpasses;
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialWiiBasic::EndBlock(GfxTarget* pTarg) {
  pTarg->FXI()->EndBlock(hModFX);
  mRenderContexInstData = 0;
}

}} // namespace ork::lev2
