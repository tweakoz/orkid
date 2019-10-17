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
  #if defined(ENABLE_COMPUTE_SHADERS)
  std::set<const FxShaderStorageBlock*> _storageBlocks;
  #endif

  inline const FxShaderTechnique* technique(std::string named) {
    auto fxi = _initialTarget->FXI();
    auto tek = fxi->technique(_shader, named);
    if (tek != nullptr)
      _techniques.insert(tek);
    return tek;
  }
  inline const FxShaderParam* param(std::string named) {
    auto fxi = _initialTarget->FXI();
    auto par = fxi->parameter(_shader, named);
    if (par != nullptr)
      _params.insert(par);
    return par;
  }
  inline const FxShaderParamBlock* paramBlock(std::string named) {
    auto fxi = _initialTarget->FXI();
    auto par = fxi->parameterBlock(_shader, named);
    if (par != nullptr)
      _paramBlocks.insert(par);
    return par;
  }

  ////////////////////////////////////////////
#if defined(ENABLE_COMPUTE_SHADERS)
  inline const FxShaderStorageBlock* storageBlock(std::string named) {
    auto fxi = _initialTarget->FXI();
    auto blk = fxi->storageBlock(_shader, named);
    if (blk != nullptr)
      _storageBlocks.insert(blk);
    return blk;
  }
#endif
  ////////////////////////////////////////////

  inline void commit() {
    auto fxi = _initialTarget->FXI();
    fxi->CommitParams();
  }


  inline void bindTechnique(const FxShaderTechnique* tek) {
    auto fxi = _initialTarget->FXI();
    fxi->BindTechnique(_shader, tek);
  }
  inline void bindParamInt(const FxShaderParam* par, int value) {
    auto fxi = _initialTarget->FXI();
    fxi->BindParamInt(_shader, par, value);
  }
  inline void bindParamFloat(const FxShaderParam* par, float value) {
    auto fxi = _initialTarget->FXI();
    fxi->BindParamFloat(_shader, par, value);
  }
  inline void bindParamCTex(const FxShaderParam* par, const Texture* tex) {
    auto fxi = _initialTarget->FXI();
    fxi->BindParamCTex(_shader, par, tex);
  }
  inline void bindParamVec2(const FxShaderParam* par, const fvec2& v) {
    auto fxi = _initialTarget->FXI();
    fxi->BindParamVect2(_shader, par, v);
  }
  inline void bindParamVec3(const FxShaderParam* par, const fvec3& v) {
    auto fxi = _initialTarget->FXI();
    fxi->BindParamVect3(_shader, par, v);
  }
  inline void bindParamVec4(const FxShaderParam* par, const fvec4& v) {
    auto fxi = _initialTarget->FXI();
    fxi->BindParamVect4(_shader, par, v);
  }

  inline void bindParamMatrix(const FxShaderParam* par, const fmtx4& m) {
    auto fxi = _initialTarget->FXI();
    fxi->BindParamMatrix(_shader, par, m);
  }
  inline void bindParamMatrixArray(const FxShaderParam* par, const fmtx4* m, size_t len) {
    auto fxi = _initialTarget->FXI();
    fxi->BindParamMatrixArray(_shader, par, m, len);
  }

#if defined (ENABLE_COMPUTE_SHADERS)
  std::set<const FxComputeShader*> _computeShaders;
  inline const FxComputeShader* computeShader(std::string named) {
    auto fxi = _initialTarget->FXI();
    auto tek = fxi->computeShader(_shader, named);
    if (tek != nullptr)
      _computeShaders.insert(tek);
    return tek;
  }
#endif

  ////////////////////////////////////////////

  GfxTarget* _initialTarget = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

inline FreestyleMaterial::FreestyleMaterial() {
  mRasterState.SetShadeModel(ESHADEMODEL_SMOOTH);
  mRasterState.SetAlphaTest(EALPHATEST_OFF);
  mRasterState.SetBlending(EBLENDING_OFF);
  mRasterState.SetDepthTest(EDEPTHTEST_LEQUALS);
  mRasterState.SetZWriteMask(true);
  mRasterState.SetCullTest(ECULLTEST_PASS_BACK);
  miNumPasses = 1;
}

///////////////////////////////////////////////////////////////////////////////

inline FreestyleMaterial::~FreestyleMaterial() {}

///////////////////////////////////////////////////////////////////////////////

inline void FreestyleMaterial::gpuInit(GfxTarget* targ,const AssetPath& assetname) {
  _initialTarget = targ;
  auto fxi       = targ->FXI();
  auto shass     = asset::AssetManager<FxShaderAsset>::Load(assetname.c_str());
  _shader        = shass->GetFxShader();
}

///////////////////////////////////////////////////////////////////////////////
// legacy methods
///////////////////////////////////////////////////////////////////////////////

inline bool FreestyleMaterial::BeginPass(GfxTarget* targ, int iPass) { return true; }
inline void FreestyleMaterial::EndPass(GfxTarget* targ) {}
inline int FreestyleMaterial::BeginBlock(GfxTarget* targ, const RenderContextInstData& RCID) { return 1; }
inline void FreestyleMaterial::EndBlock(GfxTarget* targ) {}

///////////////////////////////////////////////////////////////////////////////

inline void FreestyleMaterial::begin(const RenderContextFrameData& RCFD) {
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
inline void FreestyleMaterial::end(const RenderContextFrameData& RCFD) {
  auto targ = RCFD.GetTarget();
  targ->FXI()->EndPass(_shader);
  targ->FXI()->EndBlock(_shader);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
