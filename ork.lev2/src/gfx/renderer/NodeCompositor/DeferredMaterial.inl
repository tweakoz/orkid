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

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct DeferredMaterial : public GfxMaterial {

  DeferredMaterial();
  ~DeferredMaterial() final;

  ////////////////////////////////////////////

  void begin(const RenderContextFrameData &RCFD);
  void end(const RenderContextFrameData &RCFD);

  ////////////////////////////////////////////

  bool BeginPass(GfxTarget *targ, int iPass = 0) final;
  void EndPass(GfxTarget *targ) final;
  int BeginBlock(GfxTarget *targ, const RenderContextInstData &RCID) final;
  void EndBlock(GfxTarget *targ) final;
  void Init(GfxTarget *targ) final;
  void Update() final {}

  ////////////////////////////////////////////

  FxShader *_shader = nullptr;

  const FxShaderTechnique *_tekBaseLighting = nullptr;
  const FxShaderTechnique *_tekPointLighting = nullptr;
  const FxShaderTechnique *_tekBaseLightingStereo = nullptr;
  const FxShaderTechnique *_tekPointLightingStereo = nullptr;

  const FxShaderParam *_parMatMVPC = nullptr;
  const FxShaderParam *_parMatMVPL = nullptr;
  const FxShaderParam *_parMatMVPR = nullptr;
  const FxShaderParam *_parMatIVPArray = nullptr;
  const FxShaderParam *_parMapGBufAlbAo = nullptr;
  const FxShaderParam *_parMapGBufNrmL = nullptr;
  const FxShaderParam *_parMapDepth = nullptr;
  const FxShaderParam *_parTime = nullptr;
  const FxShaderParam *_parInvViewSize = nullptr;
  const FxShaderParam *_parInvVpDim = nullptr;
  const FxShaderParam *_parLightPosR = nullptr;
  const FxShaderParam *_parLightColor = nullptr;
  const FxShaderParam *_parLightPos = nullptr;

  fmtx4 _mvpC;
  fmtx4 _mvpL;
  fmtx4 _mvpR;
  fmtx4 _ivp[2];
  fmtx4 _lightmatrix;
  fvec3 _lightpos;
  float _lightradius = 0.0f;
  fvec3 _lightcolor;
  fvec2 _invviewsize;
  float _time = 0.0f;
  Texture *_albedoAoMap = nullptr;
  Texture *_depthMap = nullptr;
  Texture *_normalLitmodelMap = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

DeferredMaterial::DeferredMaterial() {
  mRasterState.SetShadeModel(ESHADEMODEL_SMOOTH);
  mRasterState.SetAlphaTest(EALPHATEST_OFF);
  mRasterState.SetBlending(EBLENDING_OFF);
  mRasterState.SetDepthTest(EDEPTHTEST_LEQUALS);
  mRasterState.SetZWriteMask(true);
  mRasterState.SetCullTest(ECULLTEST_PASS_BACK);
  miNumPasses = 1;
}

///////////////////////////////////////////////////////////////////////////////

DeferredMaterial::~DeferredMaterial() {}

///////////////////////////////////////////////////////////////////////////////

void DeferredMaterial::Init(GfxTarget *targ) {
  auto fxi = targ->FXI();
  auto shass = asset::AssetManager<FxShaderAsset>::Load("orkshader://deferred");
  _shader = shass->GetFxShader();
  _tekBaseLighting = fxi->GetTechnique(_shader, "baselight");
  _tekPointLighting = fxi->GetTechnique(_shader, "pointlight");
  _tekBaseLightingStereo = fxi->GetTechnique(_shader, "baselight_stereo");
  _tekPointLightingStereo = fxi->GetTechnique(_shader, "pointlight_stereo");

  _parMatMVPC = fxi->GetParameterH(_shader, "MVPC");
  _parMatMVPL = fxi->GetParameterH(_shader, "MVPL");
  _parMatMVPR = fxi->GetParameterH(_shader, "MVPR");
  _parMatIVPArray = fxi->GetParameterH(_shader, "IVPArray");
  _parLightColor = fxi->GetParameterH(_shader, "LightColor");
  _parLightPosR = fxi->GetParameterH(_shader, "LightPosR");
  _parMapGBufAlbAo = fxi->GetParameterH(_shader, "MapAlbedoAo");
  _parMapGBufNrmL = fxi->GetParameterH(_shader, "MapNormalL");
  _parMapDepth = fxi->GetParameterH(_shader, "MapDepth");
  _parInvViewSize = fxi->GetParameterH(_shader, "InvViewportSize");
  _parTime = fxi->GetParameterH(_shader, "Time");
}

///////////////////////////////////////////////////////////////////////////////
// legacy methods
///////////////////////////////////////////////////////////////////////////////

bool DeferredMaterial::BeginPass(GfxTarget *targ, int iPass) { return true; }
void DeferredMaterial::EndPass(GfxTarget *targ) {}
int DeferredMaterial::BeginBlock(GfxTarget *targ,
                                 const RenderContextInstData &RCID) {
  return 1;
}
void DeferredMaterial::EndBlock(GfxTarget *targ) {}

///////////////////////////////////////////////////////////////////////////////

void DeferredMaterial::begin(const RenderContextFrameData &RCFD) {
  auto targ = RCFD.GetTarget();
  const auto &CPD = RCFD.topCPD();
  bool stereo1pass = CPD.isStereoOnePass();
  auto mtxi = targ->MTXI();
  auto fxi = targ->FXI();
  auto rsi = targ->RSI();

  RenderContextInstData RCID(RCFD);
  int npasses = fxi->BeginBlock(_shader, RCID);
  rsi->BindRasterState(mRasterState);
  fxi->BindPass(_shader, 0);
  fxi->BindParamMatrix(_shader, _parMatMVPC, _mvpC);
  fxi->BindParamMatrix(_shader, _parMatMVPL, _mvpL);
  fxi->BindParamMatrix(_shader, _parMatMVPR, _mvpR);
  fxi->BindParamMatrixArray(_shader, _parMatIVPArray, _ivp, 2);

  fxi->BindParamCTex(_shader, _parMapGBufAlbAo, _albedoAoMap);
  fxi->BindParamCTex(_shader, _parMapGBufNrmL, _normalLitmodelMap);
  fxi->BindParamCTex(_shader, _parMapDepth, _depthMap);

  fxi->BindParamVect2(_shader, _parInvViewSize, _invviewsize);
  fxi->BindParamVect3(_shader, _parLightColor, _lightcolor);
  fxi->BindParamVect4(_shader, _parLightPosR, fvec4(_lightpos, _lightradius));
  fxi->BindParamFloat(_shader, _parTime, 0.0f);
}
void DeferredMaterial::end(const RenderContextFrameData &RCFD) {
  auto targ = RCFD.GetTarget();
  targ->FXI()->EndPass(_shader);
  targ->FXI()->EndBlock(_shader);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
