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
#include <pkg/ent/HeightFieldDrawable.h>

namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////

using namespace ::ork::lev2;

struct TerrainMaterialValues {

  TerrainMaterialValues(const HeightFieldDrawableData& data)
      : _data(data) {}

  const HeightFieldDrawableData& _data;
  fmtx4 _matMVPL;
  fmtx4 _matMVPC;
  fmtx4 _matMVPR;
  fvec3 _camPos;
  fvec4 _modcolor;
  Texture* _hfTextureA = nullptr;
  Texture* _hfTextureB = nullptr;
};

struct TerrainMaterial : public GfxMaterial {

  TerrainMaterial(GfxTarget* targ);
  ~TerrainMaterial() final;

  ////////////////////////////////////////////

  void begin(const RenderContextInstData& RCID, const TerrainMaterialValues& TMV);
  void end(const RenderContextInstData& RCID);

  ////////////////////////////////////////////

private:
  bool BeginPass(GfxTarget* targ, int iPass = 0) final;
  void EndPass(GfxTarget* targ) final;
  int BeginBlock(GfxTarget* targ, const RenderContextInstData& RCID) final;
  void EndBlock(GfxTarget* targ) final;
  void Init(GfxTarget* targ) final;
  void Update() final {}

  ////////////////////////////////////////////

  FxShader* _shader = nullptr;

  const FxShaderTechnique* _tekBasic  = nullptr;
  const FxShaderTechnique* _tekBasicDefGbuf1 = nullptr;
  const FxShaderTechnique* _tekStereo = nullptr;
  const FxShaderTechnique* _tekPick   = nullptr;

  const FxShaderParam* _parMatVPL       = nullptr;
  const FxShaderParam* _parMatVPC       = nullptr;
  const FxShaderParam* _parMatVPR       = nullptr;
  const FxShaderParam* _parCamPos       = nullptr;
  const FxShaderParam* _parTexA         = nullptr;
  const FxShaderParam* _parTexB         = nullptr;
  const FxShaderParam* _parTexEnv       = nullptr;
  const FxShaderParam* _parModColor     = nullptr;
  const FxShaderParam* _parTime         = nullptr;
  const FxShaderParam* _parTestXXX      = nullptr;
  const FxShaderParam* _parFogColor     = nullptr;
  const FxShaderParam* _parGrass        = nullptr;
  const FxShaderParam* _parSnow         = nullptr;
  const FxShaderParam* _parRock1        = nullptr;
  const FxShaderParam* _parRock2        = nullptr;
  const FxShaderParam* _parGblendYscale = nullptr;
  const FxShaderParam* _parGblendYbias  = nullptr;
  const FxShaderParam* _parGblendStepLo = nullptr;
  const FxShaderParam* _parGblendStepHi = nullptr;
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
  _tekBasicDefGbuf1 = fxi->GetTechnique(_shader, "terrain_gbuf1");

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

  _parFogColor     = fxi->GetParameterH(_shader, "FogColor");
  _parGrass        = fxi->GetParameterH(_shader, "GrassColor");
  _parSnow         = fxi->GetParameterH(_shader, "SnowColor");
  _parRock1        = fxi->GetParameterH(_shader, "Rock1Color");
  _parRock2        = fxi->GetParameterH(_shader, "Rock2Color");
  _parGblendYscale = fxi->GetParameterH(_shader, "GBlendYScale");
  _parGblendYbias  = fxi->GetParameterH(_shader, "GBlendYBias");
  _parGblendStepLo = fxi->GetParameterH(_shader, "GBlendStepLo");
  _parGblendStepHi = fxi->GetParameterH(_shader, "GBlendStepHi");
}

///////////////////////////////////////////////////////////////////////////////
// legacy methods
///////////////////////////////////////////////////////////////////////////////

bool TerrainMaterial::BeginPass(GfxTarget* targ, int iPass) { return true; }
void TerrainMaterial::EndPass(GfxTarget* targ) {}
int TerrainMaterial::BeginBlock(GfxTarget* targ, const RenderContextInstData& RCID) { return 1; }
void TerrainMaterial::EndBlock(GfxTarget* targ) {}

///////////////////////////////////////////////////////////////////////////////

void TerrainMaterial::begin(const RenderContextInstData& RCID, const TerrainMaterialValues& TMV) {
  const auto& HFDD = TMV._data;
  auto R           = RCID.GetRenderer();
  auto targ        = R->GetTarget();
  auto RCFD   = targ->topRenderContextFrameData();
  const auto& CPD = RCFD->topCPD();
  bool stereo1pass = CPD.isStereoOnePass();
  auto mtxi = targ->MTXI();
  auto fxi  = targ->FXI();
  auto rsi  = targ->RSI();

  auto tek = stereo1pass ? _tekStereo : _tekBasic;
  if( CPD._passID == "defgbuffer1"_crcu ){
    tek = _tekBasicDefGbuf1;
  }

  fxi->BindTechnique(_shader,tek);

  int npasses = fxi->BeginBlock(_shader, RCID);
  rsi->BindRasterState(mRasterState);
  fxi->BindPass(_shader, 0);
  fxi->BindParamMatrix(_shader, _parMatVPL, TMV._matMVPL);
  fxi->BindParamMatrix(_shader, _parMatVPC, TMV._matMVPC);
  fxi->BindParamMatrix(_shader, _parMatVPR, TMV._matMVPR);
  fxi->BindParamCTex(_shader, _parTexA, TMV._hfTextureA);
  fxi->BindParamCTex(_shader, _parTexB, TMV._hfTextureB);
  fxi->BindParamVect3(_shader, _parCamPos, TMV._camPos);
  fxi->BindParamVect4(_shader, _parModColor, TMV._modcolor);
  fxi->BindParamFloat(_shader, _parTime, 0.0f);

  fxi->BindParamTex(_shader, _parTexEnv, HFDD._sphericalenvmap);
  fxi->BindParamFloat(_shader, _parTestXXX, HFDD._testxxx);

  fxi->BindParamVect3(_shader, _parFogColor, HFDD._fogcolor);
  fxi->BindParamVect3(_shader, _parGrass, HFDD._grass);
  fxi->BindParamVect3(_shader, _parSnow, HFDD._snow);
  fxi->BindParamVect3(_shader, _parRock1, HFDD._rock1);
  fxi->BindParamVect3(_shader, _parRock2, HFDD._rock2);

  fxi->BindParamFloat(_shader, _parGblendYscale, HFDD._gblend_yscale);
  fxi->BindParamFloat(_shader, _parGblendYbias, HFDD._gblend_ybias);
  fxi->BindParamFloat(_shader, _parGblendStepLo, HFDD._gblend_steplo);
  fxi->BindParamFloat(_shader, _parGblendStepHi, HFDD._gblend_stephi);

}
void TerrainMaterial::end(const RenderContextInstData& RCID) {
  auto R    = RCID.GetRenderer();
  auto targ = R->GetTarget();
  targ->FXI()->EndPass(_shader);
  targ->FXI()->EndBlock(_shader);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ent
