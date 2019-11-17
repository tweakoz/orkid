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

struct PBRMaterial : public GfxMaterial {

  PBRMaterial();
  ~PBRMaterial() final;

  ////////////////////////////////////////////

  void begin(const RenderContextFrameData& RCFD);
  void end(const RenderContextFrameData& RCFD);

  ////////////////////////////////////////////

  bool BeginPass(GfxTarget* targ, int iPass = 0) final;
  void EndPass(GfxTarget* targ) final;
  int BeginBlock(GfxTarget* targ, const RenderContextInstData& RCID) final;
  void EndBlock(GfxTarget* targ) final;
  void Init(GfxTarget* targ) final;
  void Update() final;

  ////////////////////////////////////////////

  FxShader* _shader = nullptr;
  GfxTarget* _initialTarget = nullptr;
  const FxShaderTechnique* _tekRigidGBUFFER = nullptr;
  const FxShaderParam* _paramMVP = nullptr;
  const FxShaderParam* _paramMV = nullptr;
  const FxShaderParam* _paramMROT = nullptr;
};

inline PBRMaterial::PBRMaterial() {
  mRasterState.SetShadeModel(ESHADEMODEL_SMOOTH);
  mRasterState.SetAlphaTest(EALPHATEST_OFF);
  mRasterState.SetBlending(EBLENDING_OFF);
  mRasterState.SetDepthTest(EDEPTHTEST_LEQUALS);
  mRasterState.SetZWriteMask(true);
  mRasterState.SetCullTest(ECULLTEST_PASS_FRONT);
  miNumPasses = 1;
}
inline PBRMaterial::~PBRMaterial() {
}
inline void PBRMaterial::begin(const RenderContextFrameData& RCFD) {
}
inline void PBRMaterial::end(const RenderContextFrameData& RCFD) {
}

////////////////////////////////////////////

inline bool PBRMaterial::BeginPass(GfxTarget* targ, int iPass) {
  auto fxi       = targ->FXI();
  auto rsi       = targ->RSI();
  auto mtxi      = targ->MTXI();
  auto mvpmtx = mtxi->RefMVPMatrix();
  auto rotmtx = mtxi->RefR3Matrix();
  auto mvmtx = mtxi->RefMVMatrix();
  const RenderContextInstData* RCID  = targ->GetRenderContextInstData();
  const RenderContextFrameData* RCFD = targ->topRenderContextFrameData();
  const auto& CPD = RCFD->topCPD();
  fxi->BindPass(_shader, 0);
  //fxi->BindParamMatrix(_shader,_paramMVP,mvpmtx);
  fxi->BindParamMatrix(_shader,_paramMV,mvmtx);
  fxi->BindParamMatrix(_shader,_paramMROT,rotmtx);
  const auto& world = mtxi->RefMMatrix();
  if (CPD.isStereoOnePass() and CPD._stereoCameraMatrices) {
    auto stereomtx = CPD._stereoCameraMatrices;
    auto MVPL = stereomtx->MVPL(world);
    auto MVPR = stereomtx->MVPR(world);
    fxi->BindParamMatrix(_shader, _paramMVP, MVPL);
    fxi->BindParamMatrix(_shader, _paramMVP, MVPR);
  } else {
    auto mcams = CPD._cameraMatrices;
    auto MVP = world
               * mcams->_vmatrix
               * mcams->_pmatrix;
    fxi->BindParamMatrix(_shader, _paramMVP, MVP);
  }
  rsi->BindRasterState(mRasterState);
  fxi->CommitParams();
  return true;
}
inline void PBRMaterial::EndPass(GfxTarget* targ) {
  targ->FXI()->EndPass(_shader);
}
inline int PBRMaterial::BeginBlock(GfxTarget* targ, const RenderContextInstData& RCID) {
  auto fxi       = targ->FXI();
  fxi->BindTechnique(_shader,_tekRigidGBUFFER);
  int numpasses = fxi->BeginBlock(_shader,RCID);
  assert(numpasses==1);
  return numpasses;
}
inline void PBRMaterial::EndBlock(GfxTarget* targ) {
  auto fxi       = targ->FXI();
  fxi->EndBlock(_shader);
}
inline void PBRMaterial::Init(GfxTarget* targ) /*final*/ {
  assert(_initialTarget==nullptr);
  _initialTarget = targ;
  auto fxi       = targ->FXI();
  auto shass     = ork::asset::AssetManager<FxShaderAsset>::Load("orkshader://pbr");
  _shader        = shass->GetFxShader();
  _tekRigidGBUFFER = fxi->technique(_shader,"rigid_gbuffer");
  _paramMVP = fxi->parameter(_shader,"mvp");
  _paramMV = fxi->parameter(_shader,"mv");
  _paramMROT = fxi->parameter(_shader,"mrot");
}
inline void PBRMaterial::Update() {
}

} // namespace ork::lev2