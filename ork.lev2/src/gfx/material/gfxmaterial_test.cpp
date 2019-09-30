////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/pch.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::GfxMaterial3DSolid, "MaterialSolid")

namespace ork { namespace lev2 {

/////////////////////////////////////////////////////////////////////////

void GfxMaterial3DSolid::Describe() {}

/////////////////////////////////////////////////////////////////////////

bool gearlyhack = true;

GfxMaterial3DSolid::GfxMaterial3DSolid(GfxTarget* pTARG)
    : meColorMode(EMODE_MOD_COLOR) {
  mRasterState.SetShadeModel(ESHADEMODEL_SMOOTH);
  mRasterState.SetAlphaTest(EALPHATEST_OFF);
  mRasterState.SetBlending(EBLENDING_OFF);
  mRasterState.SetDepthTest(EDEPTHTEST_LEQUALS);
  mRasterState.SetZWriteMask(true);
  mRasterState.SetCullTest(ECULLTEST_OFF);

  miNumPasses = 1;

  if (false == gearlyhack) {
    hModFX = asset::AssetManager<FxShaderAsset>::Load("orkshader://solid")->GetFxShader();
  }

  if (pTARG) {
    Init(pTARG);
  }
}

GfxMaterial3DSolid::GfxMaterial3DSolid(
    GfxTarget* pTARG, const char* puserfx, const char* pusertek, bool allowcompilefailure, bool unmanaged)
    : meColorMode(EMODE_USER)
    , mUserFxName(puserfx)
    , mUserTekName(pusertek)
    , mUnManaged(unmanaged)
    , mAllowCompileFailure(allowcompilefailure) {

  mRasterState.SetShadeModel(ESHADEMODEL_SMOOTH);
  mRasterState.SetAlphaTest(EALPHATEST_OFF);
  mRasterState.SetBlending(EBLENDING_OFF);
  mRasterState.SetDepthTest(EDEPTHTEST_LEQUALS);
  mRasterState.SetZWriteMask(true);
  mRasterState.SetCullTest(ECULLTEST_OFF);

  miNumPasses = 1;

  if (pTARG) {
    Init(pTARG);
  } else {
    FxShaderAsset* passet = nullptr;

    if (mUnManaged)
      passet = asset::AssetManager<FxShaderAsset>::LoadUnManaged(mUserFxName.c_str());
    else
      passet = asset::AssetManager<FxShaderAsset>::Load(mUserFxName.c_str());

    hModFX = passet ? passet->GetFxShader() : 0;

    if (hModFX)
      hModFX->SetAllowCompileFailure(mAllowCompileFailure);
  }
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterial3DSolid::Init(ork::lev2::GfxTarget* pTarg) {

  auto fxi = pTarg->FXI();

  if (mUserFxName.length()) {
    FxShaderAsset* passet = nullptr;

    if (mUnManaged)
      passet = asset::AssetManager<FxShaderAsset>::LoadUnManaged(mUserFxName.c_str());
    else
      passet = asset::AssetManager<FxShaderAsset>::Load(mUserFxName.c_str());

    hModFX = passet ? passet->GetFxShader() : 0;

    if (hModFX)
      hModFX->SetAllowCompileFailure(mAllowCompileFailure);

  } else {
    // orkprintf( "Attempting to Load Shader<orkshader://solid>\n" );
    hModFX = asset::AssetManager<FxShaderAsset>::Load("orkshader://solid")->GetFxShader();
  }
  if (0 == hModFX) {
    return;
  }
  if (mUserTekName.length()) {
    hTekUser       = fxi->GetTechnique(hModFX, mUserTekName);
    hTekUserStereo = fxi->GetTechnique(hModFX, mUserTekName + "_stereo");
  }
  if (meColorMode != EMODE_USER) {
    hTekVertexColor    = fxi->GetTechnique(hModFX, "vtxcolor");
    hTekVertexModColor = fxi->GetTechnique(hModFX, "vtxmodcolor");
    hTekModColor       = fxi->GetTechnique(hModFX, "mmodcolor");
    hTekTexColor       = fxi->GetTechnique(hModFX, "texcolor");
    hTekTexModColor    = fxi->GetTechnique(hModFX, "texmodcolor");
    hTekTexTexModColor = fxi->GetTechnique(hModFX, "textexmodcolor");
    hTekTexVertexColor = fxi->GetTechnique(hModFX, "texvtxcolor");
  }

  hTekPick = fxi->GetTechnique(hModFX, "tek_pick");

  hMatAux = fxi->GetParameterH(hModFX, "MatAux");
  hMatRot = fxi->GetParameterH(hModFX, "MatRotW");

  hMatMVPL       = fxi->GetParameterH(hModFX, "MatMVPL");
  hMatMVPR       = fxi->GetParameterH(hModFX, "MatMVPR");
  hMatMVPC       = fxi->GetParameterH(hModFX, "MatMVPC");
  hMatMVP        = fxi->GetParameterH(hModFX, "MatMVP");
  hMatMV         = fxi->GetParameterH(hModFX, "MatMV");
  hMatV          = fxi->GetParameterH(hModFX, "MatV");
  hMatM          = fxi->GetParameterH(hModFX, "MatM");
  hMatP          = fxi->GetParameterH(hModFX, "MatP");
  hParamModColor = fxi->GetParameterH(hModFX, "modcolor");

  hVolumeMap = fxi->GetParameterH(hModFX, "VolumeMap");
  hColorMap  = fxi->GetParameterH(hModFX, "ColorMap");
  hColorMap2 = fxi->GetParameterH(hModFX, "ColorMap2");
  hColorMap3 = fxi->GetParameterH(hModFX, "ColorMap3");
  hColorMap4 = fxi->GetParameterH(hModFX, "ColorMap4");

  hParamUser0 = fxi->GetParameterH(hModFX, "User0");
  hParamUser1 = fxi->GetParameterH(hModFX, "User1");
  hParamUser2 = fxi->GetParameterH(hModFX, "User2");
  hParamUser3 = fxi->GetParameterH(hModFX, "User3");

  hParamTime = fxi->GetParameterH(hModFX, "Time");

  hParamNoiseAmp   = fxi->GetParameterH(hModFX, "NoiseAmp");
  hParamNoiseFreq  = fxi->GetParameterH(hModFX, "NoiseFreq");
  hParamNoiseShift = fxi->GetParameterH(hModFX, "NoiseShift");
}

/////////////////////////////////////////////////////////////////////////

bool GfxMaterial3DSolid::IsUserFxOk() const {
  if (meColorMode == EMODE_USER)
    return (hTekUser != nullptr);
  return false;
}

/////////////////////////////////////////////////////////////////////////

int GfxMaterial3DSolid::BeginBlock(GfxTarget* pTarg, const RenderContextInstData& RCID) {

  const RenderContextFrameData* RCFD = pTarg->topRenderContextFrameData();
  const auto& CPD = RCFD->topCPD();
  bool is_picking                    = CPD.isPicking();
  bool is_stereo                     = CPD.isStereoOnePass();

  if (is_picking and _enablePick and hTekPick) {
    pTarg->FXI()->BindTechnique(hModFX, hTekPick);
  } else
    switch (meColorMode) {
      case EMODE_VERTEX_COLOR:
        pTarg->FXI()->BindTechnique(hModFX, hTekVertexColor);
        break;
      case EMODE_VERTEXMOD_COLOR:
        pTarg->FXI()->BindTechnique(hModFX, hTekVertexModColor);
        break;
      case EMODE_MOD_COLOR:
        pTarg->FXI()->BindTechnique(hModFX, hTekModColor);
        break;
      case EMODE_INTERNAL_COLOR:
        pTarg->FXI()->BindTechnique(hModFX, hTekModColor);
        break;
      case EMODE_TEX_COLOR:
        pTarg->FXI()->BindTechnique(hModFX, hTekTexColor);
        break;
      case EMODE_TEXMOD_COLOR:
        pTarg->FXI()->BindTechnique(hModFX, hTekTexModColor);
        break;
      case EMODE_TEXTEXMOD_COLOR:
        pTarg->FXI()->BindTechnique(hModFX, hTekTexTexModColor);
        break;
      case EMODE_TEXVERTEX_COLOR:
        pTarg->FXI()->BindTechnique(hModFX, hTekTexVertexColor);
        break;
      case EMODE_USER:
        pTarg->FXI()->BindTechnique(hModFX, is_stereo ? hTekUserStereo : hTekUser);
        break;
    }
  int inumpasses = pTarg->FXI()->BeginBlock(hModFX, RCID);
  return inumpasses;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterial3DSolid::EndBlock(GfxTarget* pTarg) { pTarg->FXI()->EndBlock(hModFX); }

/////////////////////////////////////////////////////////////////////////

static bool gbskip = false;

bool GfxMaterial3DSolid::BeginPass(GfxTarget* pTarg, int iPass) {
  if (gbskip)
    return false;

  const RenderContextInstData* RCID  = pTarg->GetRenderContextInstData();
  const RenderContextFrameData* RCFD = pTarg->topRenderContextFrameData();
  const auto& CPD = RCFD->topCPD();
  bool is_picking                    = CPD.isPicking();
  bool is_stereo                     = CPD.isStereoOnePass();
  bool is_forcenoz                   = RCID ? RCID->IsForceNoZWrite() : false;

  pTarg->RSI()->BindRasterState(mRasterState);
  pTarg->FXI()->BindPass(hModFX, iPass);

  if (hModFX->GetFailedCompile()){
    assert(false);
    return false;
  }

  auto MTXI = pTarg->MTXI();
  auto FXI  = pTarg->FXI();

  FXI->BindParamMatrix(hModFX, hMatM, MTXI->RefMMatrix());
  FXI->BindParamMatrix(hModFX, hMatMV, MTXI->RefMVMatrix());
  FXI->BindParamMatrix(hModFX, hMatP, MTXI->RefPMatrix());

  const auto& world = MTXI->RefMMatrix();
  if (is_stereo and CPD._stereoCameraMatrices) {
    auto stereomtx = CPD._stereoCameraMatrices;
    auto MVPL = stereomtx->MVPL(world);
    auto MVPR = stereomtx->MVPR(world);
    FXI->BindParamMatrix(hModFX, hMatMVPL, MVPL);
    FXI->BindParamMatrix(hModFX, hMatMVPR, MVPR);
  } else if (CPD._cameraMatrices) {
    auto mcams = CPD._cameraMatrices;
    auto MVP = world
             * mcams->_vmatrix
             * mcams->_pmatrix;
    FXI->BindParamMatrix(hModFX, hMatMVP, MVP);
  } else {
    FXI->BindParamMatrix(hModFX, hMatMVP, MTXI->RefMVPMatrix());
  }

  FXI->BindParamMatrix(hModFX, hMatAux, mMatAux);

  if (hMatV) {
    FXI->BindParamMatrix(hModFX, hMatV, MTXI->RefVMatrix());
  }

  if (hMatRot)
    FXI->BindParamMatrix(hModFX, hMatRot, MTXI->RefR3Matrix());

  if (pTarg->FBI()->IsPickState()) {
    FXI->BindParamVect4(hModFX, hParamModColor, pTarg->RefModColor());
  } else {
    if (meColorMode == EMODE_INTERNAL_COLOR) {
      FXI->BindParamVect4(hModFX, hParamModColor, Color);
    } else {
      FXI->BindParamVect4(hModFX, hParamModColor, pTarg->RefModColor());
    }
  }

  if (hParamNoiseAmp) {
    FXI->BindParamVect4(hModFX, hParamNoiseAmp, mNoiseAmp);
  }
  if (hParamNoiseFreq) {
    FXI->BindParamVect4(hModFX, hParamNoiseFreq, mNoiseFreq);
  }
  if (hParamNoiseShift) {
    FXI->BindParamVect4(hModFX, hParamNoiseShift, mNoiseShift);
  }

  if (hParamTime) {
    float reltime = fmodf(OldSchool::GetRef().GetLoResRelTime(), 300.0f);
    // printf( "reltime<%f>\n", reltime );
    FXI->BindParamFloat(hModFX, hParamTime, reltime);
  }

  if (hParamUser0) {
    FXI->BindParamVect4(hModFX, hParamUser0, mUser0);
  }
  if (hParamUser1) {
    FXI->BindParamVect4(hModFX, hParamUser1, mUser1);
  }
  if (hParamUser2) {
    FXI->BindParamVect4(hModFX, hParamUser2, mUser2);
  }
  if (hParamUser3) {
    FXI->BindParamVect4(hModFX, hParamUser3, mUser3);
  }

  if (mVolumeTexture && hVolumeMap) {
    FXI->BindParamCTex(hModFX, hVolumeMap, mVolumeTexture);
  }

  if (mCurrentTexture && hColorMap) {
    if (IsDebug())
      printf("Binding texmap<%p> to param<%p>\n", mCurrentTexture, hColorMap);
    FXI->BindParamCTex(hModFX, hColorMap, mCurrentTexture);
  }
  if (mCurrentTexture2 && hColorMap2) {
    // printf( "Binding texmap2<%p> to param<%p>\n", mCurrentTexture2, hColorMap2 );
    FXI->BindParamCTex(hModFX, hColorMap2, mCurrentTexture2);
  }

  if (mCurrentTexture3 && hColorMap3) {
    FXI->BindParamCTex(hModFX, hColorMap3, mCurrentTexture3);
  }

  if (mCurrentTexture4 && hColorMap4) {
    FXI->BindParamCTex(hModFX, hColorMap4, mCurrentTexture4);
  }

  FXI->CommitParams();
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

void GfxMaterial3DSolid::EndPass(GfxTarget* pTarg) {
  if (false == gbskip)
    pTarg->FXI()->EndPass(hModFX);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

void GfxMaterial3DSolid::SetMaterialProperty(const char* prop, const char* val) // virtual
{
  ////////////////////////////////////////////////
  // colormode
  ////////////////////////////////////////////////
  if (0 == strcmp(prop, "colormode")) {
    if (0 == strcmp(val, "EMODE_INTERNAL_COLOR")) {
      meColorMode = EMODE_INTERNAL_COLOR;
    }
  }
  ////////////////////////////////////////////////
  // colormode
  ////////////////////////////////////////////////
  if (0 == strcmp(prop, "color")) {
    if ((strlen(val) == 9) && (val[0] == '#')) {
      struct hexchar2int {
        static int doit(const char ch) {
          if ((ch >= 'a') && (ch <= 'f')) {
            return 10 + (ch - 'a');
          } else if ((ch >= '0') && (ch <= '9')) {
            return (ch - '0');
          } else {
            OrkAssert(false);
            return -1;
          }
        }
      };
      char hexd0 = val[1];
      char hexd1 = val[2];
      char hexd2 = val[3];
      char hexd3 = val[4];
      char hexd4 = val[5];
      char hexd5 = val[6];
      char hexd6 = val[7];
      char hexd7 = val[8];

      u32 ucolor = 0;
      ucolor |= hexchar2int::doit(hexd7) << 0;
      ucolor |= hexchar2int::doit(hexd6) << 4;
      ucolor |= hexchar2int::doit(hexd5) << 8;
      ucolor |= hexchar2int::doit(hexd4) << 12;
      ucolor |= hexchar2int::doit(hexd3) << 16;
      ucolor |= hexchar2int::doit(hexd2) << 20;
      ucolor |= hexchar2int::doit(hexd1) << 24;
      ucolor |= hexchar2int::doit(hexd0) << 28;
      printf("color<0x%08x>\n", ucolor);

      Color = fvec4(ucolor);
      printf("color<%f %f %f %f>\n", Color.GetX(), Color.GetY(), Color.GetZ(), Color.GetW());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
