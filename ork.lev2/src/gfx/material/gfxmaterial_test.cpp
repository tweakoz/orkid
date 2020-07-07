////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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

void GfxMaterial3DSolid::Describe() {
}

/////////////////////////////////////////////////////////////////////////

bool gearlyhack = true;

GfxMaterial3DSolid::GfxMaterial3DSolid(Context* pTARG)
    : meColorMode(EMODE_MOD_COLOR) {
  _rasterstate.SetShadeModel(ESHADEMODEL_SMOOTH);
  _rasterstate.SetAlphaTest(EALPHATEST_OFF);
  _rasterstate.SetBlending(Blending::OFF);
  _rasterstate.SetDepthTest(EDEPTHTEST_LEQUALS);
  _rasterstate.SetZWriteMask(true);
  _rasterstate.SetCullTest(ECULLTEST_OFF);

  miNumPasses = 1;

  if (false == gearlyhack) {
    _shaderasset = asset::AssetManager<FxShaderAsset>::load("orkshader://solid");
    _shader      = _shaderasset->GetFxShader();
  }

  if (pTARG) {
    gpuInit(pTARG);
  }
}

GfxMaterial3DSolid::GfxMaterial3DSolid(Context* pTARG, const char* puserfx, const char* pusertek, bool allowcompilefailure)
    : meColorMode(EMODE_USER)
    , mUserFxName(puserfx)
    , mUserTekName(pusertek)
    , mAllowCompileFailure(allowcompilefailure) {

  _rasterstate.SetShadeModel(ESHADEMODEL_SMOOTH);
  _rasterstate.SetAlphaTest(EALPHATEST_OFF);
  _rasterstate.SetBlending(Blending::OFF);
  _rasterstate.SetDepthTest(EDEPTHTEST_LEQUALS);
  _rasterstate.SetZWriteMask(true);
  _rasterstate.SetCullTest(ECULLTEST_OFF);

  miNumPasses = 1;

  if (pTARG) {
    gpuInit(pTARG);
  } else {
    std::shared_ptr<FxShaderAsset> fxshaderasset;
    _shaderasset = asset::AssetManager<FxShaderAsset>::load(mUserFxName.c_str());
    _shader      = _shaderasset->GetFxShader();

    if (_shader)
      _shader->SetAllowCompileFailure(mAllowCompileFailure);
  }
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterial3DSolid::gpuInit(ork::lev2::Context* pTarg) {

  auto fxi = pTarg->FXI();

  if (mUserFxName.length()) {
    _shaderasset = asset::AssetManager<FxShaderAsset>::load(mUserFxName.c_str());

    _shader = _shaderasset ? _shaderasset->GetFxShader() : 0;

    if (_shader)
      _shader->SetAllowCompileFailure(mAllowCompileFailure);

  } else {
    // orkprintf( "Attempting to Load Shader<orkshader://solid>\n" );
    _shaderasset = asset::AssetManager<FxShaderAsset>::load("orkshader://solid");
    _shader      = _shaderasset->GetFxShader();
  }
  if (0 == _shader) {
    return;
  }
  if (mUserTekName.length()) {
    hTekUser       = fxi->technique(_shader, mUserTekName);
    hTekUserStereo = fxi->technique(_shader, mUserTekName + "_stereo");
  }
  if (meColorMode != EMODE_USER) {
    hTekVertexColor    = fxi->technique(_shader, "vtxcolor");
    hTekVertexModColor = fxi->technique(_shader, "vtxmodcolor");
    hTekModColor       = fxi->technique(_shader, "mmodcolor");
    hTekTexColor       = fxi->technique(_shader, "texcolor");
    hTekTexColorStereo = fxi->technique(_shader, "texcolorstereo");
    hTekTexModColor    = fxi->technique(_shader, "texmodcolor");
    hTekTexTexModColor = fxi->technique(_shader, "textexmodcolor");
    hTekTexVertexColor = fxi->technique(_shader, "texvtxcolor");
  }

  hTekPick = fxi->technique(_shader, "tek_pick");

  hMatAux  = fxi->parameter(_shader, "MatAux");
  hMatAux2 = fxi->parameter(_shader, "MatAux2");
  hMatRot  = fxi->parameter(_shader, "MatRotW");

  hMatMVPL       = fxi->parameter(_shader, "MatMVPL");
  hMatMVPR       = fxi->parameter(_shader, "MatMVPR");
  hMatMVPC       = fxi->parameter(_shader, "MatMVPC");
  hMatMVP        = fxi->parameter(_shader, "MatMVP");
  hMatMV         = fxi->parameter(_shader, "MatMV");
  hMatV          = fxi->parameter(_shader, "MatV");
  hMatM          = fxi->parameter(_shader, "MatM");
  hMatP          = fxi->parameter(_shader, "MatP");
  hParamModColor = fxi->parameter(_shader, "modcolor");

  hVolumeMap = fxi->parameter(_shader, "VolumeMap");
  hColorMap  = fxi->parameter(_shader, "ColorMap");
  hColorMap2 = fxi->parameter(_shader, "ColorMap2");
  hColorMap3 = fxi->parameter(_shader, "ColorMap3");
  hColorMap4 = fxi->parameter(_shader, "ColorMap4");

  hParamUser0 = fxi->parameter(_shader, "User0");
  hParamUser1 = fxi->parameter(_shader, "User1");
  hParamUser2 = fxi->parameter(_shader, "User2");
  hParamUser3 = fxi->parameter(_shader, "User3");

  hParamTime = fxi->parameter(_shader, "Time");

  hParamNoiseAmp   = fxi->parameter(_shader, "NoiseAmp");
  hParamNoiseFreq  = fxi->parameter(_shader, "NoiseFreq");
  hParamNoiseShift = fxi->parameter(_shader, "NoiseShift");
}

/////////////////////////////////////////////////////////////////////////

bool GfxMaterial3DSolid::IsUserFxOk() const {
  if (meColorMode == EMODE_USER)
    return (hTekUser != nullptr);
  return false;
}

/////////////////////////////////////////////////////////////////////////

int GfxMaterial3DSolid::BeginBlock(Context* pTarg, const RenderContextInstData& RCID) {

  const RenderContextFrameData* RCFD = pTarg->topRenderContextFrameData();
  const auto& CPD                    = RCFD->topCPD();
  bool is_picking                    = CPD.isPicking();
  bool is_stereo                     = CPD.isStereoOnePass();

  if (is_picking and _enablePick and hTekPick) {
    return pTarg->FXI()->BeginBlock(hTekPick, RCID);
  } else
    switch (meColorMode) {
      case EMODE_VERTEX_COLOR:
        return pTarg->FXI()->BeginBlock(hTekVertexColor, RCID);
        break;
      case EMODE_VERTEXMOD_COLOR:
        return pTarg->FXI()->BeginBlock(hTekVertexModColor, RCID);
        break;
      case EMODE_MOD_COLOR:
        return pTarg->FXI()->BeginBlock(hTekModColor, RCID);
        break;
      case EMODE_INTERNAL_COLOR:
        return pTarg->FXI()->BeginBlock(hTekModColor, RCID);
        break;
      case EMODE_TEX_COLOR:
        return pTarg->FXI()->BeginBlock(is_stereo ? hTekTexColorStereo : hTekTexColor, RCID);
        break;
      case EMODE_TEXMOD_COLOR:
        return pTarg->FXI()->BeginBlock(hTekTexModColor, RCID);
        break;
      case EMODE_TEXTEXMOD_COLOR:
        return pTarg->FXI()->BeginBlock(hTekTexTexModColor, RCID);
        break;
      case EMODE_TEXVERTEX_COLOR:
        return pTarg->FXI()->BeginBlock(hTekTexVertexColor, RCID);
        break;
      case EMODE_USER:
        return pTarg->FXI()->BeginBlock(is_stereo ? hTekUserStereo : hTekUser, RCID);
        break;
    }
  return 0;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterial3DSolid::EndBlock(Context* pTarg) {
  pTarg->FXI()->EndBlock();
}

/////////////////////////////////////////////////////////////////////////

static bool gbskip = false;

bool GfxMaterial3DSolid::BeginPass(Context* pTarg, int iPass) {
  if (gbskip)
    return false;

  const RenderContextInstData* RCID  = pTarg->GetRenderContextInstData();
  const RenderContextFrameData* RCFD = pTarg->topRenderContextFrameData();
  const auto& CPD                    = RCFD->topCPD();
  bool is_picking                    = CPD.isPicking();
  bool is_stereo                     = CPD.isStereoOnePass();

  pTarg->RSI()->BindRasterState(_rasterstate);
  pTarg->FXI()->BindPass(iPass);

  if (_shader->GetFailedCompile()) {
    assert(false);
    return false;
  }

  auto MTXI = pTarg->MTXI();
  auto FXI  = pTarg->FXI();

  FXI->BindParamMatrix(hMatM, MTXI->RefMMatrix());
  FXI->BindParamMatrix(hMatMV, MTXI->RefMVMatrix());
  FXI->BindParamMatrix(hMatP, MTXI->RefPMatrix());

  const auto& world = MTXI->RefMMatrix();
  if (is_stereo and CPD._stereoCameraMatrices) {
    auto stereomtx = CPD._stereoCameraMatrices;
    auto MVPL      = stereomtx->MVPL(world);
    auto MVPR      = stereomtx->MVPR(world);
    FXI->BindParamMatrix(hMatMVPL, MVPL);
    FXI->BindParamMatrix(hMatMVPR, MVPR);
  } else if (CPD._cameraMatrices) {
    auto mcams = CPD._cameraMatrices;
    auto MVP   = world * mcams->_vmatrix * mcams->_pmatrix;
    FXI->BindParamMatrix(hMatMVP, MVP);
  } else {
    auto MVP = MTXI->RefMVPMatrix();
    FXI->BindParamMatrix(hMatMVP, MVP);
  }

  if (hMatAux)
    FXI->BindParamMatrix(hMatAux, mMatAux);

  if (hMatAux2)
    FXI->BindParamMatrix(hMatAux2, mMatAux2);

  if (hMatV) {
    FXI->BindParamMatrix(hMatV, MTXI->RefVMatrix());
  }

  if (hMatRot)
    FXI->BindParamMatrix(hMatRot, MTXI->RefR3Matrix());

  if (pTarg->FBI()->isPickState()) {
    FXI->BindParamVect4(hParamModColor, pTarg->RefModColor());
  } else {
    if (meColorMode == EMODE_INTERNAL_COLOR) {
      FXI->BindParamVect4(hParamModColor, Color);
    } else {
      FXI->BindParamVect4(hParamModColor, pTarg->RefModColor());
    }
  }

  if (hParamNoiseAmp) {
    FXI->BindParamVect4(hParamNoiseAmp, mNoiseAmp);
  }
  if (hParamNoiseFreq) {
    FXI->BindParamVect4(hParamNoiseFreq, mNoiseFreq);
  }
  if (hParamNoiseShift) {
    FXI->BindParamVect4(hParamNoiseShift, mNoiseShift);
  }

  if (hParamTime) {
    float reltime = fmodf(OldSchool::GetRef().GetLoResRelTime(), 300.0f);
    // printf( "reltime<%f>\n", reltime );
    FXI->BindParamFloat(hParamTime, reltime);
  }

  if (hParamUser0) {
    FXI->BindParamVect4(hParamUser0, mUser0);
  }
  if (hParamUser1) {
    FXI->BindParamVect4(hParamUser1, mUser1);
  }
  if (hParamUser2) {
    FXI->BindParamVect4(hParamUser2, mUser2);
  }
  if (hParamUser3) {
    FXI->BindParamVect4(hParamUser3, mUser3);
  }

  if (mVolumeTexture && hVolumeMap) {
    FXI->BindParamCTex(hVolumeMap, mVolumeTexture);
  }

  if (mCurrentTexture && hColorMap) {
    // if (IsDebug())
    // printf("Binding texmap<%p:%s> to param<%p>\n", mCurrentTexture, mCurrentTexture->_debugName.c_str(), hColorMap);
    FXI->BindParamCTex(hColorMap, mCurrentTexture);
  }
  if (mCurrentTexture2 && hColorMap2) {
    // printf( "Binding texmap2<%p> to param<%p>\n", mCurrentTexture2, hColorMap2 );
    FXI->BindParamCTex(hColorMap2, mCurrentTexture2);
  }

  if (mCurrentTexture3 && hColorMap3) {
    FXI->BindParamCTex(hColorMap3, mCurrentTexture3);
  }

  if (mCurrentTexture4 && hColorMap4) {
    FXI->BindParamCTex(hColorMap4, mCurrentTexture4);
  }

  FXI->CommitParams();
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

void GfxMaterial3DSolid::EndPass(Context* pTarg) {
  if (false == gbskip)
    pTarg->FXI()->EndPass();
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
      printf("color<%f %f %f %f>\n", Color.x, Color.y, Color.z, Color.w);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
