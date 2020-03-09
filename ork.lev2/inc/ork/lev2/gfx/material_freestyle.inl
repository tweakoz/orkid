////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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

struct FreestyleMaterial : public GfxMaterial {

  FreestyleMaterial();
  ~FreestyleMaterial() final;

  void dump() const;

  ////////////////////////////////////////////

  void begin(const RenderContextFrameData& RCFD);
  void end(const RenderContextFrameData& RCFD);

  ////////////////////////////////////////////

  bool BeginPass(Context* targ, int iPass = 0) final;
  void EndPass(Context* targ) final;
  int BeginBlock(Context* targ, const RenderContextInstData& RCID) final;
  void EndBlock(Context* targ) final;
  void gpuInit(Context* targ, const AssetPath& assetname);
  void gpuInitFromShaderText(Context* targ, const std::string& shadername, const std::string& shadertext);
  void Init(Context* targ) final {
  }
  void Update() final {
  }

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
  inline void bindParamMatrix(const FxShaderParam* par, const fmtx3& m) {
    auto fxi = _initialTarget->FXI();
    fxi->BindParamMatrix(_shader, par, m);
  }
  inline void bindParamMatrixArray(const FxShaderParam* par, const fmtx4* m, size_t len) {
    auto fxi = _initialTarget->FXI();
    fxi->BindParamMatrixArray(_shader, par, m, len);
  }

#if defined(ENABLE_COMPUTE_SHADERS)
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

  Context* _initialTarget = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

inline FreestyleMaterial::FreestyleMaterial() {
  _rasterstate.SetShadeModel(ESHADEMODEL_SMOOTH);
  _rasterstate.SetAlphaTest(EALPHATEST_OFF);
  _rasterstate.SetBlending(EBLENDING_OFF);
  _rasterstate.SetDepthTest(EDEPTHTEST_LEQUALS);
  _rasterstate.SetZWriteMask(true);
  _rasterstate.SetCullTest(ECULLTEST_PASS_BACK);
  miNumPasses = 1;
}

///////////////////////////////////////////////////////////////////////////////

inline FreestyleMaterial::~FreestyleMaterial() {
}

///////////////////////////////////////////////////////////////////////////////

inline void FreestyleMaterial::dump() const {

  printf("freestylematerial<%p>\n", this);
  printf("fxshader<%p>\n", _shader);

  for (auto item : _shader->_techniques) {

    auto name = item.first;
    auto tek  = item.second;
    printf("tek<%p:%s> valid<%d>\n", tek, name.c_str(), int(tek->mbValidated));
  }
  for (auto item : _shader->_parameterByName) {
    auto name = item.first;
    auto par  = item.second;
    printf("par<%p:%s> type<%s>\n", par, name.c_str(), par->mParameterType.c_str());
  }
  for (auto item : _shader->_parameterBlockByName) {
    auto name   = item.first;
    auto parblk = item.second;
    printf("parblk<%p:%s>\n", parblk, name.c_str());
  }
  for (auto item : _shader->_computeShaderByName) {
    auto name = item.first;
    auto csh  = item.second;
    printf("csh<%p:%s>\n", csh, name.c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////

inline void FreestyleMaterial::gpuInit(Context* targ, const AssetPath& assetname) {
  if (_initialTarget == nullptr) {
    _initialTarget = targ;
    auto fxi       = targ->FXI();
    auto shass     = asset::AssetManager<FxShaderAsset>::Load(assetname.c_str());
    _shader        = shass->GetFxShader();
    OrkAssert(_shader);
  }
}

///////////////////////////////////////////////////////////////////////////////

inline void FreestyleMaterial::gpuInitFromShaderText(Context* targ, const std::string& shadername, const std::string& shadertext) {
  if (_initialTarget == nullptr) {
    _initialTarget = targ;
    _shader        = targ->FXI()->shaderFromShaderText(shadername, shadertext);
    OrkAssert(_shader);
  }
}

///////////////////////////////////////////////////////////////////////////////
// legacy methods
///////////////////////////////////////////////////////////////////////////////

inline bool FreestyleMaterial::BeginPass(Context* targ, int iPass) {
  return true;
}
inline void FreestyleMaterial::EndPass(Context* targ) {
}
inline int FreestyleMaterial::BeginBlock(Context* targ, const RenderContextInstData& RCID) {
  return 1;
}
inline void FreestyleMaterial::EndBlock(Context* targ) {
}

///////////////////////////////////////////////////////////////////////////////

inline void FreestyleMaterial::begin(const RenderContextFrameData& RCFD) {
  auto targ = RCFD.GetTarget();
  // const auto& CPD  = RCFD.topCPD();
  // bool stereo1pass = CPD.isStereoOnePass();
  auto mtxi = targ->MTXI();
  auto fxi  = targ->FXI();
  auto rsi  = targ->RSI();

  RenderContextInstData RCID(RCFD);
  int npasses = fxi->BeginBlock(_shader, RCID);
  rsi->BindRasterState(_rasterstate);
  fxi->BindPass(_shader, 0);
}
inline void FreestyleMaterial::end(const RenderContextFrameData& RCFD) {
  auto targ = RCFD.GetTarget();
  targ->FXI()->EndPass(_shader);
  targ->FXI()->EndBlock(_shader);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
