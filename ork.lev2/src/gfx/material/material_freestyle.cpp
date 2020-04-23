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
  return targ->FXI()->BindPass(_shader, iPass);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::EndPass(Context* targ) { // final
  targ->FXI()->EndPass(_shader);
}
///////////////////////////////////////////////////////////////////////////////
int FreestyleMaterial::BeginBlock(Context* targ, const RenderContextInstData& RCID) { // final
  auto fxi    = targ->FXI();
  int npasses = fxi->BeginBlock(_shader, RCID);
  return npasses;
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::EndBlock(Context* targ) { // final
  targ->FXI()->EndBlock(_shader);
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
void FreestyleMaterial::setInstanceMvpParams(
    materialinst_ptr_t materialinst, //
    std::string monocam,
    std::string stereocamL,
    std::string stereocamR) {
  if (auto mvp_mono = this->param(monocam)) {
    crcstring_ptr_t tok_mono = std::make_shared<CrcString>("RCFD_Camera_MVP_Mono");
    materialinst->_params[mvp_mono].Set<crcstring_ptr_t>(tok_mono);
    printf("tok_mono<0x%zx:%zu>\n", tok_mono->hashed(), tok_mono->hashed());
  }
  if (auto mvp_left = this->param(stereocamL)) {
    crcstring_ptr_t tok_stereoL = std::make_shared<CrcString>("RCFD_Camera_MVP_Left");
    materialinst->_params[mvp_left].Set<crcstring_ptr_t>(tok_stereoL);
    printf("tok_stereoL<0x%zx:%zu>\n", tok_stereoL->hashed(), tok_stereoL->hashed());
  }
  if (auto mvp_right = this->param(stereocamR)) {
    crcstring_ptr_t tok_stereoR = std::make_shared<CrcString>("RCFD_Camera_MVP_Right");
    materialinst->_params[mvp_right].Set<crcstring_ptr_t>(tok_stereoR);
    printf("tok_stereoR<0x%zx:%zu>\n", tok_stereoR->hashed(), tok_stereoR->hashed());
  }
}
////////////////////////////////////////////
void FreestyleMaterial::commit() {
  auto fxi = _initialTarget->FXI();
  fxi->CommitParams();
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::bindTechnique(const FxShaderTechnique* tek) {
  OrkAssert(tek);
  auto fxi = _initialTarget->FXI();
  fxi->BindTechnique(_shader, tek);
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
void FreestyleMaterial::begin(const RenderContextFrameData& RCFD) {
  auto targ = RCFD.GetTarget();
  auto fxi  = targ->FXI();
  auto rsi  = targ->RSI();
  RenderContextInstData RCID(&RCFD);
  int npasses = this->BeginBlock(targ, RCID);
  rsi->BindRasterState(_rasterstate);
  fxi->BindPass(_shader, 0);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::begin(const FxShaderTechnique* tek, const RenderContextFrameData& RCFD) {
  bindTechnique(tek);
  begin(RCFD);
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
///////////////////////////////////////////////////////////////////////////////
int FreestyleMaterial::materialInstanceBeginBlock(materialinst_ptr_t minst, const RenderContextInstData& RCID) {
  auto context    = RCID._RCFD->GetTarget();
  const auto& CPD = RCID._RCFD->topCPD();
  bool is_picking = CPD.isPicking();
  bool is_stereo  = CPD.isStereoOnePass();
  // auto tek     = minst->valueForKey("technique").Get<fxtechnique_constptr_t>();
  this->bindTechnique(is_stereo ? minst->_stereoTek : minst->_monoTek);
  return this->BeginBlock(context, RCID);
}
///////////////////////////////////////////////////////////////////////////////
bool FreestyleMaterial::materialInstanceBeginPass(materialinst_ptr_t minst, const RenderContextInstData& RCID, int ipass) {
  auto context    = RCID._RCFD->GetTarget();
  auto MTXI       = context->MTXI();
  const auto& CPD = RCID._RCFD->topCPD();
  bool is_picking = CPD.isPicking();
  bool is_stereo  = CPD.isStereoOnePass();

  bool rval = this->BeginPass(context, ipass);

  const auto& worldmatrix = RCID._dagrenderable //
                                ? RCID._dagrenderable->_worldMatrix
                                : MTXI->RefMMatrix();

  auto stereocams = CPD._stereoCameraMatrices;
  auto monocams   = CPD._cameraMatrices;

  for (auto item : minst->_params) {
    fxparam_constptr_t param = item.first;
    const varmap::val_t& val = item.second;
    if (auto as_mtx4 = val.TryAs<fmtx4_ptr_t>()) {
      this->bindParamMatrix(param, *as_mtx4.value().get());
    } else if (auto as_crcstr = val.TryAs<crcstring_ptr_t>()) {
      const auto& crcstr = *as_crcstr.value().get();
      switch (crcstr.hashed()) {
        case "RCFD_Camera_MVP_Mono"_crcu: {
          if (monocams) {
            this->bindParamMatrix(param, monocams->MVPMONO(worldmatrix));
          } else {
            auto MVP = worldmatrix * MTXI->RefVPMatrix();
            this->bindParamMatrix(param, MVP);
          }
          break;
        }
        case "RCFD_Camera_MVP_Left"_crcu: {
          if (is_stereo and stereocams) {
            this->bindParamMatrix(param, stereocams->MVPL(worldmatrix));
          }
          break;
        }
        case "RCFD_Camera_MVP_Right"_crcu: {
          if (is_stereo and stereocams) {
            this->bindParamMatrix(param, stereocams->MVPR(worldmatrix));
          }
          break;
        }
        default:
          OrkAssert(false);
          break;
      }
    } else if (auto as_fvec4_ = val.TryAs<fvec4_ptr_t>()) {
      this->bindParamVec4(param, *as_fvec4_.value().get());
    } else if (auto as_fvec3 = val.TryAs<fvec3_ptr_t>()) {
      this->bindParamVec3(param, *as_fvec3.value().get());
    } else if (auto as_fvec2 = val.TryAs<fvec2_ptr_t>()) {
      this->bindParamVec2(param, *as_fvec2.value().get());
    } else if (auto as_fmtx3 = val.TryAs<fmtx3_ptr_t>()) {
      this->bindParamMatrix(param, *as_fmtx3.value().get());
    } else if (auto as_fquat = val.TryAs<fquat_ptr_t>()) {
      const auto& Q = *as_fquat.value().get();
      fvec4 as_vec4(Q.x, Q.y, Q.z, Q.w);
      this->bindParamVec4(param, as_vec4);
    } else if (auto as_fplane3 = val.TryAs<fplane3_ptr_t>()) {
      const auto& P = *as_fplane3.value().get();
      fvec4 as_vec4(P.n, P.d);
      this->bindParamVec4(param, as_vec4);
    } else if (auto as_texture = val.TryAs<Texture*>()) {
      auto texture = as_texture.value();
      this->bindParamCTex(param, texture);
    } else {
      OrkAssert(false);
    }
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::materialInstanceEndPass(materialinst_ptr_t minst, const RenderContextInstData& RCID) {
  auto context = RCID._RCFD->GetTarget();
  this->EndPass(context);
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleMaterial::materialInstanceEndBlock(materialinst_ptr_t minst, const RenderContextInstData& RCID) {
  auto context = RCID._RCFD->GetTarget();
  this->EndBlock(context);
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
