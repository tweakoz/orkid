////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/material_freestyle.h>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
FreestyleMaterial::FreestyleMaterial() {
  _rasterstate.SetShadeModel(ESHADEMODEL_SMOOTH);
  _rasterstate.SetAlphaTest(EALPHATEST_OFF);
  _rasterstate.SetBlending(EBLENDING_OFF);
  _rasterstate.SetDepthTest(EDEPTHTEST_LEQUALS);
  _rasterstate.SetZWriteMask(true);
  _rasterstate.SetCullTest(ECULLTEST_PASS_BACK);
  miNumPasses = 1;
}
///////////////////////////////////////////////////////////////////////////////
FreestyleMaterial::~FreestyleMaterial() {
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::dump() const {

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
// legacy methods
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::Init(Context* targ) { // final
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::Update() { // final
}
///////////////////////////////////////////////////////////////////////////////
bool FreestyleMaterial::BeginPass(Context* targ, int iPass) { // final
  return targ->FXI()->BindPass(iPass);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::EndPass(Context* targ) { // final
  targ->FXI()->EndPass();
}
///////////////////////////////////////////////////////////////////////////////
int FreestyleMaterial::BeginBlock(Context* targ, const RenderContextInstData& RCID) { // final
  auto fxi    = targ->FXI();
  int npasses = fxi->BeginBlock(_selectedTEK, RCID);
  return npasses;
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::EndBlock(Context* targ) { // final
  targ->FXI()->EndBlock();
}
///////////////////////////////////////////////////////////////////////////////
// new style interface
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::gpuInit(Context* targ, const AssetPath& assetname) {
  if (_initialTarget == nullptr) {
    _initialTarget = targ;
    auto fxi       = targ->FXI();
    auto shass     = asset::AssetManager<FxShaderAsset>::Load(assetname.c_str());
    _shader        = shass->GetFxShader();
    OrkAssert(_shader);
  }
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::gpuInitFromShaderText(Context* targ, const std::string& shadername, const std::string& shadertext) {
  if (_initialTarget == nullptr) {
    _initialTarget = targ;
    _shader        = targ->FXI()->shaderFromShaderText(shadername, shadertext);
    OrkAssert(_shader);
  }
}
///////////////////////////////////////////////////////////////////////////////
const FxShaderTechnique* FreestyleMaterial::technique(std::string named) {
  auto fxi = _initialTarget->FXI();
  auto tek = fxi->technique(_shader, named);
  if (tek != nullptr)
    _techniques.insert(tek);
  return tek;
}
///////////////////////////////////////////////////////////////////////////////
const FxShaderParam* FreestyleMaterial::param(std::string named) {
  auto fxi = _initialTarget->FXI();
  auto par = fxi->parameter(_shader, named);
  if (par != nullptr)
    _params.insert(par);
  return par;
}
///////////////////////////////////////////////////////////////////////////////
const FxShaderParamBlock* FreestyleMaterial::paramBlock(std::string named) {
  auto fxi = _initialTarget->FXI();
  auto par = fxi->parameterBlock(_shader, named);
  if (par != nullptr)
    _paramBlocks.insert(par);
  return par;
}
////////////////////////////////////////////
void FreestyleMaterial::commit() {
  auto fxi = _initialTarget->FXI();
  fxi->CommitParams();
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindTechnique(const FxShaderTechnique* tek) {
  _selectedTEK = tek;
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamInt(const FxShaderParam* par, int value) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamInt(_shader, par, value);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamFloat(const FxShaderParam* par, float value) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamFloat(_shader, par, value);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamCTex(const FxShaderParam* par, const Texture* tex) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamCTex(_shader, par, tex);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamVec2(const FxShaderParam* par, const fvec2& v) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamVect2(_shader, par, v);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamVec3(const FxShaderParam* par, const fvec3& v) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamVect3(_shader, par, v);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamVec4(const FxShaderParam* par, const fvec4& v) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamVect4(_shader, par, v);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamMatrix(const FxShaderParam* par, const fmtx4& m) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamMatrix(_shader, par, m);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamMatrix(const FxShaderParam* par, const fmtx3& m) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamMatrix(_shader, par, m);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindParamMatrixArray(const FxShaderParam* par, const fmtx4* m, size_t len) {
  OrkAssert(par);
  auto fxi = _initialTarget->FXI();
  fxi->BindParamMatrixArray(_shader, par, m, len);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::begin(const FxShaderTechnique* tek, const RenderContextFrameData& RCFD) {
  auto targ = RCFD.GetTarget();
  auto fxi  = targ->FXI();
  auto rsi  = targ->RSI();
  RenderContextInstData RCID(&RCFD);
  _selectedTEK = tek;
  int npasses  = this->BeginBlock(targ, RCID);
  rsi->BindRasterState(_rasterstate);
  fxi->BindPass(0);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::begin(
    const FxShaderTechnique* tekMono,
    const FxShaderTechnique* tekStereo,
    const RenderContextFrameData& RCFD) {
  const auto& CPD = RCFD.topCPD();
  begin(CPD.isStereoOnePass() ? tekStereo : tekMono, RCFD);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::end(const RenderContextFrameData& RCFD) {
  auto targ = RCFD.GetTarget();
  this->EndPass(targ);
  this->EndBlock(targ);
}
////////////////////////////////////////////////////////////////////////////////
// Compute Shaders ?
////////////////////////////////////////////////////////////////////////////////
#if defined(ENABLE_COMPUTE_SHADERS)
const FxShaderStorageBlock* FreestyleMaterial::storageBlock(std::string named) {
  auto fxi = _initialTarget->FXI();
  auto blk = fxi->storageBlock(_shader, named);
  if (blk != nullptr)
    _storageBlocks.insert(blk);
  return blk;
}
///////////////////////////////////////////////////////////////////////////////
const FxComputeShader* FreestyleMaterial::computeShader(std::string named) {
  auto fxi = _initialTarget->FXI();
  auto tek = fxi->computeShader(_shader, named);
  if (tek != nullptr)
    _computeShaders.insert(tek);
  return tek;
}
#endif
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
