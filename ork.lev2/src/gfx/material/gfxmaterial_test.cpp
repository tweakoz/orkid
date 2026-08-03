////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
  //_rasterstate->setShadeModel(ESHADEMODEL_SMOOTH);
  _rasterstate->setBlendingMacro(BlendingMacro::OFF);
  _rasterstate->setDepthTest(EDepthTest::LEQUALS);
  _rasterstate->setWriteMaskZ(true);
  _rasterstate->setCullTest(ECullTest::OFF);

  miNumPasses = 1;

  if (false == gearlyhack) {
    auto mtl_load_req = std::make_shared<asset::LoadRequest>();
    mtl_load_req->_asset_path = "orkshader://solid";
    _shaderasset = asset::AssetManager<FxShaderAsset>::load(mtl_load_req);
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

  //_rasterstate->setShadeModel(ESHADEMODEL_SMOOTH);
  _rasterstate->setBlendingMacro(BlendingMacro::OFF);
  _rasterstate->setDepthTest(EDepthTest::LEQUALS);
  _rasterstate->setWriteMaskZ(true);
  _rasterstate->setCullTest(ECullTest::OFF);

  miNumPasses = 1;

  if (pTARG) {
    gpuInit(pTARG);
  } else {
    std::shared_ptr<FxShaderAsset> fxshaderasset;
    auto mtl_load_req = std::make_shared<asset::LoadRequest>();
    mtl_load_req->_asset_path = mUserFxName.c_str();
    _shaderasset = asset::AssetManager<FxShaderAsset>::load(mtl_load_req);
    _shader      = _shaderasset->GetFxShader();

    if (_shader)
      _shader->SetAllowCompileFailure(mAllowCompileFailure);
  }
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterial3DSolid::gpuInit(ork::lev2::Context* pTarg) {

  auto fxi = pTarg->FXI();

  if (mUserFxName.length()) {
    auto mtl_load_req = std::make_shared<asset::LoadRequest>();
    mtl_load_req->_asset_path = mUserFxName.c_str();
    _shaderasset = asset::AssetManager<FxShaderAsset>::load(mtl_load_req);

    _shader = _shaderasset ? _shaderasset->GetFxShader() : 0;

    if (_shader)
      _shader->SetAllowCompileFailure(mAllowCompileFailure);

  } else {
    // orkprintf( "Attempting to Load Shader<orkshader://solid>\n" );
    auto mtl_load_req = std::make_shared<asset::LoadRequest>();
    mtl_load_req->_asset_path = "orkshader://solid";
    _shaderasset = asset::AssetManager<FxShaderAsset>::load(mtl_load_req);
    _shader      = _shaderasset->GetFxShader();
  }
  if (0 == _shader) {
    return;
  }
  if (mUserTekName.length()) {
    hTekUser       = fxi->technique(_shader, mUserTekName);
  }
  if (meColorMode != EMODE_USER) {
    hTekVertexColor    = fxi->technique(_shader, "vtxcolor");
    hTekVertexModColor = fxi->technique(_shader, "vtxmodcolor");
    hTekModColor       = fxi->technique(_shader, "mmodcolor");
    hTekTexColor       = fxi->technique(_shader, "texcolor");
    hTekTexModColor    = fxi->technique(_shader, "texmodcolor");
    hTekTexTexModColor = fxi->technique(_shader, "textexmodcolor");
    hTekTexVertexColor = fxi->technique(_shader, "texvtxcolor");
    hTekObjNormal = fxi->technique(_shader, "tek_objnormal");
    hTekWldNormal = fxi->technique(_shader, "tek_wldnormal");

  }

  hTekPick = fxi->technique(_shader, "tek_pick");

  hMatAux  = fxi->parameter(_shader, "MatAux");
  hMatAux2 = fxi->parameter(_shader, "MatAux2");
  hMatRot  = fxi->parameter(_shader, "MatRotW");

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

static bool gbskip = false;

int GfxMaterial3DSolid::BeginBlock(Context* pTarg, const RenderContextInstData& RCID) {

  auto RCFD = pTarg->topRenderContextFrameData();
  const auto& CPD                    = RCFD->topCPD();
  bool is_picking                    = CPD.isPicking();
  auto MTXI = pTarg->MTXI();
  auto FXI  = pTarg->FXI();

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
        return pTarg->FXI()->BeginBlock(hTekTexColor, RCID);
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
      case EMODE_WNORMAL_COLOR:
        return pTarg->FXI()->BeginBlock(hTekWldNormal, RCID);
        break;
      case EMODE_ONORMAL_COLOR:
        return pTarg->FXI()->BeginBlock(hTekObjNormal, RCID);
        break;
      case EMODE_USER:
        return pTarg->FXI()->BeginBlock(hTekUser, RCID);
        break;
    }
  if (gbskip)
    return 0;


  if (_shader->GetFailedCompile()) {
    assert(false);
    return 0;
  }

  FXI->bindParamMatrix(hMatM, MTXI->RefMMatrix());
  FXI->bindParamMatrix(hMatMV, MTXI->RefMVMatrix());
  FXI->bindParamMatrix(hMatP, MTXI->RefPMatrix());

  // MONO ONLY, deliberately. None of the shaders this material loads declares a
  // per-eye transform (MatMVPL/MatMVPR resolve nowhere), so the single-pass-stereo
  // fork that used to sit here could feed nothing -- what it DID do was skip the
  // MatMVP bind entirely under a stereo pass, leaving the vertex stage on whatever
  // matrix the previous draw left behind. Both eye layers get the mono transform.
  const auto& world = MTXI->RefMMatrix();
  if (CPD._mono_cam_matrices) {
    auto mcams = CPD._mono_cam_matrices;
    auto MVP   = fmtx4::multiply_ltor(world,mcams->_vmatrix,mcams->_pmatrix);
    FXI->bindParamMatrix(hMatMVP, MVP);
  } else {
    auto MVP = MTXI->RefMVPMatrix();
    FXI->bindParamMatrix(hMatMVP, MVP);
  }

  if (hMatAux)
    FXI->bindParamMatrix(hMatAux, mMatAux);

  if (hMatAux2)
    FXI->bindParamMatrix(hMatAux2, mMatAux2);

  if (hMatV) {
    FXI->bindParamMatrix(hMatV, MTXI->RefVMatrix());
  }

  if (hMatRot)
    FXI->bindParamMatrix(hMatRot, MTXI->RefR3Matrix());

  if (pTarg->FBI()->isPickState()) {
    FXI->bindParamVect4(hParamModColor, pTarg->RefModColor());
  } else {
    if (meColorMode == EMODE_INTERNAL_COLOR) {
      FXI->bindParamVect4(hParamModColor, Color);
    } else {
      FXI->bindParamVect4(hParamModColor, pTarg->RefModColor());
    }
  }

  if (hParamNoiseAmp) {
    FXI->bindParamVect4(hParamNoiseAmp, mNoiseAmp);
  }
  if (hParamNoiseFreq) {
    FXI->bindParamVect4(hParamNoiseFreq, mNoiseFreq);
  }
  if (hParamNoiseShift) {
    FXI->bindParamVect4(hParamNoiseShift, mNoiseShift);
  }

  if (hParamTime) {
    float reltime = fmodf(OldSchool::GetRef().GetLoResRelTime(), 300.0f);
    // printf( "reltime<%f>\n", reltime );
    FXI->bindParamFloat(hParamTime, reltime);
  }

  if (hParamUser0) {
    FXI->bindParamVect4(hParamUser0, mUser0);
  }
  if (hParamUser1) {
    FXI->bindParamVect4(hParamUser1, mUser1);
  }
  if (hParamUser2) {
    FXI->bindParamVect4(hParamUser2, mUser2);
  }
  if (hParamUser3) {
    FXI->bindParamVect4(hParamUser3, mUser3);
  }

  if (mVolumeTexture && hVolumeMap) {
    FXI->bindParamTexture(hVolumeMap, mVolumeTexture);
  }

  if (mCurrentTexture && hColorMap) {
    // if (IsDebug())
    // printf("Binding texmap<%p:%s> to param<%p>\n", mCurrentTexture, mCurrentTexture->_debugName.c_str(), hColorMap);
    FXI->bindParamTexture(hColorMap, mCurrentTexture);
  }
  if (mCurrentTexture2 && hColorMap2) {
    // printf( "Binding texmap2<%p> to param<%p>\n", mCurrentTexture2, hColorMap2 );
    FXI->bindParamTexture(hColorMap2, mCurrentTexture2);
  }

  if (mCurrentTexture3 && hColorMap3) {
    FXI->bindParamTexture(hColorMap3, mCurrentTexture3);
  }

  if (mCurrentTexture4 && hColorMap4) {
    FXI->bindParamTexture(hColorMap4, mCurrentTexture4);
  }

  FXI->CommitParams();
  pTarg->FXI()->applyRasterState(*_rasterstate);
  return 0;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterial3DSolid::EndBlock(Context* pTarg) {
  pTarg->FXI()->EndBlock();
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
