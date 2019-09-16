////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/lev2_asset.h>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct TerrainMaterialParams {
  fmtx4 _matMVPL;
  fmtx4 _matMVPC;
  fmtx4 _matMVPR;
  fvec3 _camPos;
  fvec4 _modcolor;
  Texture* _hfTextureA = nullptr;
  Texture* _hfTextureB = nullptr;
  Texture* _envTexture = nullptr;
  float _testxxx = 0.0f;
  fvec3 _fogcolor;
  fvec3 _grass, _snow, _rock1, _rock2;
  float _gblend_yscale = 1.0f;
  float _gblend_ybias = 0.0f;
  float _gblend_steplo = 0.5f;
  float _gblend_stephi = 0.6f;
};

struct TerrainMaterial : public GfxMaterial {

  TerrainMaterial(GfxTarget* targ);
  ~TerrainMaterial() final;

  ////////////////////////////////////////////

  bool BeginPass(GfxTarget* targ, int iPass = 0) final;
  void EndPass(GfxTarget* targ) final;
  int BeginBlock(GfxTarget* targ, const RenderContextInstData& RCID) final;
  void EndBlock(GfxTarget* targ) final;
  void Init(GfxTarget* targ) final;
  void Update() final {}

  ////////////////////////////////////////////

  void begin(const RenderContextInstData& RCID);
  void end(GfxTarget* targ);

  ////////////////////////////////////////////

  FxShader* _shader = nullptr;

  const FxShaderTechnique* _tekBasic  = nullptr;
  const FxShaderTechnique* _tekStereo = nullptr;
  const FxShaderTechnique* _tekPick   = nullptr;

  const FxShaderParam* _parMatVPL   = nullptr;
  const FxShaderParam* _parMatVPC   = nullptr;
  const FxShaderParam* _parMatVPR   = nullptr;
  const FxShaderParam* _parCamPos   = nullptr;
  const FxShaderParam* _parTexA     = nullptr;
  const FxShaderParam* _parTexB     = nullptr;
  const FxShaderParam* _parTexEnv   = nullptr;
  const FxShaderParam* _parModColor = nullptr;
  const FxShaderParam* _parTime     = nullptr;
  const FxShaderParam* _parTestXXX  = nullptr;
  const FxShaderParam* _parFogColor  = nullptr;
  const FxShaderParam* _parGrass  = nullptr;
  const FxShaderParam* _parSnow  = nullptr;
  const FxShaderParam* _parRock1  = nullptr;
  const FxShaderParam* _parRock2  = nullptr;
  const FxShaderParam* _parGblendYscale  = nullptr;
  const FxShaderParam* _parGblendYbias  = nullptr;
  const FxShaderParam* _parGblendStepLo  = nullptr;
  const FxShaderParam* _parGblendStepHi  = nullptr;

  TerrainMaterialParams _paramVal;
};

///////////////////////////////////////////////////////////////////////////////

TerrainMaterial::TerrainMaterial(GfxTarget* targ) {
  mRasterState.SetShadeModel(ESHADEMODEL_SMOOTH);
  mRasterState.SetAlphaTest(EALPHATEST_OFF);
  mRasterState.SetBlending(EBLENDING_OFF);
  mRasterState.SetDepthTest(EDEPTHTEST_LEQUALS);
  mRasterState.SetZWriteMask(true);
  mRasterState.SetCullTest(ECULLTEST_PASS_BACK);
  miNumPasses = 1;
  Init(targ);
}

///////////////////////////////////////////////////////////////////////////////

TerrainMaterial::~TerrainMaterial() {}

///////////////////////////////////////////////////////////////////////////////

