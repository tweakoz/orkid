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

struct FreestyleMaterial : public GfxMaterial {

  FreestyleMaterial();
  ~FreestyleMaterial() final;

  ////////////////////////////////////////////

  void begin(const RenderContextFrameData& RCFD);
  void end(const RenderContextFrameData& RCFD);

  ////////////////////////////////////////////

  bool BeginPass(GfxTarget* targ, int iPass = 0) final;
  void EndPass(GfxTarget* targ) final;
  int BeginBlock(GfxTarget* targ, const RenderContextInstData& RCID) final;
  void EndBlock(GfxTarget* targ) final;
  void gpuInit(GfxTarget* targ,const AssetPath& assetname);
  void Init(GfxTarget* targ) final {}
  void Update() final {}

  ////////////////////////////////////////////

  FxShader* _shader = nullptr;

  std::set<const FxShaderTechnique*> _techniques;
  std::set<const FxShaderParam*> _params;
  std::set<const FxShaderParamBlock*> _paramBlocks;

  const FxShaderTechnique* technique(std::string named) {
    auto fxi = _initialTarget->FXI();
    auto tek = fxi->GetTechnique(_shader, named);
    if (tek != nullptr)
      _techniques.insert(tek);
    return tek;
  }
  const FxShaderParam* param(std::string named) {
    auto fxi = _initialTarget->FXI();
    auto par = fxi->GetParameterH(_shader, named);
    if (par != nullptr)
      _params.insert(par);
    return par;
  }
  const FxShaderParamBlock* paramBlock(std::string named) {
    auto fxi = _initialTarget->FXI();
    auto par = fxi->GetParameterBlockH(_shader, named);
    if (par != nullptr)
      _paramBlocks.insert(par);
    return par;
  }

  ////////////////////////////////////////////

  void commit() {
    auto fxi = _initialTarget->FXI();
    fxi->CommitParams();
  }


  void bindTechnique(const FxShaderTechnique* tek) {
    auto fxi = _initialTarget->FXI();
    fxi->BindTechnique(_shader, tek);
  }
  void bindParamFloat(const FxShaderParam* par, float value) {
    auto fxi = _initialTarget->FXI();
    fxi->BindParamFloat(_shader, par, value);
  }
  void bindParamCTex(const FxShaderParam* par, const Texture* tex) {
    auto fxi = _initialTarget->FXI();
    fxi->BindParamCTex(_shader, par, tex);
  }
  void bindParamVec2(const FxShaderParam* par, const fvec2& v) {
    auto fxi = _initialTarget->FXI();
    fxi->BindParamVect2(_shader, par, v);
  }
  void bindParamVec3(const FxShaderParam* par, const fvec3& v) {
    auto fxi = _initialTarget->FXI();
    fxi->BindParamVect3(_shader, par, v);
  }
  void bindParamVec4(const FxShaderParam* par, const fvec4& v) {
    auto fxi = _initialTarget->FXI();
    fxi->BindParamVect4(_shader, par, v);
  }

  void bindParamMatrix(const FxShaderParam* par, const fmtx4& m) {
    auto fxi = _initialTarget->FXI();
    fxi->BindParamMatrix(_shader, par, m);
  }
  void bindParamMatrixArray(const FxShaderParam* par, const fmtx4* m, size_t len) {
    auto fxi = _initialTarget->FXI();
    fxi->BindParamMatrixArray(_shader, par, m, len);
  }

  ////////////////////////////////////////////

  GfxTarget* _initialTarget = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

FreestyleMaterial::FreestyleMaterial() {
  mRasterState.SetShadeModel(ESHADEMODEL_SMOOTH);
  mRasterState.SetAlphaTest(EALPHATEST_OFF);
  mRasterState.SetBlending(EBLENDING_OFF);
  mRasterState.SetDepthTest(EDEPTHTEST_LEQUALS);
  mRasterState.SetZWriteMask(true);
  mRasterState.SetCullTest(ECULLTEST_PASS_BACK);
  miNumPasses = 1;
}

///////////////////////////////////////////////////////////////////////////////

FreestyleMaterial::~FreestyleMaterial() {}

///////////////////////////////////////////////////////////////////////////////

void FreestyleMaterial::gpuInit(GfxTarget* targ,const AssetPath& assetname) {
  _initialTarget = targ;
  auto fxi       = targ->FXI();
  auto shass     = asset::AssetManager<FxShaderAsset>::Load(assetname.c_str());
  _shader        = shass->GetFxShader();
}

///////////////////////////////////////////////////////////////////////////////
// legacy methods
///////////////////////////////////////////////////////////////////////////////

bool FreestyleMaterial::BeginPass(GfxTarget* targ, int iPass) { return true; }
void FreestyleMaterial::EndPass(GfxTarget* targ) {}
int FreestyleMaterial::BeginBlock(GfxTarget* targ, const RenderContextInstData& RCID) { return 1; }
void FreestyleMaterial::EndBlock(GfxTarget* targ) {}

///////////////////////////////////////////////////////////////////////////////

void FreestyleMaterial::begin(const RenderContextFrameData& RCFD) {
  auto targ        = RCFD.GetTarget();
  const auto& CPD  = RCFD.topCPD();
  bool stereo1pass = CPD.isStereoOnePass();
  auto mtxi        = targ->MTXI();
  auto fxi         = targ->FXI();
  auto rsi         = targ->RSI();

  RenderContextInstData RCID(RCFD);
  int npasses = fxi->BeginBlock(_shader, RCID);
  rsi->BindRasterState(mRasterState);
  fxi->BindPass(_shader, 0);
}
void FreestyleMaterial::end(const RenderContextFrameData& RCFD) {
  auto targ = RCFD.GetTarget();
  targ->FXI()->EndPass(_shader);
  targ->FXI()->EndBlock(_shader);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