void TerrainMaterial::Init(GfxTarget* targ) {
  auto fxi     = targ->FXI();
  auto shass   = asset::AssetManager<FxShaderAsset>::Load("orkshader://terrain");
  _shader      = shass->GetFxShader();
  _tekBasic    = fxi->GetTechnique(_shader, "terrain");
  _tekStereo   = fxi->GetTechnique(_shader, "terrain_stereo");
  _tekPick     = fxi->GetTechnique(_shader, "pick");
  _parMatVPL   = fxi->GetParameterH(_shader, "MatMVPL");
  _parMatVPC   = fxi->GetParameterH(_shader, "MatMVPC");
  _parMatVPR   = fxi->GetParameterH(_shader, "MatMVPR");
  _parCamPos   = fxi->GetParameterH(_shader, "CamPos");
  _parTexA     = fxi->GetParameterH(_shader, "HFAMap");
  _parTexB     = fxi->GetParameterH(_shader, "HFBMap");
  _parTexEnv   = fxi->GetParameterH(_shader, "EnvMap");
  _parModColor = fxi->GetParameterH(_shader, "ModColor");
  _parTime     = fxi->GetParameterH(_shader, "Time");
  _parTestXXX  = fxi->GetParameterH(_shader, "testxxx");

  _parFogColor  = fxi->GetParameterH(_shader, "FogColor");
  _parGrass  = fxi->GetParameterH(_shader, "GrassColor");
  _parSnow  = fxi->GetParameterH(_shader, "SnowColor");
  _parRock1  = fxi->GetParameterH(_shader, "Rock1Color");
  _parRock2  = fxi->GetParameterH(_shader, "Rock2Color");
  _parGblendYscale  = fxi->GetParameterH(_shader, "GBlendYScale");
  _parGblendYbias  = fxi->GetParameterH(_shader, "GBlendYBias");
  _parGblendStepLo  = fxi->GetParameterH(_shader, "GBlendStepLo");
  _parGblendStepHi  = fxi->GetParameterH(_shader, "GBlendStepHi");


}

///////////////////////////////////////////////////////////////////////////////

bool TerrainMaterial::BeginPass(GfxTarget* targ, int iPass) {
  auto mtxi = targ->MTXI();
  auto fxi  = targ->FXI();
  auto rsi  = targ->RSI();
  rsi->BindRasterState(mRasterState);
  fxi->BindPass(_shader, iPass);
  fxi->BindParamMatrix(_shader, _parMatVPL, _paramVal._matMVPL);
  fxi->BindParamMatrix(_shader, _parMatVPC, _paramVal._matMVPC);
  fxi->BindParamMatrix(_shader, _parMatVPR, _paramVal._matMVPR);
  fxi->BindParamCTex(_shader, _parTexA, _paramVal._hfTextureA);
  fxi->BindParamCTex(_shader, _parTexB, _paramVal._hfTextureB);
  fxi->BindParamCTex(_shader, _parTexEnv, _paramVal._envTexture);
  fxi->BindParamVect3(_shader, _parCamPos, _paramVal._camPos);
  fxi->BindParamVect4(_shader, _parModColor, _paramVal._modcolor);
  fxi->BindParamFloat(_shader, _parTime, 0.0f);
  fxi->BindParamFloat(_shader, _parTestXXX, _paramVal._testxxx );

  fxi->BindParamVect3(_shader, _parFogColor, _paramVal._fogcolor );
  fxi->BindParamVect3(_shader, _parGrass, _paramVal._grass );
  fxi->BindParamVect3(_shader, _parSnow, _paramVal._snow );
  fxi->BindParamVect3(_shader, _parRock1, _paramVal._rock1 );
  fxi->BindParamVect3(_shader, _parRock2, _paramVal._rock2 );

  fxi->BindParamFloat(_shader, _parGblendYscale, _paramVal._gblend_yscale );
  fxi->BindParamFloat(_shader, _parGblendYbias, _paramVal._gblend_ybias );
  fxi->BindParamFloat(_shader, _parGblendStepLo, _paramVal._gblend_steplo );
  fxi->BindParamFloat(_shader, _parGblendStepHi, _paramVal._gblend_stephi );
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void TerrainMaterial::EndPass(GfxTarget* targ) { targ->FXI()->EndPass(_shader); }

///////////////////////////////////////////////////////////////////////////////

int TerrainMaterial::BeginBlock(GfxTarget* targ, const RenderContextInstData& RCID) {
  auto fxi = targ->FXI();
  auto framedata            = targ->GetRenderContextFrameData();
  bool stereo1pass = framedata->isStereoOnePass();
  fxi->BindTechnique(_shader, stereo1pass ? _tekStereo : _tekBasic);
  return fxi->BeginBlock(_shader, RCID);
}

///////////////////////////////////////////////////////////////////////////////

void TerrainMaterial::EndBlock(GfxTarget* targ) { targ->FXI()->EndBlock(_shader); }

///////////////////////////////////////////////////////////////////////////////

void TerrainMaterial::begin(const RenderContextInstData& RCID) {
  auto R         = RCID.GetRenderer();
  auto targ      = R->GetTarget();
  int inumpasses = BeginBlock(targ, RCID);
  assert(inumpasses == 1);
  bool bDRAW = BeginPass(targ, 0);
  assert(bDRAW);
}
void TerrainMaterial::end(GfxTarget* targ) { EndBlock(targ); }

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
